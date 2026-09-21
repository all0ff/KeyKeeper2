#include "security/pin_manager.hpp"

#include "event_bus/event_bus.hpp"
#include "settings/settings.hpp"
#include "storage/nvs_storage.hpp"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "psa/crypto.h"

#include <cstring>

namespace security::pin {

namespace {

constexpr char TAG[] = "security.pin";
constexpr char NVS_NAMESPACE[] = "security";
constexpr char NVS_KEY[] = "pin";
constexpr char NVS_CHECKPOINT_KEY[] = "pin_ckpt";

constexpr uint8_t FAILURE_CHECKPOINT_1 = 3;
constexpr uint8_t FAILURE_LOCKOUT = 6;
constexpr uint8_t FAILURE_CHECKPOINT_3 = 9;
constexpr uint8_t WIPE_THRESHOLD = 12;
constexpr uint32_t LOCKOUT_DURATION_MS = 30'000;

enum class Checkpoint : uint32_t
{
    None = 0,
    At3 = 1,
    At6Lockout = 2,
    At9 = 3,
};

constexpr size_t SALT_LEN = 16;
constexpr size_t HASH_LEN = 32;
// TEMPORARY TEST PROFILE: new PINs use 10k iterations.
// Existing PIN blobs with reserved[0] == 0 remain on the original 100k cost.
constexpr uint32_t PBKDF2_ITERATIONS_LEGACY = 100'000;
constexpr uint32_t PBKDF2_ITERATIONS_TEST = 10'000;
constexpr uint8_t PBKDF2_PROFILE_LEGACY = 0;
constexpr uint8_t PBKDF2_PROFILE_TEST = 1;
constexpr uint32_t PIN_BLOB_MAGIC = 0x4B4B5032; // "KKP2"
constexpr uint8_t PIN_BLOB_VERSION = 1;
constexpr uint8_t DURESS_PIN_BLOB_VERSION = 2;

struct StoredPin
{
    uint32_t magic;
    uint8_t version;
    uint8_t pin_length;
    uint8_t reserved[2];
    uint8_t salt[SALT_LEN];
    uint8_t hash[HASH_LEN];
};

bool initialized = false;
bool pin_set = false;
StoredPin stored{};

constexpr char NVS_DURESS_KEY[] = "duress_pin";
bool duress_pin_set = false;
StoredPin stored_duress{};

uint8_t consecutive_failures = 0;
uint32_t lockout_started_ms = 0;
bool locked_out = false;

bool psa_ready = false;

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

bool compute_hash(const uint8_t* salt, const char* pin_digits,
                  uint32_t iterations, uint8_t out_hash[HASH_LEN])
{
    const size_t pin_len = strlen(pin_digits);

    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
    psa_status_t status = psa_key_derivation_setup(
        &operation, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_key_derivation_setup failed (status %d)", static_cast<int>(status));
        return false;
    }

    status = psa_key_derivation_input_integer(
        &operation, PSA_KEY_DERIVATION_INPUT_COST, iterations);
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_bytes(
            &operation, PSA_KEY_DERIVATION_INPUT_SALT, salt, SALT_LEN);
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_bytes(
            &operation, PSA_KEY_DERIVATION_INPUT_PASSWORD,
            reinterpret_cast<const uint8_t*>(pin_digits), pin_len);
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_set_capacity(&operation, HASH_LEN);
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_output_bytes(&operation, out_hash, HASH_LEN);
    }

    psa_key_derivation_abort(&operation);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "PBKDF2 failed (status %d)", static_cast<int>(status));
        return false;
    }

    return true;
}

bool compute_duress_hash(const uint8_t* salt, const char* pin_digits,
                         uint8_t out_hash[HASH_LEN])
{
    const size_t pin_len = strlen(pin_digits);
    uint8_t input[SALT_LEN + 6]{};

    std::memcpy(input, salt, SALT_LEN);
    std::memcpy(input + SALT_LEN, pin_digits, pin_len);

    size_t hash_len = 0;
    const psa_status_t status = psa_hash_compute(
        PSA_ALG_SHA_256, input, SALT_LEN + pin_len,
        out_hash, HASH_LEN, &hash_len);

    std::memset(input, 0, sizeof(input));

    if (status != PSA_SUCCESS || hash_len != HASH_LEN) {
        ESP_LOGE(TAG, "Duress SHA-256 failed (status %d)", static_cast<int>(status));
        std::memset(out_hash, 0, HASH_LEN);
        return false;
    }

    return true;
}

void generate_salt(uint8_t out_salt[SALT_LEN])
{
    esp_fill_random(out_salt, SALT_LEN);
}

