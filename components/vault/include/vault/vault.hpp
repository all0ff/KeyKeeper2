#pragma once

#include "vault/vault_model.hpp"

// =============================================================================
// vault -- VaultService (vault.hpp)
//
// Public facade. Every operation requires the device to be Unlocked
// (security::lock::is_locked() == false) -- that is the baseline gate
// for "you can touch the vault at all", matching the user's own
// architecture diagram (SecurityService -> VaultService). The six
// specific permission::Operation checks (PrintPassword, ExportVault,
// ...) are NOT invoked here for plain CRUD -- they belong to whichever
// component actually performs that specific sensitive action (USB HID
// typing, backup/import/export -- none built yet), per
// docs/REQUIREMENTS.md 9.3's own scoping of what each one gates.
//
// Delegates all persistence to VaultRepository (vault_repository.hpp)
// -- this file adds the lock gate and event_bus notifications on top,
// nothing else. No KeyManager, no AES, no encryption -- see
// components/vault/README.md and components/security/README.md.
// =============================================================================

namespace vault {

/// IDs for event_bus::Category::Vault, owned here since vault is the
/// component that category belongs to (see event_bus/event.hpp's
/// file-level comment on category ownership).
enum class VaultEventId : uint32_t
{
    EntryCreated,
    EntryUpdated,
    EntryDeleted,
};

/**
 * @brief Load the vault (via VaultRepository) into memory.
 *
 * Must be called after storage::init() and security::init(). Safe to
 * call once; a second call is a no-op that returns true.
 */
bool init();

bool is_initialized();

size_t entry_count();

/// See VaultRepository::list() -- same paging semantics. Still gated
/// like every other operation here: returns 0 while locked.
size_t list_entries(VaultEntry* out, size_t max_count, size_t offset = 0);

bool get_entry(uint32_t id, VaultEntry& out);

/// @return The new entry's id, or vault::INVALID_ID on failure
///         (locked, validation failed, or the write to vault.db
///         failed). Publishes VaultEventId::EntryCreated on success.
uint32_t create_entry(const VaultEntry& entry);

/// Publishes VaultEventId::EntryUpdated on success.
bool update_entry(const VaultEntry& entry);

/// Publishes VaultEventId::EntryDeleted on success.
bool delete_entry(uint32_t id);

} // namespace vault
