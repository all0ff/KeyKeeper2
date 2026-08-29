#pragma once

#include <cstdint>

// =============================================================================
// ui::icon
//
// docs/GUI.md section 18 calls for a custom monochrome SVG icon set
// covering the 31 icons listed there, with no third-party icon
// library. No such SVG asset set exists yet -- producing one is a
// design task, not a code task.
//
// PLACEHOLDER: this maps each IconId to LVGL's own built-in symbol
// font (bundled with LVGL, which the project already depends on --
// not an external icon pack, so it doesn't violate the "no
// third-party icon library" rule in spirit, just in exact visual
// style). Screens/widgets should reference icons only through IconId,
// never LV_SYMBOL_* directly -- so swapping in the real custom icon
// set later is a one-file change here, not a hunt through every
// screen.
//
// Several IDs have no reasonable LVGL symbol equivalent (Vault,
// Favorite, User, Globe, Otp, Language, Theme, Flash) and currently
// return an empty string -- rendered as blank until real icons exist.
// See components/ui/README.md.
// =============================================================================

namespace ui::icon {

enum class IconId : uint8_t
{
    Vault,
    Folder,
    Favorite,
    Search,
    Settings,
    Usb,
    WiFi,
    Backup,
    Restore,
    Import,
    Export,
    Lock,
    Unlock,
    Password,
    User,
    Url,
    Globe,
    Otp,
    Edit,
    Delete,
    Save,
    Cancel,
    Warning,
    Error,
    Success,
    Info,
    Language,
    Theme,
    Brightness,
    SdCard,
    Flash,
    About,
};

/**
 * @brief Return an LVGL symbol string usable directly as label text
 *        (e.g. lv_label_set_text(label, ui::icon::symbol(...))).
 *
 * May return "" for IDs with no placeholder available yet -- callers
 * should handle that gracefully (e.g. skip the icon, don't assume
 * non-empty).
 */
const char* symbol(IconId id);

} // namespace ui::icon
