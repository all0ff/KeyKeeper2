#include "vault/vault.hpp"

#include "vault/vault_repository.hpp"

#include "event_bus/event_bus.hpp"
#include "security/lock_manager.hpp"

#include "esp_log.h"

#include <cctype>
#include <string>

namespace vault {

namespace {

constexpr char TAG[] = "vault";

bool initialized = false;
int system_sub_handle = -1;

bool is_unlocked()
{
    return security::lock::is_initialized() &&
           security::lock::state() == security::lock::State::Unlocked;
}

bool ensure_loaded()
{
    if (!initialized || !is_unlocked()) {
        return false;
    }

    return repository::is_loaded() || repository::load();
}

void on_system_event(const event_bus::Event& event, void* /*ctx*/)
{
    if (event.category != event_bus::Category::System) {
        return;
    }

    switch (static_cast<event_bus::SystemEventId>(event.id)) {
        case event_bus::SystemEventId::DeviceUnlocked:
            if (!repository::load()) {
                ESP_LOGE(TAG, "Failed to load vault after unlock");
            }
            break;

        case event_bus::SystemEventId::DeviceLocked:
            repository::clear();
            break;

        default:
            break;
    }
}

void publish(VaultEventId id, uint32_t entry_id)
{
    if (!event_bus::is_initialized()) {
        return;
    }

    event_bus::Payload payload{};
    payload.u32 = entry_id;
    event_bus::publish(event_bus::Category::Vault, static_cast<uint32_t>(id), payload);
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!repository::init()) {
        ESP_LOGE(TAG, "VaultRepository init failed");
        return false;
    }

    if (!event_bus::is_initialized()) {
        ESP_LOGE(TAG, "EventBus must be initialized before Vault");
        return false;
    }

    system_sub_handle =
        event_bus::subscribe(event_bus::Category::System, on_system_event, nullptr);
    if (system_sub_handle < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to System events");
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "Vault initialized (database remains unloaded while locked)");
    return true;
}

bool is_initialized()
{
    return initialized;
}

size_t entry_count()
{
    return ensure_loaded() ? repository::entry_count() : 0;
}

size_t list_entries(VaultEntry* out, size_t max_count, size_t offset)
{
    if (!ensure_loaded()) {
        return 0;
    }
    return repository::list(out, max_count, offset);
}

bool get_entry(uint32_t id, VaultEntry& out)
{
    if (!ensure_loaded()) {
        return false;
    }
    return repository::get(id, out);
}

namespace {

std::string to_lower_copy(const std::string& s)
{
    std::string out = s;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool contains_ci(const std::string& haystack, const std::string& needle_lower)
{
    if (needle_lower.empty()) {
        return true;
    }
    return to_lower_copy(haystack).find(needle_lower) != std::string::npos;
}

} // namespace

size_t search_entries(const char* query, VaultEntry* out, size_t max_count)
{
    if (!ensure_loaded() || out == nullptr || max_count == 0) {
        return 0;
    }

    const std::string query_lower = to_lower_copy(query != nullptr ? query : "");

    size_t found = 0;
    constexpr size_t PAGE = 16;
    VaultEntry buf[PAGE];
    size_t offset = 0;

    while (found < max_count) {
        const size_t n = repository::list(buf, PAGE, offset);
        if (n == 0) {
            break;
        }
        for (size_t i = 0; i < n && found < max_count; ++i) {
            const VaultEntry& e = buf[i];
            if (contains_ci(e.login, query_lower) || contains_ci(e.url, query_lower) ||
                contains_ci(e.notes, query_lower)) {
                out[found++] = e;
            }
        }
        offset += n;
        if (n < PAGE) {
            break;
        }
    }

    return found;
}

uint32_t create_entry(const VaultEntry& entry)
{
    if (!ensure_loaded()) {
        ESP_LOGW(TAG, "create_entry: denied (locked, not initialized, or vault not loaded)");
        return INVALID_ID;
    }

    const uint32_t id = repository::add(entry);
    if (id != INVALID_ID) {
        publish(VaultEventId::EntryCreated, id);
    }
    return id;
}

bool update_entry(const VaultEntry& entry)
{
    if (!ensure_loaded()) {
        ESP_LOGW(TAG, "update_entry: denied (locked, not initialized, or vault not loaded)");
        return false;
    }

    if (!repository::update(entry)) {
        return false;
    }
    publish(VaultEventId::EntryUpdated, entry.id);
    return true;
}

bool delete_entry(uint32_t id)
{
    if (!ensure_loaded()) {
        ESP_LOGW(TAG, "delete_entry: denied (locked, not initialized, or vault not loaded)");
        return false;
    }

    if (!repository::remove(id)) {
        return false;
    }
    publish(VaultEventId::EntryDeleted, id);
    return true;
}

} // namespace vault
