#include "storage/nvs_storage.hpp"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace storage::nvs {

namespace {

constexpr char TAG[] = "storage.nvs";

bool initialized = false;

/**
 * @brief RAII helper: opens an NVS handle on construction, closes it
 *        on destruction. Keeps every public function below to a
 *        single early-return-free block instead of manual
 *        open/close bookkeeping at every exit point.
 */
class Handle
{
public:
    Handle(const char* ns, ::nvs_open_mode_t mode)
    {
        err_ = ::nvs_open(ns, mode, &handle_);
    }

    ~Handle()
    {
        if (err_ == ESP_OK) {
            ::nvs_close(handle_);
        }
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    bool ok() const { return err_ == ESP_OK; }
    esp_err_t error() const { return err_; }
    ::nvs_handle_t get() const { return handle_; }

private:
    ::nvs_handle_t handle_ = 0;
    esp_err_t err_ = ESP_FAIL;
};

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    esp_err_t err = ::nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Expected the first time this partition table / NVS layout is
        // used on a chip -- erase and retry once.
        ESP_LOGW(TAG, "NVS needs erase (%s), reinitializing", esp_err_to_name(err));
        ESP_ERROR_CHECK(::nvs_flash_erase());
        err = ::nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return false;
    }

    initialized = true;
    return true;
}

bool is_initialized()
{
    return initialized;
}

bool set_u32(const char* ns, const char* key, uint32_t value)
{
    Handle h(ns, NVS_READWRITE);
    if (!h.ok()) {
        ESP_LOGE(TAG, "open('%s') failed: %s", ns, esp_err_to_name(h.error()));
        return false;
    }

    esp_err_t err = ::nvs_set_u32(h.get(), key, value);
    if (err == ESP_OK) {
        err = ::nvs_commit(h.get());
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_u32('%s'/'%s') failed: %s", ns, key, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool get_u32(const char* ns, const char* key, uint32_t& out)
{
    Handle h(ns, NVS_READONLY);
    if (!h.ok()) {
        return false; // namespace not created yet -- not an error, just "no value"
    }

    const esp_err_t err = ::nvs_get_u32(h.get(), key, &out);
    return err == ESP_OK;
}

bool set_str(const char* ns, const char* key, const char* value)
{
    Handle h(ns, NVS_READWRITE);
    if (!h.ok()) {
        ESP_LOGE(TAG, "open('%s') failed: %s", ns, esp_err_to_name(h.error()));
        return false;
    }

    esp_err_t err = ::nvs_set_str(h.get(), key, value);
    if (err == ESP_OK) {
        err = ::nvs_commit(h.get());
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_str('%s'/'%s') failed: %s", ns, key, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool get_str(const char* ns, const char* key, char* out, size_t out_capacity)
{
    Handle h(ns, NVS_READONLY);
    if (!h.ok()) {
        return false;
    }

    size_t required = out_capacity;
    const esp_err_t err = ::nvs_get_str(h.get(), key, out, &required);

    if (err == ESP_ERR_NVS_INVALID_LENGTH) {
        ESP_LOGE(TAG, "get_str('%s'/'%s'): buffer too small (need %u, have %u)",
                 ns, key, static_cast<unsigned>(required), static_cast<unsigned>(out_capacity));
        return false;
    }

    return err == ESP_OK;
}

bool set_blob(const char* ns, const char* key, const void* data, size_t len)
{
    Handle h(ns, NVS_READWRITE);
    if (!h.ok()) {
        ESP_LOGE(TAG, "open('%s') failed: %s", ns, esp_err_to_name(h.error()));
        return false;
    }

    esp_err_t err = ::nvs_set_blob(h.get(), key, data, len);
    if (err == ESP_OK) {
        err = ::nvs_commit(h.get());
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_blob('%s'/'%s') failed: %s", ns, key, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool get_blob(const char* ns, const char* key, void* out, size_t& inout_len)
{
    Handle h(ns, NVS_READONLY);
    if (!h.ok()) {
        return false;
    }

    const esp_err_t err = ::nvs_get_blob(h.get(), key, out, &inout_len);

    if (err != ESP_OK && err != ESP_ERR_NVS_INVALID_LENGTH) {
        return false;
    }
    // ESP_ERR_NVS_INVALID_LENGTH: inout_len now holds the actual size
    // the caller needs -- treated as a soft failure, not logged as an
    // error, since "tell me the real size" is a normal usage pattern
    // (call once with a null/zero-size buffer to size it, then again).
    return err == ESP_OK;
}

bool erase_key(const char* ns, const char* key)
{
    Handle h(ns, NVS_READWRITE);
    if (!h.ok()) {
        return false;
    }

    esp_err_t err = ::nvs_erase_key(h.get(), key);
    if (err == ESP_OK) {
        err = ::nvs_commit(h.get());
    }
    return err == ESP_OK;
}

bool erase_namespace(const char* ns)
{
    Handle h(ns, NVS_READWRITE);
    if (!h.ok()) {
        return false;
    }

    esp_err_t err = ::nvs_erase_all(h.get());
    if (err == ESP_OK) {
        err = ::nvs_commit(h.get());
    }
    return err == ESP_OK;
}

} // namespace storage::nvs
