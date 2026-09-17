#include "totp/totp.hpp"

#include "rtc_time/rtc_time.hpp"

#include "psa/crypto.h"

#include "esp_log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

namespace totp {

namespace {

constexpr char TAG[] = "totp";
constexpr uint32_t STEP_SECONDS = 30;
constexpr uint32_t DIGITS = 6;
constexpr uint32_t DIGITS_MOD = 1000000; // 10^DIGITS

bool psa_ready = false;

bool ensure_psa_ready()
{
    if (psa_ready) {
        return true;
    }
    const psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed (status %d)", static_cast<int>(status));
        return false;
    }
    psa_ready = true;
    return true;
}

/**
 * @brief Decode RFC 4648 Base32 (standard alphabet, case-insensitive).
 *        '=' padding and stray spaces/dashes (some apps format manual-
 *        entry keys with either) are skipped, not treated as errors.
 *
 * @return false on any character outside the Base32 alphabet (and
 *         those two tolerated separators).
 */
bool base32_decode(const std::string& input, std::vector<uint8_t>& out)
{
    constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    out.clear();
    uint32_t buffer = 0;
    int bits_in_buffer = 0;

    for (char c : input) {
        if (c == '=' || c == ' ' || c == '-') {
            continue;
        }

        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        const char* pos = std::strchr(ALPHABET, upper);
        if (pos == nullptr || upper == '\0') {
            return false; // character outside the Base32 alphabet
        }
        const uint32_t value = static_cast<uint32_t>(pos - ALPHABET);

        buffer = (buffer << 5) | value;
        bits_in_buffer += 5;

        if (bits_in_buffer >= 8) {
            bits_in_buffer -= 8;
            out.push_back(static_cast<uint8_t>((buffer >> bits_in_buffer) & 0xFF));
        }
    }

    return true;
}

/**
 * @brief HOTP per RFC 4226: HMAC-SHA-1(key, 8-byte big-endian counter),
 *        dynamic truncation (section 5.3), mod 10^DIGITS.
 */
bool hotp(const std::vector<uint8_t>& key, uint64_t counter, uint32_t& out_code)
{
    if (!ensure_psa_ready()) {
        return false;
    }
    if (key.empty()) {
        return false;
    }

    uint8_t counter_be[8];
    for (int i = 7; i >= 0; --i) {
        counter_be[i] = static_cast<uint8_t>(counter & 0xFF);
        counter >>= 8;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_1));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, static_cast<size_t>(key.size()) * 8);

    psa_key_id_t key_id = 0;
    psa_status_t status = psa_import_key(&attributes, key.data(), key.size(), &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed (status %d)", static_cast<int>(status));
        return false;
    }

    uint8_t mac[PSA_MAC_MAX_SIZE];
    size_t mac_len = 0;
    status = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_1), counter_be, sizeof(counter_be), mac, sizeof(mac),
                              &mac_len);
    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS || mac_len < 20) {
        ESP_LOGE(TAG, "psa_mac_compute failed (status %d)", static_cast<int>(status));
        return false;
    }

    // Dynamic truncation, RFC 4226 section 5.3.
    const uint8_t offset = mac[19] & 0x0F;
    const uint32_t binary = (static_cast<uint32_t>(mac[offset] & 0x7F) << 24) |
                             (static_cast<uint32_t>(mac[offset + 1] & 0xFF) << 16) |
                             (static_cast<uint32_t>(mac[offset + 2] & 0xFF) << 8) |
                             static_cast<uint32_t>(mac[offset + 3] & 0xFF);

    out_code = binary % DIGITS_MOD;

    std::memset(mac, 0, sizeof(mac));
    return true;
}

} // namespace

bool generate(const std::string& secret_base32, char* out_code, size_t out_code_size)
{
    if (out_code == nullptr || out_code_size < DIGITS + 1) {
        return false;
    }
    if (secret_base32.empty()) {
        return false;
    }
    if (!rtc_time::is_synced()) {
        // See rtc_time.hpp's own comment -- generating a code against
        // an unsynced clock wouldn't just be imprecise, it would be
        // confidently wrong for a plausible-looking but incorrect
        // moment. Fail cleanly instead.
        return false;
    }

    std::vector<uint8_t> key;
    if (!base32_decode(secret_base32, key) || key.empty()) {
        return false;
    }

    const uint64_t counter = rtc_time::unix_time() / STEP_SECONDS;

    uint32_t code = 0;
    const bool ok = hotp(key, counter, code);

    // Zero the decoded key before returning either way -- it's a
    // stack/heap-allocated copy of a real secret, no reason to leave
    // it sitting around longer than needed.
    std::fill(key.begin(), key.end(), 0);

    if (!ok) {
        return false;
    }

    std::snprintf(out_code, out_code_size, "%0*u", static_cast<int>(DIGITS), static_cast<unsigned>(code));
    return true;
}

uint32_t seconds_remaining()
{
    const uint64_t now = rtc_time::unix_time();
    return static_cast<uint32_t>(STEP_SECONDS - (now % STEP_SECONDS));
}

} // namespace totp
