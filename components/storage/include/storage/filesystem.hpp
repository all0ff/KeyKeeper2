#pragma once

#include <cstddef>
#include <cstdint>

// =============================================================================
// storage::fs
//
// Mounts the internal LittleFS partition ("storage" in partitions.csv)
// at storage_paths::INTERNAL_MOUNT_POINT. This is where vault.db
// lives, and it must always be mountable -- there is no fallback if
// this fails, unlike the microSD card which is allowed to be absent.
//
// After init() succeeds, use plain POSIX stdio (fopen/fread/fwrite/
// fclose, mkdir, stat, ...) against paths under the mount point.
// This wrapper only owns mounting/unmounting and usage reporting, not
// file I/O itself -- ESP-IDF's VFS layer already gives every
// component that for free once mounted.
// =============================================================================

namespace storage::fs {

/**
 * @brief Mount the internal LittleFS partition.
 *
 * Formats the partition automatically if it fails to mount (e.g.
 * first boot, or a corrupted filesystem) -- for this device that is
 * the right default: an unreadable vault partition is not something
 * the firmware can recover from any other way, and the vault
 * component is responsible for detecting "empty vault" vs. "existing
 * vault" independently of this layer.
 *
 * @return true on success (mounted, whether freshly formatted or not).
 */
bool init();

bool is_initialized();

/**
 * @brief Report space usage on the internal partition.
 *
 * @return true on success.
 */
bool get_usage(size_t& total_bytes, size_t& used_bytes);

} // namespace storage::fs
