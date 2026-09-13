#pragma once

#include "vault/vault_model.hpp"

#include <cstdint>

// =============================================================================
// vault -- VaultRepository (vault_repository.hpp)
//
// Owns the on-disk format of vault.db and the in-memory copy of every
// entry. The ONLY thing in this firmware that calls storage::vaultfile
// is VaultRepository; higher layers go through this interface.
// =============================================================================

namespace vault::repository {

/// v1: id/login/password/url/notes/totp_secret/created_at/updated_at.
/// v2: adds category, favorite. Both are read by this firmware --
/// see vault_repository.cpp's decode_all() for how an older v1 file
/// loads (missing fields default to empty/false, nothing is rejected).
inline constexpr uint16_t VAULT_FORMAT_VERSION = 2;

bool init();
bool load();

/**
 * @brief Wipe both the in-memory vault and the persistent vault.db.
 *
 * The operation deliberately does not touch SD-card backups or settings.
 * Secret contents are cleared from RAM before the persistent file is
 * removed.
 */
bool wipe();

/**
 * @brief Clear the in-memory vault copy without touching vault.db.
 */
void clear();

bool is_initialized();
bool is_loaded();
size_t entry_count();

size_t list(VaultEntry* out, size_t max_count, size_t offset = 0);
bool get(uint32_t id, VaultEntry& out);
uint32_t add(VaultEntry entry);
bool update(const VaultEntry& entry);
bool remove(uint32_t id);

} // namespace vault::repository
