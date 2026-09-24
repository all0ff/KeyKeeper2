#include "web_vault_routes.hpp"
#include "web_json_helpers.hpp"

#include "security/lock_manager.hpp"
#include "vault/bip39.hpp"
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
    cJSON* codes = cJSON_CreateArray();
    for (const vault::RecoveryCode& rc : e.recovery_codes) {
        cJSON* code_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(code_obj, "code", rc.code.c_str());
        cJSON_AddBoolToObject(code_obj, "used", rc.used);
        cJSON_AddItemToArray(codes, code_obj);
    }
    cJSON_AddItemToObject(obj, "recovery_codes", codes);
    cJSON* seed_words = cJSON_CreateArray();
    for (const std::string& w : e.seed_phrase) {
        cJSON_AddItemToArray(seed_words, cJSON_CreateString(w.c_str()));
    }
    cJSON_AddItemToObject(obj, "seed_phrase", seed_words);
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

// GET /api/v1/search?q=<query> -- same case-insensitive login/url/notes
// substring match ui::screens::SearchScreen uses on the device (both
// now call vault::search_entries(), one shared implementation).
// Missing/empty q matches every entry, same as an empty query does
// on-device.
esp_err_t handle_search(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    char query_str[128] = "";
    char raw_q[96] = "";
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        httpd_query_key_value(query_str, "q", raw_q, sizeof(raw_q));
    }
    url_decode_in_place(raw_q);

    // Heap-allocated, NOT a stack array -- vault::VaultEntry holds
    // several std::string/std::vector members, and MAX_LIST_ENTRIES
    // (256) of those on this worker's 8192-byte stack would overflow
    // it outright (the exact class of bug this project already hit
    // once before, in read_body() -- see that function's own
    // comment).
    std::vector<vault::VaultEntry> matches(MAX_LIST_ENTRIES);
    const size_t found = vault::search_entries(raw_q, matches.data(), MAX_LIST_ENTRIES);

    cJSON* array = cJSON_CreateArray();
    for (size_t i = 0; i < found; ++i) {
        cJSON_AddItemToArray(array, entry_to_json_summary(matches[i]));
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "entries", array);
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

cJSON* recovery_codes_to_json(const std::vector<vault::RecoveryCode>& codes)
{
    cJSON* array = cJSON_CreateArray();
    for (const vault::RecoveryCode& rc : codes) {
        cJSON* code_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(code_obj, "code", rc.code.c_str());
        cJSON_AddBoolToObject(code_obj, "used", rc.used);
        cJSON_AddItemToArray(array, code_obj);
    }
    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "recovery_codes", array);
    return data;
}

// PUT /api/v1/entry/recovery_codes?id=N -- sets (REPLACES) the whole
// set with codes THE PERSON PROVIDES. Body: {"codes": ["6458f-49d3c", ...]}.
//
// Deliberately NOT "generate a random set on-device" (an earlier
// version of this endpoint did exactly that, and it was a real
// mistake, not a design choice -- caught by the project owner:
// recovery codes only mean anything in relation to whatever OUTSIDE
// service (GitHub, a bank, an exchange, ...) they're for, and that
// service is the only thing that can generate ones it will actually
// accept back. A device-invented code looks exactly like a real one
// and is completely useless for actually recovering that account --
// worse than not storing anything, since it creates false confidence.
// So this only ever stores what the person actually copied or
// imported from that service's own recovery-codes page/file.
//
// No specific format is enforced beyond vault::validate()'s own
// length/count caps (MAX_RECOVERY_CODES, MAX_RECOVERY_CODE_LEN) --
// unlike a seed phrase's fixed BIP-39 shape, real services use very
// different code formats from each other, so this can't validate
// against any one pattern.
esp_err_t handle_set_recovery_codes(httpd_req_t* req)
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

    std::vector<vault::RecoveryCode> codes;
    const cJSON* codes_item = cJSON_GetObjectItemCaseSensitive(root, "codes");
    if (cJSON_IsArray(codes_item)) {
        const cJSON* c = nullptr;
        cJSON_ArrayForEach(c, codes_item) {
            if (cJSON_IsString(c) && c->valuestring != nullptr && c->valuestring[0] != '\0') {
                codes.push_back(vault::RecoveryCode{std::string(c->valuestring), false});
            }
        }
    }
    cJSON_Delete(root);

    if (codes.empty()) {
        respond_error(req, "400 Bad Request", "No codes provided");
        return ESP_OK;
    }
    if (codes.size() > vault::MAX_RECOVERY_CODES) {
        std::string error_message =
            "Too many codes (max " +
            std::to_string(vault::MAX_RECOVERY_CODES) + ")";

        respond_error(req, "400 Bad Request", error_message.c_str());
        return ESP_OK;
    }

    entry.recovery_codes = std::move(codes);

    if (!vault::validate(entry)) {
        respond_error(req, "400 Bad Request", "One or more codes is too long");
        return ESP_OK;
    }

    if (!vault::update_entry(entry)) {
        respond_error(req, "500 Internal Server Error", "Failed to save recovery codes");
        return ESP_OK;
    }

    respond_ok(req, recovery_codes_to_json(entry.recovery_codes));
    return ESP_OK;
}

