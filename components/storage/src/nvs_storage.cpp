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

    // Query the REAL stored size first, always -- nvs_get_blob()'s
    // own *length behavior only reliably reports the actual size back
    // to the caller in two cases: this explicit "out=nullptr" query
    // mode, or when the caller's buffer was too small (returns
    // ESP_ERR_NVS_INVALID_LENGTH and updates *length to the size
    // actually needed). When the caller's buffer is LARGER than what
    // is actually stored, a direct read still returns ESP_OK, but
    // *length is NOT shrunk down to the true (smaller) stored size --
    // confirmed against ESP-IDF's own documented error semantics
    // ("ESP_ERR_NVS_INVALID_LENGTH if length is not sufficient", only
    // covering buffer-too-small). Skipping this query step and
    // reading straight into a same-size-or-bigger buffer -- which is
    // what this function used to do -- meant a caller like
    // settings::load_section() comparing the post-read length against
    // sizeof(T) could never actually detect "the stored blob is a
    // different (smaller, older) shape than what's being read into
    // now": the length would silently read back as whatever the
    // caller's buffer size already was, not the true stored size,
    // making the size-mismatch check it relies on a no-op. Real
    // confirmed consequence: a struct field inserted in the MIDDLE of
    // settings::GeneralSettings (not appended at the end) silently
    // left later fields (display_brightness in this specific case)
    // populated from bytes that used to belong to a DIFFERENT field
    // at the old layout's offset, rather than cleanly falling back to
    // defaults as intended -- not a hypothetical, this is what
    // produced a real "brightness=0%, blank screen" boot after such a
    // change, diagnosed from the affected person's own serial log.
    size_t stored_len = 0;
    esp_err_t err = ::nvs_get_blob(h.get(), key, nullptr, &stored_len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return false;
    }
    if (err != ESP_OK) {
        return false;
    }

    if (stored_len != inout_len) {
        // Report the true stored size back to the caller (matches
        // this function's own documented contract) without touching
        // `out` at all -- a caller like load_section() that requires
        // an exact match should treat this as "not usable", not
        // attempt a partial/reinterpreted read.
        inout_len = stored_len;
        return false;
    }

    err = ::nvs_get_blob(h.get(), key, out, &inout_len);
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
