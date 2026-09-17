#include "vault/vault_model.hpp"

#include "vault/bip39.hpp"

#include "esp_random.h"

namespace vault {

bool validate(const VaultEntry& entry)
{
    if (entry.login.size() > MAX_LOGIN_LEN) return false;
    if (entry.password.size() > MAX_PASSWORD_LEN) return false;
    if (entry.url.size() > MAX_URL_LEN) return false;
    if (entry.notes.size() > MAX_NOTES_LEN) return false;
    if (entry.totp_secret.size() > MAX_TOTP_SECRET_LEN) return false;
    if (entry.category.size() > MAX_CATEGORY_LEN) return false;

    if (entry.recovery_codes.size() > MAX_RECOVERY_CODES) return false;
    for (const RecoveryCode& rc : entry.recovery_codes) {
        if (rc.code.empty() || rc.code.size() > MAX_RECOVERY_CODE_LEN) return false;
    }

    // Empty (no seed phrase set) is fine. Non-empty must be a FULLY
    // valid BIP-39 phrase -- see bip39::validate_seed_phrase()'s own
    // comment for exactly what that checks (length + wordlist, not
    // the checksum). Enforced here, not just a size cap like the
    // other fields above, because this is the single gate every
    // create_entry()/update_entry() call goes through -- letting a
    // malformed phrase (wrong word count, a typo'd word) through
    // would mean silently saving something the person likely can't
    // actually recover their wallet from later.
    if (!entry.seed_phrase.empty() && !bip39::validate_seed_phrase(entry.seed_phrase)) return false;

    return true;
}

std::vector<RecoveryCode> generate_recovery_codes(size_t count)
{
    if (count < 1) {
        count = 1;
    }
    if (count > MAX_RECOVERY_CODES) {
        count = MAX_RECOVERY_CODES;
    }

    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    constexpr size_t DIGITS_PER_CODE = 10; // 5 + 5, split by one dash
    constexpr size_t SPLIT_AT = 5;

    std::vector<RecoveryCode> codes;
    codes.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        std::string code;
        code.reserve(DIGITS_PER_CODE + 1);

        uint8_t random_bytes[DIGITS_PER_CODE];
        esp_fill_random(random_bytes, sizeof(random_bytes));

        for (size_t d = 0; d < DIGITS_PER_CODE; ++d) {
            if (d == SPLIT_AT) {
                code += '-';
            }
            code += HEX_DIGITS[random_bytes[d] & 0x0F];
        }

        codes.push_back(RecoveryCode{std::move(code), false});
    }

    return codes;
}

} // namespace vault
