#pragma once

#include <cstdint>

// =============================================================================
// storage::sd
//
// Mounts the microSD card (4-bit SDMMC) as a FAT filesystem at
// storage_paths::SDCARD_MOUNT_POINT. FAT, not LittleFS -- the whole
// point of this card is that the user can pull it and read/write it
// on a PC (import/export/backup), and FAT is what every OS
// understands natively.
//
// Unlike storage::fs (internal LittleFS), a missing or unmountable
// card here is NOT a fatal error -- the card is removable by design.
// Callers must check is_mounted() / storage::status().sdcard_present
// before touching paths under SDCARD_MOUNT_POINT.
//
// There is no card-detect GPIO on this board (see bsp/pins.hpp),
// so insertion/removal cannot be detected automatically. A UI action
// (e.g. "Refresh SD card") should call remount() after the user says
// they've inserted/swapped a card.
// =============================================================================

namespace storage::sd {

/**
 * @brief Attempt to mount the microSD card.
 *
 * Safe to call even with no card inserted -- returns false in that
 * case, which is an expected outcome, not a fatal error for the rest
 * of the firmware.
 *
 * @return true if a card was found and mounted successfully.
 */
bool init();

bool is_mounted();

/**
 * @brief Unmount, then attempt to mount again.
 *
 * Use this after the user has inserted, removed, or swapped a card,
 * since there is no hardware detect signal to trigger it
 * automatically.
 *
 * @return true if a card is mounted after the attempt.
 */
bool remount();

void unmount();

/**
 * @brief Report space usage on the card.
 *
 * @return true on success (false if no card is mounted).
 */
bool get_usage(uint64_t& total_bytes, uint64_t& used_bytes);

} // namespace storage::sd
