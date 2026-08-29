#pragma once

#include "settings/settings_types.hpp"

// =============================================================================
// settings -- SettingsService
//
// Single access point for all user settings, per docs/ARCHITECTURE.md
// ("Настройки изменяются только через SettingsService" / "Прямой
// доступ к NVS из других компонентов запрещён"): from this component
// onward, nothing else in the firmware should call storage::nvs
// directly for application settings -- go through settings:: instead.
// (storage::nvs itself stays available for components that have their
// own genuinely private key-value needs unrelated to user-facing
// settings, but General/USB/Security/GUI settings specifically live
// here and only here.)
//
// Each section persists independently (its own NVS blob) so changing
// one doesn't rewrite the others, and publishes
// event_bus::SystemEventId::SettingsChanged (payload: which Section)
// after a successful write, per docs/SOFTWARE.md's EventBus example
// list.
// =============================================================================

namespace settings {

/**
 * @brief Load settings from NVS via storage::nvs, or fall back to
 *        defaults for any section that's missing or the wrong size
 *        (first boot, or a firmware update that changed a struct's
 *        layout).
 *
 * Must be called after storage::init(). Safe to call once; a second
 * call is a no-op that returns true.
 */
bool init();

bool is_initialized();

/**
 * @brief The full current settings snapshot.
 *
 * Returns a reference to the live in-RAM copy -- valid until the next
 * set_*() call, so copy out anything you need to keep past that.
 */
const AllSettings& all();

/**
 * @brief Replace one section and persist it immediately.
 *
 * On success, publishes event_bus::SystemEventId::SettingsChanged
 * with the corresponding Section as payload.
 *
 * @return true on success, false if the NVS write failed (the in-RAM
 *         copy is left unchanged in that case).
 */
bool set_general(const GeneralSettings& value);
bool set_usb(const UsbSettings& value);
bool set_security(const SecuritySettings& value);
bool set_gui(const GuiSettings& value);

/**
 * @brief Reset every section to its default values and persist all
 *        four.
 *
 * Rarely needed -- a "factory reset settings" action. Publishes
 * SettingsChanged once per section.
 *
 * @return true only if every section persisted successfully.
 */
bool reset_to_defaults();

} // namespace settings
