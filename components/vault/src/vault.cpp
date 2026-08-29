#include "vault/vault.hpp"

#include "vault/vault_repository.hpp"

#include "event_bus/event_bus.hpp"
#include "security/lock_manager.hpp"

#include "esp_log.h"

namespace vault {

namespace {

constexpr char TAG[] = "vault";

bool initialized = false;

bool is_unlocked()
{
    return security::lock::is_initialized() &&
           security::lock::state() == security::lock::State::Unlocked;
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

    initialized = true;
    ESP_LOGI(TAG, "Vault initialized (%u entries)",
             static_cast<unsigned>(repository::entry_count()));
    return true;
}

bool is_initialized()
{
    return initialized;
}

size_t entry_count()
{
    return initialized ? repository::entry_count() : 0;
}

size_t list_entries(VaultEntry* out, size_t max_count, size_t offset)
{
    if (!initialized || !is_unlocked()) {
        return 0;
    }
    return repository::list(out, max_count, offset);
}

bool get_entry(uint32_t id, VaultEntry& out)
{
    if (!initialized || !is_unlocked()) {
        return false;
    }
    return repository::get(id, out);
}

uint32_t create_entry(const VaultEntry& entry)
{
    if (!initialized || !is_unlocked()) {
        ESP_LOGW(TAG, "create_entry: denied (locked or not initialized)");
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
    if (!initialized || !is_unlocked()) {
        ESP_LOGW(TAG, "update_entry: denied (locked or not initialized)");
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
    if (!initialized || !is_unlocked()) {
        ESP_LOGW(TAG, "delete_entry: denied (locked or not initialized)");
        return false;
    }

    if (!repository::remove(id)) {
        return false;
    }
    publish(VaultEventId::EntryDeleted, id);
    return true;
}

} // namespace vault
