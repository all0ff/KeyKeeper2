#include "web_reorder_routes.hpp"

#include "security/lock_manager.hpp"
#include "web_json_helpers.hpp"
#include "web_reorder_state.hpp"

#include "cJSON.h"
#include "esp_log.h"

#include <cstdint>
#include <string>
#include <vector>

namespace web {

namespace {

constexpr char TAG[] = "web.reorder";

bool require_unlocked(httpd_req_t* req)
{
    if (security::lock::state() != security::lock::State::Unlocked) {
        respond_error(req, "401 Unauthorized", "Not authenticated");
        return false;
    }
    security::lock::notify_activity();
    return true;
}

esp_err_t handle_get(httpd_req_t* req)
{
    if (!require_unlocked(req)) {
        return ESP_OK;
    }

    cJSON* ids = cJSON_CreateArray();
    for (const uint32_t id : reorder::get_order()) {
        cJSON_AddItemToArray(ids, cJSON_CreateNumber(static_cast<double>(id)));
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "ids", ids);
    respond_ok(req, data);
    return ESP_OK;
}

esp_err_t handle_put(httpd_req_t* req)
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

    const cJSON* ids_item = cJSON_GetObjectItemCaseSensitive(root, "ids");
    std::vector<uint32_t> ids;
    if (cJSON_IsArray(ids_item)) {
        const cJSON* item = nullptr;
        cJSON_ArrayForEach(item, ids_item) {
            if (!cJSON_IsNumber(item) || item->valuedouble <= 0.0 || item->valuedouble > 4294967295.0) {
                cJSON_Delete(root);
                respond_error(req, "400 Bad Request", "Invalid account order");
                return ESP_OK;
            }
            ids.push_back(static_cast<uint32_t>(item->valuedouble));
        }
    }
    cJSON_Delete(root);

    if (!reorder::set_order(ids)) {
        ESP_LOGW(TAG, "Rejected invalid account order update");
        respond_error(req, "400 Bad Request", "Account order does not match the current accounts");
        return ESP_OK;
    }

    respond_ok(req, nullptr);
    return ESP_OK;
}

} // namespace

void register_reorder_routes(httpd_handle_t server)
{
    static httpd_uri_t get_uri{};
    static char get_path[64];
    build_prefixed_path(get_path, sizeof(get_path), "/api/v1/account-order");
    get_uri.uri = get_path;
    get_uri.method = HTTP_GET;
    get_uri.handler = handle_get;
    get_uri.user_ctx = nullptr;

    static httpd_uri_t put_uri{};
    static char put_path[64];
    build_prefixed_path(put_path, sizeof(put_path), "/api/v1/account-order");
    put_uri.uri = put_path;
    put_uri.method = HTTP_PUT;
    put_uri.handler = handle_put;
    put_uri.user_ctx = nullptr;

    httpd_register_uri_handler(server, &get_uri);
    httpd_register_uri_handler(server, &put_uri);
}

} // namespace web
