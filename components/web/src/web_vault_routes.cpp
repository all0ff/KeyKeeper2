#include "web_vault_routes.hpp"
#include "web_json_helpers.hpp"

#include "security/lock_manager.hpp"
#include "vault/vault.hpp"

#include "cJSON.h"
#include "esp_log.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace web {

namespace {

constexpr char TAG[] = "web.vault";

// Same placeholder-cap reasoning as the on-device UI (e.g.
// VaultListScreen/FavoritesScreen's own MAX_ROWS/SCAN_CAP) -- fine
// for a personal vault's realistic size, not a considered limit.
constexpr size_t MAX_LIST_ENTRIES = 256;

// Comfortably fits a maximal entry as JSON: MAX_LOGIN_LEN(128) +
// MAX_PASSWORD_LEN(128) + MAX_URL_LEN(256) + MAX_NOTES_LEN(512) +
// MAX_TOTP_SECRET_LEN(128) + MAX_CATEGORY_LEN(64) = 1216 chars of
// field content alone, plus JSON keys/quoting/escaping overhead.
constexpr size_t MAX_BODY_LEN = 2048;

bool require_unlocked(httpd_req_t* req)
{
    if (security::lock::state() != security::lock::State::Unlocked) {
        respond_error(req, "401 Unauthorized", "Not authenticated");
        return false;
    }
    return true;
}

bool get_id_from_query(httpd_req_t* req, uint32_t& out_id)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    char id_str[16];
    if (httpd_query_key_value(query, "id", id_str, sizeof(id_str)) != ESP_OK) {
        return false;
    }
    out_id = static_cast<uint32_t>(std::strtoul(id_str, nullptr, 10));
    return true;
}

bool read_body(httpd_req_t* req, std::string& out)
{
    if (req->content_len == 0 || req->content_len >= MAX_BODY_LEN) {
        return false;
    }

    // Heap-allocated, NOT a stack-local char[MAX_BODY_LEN] (2048
    // bytes) -- that used to sit on the httpd worker task's own
    // stack, which is a modest, fixed size (see web_service.cpp's
    // start(), which now sets it explicitly rather than trusting the
    // default). Combined with cJSON's own parsing frames and several
    // vault::VaultEntry std::string members further up this same call
    // chain, a 2KB stack buffer here was a real, confirmed cause of a
    // stack overflow -> panic -> full device reboot on save, not a
    // hypothetical.
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

const char* json_get_string(const cJSON* root, const char* key)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (item != nullptr && cJSON_IsString(item) && item->valuestring != nullptr) {
        return item->valuestring;
    }
    return "";
}

bool json_get_bool(const cJSON* root, const char* key)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return (item != nullptr) && cJSON_IsBool(item) && cJSON_IsTrue(item);
}

cJSON* entry_to_json_full(const vault::VaultEntry& e)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id", e.id);
    cJSON_AddStringToObject(obj, "login", e.login.c_str());
    cJSON_AddStringToObject(obj, "password", e.password.c_str());
    cJSON_AddStringToObject(obj, "url", e.url.c_str());
    cJSON_AddStringToObject(obj, "notes", e.notes.c_str());
    cJSON_AddStringToObject(obj, "totp_secret", e.totp_secret.c_str());
    cJSON_AddStringToObject(obj, "category", e.category.c_str());
    cJSON_AddBoolToObject(obj, "favorite", e.favorite);
    cJSON_AddNumberToObject(obj, "created_at", e.created_at);
    cJSON_AddNumberToObject(obj, "updated_at", e.updated_at);
    return obj;
}

// For the list endpoint -- omits password/notes/totp_secret to keep
// the response small when there are many entries; has_otp stands in
// for totp_secret's presence.
cJSON* entry_to_json_summary(const vault::VaultEntry& e)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id", e.id);
    cJSON_AddStringToObject(obj, "login", e.login.c_str());
    cJSON_AddStringToObject(obj, "url", e.url.c_str());
    cJSON_AddStringToObject(obj, "category", e.category.c_str());
    cJSON_AddBoolToObject(obj, "favorite", e.favorite);
    cJSON_AddBoolToObject(obj, "has_otp", !e.totp_secret.empty());
    return obj;
}

// JSON -> entry fields. Deliberately does NOT touch id/created_at/
// updated_at -- those are server-owned and never accepted from the
// client, regardless of what the request body contains.
void entry_from_json(const cJSON* root, vault::VaultEntry& out)
{
    out.login = json_get_string(root, "login");
    out.password = json_get_string(root, "password");
    out.url = json_get_string(root, "url");
    out.notes = json_get_string(root, "notes");
    out.totp_secret = json_get_string(root, "totp_secret");
    out.category = json_get_string(root, "category");
    out.favorite = json_get_bool(root, "favorite");
}

esp_err_t handle_list_entries(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    const size_t total = vault::entry_count();
    const size_t to_fetch = (total > MAX_LIST_ENTRIES) ? MAX_LIST_ENTRIES : total;

    std::vector<vault::VaultEntry> entries(to_fetch);
    if (to_fetch > 0) {
        vault::list_entries(entries.data(), to_fetch, 0);
    }

    if (total > MAX_LIST_ENTRIES) {
        ESP_LOGW(TAG, "Vault has %u entries, only returning the first %u",
                 static_cast<unsigned>(total), static_cast<unsigned>(MAX_LIST_ENTRIES));
    }

    cJSON* array = cJSON_CreateArray();
    for (const vault::VaultEntry& e : entries) {
        cJSON_AddItemToArray(array, entry_to_json_summary(e));
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "entries", array);
    cJSON_AddNumberToObject(data, "total", static_cast<double>(total));
    respond_ok(req, data);
    return ESP_OK;
}

