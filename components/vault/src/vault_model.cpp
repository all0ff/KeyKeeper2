#include "vault/vault_model.hpp"

namespace vault {

bool validate(const VaultEntry& entry)
{
    if (entry.login.size() > MAX_LOGIN_LEN) return false;
    if (entry.password.size() > MAX_PASSWORD_LEN) return false;
    if (entry.url.size() > MAX_URL_LEN) return false;
    if (entry.notes.size() > MAX_NOTES_LEN) return false;
    if (entry.totp_secret.size() > MAX_TOTP_SECRET_LEN) return false;

    return true;
}

} // namespace vault
