#pragma once

#include <cstddef>
#include <cstdint>

// =============================================================================
// storage::vaultfile
//
// The ONLY sanctioned way to read/write vault.db, per docs/STORAGE.md's
// access rule (VaultService -> StorageService -> LittleFS; GUI/USB/
// anything else talking to LittleFS directly is explicitly forbidden
// there). components/vault's VaultRepository is the intended --
// and, in this firmware, only -- caller.
//
// Whole-file semantics on purpose: vault.db is small enough (a
// handful of accounts, well under the LittleFS partition's 4 MB) that
// "read it all into RAM, parse/modify, write it all back" is simpler
// and safer than incremental file I/O, and it's what VaultRepository's
// TLV-record parsing needs anyway -- the whole buffer, in memory, to
// walk record by record.
//
// write_all() is crash-safe: it writes to a temp file first, then
// renames over vault.db, so a power loss mid-write can't leave a
// half-written, corrupted database. A power loss between the write
// and the rename just leaves the old vault.db untouched and an orphan
// temp file, which the next write_all() overwrites again.
// =============================================================================

namespace storage::vaultfile {

bool exists();

/// Current size in bytes, or 0 if the file doesn't exist yet (a brand
/// new vault, before the first write_all()).
size_t size();

/**
 * @brief Read the entire file into a caller-provided buffer.
 *
 * @param out Buffer to read into.
 * @param inout_size In: buffer capacity. Out: the file's actual size,
 *                    set even if the buffer was too small, so the
 *                    caller can retry with a bigger buffer (call
 *                    size() first to avoid ever hitting this).
 * @return true on success.
 */
bool read_all(uint8_t* out, size_t& inout_size);

/**
 * @brief Atomically replace vault.db with a full new buffer.
 *
 * @return true on success. On failure, the previous vault.db (if any)
 *         is left untouched.
 */
bool write_all(const uint8_t* data, size_t size);

/// Rarely needed -- e.g. a "factory reset vault" action. Does not
/// touch backups.
bool remove();

} // namespace storage::vaultfile
