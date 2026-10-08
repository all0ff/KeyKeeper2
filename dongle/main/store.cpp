#include "store.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace dongle::store {

namespace {

const char* TAG = "store";
constexpr char NS[] = "kklink";
constexpr char KEY_SECRET[] = "sk";
constexpr char KEY_PEER[] = "peer";

bool load_blob(const char* key, uint8_t out[32])
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    size_t n = 32;
    uint8_t tmp[32];
    const esp_err_t e = nvs_get_blob(h, key, tmp, &n);
    nvs_close(h);
    if (e != ESP_OK || n != 32) {
        return false;
    }
    for (int i = 0; i < 32; ++i) out[i] = tmp[i];
    return true;
}

bool save_blob(const char* key, const uint8_t in[32])
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t e = nvs_set_blob(h, key, in, 32);
    if (e == ESP_OK) {
        e = nvs_commit(h);
    }
    nvs_close(h);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "saving '%s' failed: %s", key, esp_err_to_name(e));
    }
    return e == ESP_OK;
}

} // namespace

bool init()
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // An empty or older-format NVS partition: start it afresh.
        ESP_LOGW(TAG, "erasing the NVS partition (%s)", esp_err_to_name(e));
        if (nvs_flash_erase() != ESP_OK) {
            return false;
        }
        e = nvs_flash_init();
    }
    return e == ESP_OK;
}

bool load_secret(uint8_t sk[32]) { return load_blob(KEY_SECRET, sk); }
bool save_secret(const uint8_t sk[32]) { return save_blob(KEY_SECRET, sk); }
bool load_peer(uint8_t pk[32]) { return load_blob(KEY_PEER, pk); }
bool save_peer(const uint8_t pk[32]) { return save_blob(KEY_PEER, pk); }

bool erase_peer()
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t e = nvs_erase_key(h, KEY_PEER);
    if (e == ESP_ERR_NVS_NOT_FOUND) {
        e = ESP_OK;
    }
    if (e == ESP_OK) {
        e = nvs_commit(h);
    }
    nvs_close(h);
    return e == ESP_OK;
}

} // namespace dongle::store