bool pin_format_ok(const char* pin)
{
    if (pin == nullptr) {
        return false;
    }

    const size_t len = strlen(pin);
    if (len < 4 || len > 6) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        if (pin[i] < '0' || pin[i] > '9') {
            return false;
        }
    }

    return true;
}

bool pin_length_ok(const char* pin)
{
    if (!pin_format_ok(pin)) {
        return false;
    }
    return strlen(pin) == settings::all().security.pin_length;
}

bool stored_pin_valid(const StoredPin& value)
{
    return value.magic == PIN_BLOB_MAGIC && value.version == PIN_BLOB_VERSION;
}

bool stored_duress_pin_valid(const StoredPin& value)
{
    return value.magic == PIN_BLOB_MAGIC && value.version == DURESS_PIN_BLOB_VERSION;
}

Checkpoint load_checkpoint_from_flash()
{
    uint32_t value = 0;
    if (!storage::nvs::get_u32(NVS_NAMESPACE, NVS_CHECKPOINT_KEY, value) || value > 3) {
        return Checkpoint::None;
    }
    return static_cast<Checkpoint>(value);
}

void save_checkpoint_to_flash(Checkpoint checkpoint)
{
    storage::nvs::set_u32(NVS_NAMESPACE, NVS_CHECKPOINT_KEY, static_cast<uint32_t>(checkpoint));
}

uint8_t checkpoint_failure_count(Checkpoint checkpoint)
{
    switch (checkpoint) {
        case Checkpoint::None: return 0;
        case Checkpoint::At3: return FAILURE_CHECKPOINT_1;
        case Checkpoint::At6Lockout: return FAILURE_LOCKOUT;
        case Checkpoint::At9: return FAILURE_CHECKPOINT_3;
    }
    return 0;
}

void reset_failure_state()
{
    consecutive_failures = 0;
    locked_out = false;
    lockout_started_ms = 0;

    if (load_checkpoint_from_flash() != Checkpoint::None) {
        save_checkpoint_to_flash(Checkpoint::None);
    }
}

void check_lockout_expiry()
{
    if (locked_out && (now_ms() - lockout_started_ms >= LOCKOUT_DURATION_MS)) {
        locked_out = false;
        lockout_started_ms = 0;
        ESP_LOGI(TAG, "Lockout expired, failure counter retained at %u",
                 static_cast<unsigned>(consecutive_failures));
    }
}

void register_failure()
{
    if (consecutive_failures < WIPE_THRESHOLD) {
        ++consecutive_failures;
    }

    if (consecutive_failures == FAILURE_CHECKPOINT_1) {
        save_checkpoint_to_flash(Checkpoint::At3);
    } else if (consecutive_failures == FAILURE_LOCKOUT) {
        save_checkpoint_to_flash(Checkpoint::At6Lockout);
    } else if (consecutive_failures == FAILURE_CHECKPOINT_3) {
        save_checkpoint_to_flash(Checkpoint::At9);
    }
}

void start_lockout_if_needed()
{
    if (consecutive_failures >= FAILURE_LOCKOUT && !locked_out) {
        locked_out = true;
        lockout_started_ms = now_ms();
        ESP_LOGW(TAG, "Too many failed PIN attempts, locked out for %u ms",
                 static_cast<unsigned>(LOCKOUT_DURATION_MS));
    }
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!psa_ready) {
        const psa_status_t status = psa_crypto_init();
        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_crypto_init failed (status %d)", static_cast<int>(status));
            return false;
        }
        psa_ready = true;
    }

    size_t len = sizeof(stored);
    if (storage::nvs::get_blob(NVS_NAMESPACE, NVS_KEY, &stored, len) &&
        len == sizeof(stored) && stored_pin_valid(stored)) {
        pin_set = true;
        ESP_LOGI(TAG, "PIN loaded from NVS");

        if (stored.pin_length < 4 || stored.pin_length > 6) {
            const uint8_t fallback = settings::all().security.pin_length;
            stored.pin_length = (fallback >= 4 && fallback <= 6) ? fallback : 6;
            ESP_LOGI(TAG, "PIN blob predates pin_length -- using %u for this session",
                     static_cast<unsigned>(stored.pin_length));
        }
    } else {
        pin_set = false;
        ESP_LOGI(TAG, "No valid PIN set yet (first boot or old PIN format)");
    }

    size_t duress_len = sizeof(stored_duress);
    if (storage::nvs::get_blob(NVS_NAMESPACE, NVS_DURESS_KEY, &stored_duress, duress_len) &&
        duress_len == sizeof(stored_duress) && stored_duress_pin_valid(stored_duress)) {
        duress_pin_set = true;
        ESP_LOGI(TAG, "Duress PIN loaded from NVS");
    } else {
        duress_pin_set = false;
    }

    const Checkpoint saved_checkpoint = load_checkpoint_from_flash();
    consecutive_failures = checkpoint_failure_count(saved_checkpoint);
    locked_out = false;
    lockout_started_ms = 0;

    if (saved_checkpoint == Checkpoint::At6Lockout) {
        locked_out = true;
        lockout_started_ms = now_ms();
        ESP_LOGW(TAG, "Restored lockout checkpoint from flash -- 30s lockout restarted from boot");
    } else if (saved_checkpoint != Checkpoint::None) {
        ESP_LOGI(TAG, "Restored failure checkpoint from flash: %u consecutive failures",
                 static_cast<unsigned>(consecutive_failures));
    }

    initialized = true;
    return true;
}

