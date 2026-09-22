#include "security/vault_key.hpp"

#include "esp_log.h"

namespace security::vault_key {

namespace {

constexpr char TAG[] = "security.vault_key";

psa_key_id_t current_handle = 0; // 0 is not a valid PSA key id -- doubles as "unset"

} // namespace

bool set(const uint8_t key[32])
{
    // Destroy whatever was set before importing the new one -- across
    // many lock/unlock cycles in one boot, never accumulating unused
    // PSA key slots.
    clear();

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_GCM);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, 256);

    const psa_status_t status = psa_import_key(&attributes, key, 32, &current_handle);
    psa_reset_key_attributes(&attributes);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed (status %d)", static_cast<int>(status));
        current_handle = 0;
        return false;
    }

    return true;
}

bool is_set()
{
    return current_handle != 0;
}

psa_key_id_t handle()
{
    return current_handle;
}

void clear()
{
    if (current_handle != 0) {
        psa_destroy_key(current_handle);
        current_handle = 0;
    }
}

} // namespace security::vault_key
