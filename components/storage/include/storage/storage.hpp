#pragma once

// =============================================================================
// storage
//
// Single entry point for the whole storage subsystem: NVS (settings),
// internal LittleFS (vault.db), and microSD (backup/import/export).
// Other components should call storage::init() and nothing else in
// this component directly, except where they need the specific
// sub-namespace (storage::nvs for settings, storage::sd::remount()
// for a UI "refresh SD card" action).
//
// See storage_paths.hpp for where things live, and each
// sub-component's header (nvs_storage.hpp, filesystem.hpp,
// sdcard.hpp) for backend-specific details.
// =============================================================================

namespace storage {

struct Status
{
    /// Always true after a successful init() -- the vault cannot
    /// function without this, so init() itself fails if it isn't.
    bool littlefs_ready;

    /// May be false at any time -- the card is removable. Check this
    /// (or call storage::sd::is_mounted() directly) before touching
    /// any path under storage::paths::SDCARD_MOUNT_POINT.
    bool sdcard_present;
};

/**
 * @brief Initialize NVS, mount internal LittleFS, and attempt to
 *        mount the microSD card.
 *
 * Fails only if NVS or LittleFS fail to come up -- a missing/
 * unmountable SD card is reported via status().sdcard_present, not
 * as an init() failure, since the card is optional by design.
 *
 * @return true on success.
 */
bool init();

bool is_initialized();

const Status& status();

/**
 * @brief Re-attempt mounting the SD card.
 *
 * Call this from a UI action after the user says they've inserted or
 * swapped a card -- there is no hardware detect signal, so this is
 * the only way the firmware finds out.
 *
 * @return true if a card is mounted after the attempt.
 */
bool refresh_sdcard();

} // namespace storage
