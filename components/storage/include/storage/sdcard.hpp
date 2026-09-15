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
 * @brief Best-effort signal for whether the LAST failed mount attempt
 *        looked like "a card is physically present but its
 *        filesystem couldn't be read" rather than "no card responded
 *        at all" -- see sdcard.cpp's own comment on why this can't be
 *        fully precise. Meaningless if is_mounted() is currently true.
 */
bool mount_looked_unreadable();

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

/**
 * @brief Unmount, then format the card as FAT32 and mount it.
 *
 * Destructive -- erases everything currently on the card. Unlike
 * init()/remount() (which deliberately never auto-format, see
 * sdcard.cpp's own comment: silently reformatting removable media the
 * user may already have files on would be destructive), this is for
 * an EXPLICIT, user-confirmed "Format SD Card" action -- e.g. a card
 * that shows as unreadable because it shipped pre-formatted exFAT
 * (common on cards 32GB and up; this project's FAT-only mount code,
 * like most embedded FAT stacks, doesn't read exFAT at all) rather
 * than genuinely being absent or faulty.
 *
 * @return true if the card was formatted and mounted successfully.
 */
bool format_and_mount();

void unmount();

/**
 * @brief Report space usage on the card.
 *
 * @return true on success (false if no card is mounted).
 */
bool get_usage(uint64_t& total_bytes, uint64_t& used_bytes);

} // namespace storage::sd