esp_err_t handle_get_entry(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    uint32_t id = vault::INVALID_ID;
    if (!get_id_from_query(req, id)) {
        respond_error(req, "400 Bad Request", "Missing 'id' query parameter");
        return ESP_OK;
    }

    vault::VaultEntry entry;
    if (!vault::get_entry(id, entry)) {
        respond_error(req, "404 Not Found", "No such entry");
        return ESP_OK;
    }

    respond_ok(req, entry_to_json_full(entry));
    return ESP_OK;
}

esp_err_t handle_create_entry(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    std::string body;
    if (!read_body(req, body)) {
        respond_error(req, "400 Bad Request", "Missing or too-large request body");
        return ESP_OK;
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        respond_error(req, "400 Bad Request", "Invalid JSON");
        return ESP_OK;
    }

    vault::VaultEntry entry;
    entry_from_json(root, entry);
    cJSON_Delete(root);

    if (entry.login.empty()) {
        respond_error(req, "400 Bad Request", "'login' must not be empty");
        return ESP_OK;
    }
    if (!vault::validate(entry)) {
        respond_error(req, "400 Bad Request", "One or more fields exceed the maximum length");
        return ESP_OK;
    }

    const uint32_t new_id = vault::create_entry(entry);
    if (new_id == vault::INVALID_ID) {
        respond_error(req, "500 Internal Server Error", "Failed to create entry");
        return ESP_OK;
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", new_id);
    respond_ok(req, data);
    return ESP_OK;
}

esp_err_t handle_update_entry(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    uint32_t id = vault::INVALID_ID;
    if (!get_id_from_query(req, id)) {
        respond_error(req, "400 Bad Request", "Missing 'id' query parameter");
        return ESP_OK;
    }

    vault::VaultEntry existing;
    if (!vault::get_entry(id, existing)) {
        respond_error(req, "404 Not Found", "No such entry");
        return ESP_OK;
    }

    std::string body;
    if (!read_body(req, body)) {
        respond_error(req, "400 Bad Request", "Missing or too-large request body");
        return ESP_OK;
    }

    cJSON* root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        respond_error(req, "400 Bad Request", "Invalid JSON");
        return ESP_OK;
    }

    vault::VaultEntry updated = existing; // preserve id/created_at/updated_at
    entry_from_json(root, updated);
    updated.id = id;
    cJSON_Delete(root);

    if (updated.login.empty()) {
        respond_error(req, "400 Bad Request", "'login' must not be empty");
        return ESP_OK;
    }
    if (!vault::validate(updated)) {
        respond_error(req, "400 Bad Request", "One or more fields exceed the maximum length");
        return ESP_OK;
    }

    if (!vault::update_entry(updated)) {
        respond_error(req, "500 Internal Server Error", "Failed to update entry");
        return ESP_OK;
    }

    respond_ok(req, nullptr);
    return ESP_OK;
}

esp_err_t handle_delete_entry(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    uint32_t id = vault::INVALID_ID;
    if (!get_id_from_query(req, id)) {
        respond_error(req, "400 Bad Request", "Missing 'id' query parameter");
        return ESP_OK;
    }

    if (!vault::delete_entry(id)) {
        respond_error(req, "404 Not Found", "No such entry");
        return ESP_OK;
    }

    respond_ok(req, nullptr);
    return ESP_OK;
}

} // namespace

void register_vault_routes(httpd_handle_t server)
{
    static char entries_path[64];
    build_prefixed_path(entries_path, sizeof(entries_path), "/api/v1/entries");

    static char entry_path[64];
    build_prefixed_path(entry_path, sizeof(entry_path), "/api/v1/entry");

    static httpd_uri_t list_uri{};
    list_uri.uri = entries_path;
    list_uri.method = HTTP_GET;
    list_uri.handler = handle_list_entries;
    list_uri.user_ctx = nullptr;

    static httpd_uri_t get_uri{};
    get_uri.uri = entry_path;
    get_uri.method = HTTP_GET;
    get_uri.handler = handle_get_entry;
    get_uri.user_ctx = nullptr;

    static httpd_uri_t create_uri{};
    create_uri.uri = entry_path;
    create_uri.method = HTTP_POST;
    create_uri.handler = handle_create_entry;
    create_uri.user_ctx = nullptr;

    static httpd_uri_t update_uri{};
    update_uri.uri = entry_path;
    update_uri.method = HTTP_PUT;
    update_uri.handler = handle_update_entry;
    update_uri.user_ctx = nullptr;

    static httpd_uri_t delete_uri{};
    delete_uri.uri = entry_path;
    delete_uri.method = HTTP_DELETE;
    delete_uri.handler = handle_delete_entry;
    delete_uri.user_ctx = nullptr;

    httpd_register_uri_handler(server, &list_uri);
    httpd_register_uri_handler(server, &get_uri);
    httpd_register_uri_handler(server, &create_uri);
    httpd_register_uri_handler(server, &update_uri);
    httpd_register_uri_handler(server, &delete_uri);

    ESP_LOGI(TAG, "Vault REST routes registered");
}

} // namespace web
