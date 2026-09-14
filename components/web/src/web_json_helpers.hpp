#pragma once

#include "cJSON.h"
#include "esp_http_server.h"

#include "settings/settings.hpp"

#include <cstdio>

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

} // namespace web