bool is_initialized()
{
    return initialized;
}

bool has_pin()
{
    return pin_set;
}

uint8_t stored_pin_length()
{
    if (!pin_set) {
        return 6;
    }
    if (stored.pin_length < 4 || stored.pin_length > 6) {
        return 6;
    }
    return stored.pin_length;
}

bool store_new_pin(const char* new_pin)
{
    if (!pin_length_ok(new_pin)) {
        return false;
    }

    StoredPin next{};
    next.magic = PIN_BLOB_MAGIC;
    next.version = PIN_BLOB_VERSION;
    next.pin_length = static_cast<uint8_t>(std::strlen(new_pin));
    next.reserved[0] = PBKDF2_PROFILE_TEST;
    generate_salt(next.salt);
    if (!compute_hash(next.salt, new_pin, PBKDF2_ITERATIONS_TEST, next.hash)) {
        ESP_LOGE(TAG, "store_new_pin: PBKDF2 computation failed");
        return false;
    }

    if (!storage::nvs::set_blob(NVS_NAMESPACE, NVS_KEY, &next, sizeof(next))) {
        ESP_LOGE(TAG, "Failed to persist PIN to NVS");
        return false;
    }

    stored = next;
    pin_set = true;
    reset_failure_state();
    clear_duress_pin();

    if (event_bus::is_initialized()) {
        event_bus::publish(event_bus::Category::System,
                            static_cast<uint32_t>(event_bus::SystemEventId::PinChanged));
    }

    ESP_LOGI(TAG, "PIN changed (temporary PBKDF2 profile: 10k iterations)");
    return true;
}

bool set_pin(const char* new_pin, const char* old_pin)
{
    if (!initialized) {
        return false;
    }

    if (pin_set) {
        if (old_pin == nullptr || verify(old_pin) != VerifyResult::Success) {
            ESP_LOGW(TAG, "set_pin: old PIN verification failed");
            return false;
        }
    }

    return store_new_pin(new_pin);
}

bool set_pin_after_verify(const char* new_pin)
{
    if (!initialized || !pin_set) {
        return false;
    }

    return store_new_pin(new_pin);
}

bool wipe()
{
    if (!initialized) {
        return false;
    }

    if (!storage::nvs::erase_key(NVS_NAMESPACE, NVS_KEY)) {
        ESP_LOGE(TAG, "Failed to erase PIN from NVS");
        return false;
    }

    std::memset(&stored, 0, sizeof(stored));
    pin_set = false;
    reset_failure_state();

    ESP_LOGW(TAG, "Stored PIN erased as part of automatic wipe");
    return true;
}

VerifyResult verify(const char* pin)
{
    if (!initialized || !pin_set) {
        return VerifyResult::NoPinSet;
    }

    check_lockout_expiry();

    if (locked_out) {
        return VerifyResult::LockedOut;
    }

    if (!pin_format_ok(pin)) {
        register_failure();
        if (consecutive_failures >= WIPE_THRESHOLD) {
            ESP_LOGE(TAG, "PIN failure threshold %u reached: wipe required",
                     static_cast<unsigned>(WIPE_THRESHOLD));
            return VerifyResult::WipeRequired;
        }
        start_lockout_if_needed();
        return VerifyResult::WrongPin;
    }

    const uint32_t iterations =
        (stored.reserved[0] == PBKDF2_PROFILE_TEST) ?
        PBKDF2_ITERATIONS_TEST : PBKDF2_ITERATIONS_LEGACY;

    uint8_t candidate_hash[HASH_LEN]{};
    if (!compute_hash(stored.salt, pin, iterations, candidate_hash)) {
        return VerifyResult::WrongPin;
    }

    uint8_t diff = 0;
    for (size_t i = 0; i < HASH_LEN; ++i) {
        diff |= static_cast<uint8_t>(candidate_hash[i] ^ stored.hash[i]);
    }

    if (diff == 0) {
        reset_failure_state();
        std::memset(candidate_hash, 0, sizeof(candidate_hash));
        return VerifyResult::Success;
    }

    register_failure();
    std::memset(candidate_hash, 0, sizeof(candidate_hash));

    if (consecutive_failures >= WIPE_THRESHOLD) {
        ESP_LOGE(TAG, "PIN failure threshold %u reached: wipe required",
                 static_cast<unsigned>(WIPE_THRESHOLD));
        return VerifyResult::WipeRequired;
    }

    start_lockout_if_needed();
    return VerifyResult::WrongPin;
}

