#pragma once

#include <cstddef>
#include <cstdint>

// =============================================================================
// storage::nvs
//
// Thin wrapper around ESP-IDF's NVS (Non-Volatile Storage) for small
// key-value settings -- things like "last selected menu index" or
// "backlight brightness", not vault data. The vault database lives on
// LittleFS (see filesystem.hpp) precisely because NVS is unsuited to
// arbitrarily large, structured, frequently-rewritten blobs.
//
// Each call opens and closes its own NVS handle. That's less
// efficient than holding a handle open across many operations, but
// settings changes are infrequent (menu-driven, not a hot path), and
// the simplicity/robustness trade-off is worth it here.
// =============================================================================

namespace storage::nvs {

/**
 * @brief Initialize the NVS flash subsystem.
 *
 * Must be called once before any other function in this namespace.
 * Handles the standard "no free pages / new NVS version found"
 * recovery path by erasing and reinitializing automatically -- this
 * is expected on first boot after a partition table change.
 *
 * @return true on success.
 */
bool init();

bool is_initialized();

bool set_u32(const char* ns, const char* key, uint32_t value);
bool get_u32(const char* ns, const char* key, uint32_t& out);

bool set_str(const char* ns, const char* key, const char* value);

/**
 * @brief Read a string value.
 *
 * @param out_capacity Size of the out buffer, including room for the
 *                      null terminator.
 * @return true on success. false if the key doesn't exist OR the
 *         buffer is too small (check the log for which).
 */
bool get_str(const char* ns, const char* key, char* out, size_t out_capacity);

bool set_blob(const char* ns, const char* key, const void* data, size_t len);

/**
 * @brief Read a blob value.
 *
 * @param inout_len In: size of the out buffer. Out: actual blob size
 *                  (set even if the buffer was too small, so the
 *                  caller can retry with a bigger buffer).
 */
bool get_blob(const char* ns, const char* key, void* out, size_t& inout_len);

bool erase_key(const char* ns, const char* key);

/// Erase every key in a namespace. Rarely needed; mainly for a
/// "factory reset settings" action.
bool erase_namespace(const char* ns);

} // namespace storage::nvs
