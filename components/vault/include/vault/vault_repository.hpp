#pragma once

#include "vault/vault_model.hpp"

#include <cstdint>
#include <vector>

// =============================================================================
// vault -- VaultRepository (vault_repository.hpp)
//
// Owns the on-disk format of vault.db and the in-memory copy of every
// entry. The ONLY thing in this firmware that calls storage::vaultfile
// is VaultRepository; higher layers go through this interface.
// =============================================================================

namespace vault::repository {

/// v1: id/login/password/url/notes/totp_secret/created_at/updated_at.
/// v2: adds category, favorite. v3: adds recovery_codes. v4: adds
/// seed_phrase. All four are read by this firmware -- see
/// vault_repository.cpp's decode_all() for how an older v1/v2/v3 file
/// loads (missing fields default to empty/false, nothing is
/// rejected).
inline constexpr uint16_t VAULT_FORMAT_VERSION = 4;

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

/**
 * @brief Re-encode and re-write vault.db from the current in-memory
 *        entries right now, under whatever the CURRENT
 *        security::vault_key session key is -- the same encrypt +
 *        write this module already does after every add()/update()/
 *        remove() on its own, just callable directly.
 *
 * For ui::AsyncPinCheck's own use: after a PIN change re-keys
 * security::vault_key (see pin_manager.cpp's store_new_pin()), the
 * already-decrypted entries this Repository is holding need
 * re-encrypting under that NEW key and writing back immediately --
 * otherwise vault.db stays encrypted under the OLD key on disk while
 * the session now holds the new one, and the next load() would fail
 * to decrypt it at all.
 *
 * @return false if not loaded (is_loaded() false), or the underlying
 *         encrypt/write itself failed.
 */
bool persist_now();

size_t list(VaultEntry* out, size_t max_count, size_t offset = 0);
bool get(uint32_t id, VaultEntry& out);

/**
 * @brief The current in-memory vault, encoded as the same plaintext
 *        v4 blob persist() would encrypt and write to vault.db --
 *        for vault::backup::create_backup()'s own use (see that
 *        file's own comment: backups are deliberately kept plaintext
 *        rather than encrypted, unlike vault.db itself, so this is
 *        NOT the same bytes persist() actually writes to disk).
 *
 * @return Empty if not loaded (is_loaded() false).
 */
std::vector<uint8_t> export_plaintext();
uint32_t add(VaultEntry entry);
bool update(const VaultEntry& entry);
bool remove(uint32_t id);

} // namespace vault::repository
