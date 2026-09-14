#pragma once

#include "cJSON.h"
#include "esp_http_server.h"

#include "settings/settings.hpp"

#include <cstdio>
#include <string>
#include <vector>

// =============================================================================
// web (internal) -- shared JSON response + routing helpers.
//
// Lives in src/, not include/web/ -- this is implementation-internal
// plumbing shared between web_service.cpp and web_vault_routes.cpp,
// not part of the component's public API (nothing outside
// components/web should need this).
//
// Response shape follows docs/WEB.md section 7 exactly:
// {"status":"ok","data":{...}} / {"status":"error","message":"..."}.
// =============================================================================

namespace web {

inline void respond_ok(httpd_req_t* req, cJSON* data)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddItemToObject(root, "data", (data != nullptr) ? data : cJSON_CreateObject());

    char* text = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    cJSON_free(text);
    cJSON_Delete(root);
}

inline void respond_error(httpd_req_t* req, const char* http_status_line, const char* message)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "message", message);

    char* text = cJSON_PrintUnformatted(root);
    httpd_resp_set_status(req, http_status_line);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    cJSON_free(text);
    cJSON_Delete(root);
}

/**
 * @brief Build a route path with the configured secret-word prefix
 *        (settings::SecuritySettings::secret_word), if any.
 *
 * From KeyKeeper 1.90's own "secretword" feature -- see
 * SecuritySettings::secret_word's doc comment. path must start with
 * '/'. Empty secret_word (the default) leaves path unchanged.
 *
 * Used only at route-REGISTRATION time (web::start()) -- routes are
 * re-registered from scratch (server stop+start) whenever the secret
 * word changes, so there's no per-request prefix stripping to do: a
 * changed prefix simply means the OLD path stops matching anything
 * and the NEW one does, once the server restarts.
 */
inline void build_prefixed_path(char* out, size_t out_size, const char* path)
{
    const settings::SecuritySettings& sec = settings::all().security;
    if (sec.secret_word[0] != '\0') {
        std::snprintf(out, out_size, "/%s%s", sec.secret_word, path);
    } else {
        std::snprintf(out, out_size, "%s", path);
    }
}

// -----------------------------------------------------------------
// JSON request-body reading helpers -- shared between
// web_vault_routes.cpp and web_settings_routes.cpp. All return a
// harmless default (empty string / false / 0) if the key is missing
// or the wrong type, rather than erroring -- callers that need a
// field to be genuinely required check for that themselves (e.g.
// vault_routes' login-must-not-be-empty check).
// -----------------------------------------------------------------

inline const char* json_get_string(const cJSON* root, const char* key)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item != nullptr && cJSON_IsString(item) && item->valuestring != nullptr) {
        return item->valuestring;
    }
    return "";
}

inline bool json_get_bool(const cJSON* root, const char* key)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return (item != nullptr) && cJSON_IsBool(item) && cJSON_IsTrue(item);
}

/// out_of_range_default is returned as-is if the key is missing/not a
/// number -- callers pass the field's OWN current value for that, so
/// a PUT that omits a field leaves it unchanged rather than zeroing it.
inline uint32_t json_get_uint32(const cJSON* root, const char* key, uint32_t default_value)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item != nullptr && cJSON_IsNumber(item) && item->valuedouble >= 0) {
        return static_cast<uint32_t>(item->valuedouble);
    }
    return default_value;
}

// Comfortably fits a maximal vault entry as JSON (see
// web_vault_routes.cpp's original sizing comment) and every settings
// section's PUT body -- none of those come close to this.
constexpr size_t MAX_BODY_LEN = 2048;

/**
 * @brief Read an HTTP request body into out, heap-allocated (NOT a
 *        stack-local buffer -- a 2KB stack buffer here was a real,
 *        confirmed cause of a stack overflow -> panic -> full device
 *        reboot on save, before this was fixed to use std::vector).
 *
 * Shared between web_vault_routes.cpp and web_settings_routes.cpp.
 */
inline bool read_body(httpd_req_t* req, std::string& out)
{
    if (req->content_len == 0 || req->content_len >= MAX_BODY_LEN) {
        return false;
    }

    std::vector<char> buf(MAX_BODY_LEN);
    int total = 0;
    while (total < static_cast<int>(req->content_len)) {
        const int ret = httpd_req_recv(req, buf.data() + total, req->content_len - total);
        if (ret <= 0) {
            return false;
        }
        total += ret;
    }
    out.assign(buf.data(), static_cast<size_t>(total));
    return true;
}

} // namespace web
