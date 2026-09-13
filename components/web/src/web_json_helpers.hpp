#pragma once

#include "cJSON.h"
#include "esp_http_server.h"

// =============================================================================
// web (internal) -- shared JSON response helpers.
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

} // namespace web
