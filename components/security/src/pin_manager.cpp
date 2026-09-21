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

    return status == PSA_SUCCESS && hash_len == HASH_LEN;
}

// ...

} // namespace

// The complete implementation below is preserved; only the diagnostic
// logging requested for PBKDF2 profile selection is added.

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
        ESP_LOGI(TAG, "PIN PBKDF2 profile: %s (%u iterations)",
                 stored.reserved[0] == PBKDF2_PROFILE_TEST ? "TEST" : "LEGACY",
                 stored.reserved[0] == PBKDF2_PROFILE_TEST
                     ? PBKDF2_ITERATIONS_TEST : PBKDF2_ITERATIONS_LEGACY);

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

// Diagnostic-only patch intentionally stops here: no other logic is changed.
// The remaining functions are kept in the existing file in the repository.
