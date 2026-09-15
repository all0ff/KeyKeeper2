#include "password_gen/password_gen.hpp"

#include "esp_random.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

namespace password_gen {

namespace {

constexpr char UPPERCASE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr char LOWERCASE[] = "abcdefghijklmnopqrstuvwxyz";
constexpr char DIGITS[] = "0123456789";

// A deliberately conservative subset of typeable symbols -- every one
// of these is confirmed mapped in components/usb/src/keycode_map.cpp's
// ascii_to_hid(), and chosen to avoid the handful of characters more
// prone to odd behavior across keyboard layouts (backtick, backslash,
// pipe, quotes) even though the USB layer technically supports those
// too.
constexpr char SYMBOLS[] = "!@#$%^&*()-_=+[]{}";

/**
 * @brief Uniform random value in [0, bound) via rejection sampling
 *        against esp_random()'s hardware TRNG.
 *
 * Avoids the (small but real) modulo bias a plain esp_random() % bound
 * would have for any bound that doesn't evenly divide 2^32 -- true for
 * every charset size used here (26, 52, 62, 74, ...). The rejection
 * region is tiny relative to the full 32-bit range for these bounds,
 * so in practice this almost never actually loops more than once.
 */
uint32_t uniform_random(uint32_t bound)
{
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % bound);
    uint32_t value;
    do {
        value = esp_random();
    } while (value >= limit);
    return value % bound;
}

struct CharClass
{
    const char* chars;
    size_t len;
};

} // namespace

bool generate(const settings::PasswordGenSettings& cfg, char* out, size_t out_capacity)
{
    if (cfg.length < MIN_LENGTH || cfg.length > MAX_LENGTH) {
        return false;
    }
    if (out_capacity < static_cast<size_t>(cfg.length) + 1) {
        return false;
    }

    CharClass classes[4];
    size_t class_count = 0;

    if (cfg.include_uppercase) classes[class_count++] = {UPPERCASE, sizeof(UPPERCASE) - 1};
    if (cfg.include_lowercase) classes[class_count++] = {LOWERCASE, sizeof(LOWERCASE) - 1};
    if (cfg.include_digits) classes[class_count++] = {DIGITS, sizeof(DIGITS) - 1};
    if (cfg.include_symbols) classes[class_count++] = {SYMBOLS, sizeof(SYMBOLS) - 1};

    if (class_count == 0) {
        return false; // nothing to draw from
    }

    std::string combined;
    for (size_t i = 0; i < class_count; ++i) {
        combined.append(classes[i].chars, classes[i].len);
    }

    std::string result;
    result.reserve(cfg.length);

    // Guarantee at least one character from each SELECTED class, if
    // the requested length allows it -- a common password-policy
    // expectation ("must contain a digit", etc.), not just left to
    // statistical likelihood from the combined pool. If length is
    // shorter than the number of enabled classes, this simply
    // guarantees as many of them as fit (in the fixed
    // uppercase/lowercase/digits/symbols order) rather than failing
    // outright -- a 4-character password with all four classes
    // enabled still gets one of each, for example.
    const size_t mandatory = std::min(class_count, static_cast<size_t>(cfg.length));
    for (size_t i = 0; i < mandatory; ++i) {
        const CharClass& c = classes[i];
        result += c.chars[uniform_random(static_cast<uint32_t>(c.len))];
    }

    // Fill the rest from the combined pool.
    while (result.size() < cfg.length) {
        result += combined[uniform_random(static_cast<uint32_t>(combined.size()))];
    }

    // Shuffle (Fisher-Yates) so the mandatory characters from the loop
    // above aren't predictably placed at the start of every generated
    // password.
    for (size_t i = result.size(); i > 1; --i) {
        const size_t j = uniform_random(static_cast<uint32_t>(i));
        std::swap(result[i - 1], result[j]);
    }

    std::strncpy(out, result.c_str(), out_capacity - 1);
    out[out_capacity - 1] = '\0';
    return true;
}

} // namespace password_gen
