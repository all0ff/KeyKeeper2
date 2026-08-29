#pragma once

#include "vault/vault_model.hpp"

#include <cstdint>

// =============================================================================
// vault -- VaultRepository (vault_repository.hpp)
//
// Owns the on-disk format of vault.db and the in-memory copy of every
// entry. The ONLY thing in this firmware that calls
// storage::vaultfile -- per docs/STORAGE.md's access rule
// (VaultService -> StorageService), everything above this (vault.cpp,
// eventually gui) goes through VaultRepository, never through
// storage:: directly.
//
// On-disk format (owned entirely by this file, per STORAGE.md's
// "формат базы данных определяется компонентом Vault"):
//
//   Header (12 bytes):
//     uint32_t magic          'K' 'K' 'V' 'T' (0x54564B4B on read as
//                              little-endian uint32, written byte by
//                              byte so endianness is a non-issue)
//     uint16_t format_version VAULT_FORMAT_VERSION
//     uint16_t reserved       must be 0, ignored on read
//     uint32_t entry_count
//
//   entry_count times:
//     uint32_t entry_length   byte length of the fields section below
//                              (NOT including this uint32_t itself)
//     fields...                TLV-encoded, see vault_repository.cpp
//
// TLV fields let a future format version add/rename fields without
// breaking this reader: an unrecognized field type is skipped using
// its length, not rejected. entry_length lets a reader skip an entire
// malformed/future entry the same way. VAULT_FORMAT_VERSION exists so
// a real migration can be written later per docs/STORAGE.md's
// "Database Version" requirement -- there is exactly one version so
// far, so there is nothing to migrate FROM yet.
// =============================================================================

namespace vault::repository {

inline constexpr uint16_t VAULT_FORMAT_VERSION = 1;

/**
 * @brief Load vault.db via storage::vaultfile into memory, or start
 *        an empty vault if it doesn't exist yet (a brand new device).
 *
 * @return true on success. false if vault.db exists but is corrupt/
 *         unreadable (an empty, nonexistent vault.db is NOT a
 *         failure -- see above).
 */
bool init();

bool is_initialized();

size_t entry_count();

/**
 * @brief Copy up to max_count entries starting at offset into out.
 *
 * @return How many were actually copied (may be less than max_count
 *         if fewer entries remain past offset).
 */
size_t list(VaultEntry* out, size_t max_count, size_t offset = 0);

bool get(uint32_t id, VaultEntry& out);

/**
 * @brief Add a new entry.
 *
 * Assigns and returns a fresh id (entry.id is ignored on input),
 * validates the entry (see vault_model.hpp), and persists the whole
 * vault immediately.
 *
 * @return The new entry's id, or INVALID_ID on failure (validation
 *         failed, or the write to vault.db failed).
 */
uint32_t add(VaultEntry entry);

/**
 * @brief Replace an existing entry (matched by entry.id) and persist
 *        immediately.
 *
 * @return false if entry.id doesn't exist, validation fails, or the
 *         write fails.
 */
bool update(const VaultEntry& entry);

bool remove(uint32_t id);

} // namespace vault::repository
