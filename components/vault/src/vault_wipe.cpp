#include "vault/vault_repository.hpp"

#include "storage/vaultfile.hpp"

#include "esp_log.h"

namespace vault::repository {

namespace {
constexpr char TAG[] = "vault.wipe";
}

bool wipe()
{
    if (!is_initialized()) {
        return false;
    }

    // Clear the in-memory copy first so plaintext secrets are no longer
    // available through the repository while the persistent file is removed.
    clear();

    if (!storage::vaultfile::exists()) {
        ESP_LOGI(TAG, "No vault.db present; wipe completed");
        return true;
    }

    if (!storage::vaultfile::remove()) {
        ESP_LOGE(TAG, "Failed to remove vault.db during wipe");
        return false;
    }

    ESP_LOGW(TAG, "Persistent vault.db removed");
    return true;
}

} // namespace vault::repository