// PUT /api/v1/entry/recovery_codes/mark?id=N -- marks ONE existing
// code used/unused (matched by its exact string, since that's what
// the client already has on screen). Body: {"code": "...", "used": true}.
esp_err_t handle_mark_recovery_code(httpd_req_t* req)
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
    const std::string code = json_get_string(root, "code");
    const bool used = json_get_bool(root, "used");
    cJSON_Delete(root);

    bool found = false;
    for (vault::RecoveryCode& rc : entry.recovery_codes) {
        if (rc.code == code) {
            rc.used = used;
            found = true;
            break;
        }
    }

    if (!found) {
        respond_error(req, "404 Not Found", "No such recovery code on this entry");
        return ESP_OK;
    }

    if (!vault::update_entry(entry)) {
        respond_error(req, "500 Internal Server Error", "Failed to save recovery codes");
        return ESP_OK;
    }

    respond_ok(req, recovery_codes_to_json(entry.recovery_codes));
    return ESP_OK;
}

cJSON* seed_phrase_to_json(const std::vector<std::string>& words)
{
    cJSON* array = cJSON_CreateArray();
    for (const std::string& w : words) {
        cJSON_AddItemToArray(array, cJSON_CreateString(w.c_str()));
    }
    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "seed_phrase", array);
    return data;
}

// PUT /api/v1/entry/seed_phrase?id=N -- sets (or REPLACES) the whole
// phrase. Body: {"words": ["abandon", "ability", ...]}. Validated in
// full server-side (vault::bip39::validate_seed_phrase() -- word
// count is one of BIP-39's five defined lengths AND every word is in
// the wordlist; see that function's own comment for what it does NOT
// check) regardless of what client-side validation the web UI itself
// does -- never trust the browser alone for something this sensitive.
esp_err_t handle_set_seed_phrase(httpd_req_t* req)
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

    std::vector<std::string> words;
    const cJSON* words_item = cJSON_GetObjectItemCaseSensitive(root, "words");
    if (cJSON_IsArray(words_item)) {
        const cJSON* w = nullptr;
        cJSON_ArrayForEach(w, words_item) {
            if (cJSON_IsString(w) && w->valuestring != nullptr) {
                words.emplace_back(w->valuestring);
            }
        }
    }
    cJSON_Delete(root);

    if (!vault::bip39::validate_seed_phrase(words)) {
        respond_error(req, "400 Bad Request",
                       "Invalid seed phrase -- must be 12/15/18/21/24 words, each from the BIP-39 wordlist");
        return ESP_OK;
    }

    entry.seed_phrase = std::move(words);

    if (!vault::update_entry(entry)) {
        respond_error(req, "500 Internal Server Error", "Failed to save seed phrase");
        return ESP_OK;
    }

    respond_ok(req, seed_phrase_to_json(entry.seed_phrase));
    return ESP_OK;
}

// DELETE /api/v1/entry/seed_phrase?id=N -- clears it.
esp_err_t handle_delete_seed_phrase(httpd_req_t* req)
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

    entry.seed_phrase.clear();

    if (!vault::update_entry(entry)) {
        respond_error(req, "500 Internal Server Error", "Failed to clear seed phrase");
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

    static char recovery_codes_path[64];
    build_prefixed_path(recovery_codes_path, sizeof(recovery_codes_path), "/api/v1/entry/recovery_codes");

    // Separate sub-path for "mark one code used" -- both it and
    // set_codes_uri below are PUT, and httpd registers one handler
    // per exact (method, path) pair, so they can't share
    // recovery_codes_path itself.
    static char recovery_codes_mark_path[64];
    build_prefixed_path(recovery_codes_mark_path, sizeof(recovery_codes_mark_path), "/api/v1/entry/recovery_codes/mark");

    static char seed_phrase_path[64];
    build_prefixed_path(seed_phrase_path, sizeof(seed_phrase_path), "/api/v1/entry/seed_phrase");

    static char search_path[64];
    build_prefixed_path(search_path, sizeof(search_path), "/api/v1/search");

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

    static httpd_uri_t set_codes_uri{};
    set_codes_uri.uri = recovery_codes_path;
    set_codes_uri.method = HTTP_PUT;
    set_codes_uri.handler = handle_set_recovery_codes;
    set_codes_uri.user_ctx = nullptr;

    static httpd_uri_t mark_code_uri{};
    mark_code_uri.uri = recovery_codes_mark_path;
    mark_code_uri.method = HTTP_PUT;
    mark_code_uri.handler = handle_mark_recovery_code;
    mark_code_uri.user_ctx = nullptr;

    static httpd_uri_t set_seed_uri{};
    set_seed_uri.uri = seed_phrase_path;
    set_seed_uri.method = HTTP_PUT;
    set_seed_uri.handler = handle_set_seed_phrase;
    set_seed_uri.user_ctx = nullptr;

    static httpd_uri_t delete_seed_uri{};
    delete_seed_uri.uri = seed_phrase_path;
    delete_seed_uri.method = HTTP_DELETE;
    delete_seed_uri.handler = handle_delete_seed_phrase;
    delete_seed_uri.user_ctx = nullptr;

    static httpd_uri_t search_uri{};
    search_uri.uri = search_path;
    search_uri.method = HTTP_GET;
    search_uri.handler = handle_search;
    search_uri.user_ctx = nullptr;

    httpd_register_uri_handler(server, &list_uri);
    httpd_register_uri_handler(server, &get_uri);
    httpd_register_uri_handler(server, &create_uri);
    httpd_register_uri_handler(server, &update_uri);
    httpd_register_uri_handler(server, &delete_uri);
    httpd_register_uri_handler(server, &set_codes_uri);
    httpd_register_uri_handler(server, &mark_code_uri);
    httpd_register_uri_handler(server, &set_seed_uri);
    httpd_register_uri_handler(server, &delete_seed_uri);
    httpd_register_uri_handler(server, &search_uri);

    ESP_LOGI(TAG, "Vault REST routes registered");
}

} // namespace web
