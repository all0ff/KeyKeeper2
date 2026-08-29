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

constexpr uint8_t MAX_ATTEMPTS = 5;
// Placeholder, not a considered anti-bruteforce policy -- see
// components/security/README.md.
constexpr uint32_t LOCKOUT_DURATION_MS = 30'000;

constexpr size_t SALT_LEN = 16;
constexpr size_t HASH_LEN = 32; // SHA-256 digest size

struct StoredPin
{
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

/**
 * ESP-IDF v6.0 moved to Mbed TLS 4.x, which removed the legacy
 * mbedtls_sha256_*() API in favor of PSA Crypto (see
 * components/security/README.md). This computes salt || pin -> SHA-256
 * in one call via psa_hash_compute() -- still a single, unstretched
 * hash, not a KDF; see the scope note at the top of pin_manager.hpp.
 */
bool compute_hash(const uint8_t* salt, const char* pin_digits, uint8_t out_hash[HASH_LEN])
{
    const size_t pin_len = strlen(pin_digits);

    // PIN length is validated elsewhere (pin_length_ok(), 4-6 digits),
    // so this headroom is generous, not a tight fit.
    uint8_t buffer[SALT_LEN + 32];
    if (pin_len > sizeof(buffer) - SALT_LEN) {
        ESP_LOGE(TAG, "PIN too long for hash buffer (%u chars)", static_cast<unsigned>(pin_len));
        return false;
    }

    memcpy(buffer, salt, SALT_LEN);
    memcpy(buffer + SALT_LEN, pin_digits, pin_len);

    size_t hash_len_out = 0;
    const psa_status_t status = psa_hash_compute(
        PSA_ALG_SHA_256, buffer, SALT_LEN + pin_len, out_hash, HASH_LEN, &hash_len_out);

    if (status != PSA_SUCCESS || hash_len_out != HASH_LEN) {
        ESP_LOGE(TAG, "psa_hash_compute failed (status %d)", static_cast<int>(status));
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
    const uint8_t expected = settings::all().security.pin_length;
    // Trust the configured preference, but never accept outside the
    // 4-6 digit range REQUIREMENTS 12.3 specifies, even if settings
    // somehow held something else.
    if (expected < 4 || expected > 6) {
        return len >= 4 && len <= 6;
    }
    return len == expected;
}

void reset_lockout_state()
{
    consecutive_failures = 0;
    locked_out = false;
}

void check_lockout_expiry()
{
    if (locked_out && (now_ms() - lockout_started_ms >= LOCKOUT_DURATION_MS)) {
        ESP_LOGI(TAG, "Lockout expired, attempts reset");
        reset_lockout_state();
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
    if (storage::nvs::get_blob(NVS_NAMESPACE, NVS_KEY, &stored, len) && len == sizeof(stored)) {
        pin_set = true;
        ESP_LOGI(TAG, "PIN loaded from NVS");
    } else {
        pin_set = false;
        ESP_LOGI(TAG, "No PIN set yet (first boot or NVS entry missing/mismatched)");
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
    generate_salt(next.salt);
    if (!compute_hash(next.salt, new_pin, next.hash)) {
        ESP_LOGE(TAG, "set_pin: hash computation failed");
        return false;
    }

    if (!storage::nvs::set_blob(NVS_NAMESPACE, NVS_KEY, &next, sizeof(next))) {
        ESP_LOGE(TAG, "Failed to persist PIN to NVS");
        return false;
    }

    stored = next;
    pin_set = true;
    reset_lockout_state();

    if (event_bus::is_initialized()) {
        event_bus::publish(event_bus::Category::System,
                            static_cast<uint32_t>(event_bus::SystemEventId::PinChanged));
    }

    ESP_LOGI(TAG, "PIN changed");
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

    if (pin == nullptr) {
        return VerifyResult::WrongPin;
    }

    uint8_t candidate_hash[HASH_LEN];
    if (!compute_hash(stored.salt, pin, candidate_hash)) {
        return VerifyResult::WrongPin;
    }

    // Constant-time-ish compare: XOR every byte instead of an early-
    // exit memcmp, so guess timing doesn't leak how many leading
    // bytes matched.
    uint8_t diff = 0;
    for (size_t i = 0; i < HASH_LEN; ++i) {
        diff |= static_cast<uint8_t>(candidate_hash[i] ^ stored.hash[i]);
    }

    if (diff == 0) {
        reset_lockout_state();
        return VerifyResult::Success;
    }

    ++consecutive_failures;
    if (consecutive_failures >= MAX_ATTEMPTS) {
        locked_out = true;
        lockout_started_ms = now_ms();
        ESP_LOGW(TAG, "Too many failed PIN attempts, locked out for %u ms",
                 static_cast<unsigned>(LOCKOUT_DURATION_MS));
    }

    return VerifyResult::WrongPin;
}

uint8_t attempts_remaining()
{
    check_lockout_expiry();
    if (locked_out || consecutive_failures >= MAX_ATTEMPTS) {
        return 0;
    }
    return MAX_ATTEMPTS - consecutive_failures;
}

bool is_locked_out()
{
    check_lockout_expiry();
    return locked_out;
}

} // namespace security::pin