uint8_t attempts_remaining()
{
    check_lockout_expiry();
    if (locked_out || consecutive_failures >= FAILURE_LOCKOUT) {
        return 0;
    }
    return FAILURE_LOCKOUT - consecutive_failures;
}

uint8_t attempts_until_wipe()
{
    if (consecutive_failures >= WIPE_THRESHOLD) {
        return 0;
    }
    return WIPE_THRESHOLD - consecutive_failures;
}

bool is_locked_out()
{
    check_lockout_expiry();
    return locked_out;
}

bool has_duress_pin()
{
    return duress_pin_set;
}

bool store_duress_pin(const char* duress_pin, const char* current_pin)
{
    if (!pin_format_ok(duress_pin)) {
        return false;
    }
    if (strlen(duress_pin) != strlen(current_pin)) {
        ESP_LOGW(TAG, "set_duress_pin: duress PIN must be the same length as the current PIN");
        return false;
    }
    if (std::strcmp(duress_pin, current_pin) == 0) {
        ESP_LOGW(TAG, "set_duress_pin: duress PIN must differ from the regular PIN");
        return false;
    }

    StoredPin next{};
    next.magic = PIN_BLOB_MAGIC;
    next.version = DURESS_PIN_BLOB_VERSION;
    generate_salt(next.salt);
    if (!compute_duress_hash(next.salt, duress_pin, next.hash)) {
        ESP_LOGE(TAG, "set_duress_pin: SHA-256 computation failed");
        return false;
    }

    if (!storage::nvs::set_blob(NVS_NAMESPACE, NVS_DURESS_KEY, &next, sizeof(next))) {
        ESP_LOGE(TAG, "Failed to persist duress PIN to NVS");
        return false;
    }

    stored_duress = next;
    duress_pin_set = true;
    ESP_LOGI(TAG, "Duress PIN configured");
    return true;
}

bool set_duress_pin(const char* duress_pin, const char* current_pin)
{
    if (!initialized || !pin_set) {
        ESP_LOGW(TAG, "set_duress_pin: no regular PIN configured yet");
        return false;
    }
    if (current_pin == nullptr || verify(current_pin) != VerifyResult::Success) {
        ESP_LOGW(TAG, "set_duress_pin: current PIN verification failed");
        return false;
    }

    return store_duress_pin(duress_pin, current_pin);
}

bool set_duress_pin_after_verify(const char* duress_pin, const char* current_pin)
{
    if (!initialized || !pin_set || current_pin == nullptr) {
        return false;
    }

    return store_duress_pin(duress_pin, current_pin);
}

bool clear_duress_pin()
{
    if (!initialized) {
        return false;
    }
    if (!duress_pin_set) {
        return true;
    }

    if (!storage::nvs::erase_key(NVS_NAMESPACE, NVS_DURESS_KEY)) {
        ESP_LOGE(TAG, "Failed to erase duress PIN from NVS");
        return false;
    }

    std::memset(&stored_duress, 0, sizeof(stored_duress));
    duress_pin_set = false;
    ESP_LOGI(TAG, "Duress PIN cleared");
    return true;
}

bool verify_duress(const char* pin)
{
    if (!initialized || !duress_pin_set || !pin_format_ok(pin)) {
        return false;
    }

    uint8_t candidate_hash[HASH_LEN]{};
    if (!compute_duress_hash(stored_duress.salt, pin, candidate_hash)) {
        return false;
    }

    uint8_t diff = 0;
    for (size_t i = 0; i < HASH_LEN; ++i) {
        diff |= static_cast<uint8_t>(candidate_hash[i] ^ stored_duress.hash[i]);
    }
    std::memset(candidate_hash, 0, sizeof(candidate_hash));

    return diff == 0;
}

void consume_pbkdf2_time()
{
    uint8_t dummy_salt[SALT_LEN]{};
    uint8_t dummy_hash[HASH_LEN]{};
    compute_hash(dummy_salt, "000000", PBKDF2_ITERATIONS_TEST, dummy_hash);
    std::memset(dummy_hash, 0, sizeof(dummy_hash));
}

} // namespace security::pin
