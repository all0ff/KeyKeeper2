#pragma once

#include <cstdint>

// =============================================================================
// rtc_time -- trustworthy wall-clock time, when available.
//
// This board has no battery-backed RTC chip -- the ONLY source of
// real time is NTP over Wi-Fi (Station mode). This is a real,
// standing limitation, not a temporary gap:
//   - Without Wi-Fi Station configured and actually connected at
//     least once since boot, there is no trustworthy time at all.
//   - A full power loss resets the SoC's own internal clock to zero
//     -- there's no battery keeping it running across power cycles
//     the way a dedicated RTC chip with its own coin cell would.
//     Every boot needs a fresh sync before is_synced() becomes true
//     again.
//   - An external RTC chip added to the board later would remove
//     this limitation; not present on the currently supported
//     hardware.
//
// This component owns exactly that: kicking off an SNTP sync when
// Wi-Fi Station connects (see wifi::apply_settings()'s own call into
// start_sync()) and tracking whether one has ever succeeded. It does
// NOT depend on wifi:: itself -- wifi:: depends on THIS component
// (calls start_sync() on its own IP_EVENT_STA_GOT_IP handler), not
// the other way around, so anything needing time (components/totp)
// can depend on just this, not the whole networking stack.
//
// Once synced, ordinary standard-library time functions (time(),
// per ESP-IDF's own SNTP implementation calling settimeofday()
// internally) return correct UTC seconds-since-epoch -- unix_time()
// here is a thin, explicitly-named wrapper for that, not a separate
// clock.
// =============================================================================

namespace rtc_time {

/**
 * @brief Set up the underlying SNTP client. Call once at boot, after
 *        event_bus::init() (needed for the sync-completed event
 *        handler) and before wifi::init().
 *
 * Does NOT start syncing by itself -- see start_sync().
 */
bool init();

bool is_initialized();

/**
 * @brief (Re)start an SNTP sync attempt against the configured
 *        server. Safe to call repeatedly (e.g. every time Wi-Fi
 *        Station reconnects) -- restarts cleanly if already running
 *        or already synced, which also helps catch client-side clock
 *        drift on a long-running session.
 */
void start_sync();

/**
 * @brief True once at least one SNTP sync has succeeded THIS BOOT.
 *
 * Everything that needs real time (components/totp in particular)
 * must check this before trusting unix_time() -- an unsynced clock
 * reads as roughly zero (Jan 1 1970), not a plausible current time,
 * so using it unchecked wouldn't just be imprecise, it would silently
 * generate a TOTP code for entirely the wrong moment.
 */
bool is_synced();

/// Current UTC time as Unix seconds-since-epoch. Meaningless (reads
/// near zero) unless is_synced() is true -- see that function's own
/// comment.
uint64_t unix_time();

} // namespace rtc_time
