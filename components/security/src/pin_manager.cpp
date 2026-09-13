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

// Staged anti-bruteforce thresholds -- see pin_manager.hpp's verify()
// doc comment. Checkpoint values persisted to flash are a plain 0-3
// state (conceptually 2 bits -- "4 states" was the design goal, not
// any particular bit pattern; a linear 0/1/2/3 enum is exactly as
// compact and much more readable than trying to match a specific
// non-sequential bit encoding).
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
constexpr uint32_t PBKDF2_ITERATIONS = 100'000;
constexpr uint32_t PIN_BLOB_MAGIC = 0x4B4B5032; // "KKP2"
constexpr uint8_t PIN_BLOB_VERSION = 1;

struct StoredPin
{
    uint32_t magic;
    uint8_t version;
    uint8_t reserved[3];
    uint8_t salt[SALT_LEN];
    uint8_t hash[HASH_LEN];
};

bool initialized = false;
bool pin_set = false;
StoredPin stored{};

uint8_t consecutive_failures = 0;
uint32_t lockout_started_ms = 0;
bool locked_out = false;

bool psa_ready = false;

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

bool compute_hash(const uint8_t* salt, const char* pin_digits, uint8_t out_hash[HASH_LEN])
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
        &operation, PSA_KEY_DERIVATION_INPUT_COST, PBKDF2_ITERATIONS);
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

void generate_salt(uint8_t out_salt[SALT_LEN])
{
    esp_fill_random(out_salt, SALT_LEN);
}

bool pin_length_ok(const char* pin)
{
    if (pin == nullptr) {
        return false;
    }

    const size_t len = strlen(pin);
    const uint8_t configured_length = settings::all().security.pin_length;

    if (len != configured_length || len < 4 || len > 6) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        if (pin[i] < '0' || pin[i] > '9') {
            return false;
        }
    }

    return true;
}

bool stored_pin_valid(const StoredPin& value)
{
    return value.magic == PIN_BLOB_MAGIC && value.version == PIN_BLOB_VERSION;
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

    // Avoid an unconditional flash write on every successful unlock --
    // only touch flash if there was actually a checkpoint to clear.
    // Reading first costs nothing (NVS reads don't wear flash).
    if (load_checkpoint_from_flash() != Checkpoint::None) {
        save_checkpoint_to_flash(Checkpoint::None);
    }
}

void check_lockout_expiry()
{
    if (locked_out && (now_ms() - lockout_started_ms >= LOCKOUT_DURATION_MS)) {
        // The lockout expires, but the failure counter is deliberately
        // retained so attempts 7..11 remain part of the same sequence
        // toward the 12th-failure wipe -- see verify()'s doc comment.
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

    // Checkpoint writes only happen at these three thresholds -- the
    // 12th failure (WIPE_THRESHOLD) intentionally writes nothing;
    // wipe() erases NVS_CHECKPOINT_KEY along with the PIN itself.
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
    } else {
        pin_set = false;
        ESP_LOGI(TAG, "No valid PIN set yet (first boot or old PIN format)");
    }

    // Restore failure progress from the flash checkpoint, NOT a plain
    // reset -- see verify()'s doc comment for why: a reboot must never
    // let an attacker regain attempts below the last checkpoint
    // reached before the reboot.
    const Checkpoint saved_checkpoint = load_checkpoint_from_flash();
    consecutive_failures = checkpoint_failure_count(saved_checkpoint);
    locked_out = false;
    lockout_started_ms = 0;

    if (saved_checkpoint == Checkpoint::At6Lockout) {
        // Rebooted during or shortly after the lockout window. We
        // can't know how much of the original 30s had already
        // elapsed (esp_timer_get_time() resets on reboot), so
        // conservatively restart the full lockout from boot --
        // rebooting can only ever cost an attacker more time, never
        // less.
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

bool set_pin(const char* new_pin, const char* old_pin)
{
    if (!initialized || !pin_length_ok(new_pin)) {
        return false;
    }

    if (pin_set) {
        if (old_pin == nullptr || verify(old_pin) != VerifyResult::Success) {
            ESP_LOGW(TAG, "set_pin: old PIN verification failed");
            return false;
        }
    }

    StoredPin next{};
    next.magic = PIN_BLOB_MAGIC;
    next.version = PIN_BLOB_VERSION;
    generate_salt(next.salt);
    if (!compute_hash(next.salt, new_pin, next.hash)) {
        ESP_LOGE(TAG, "set_pin: PBKDF2 computation failed");
        return false;
    }

    if (!storage::nvs::set_blob(NVS_NAMESPACE, NVS_KEY, &next, sizeof(next))) {
        ESP_LOGE(TAG, "Failed to persist PIN to NVS");
        return false;
    }

    stored = next;
    pin_set = true;
    reset_failure_state();

    if (event_bus::is_initialized()) {
        event_bus::publish(event_bus::Category::System,
                            static_cast<uint32_t>(event_bus::SystemEventId::PinChanged));
    }

    ESP_LOGI(TAG, "PIN changed");
    return true;
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

    if (!pin_length_ok(pin)) {
        register_failure();
        if (consecutive_failures >= WIPE_THRESHOLD) {
            ESP_LOGE(TAG, "PIN failure threshold %u reached: wipe required",
                     static_cast<unsigned>(WIPE_THRESHOLD));
            return VerifyResult::WipeRequired;
        }
        start_lockout_if_needed();
        return VerifyResult::WrongPin;
    }

    uint8_t candidate_hash[HASH_LEN]{};
    if (!compute_hash(stored.salt, pin, candidate_hash)) {
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

} // namespace security::pin
