#include "web_backup_routes.hpp"
#include "web_json_helpers.hpp"

#include "security/lock_manager.hpp"
#include "security/permission_manager.hpp"
#include "vault/vault_backup.hpp"

#include "cJSON.h"
#include "esp_log.h"

#include <cstring>

namespace web {

namespace {

constexpr char TAG[] = "web.backup";
constexpr size_t MAX_BACKUPS = 32; // generous headroom over what a real SD card would realistically accumulate

bool require_unlocked(httpd_req_t* req)
{
    if (security::lock::state() != security::lock::State::Unlocked) {
        respond_error(req, "401 Unauthorized", "Not authenticated");
        return false;
    }
    // See security::lock::notify_activity()'s own comment -- a real,
    // confirmed-on-hardware bug: without this, using the device
    // purely through the Web UI still auto-locked on the physical
    // input timer's own schedule, and the very next request after a
    // web-based unlock would immediately auto-lock again since
    // nothing about that unlock had touched input's own activity
    // timestamp.
    security::lock::notify_activity();
    return true;
}

bool get_filename_from_query(httpd_req_t* req, char* out, size_t out_size)
{
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    if (httpd_query_key_value(query, "filename", out, out_size) != ESP_OK) {
        return false;
    }
    url_decode_in_place(out);
    return out[0] != '\0';
}

cJSON* backup_to_json(const vault::backup::BackupInfo& b)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "filename", b.filename);
    cJSON_AddNumberToObject(obj, "size_bytes", static_cast<double>(b.size_bytes));
    return obj;
}

// GET /api/v1/backups -- list existing backups, most recent first
// (same order vault::backup::list_backups() itself returns).
esp_err_t handle_list_backups(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    // Heap-allocated, not a stack array -- see
    // web_vault_routes.cpp's handle_search() for why that distinction
    // matters on this worker's 8192-byte stack (BackupInfo itself is
    // small/fixed-size here, so this particular array would be fine
    // either way, but matching the established pattern is one less
    // thing to get wrong later if BackupInfo ever grows a
    // std::string).
    std::vector<vault::backup::BackupInfo> backups(MAX_BACKUPS);
    const size_t count = vault::backup::list_backups(backups.data(), MAX_BACKUPS);

    cJSON* array = cJSON_CreateArray();
    for (size_t i = 0; i < count; ++i) {
        cJSON_AddItemToArray(array, backup_to_json(backups[i]));
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "backups", array);
    respond_ok(req, data);
    return ESP_OK;
}

// POST /api/v1/backups -- create a new backup now. No request body.
esp_err_t handle_create_backup(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    char filename[32] = "";
    if (!vault::backup::create_backup(filename, sizeof(filename))) {
        respond_error(req, "500 Internal Server Error", "Failed to create backup (no SD card?)");
        return ESP_OK;
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "filename", filename);
    respond_ok(req, data);
    return ESP_OK;
}

// DELETE /api/v1/backups?filename=X
esp_err_t handle_delete_backup(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    char filename[32];
    if (!get_filename_from_query(req, filename, sizeof(filename))) {
        respond_error(req, "400 Bad Request", "Missing 'filename' query parameter");
        return ESP_OK;
    }

    if (!vault::backup::delete_backup(filename)) {
        respond_error(req, "404 Not Found", "No such backup");
        return ESP_OK;
    }

    respond_ok(req, nullptr);
    return ESP_OK;
}

// POST /api/v1/backups/restore?filename=X -- DESTRUCTIVE, see this
// file's own header comment. Restarts the device on success, right
// after sending the response.
esp_err_t handle_restore_backup(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    const security::permission::Result result =
        security::permission::check(security::permission::Operation::RestoreBackup);
    if (result != security::permission::Result::Allowed) {
        ESP_LOGI(TAG, "Restore backup denied (%d)", static_cast<int>(result));
        respond_error(req, "403 Forbidden", "Not allowed");
        return ESP_OK;
    }

    char filename[32];
    if (!get_filename_from_query(req, filename, sizeof(filename))) {
        respond_error(req, "400 Bad Request", "Missing 'filename' query parameter");
        return ESP_OK;
    }

    if (!vault::backup::restore_backup(filename)) {
        respond_error(req, "500 Internal Server Error", "Restore failed -- vault.db left untouched");
        return ESP_OK;
    }

    // Response first, restart after -- the browser needs to actually
    // receive this before the connection drops out from under it.
    cJSON* data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "restarting", true);
    respond_ok(req, data);

    ESP_LOGW(TAG, "Restoring backup '%s' -- restarting device now, same as "
                  "ui::screens::BackupScreen's own restore action",
             filename);
    esp_restart(); // does not return
    return ESP_OK;
}

} // namespace

void register_backup_routes(httpd_handle_t server)
{
    static char backups_path[64];
    build_prefixed_path(backups_path, sizeof(backups_path), "/api/v1/backups");

    static char restore_path[64];
    build_prefixed_path(restore_path, sizeof(restore_path), "/api/v1/backups/restore");

    static httpd_uri_t list_uri{};
    list_uri.uri = backups_path;
    list_uri.method = HTTP_GET;
    list_uri.handler = handle_list_backups;
    list_uri.user_ctx = nullptr;

    static httpd_uri_t create_uri{};
    create_uri.uri = backups_path;
    create_uri.method = HTTP_POST;
    create_uri.handler = handle_create_backup;
    create_uri.user_ctx = nullptr;

    static httpd_uri_t delete_uri{};
    delete_uri.uri = backups_path;
    delete_uri.method = HTTP_DELETE;
    delete_uri.handler = handle_delete_backup;
    delete_uri.user_ctx = nullptr;

    static httpd_uri_t restore_uri{};
    restore_uri.uri = restore_path;
    restore_uri.method = HTTP_POST;
    restore_uri.handler = handle_restore_backup;
    restore_uri.user_ctx = nullptr;

    httpd_register_uri_handler(server, &list_uri);
    httpd_register_uri_handler(server, &create_uri);
    httpd_register_uri_handler(server, &delete_uri);
    httpd_register_uri_handler(server, &restore_uri);

    ESP_LOGI(TAG, "Backup REST routes registered");
}

} // namespace web
