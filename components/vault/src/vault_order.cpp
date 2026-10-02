#include "vault/vault_order.hpp"

#include "vault/vault.hpp"

#include "nvs.h"
#include "esp_log.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_set>

namespace vault::order {

namespace {

constexpr char TAG[] = "vault.order";
constexpr char NVS_NAMESPACE[] = "vault_order";
constexpr char NVS_KEY[] = "account_order";
constexpr size_t MAX_ORDER_ENTRIES = 256;

bool initialized = false;

std::vector<uint32_t> current_vault_ids()
{
    const size_t count = vault::entry_count();
    const size_t limit = std::min(count, MAX_ORDER_ENTRIES);
    std::vector<vault::VaultEntry> entries(limit);
    if (limit > 0) {
        const size_t got = vault::list_entries(entries.data(), limit, 0);
        entries.resize(got);
    }

    std::vector<uint32_t> ids;
    ids.reserve(entries.size());
    for (const auto& entry : entries) {
        ids.push_back(entry.id);
    }
    return ids;
}

bool read_raw(std::string& out)
{
    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t required = 0;
    const esp_err_t size_err = nvs_get_str(handle, NVS_KEY, nullptr, &required);
    if (size_err != ESP_OK || required == 0 || required > 4000) {
        nvs_close(handle);
        return false;
    }

    std::string value(required, '\0');
    const esp_err_t read_err = nvs_get_str(handle, NVS_KEY, value.data(), &required);
    nvs_close(handle);
    if (read_err != ESP_OK) {
        return false;
    }

    value.resize(required > 0 ? required - 1 : 0);
    out = std::move(value);
    return true;
}

std::vector<uint32_t> parse(const std::string& raw)
{
    std::vector<uint32_t> ids;
    size_t start = 0;
    while (start < raw.size() && ids.size() < MAX_ORDER_ENTRIES) {
        const size_t comma = raw.find(',', start);
        const size_t end = (comma == std::string::npos) ? raw.size() : comma;
        if (end > start) {
            const char* first = raw.c_str() + start;
            char* last = nullptr;
            const unsigned long value = std::strtoul(first, &last, 10);
            if (last != first && static_cast<size_t>(last - first) == end - start && value != 0) {
                ids.push_back(static_cast<uint32_t>(value));
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return ids;
}

bool write(const std::vector<uint32_t>& ids)
{
    std::string raw;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) raw.push_back(',');
        raw += std::to_string(ids[i]);
    }

    nvs_handle_t handle = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return false;
    }

    const esp_err_t set_err = nvs_set_str(handle, NVS_KEY, raw.c_str());
    const esp_err_t commit_err = (set_err == ESP_OK) ? nvs_commit(handle) : set_err;
    nvs_close(handle);

    if (commit_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save account order: %s", esp_err_to_name(commit_err));
        return false;
    }
    return true;
}

std::vector<uint32_t> normalize(const std::vector<uint32_t>& stored,
                                const std::vector<uint32_t>& current)
{
    std::unordered_set<uint32_t> valid(current.begin(), current.end());
    std::unordered_set<uint32_t> seen;
    std::vector<uint32_t> result;
    result.reserve(current.size());

    for (const uint32_t id : stored) {
        if (valid.count(id) != 0 && seen.insert(id).second) {
            result.push_back(id);
        }
    }

    // New accounts, or accounts created before an order was first stored,
    // are appended in the vault's native order.
    for (const uint32_t id : current) {
        if (seen.insert(id).second) {
            result.push_back(id);
        }
    }
    return result;
}

} // namespace

bool init()
{
    if (initialized) {
        return true;
    }
    initialized = true;
    ESP_LOGI(TAG, "Vault account order state initialized");
    return true;
}

std::vector<uint32_t> get_order()
{
    const std::vector<uint32_t> current = current_vault_ids();
    if (current.empty()) {
        return {};
    }

    std::string raw;
    if (!read_raw(raw)) {
        return current;
    }

    return normalize(parse(raw), current);
}

bool set_order(const std::vector<uint32_t>& ids)
{
    if (!initialized || ids.size() > MAX_ORDER_ENTRIES) {
        return false;
    }

    const std::vector<uint32_t> current = current_vault_ids();
    if (ids.size() != current.size()) {
        return false;
    }

    std::unordered_set<uint32_t> valid(current.begin(), current.end());
    std::unordered_set<uint32_t> seen;
    for (const uint32_t id : ids) {
        if (id == 0 || valid.count(id) == 0 || !seen.insert(id).second) {
            return false;
        }
    }

    return write(ids);
}

} // namespace vault::order
