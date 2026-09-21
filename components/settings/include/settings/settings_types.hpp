#pragma once

#include <cstdint>

// =============================================================================
// settings -- settings_types.hpp
//
// Field set taken directly from docs/REQUIREMENTS.md section 12
// (12.1 General, 12.2 USB, 12.3 Security, 12.4 GUI) -- not invented
// here. Where that document only names a field without pinning down
// its exact type/range/default (e.g. "typing sequence", "Web UI
// permissions"), a reasonable placeholder is defined and flagged in
// components/settings/README.md rather than silently guessed.
//
// The PIN itself is deliberately NOT here -- only the user-facing
// preference for its length (SecuritySettings::pin_length) is a
// setting. The PIN value belongs to components/security (not built
// yet), which is a secret, not a preference.
// =============================================================================

namespace settings {

enum class Language : uint8_t
{
    English,
    Russian,
};

/// Two themes: Dark (the original, still the default) and Light,
/// added alongside a real Light palette in ui::theme -- see that
/// component's own file comment for the palette values themselves.
enum class Theme : uint8_t
{
    Dark,
    Light,
};

/// Screen orientation. Auto uses components/imu (the board's onboard
/// QMI8658 accelerometer, confirmed present on this exact board) to
/// flip automatically as the device is physically turned -- see that
/// component's own file comment for what "flipped" means and its
/// current confidence level (the exact axis/sign QMI8658 reports for
/// a 180-degree flip on THIS board hasn't been verified against real
/// hardware yet). Rotate0/Rotate180 work regardless of whether an IMU
/// is even present.
enum class Orientation : uint8_t
{
    Rotate0,
    Rotate180,
    Auto,
};

/// REQUIREMENTS 12.2 names "print sequence" without specifying the
/// exact options -- placeholder, see README.md.
enum class TypingOrder : uint8_t
{
    LoginTabPasswordEnter,
    PasswordOnly,
    PasswordEnter,
};

/// REQUIREMENTS 12.3 names "Web UI permissions" without specifying
/// the exact set -- placeholder, see README.md.
enum WebUiPermission : uint32_t
{
    WEB_UI_NONE            = 0,
    WEB_UI_VIEW_ACCOUNTS   = 1u << 0,
    WEB_UI_EDIT_ACCOUNTS   = 1u << 1,
    WEB_UI_EXPORT_DATA     = 1u << 2,
    WEB_UI_CHANGE_SETTINGS = 1u << 3,
};

/// REQUIREMENTS 11.1 lists Access Point / Station / Captive Portal
/// support without specifying settings fields -- placeholder, see
/// README.md. sta_password/ap_password sit in NVS in plaintext, same
/// exposure as everything else in this project pre-Flash-Encryption
/// (see components/security's scope note) -- not a separate, smaller
/// risk.
enum class WifiMode : uint8_t
{
    Disabled,
    AccessPoint,
    Station,
};

/// Which section changed -- carried as the payload of a
/// SystemEventId::SettingsChanged event_bus event.
enum class Section : uint8_t
{
    General,
    Usb,
    Security,
    Gui,
    Wifi,
    PasswordGen,
};

struct GeneralSettings
{
    Language language = Language::English;
    Theme theme = Theme::Dark;
    Orientation orientation = Orientation::Rotate0;
    uint8_t display_brightness = 80; // 0-100
    uint32_t display_off_timeout_s = 30;
};

struct UsbSettings
{
    // "Quick password without PIN" (KeyKeeper 1.90's own
    // "Быстрый пароль без PIN-кода" / quickpass) -- deliberately
    // reachable from QuickScreen's BackLong WITHOUT unlocking (see
    // that screen's own file comment). A real secret typed in the
    // clear via USB HID without any authentication -- this is an
    // intentional convenience/security trade-off the person configuring
    // it is choosing to accept, not an oversight. Anyone who can hold
    // BackLong on a locked device can have this typed for them.
    char default_password[32] = "";
    TypingOrder typing_order = TypingOrder::LoginTabPasswordEnter;
    uint16_t delay_before_typing_ms = 500;
    uint16_t delay_between_chars_ms = 10;
    uint16_t delay_between_fields_ms = 100;

    // When true, usb::TypeEngine sends a layout-switch hotkey
    // (Alt+Shift) to the host before/after each run of Cyrillic
    // characters it types -- see usb::cyrillic_layout.hpp for the
    // full mechanism. Confirmed working for the FIRST switch in a
    // real test, but the switch-BACK after a Cyrillic run didn't
    // reliably register (produced wrong characters for what followed,
    // not just missing ones) -- inherently best-effort, since the
    // device can't know what's actually configured on the host it's
    // plugged into. Defaults to false: the person manually switching
    // their own host's keyboard layout before printing is both more
    // predictable and was explicitly what the project owner asked
    // for, given the demonstrated unreliability. When false, this
    // device never sends the hotkey at all -- Cyrillic characters
    // still type via their ЙЦУКЕН physical-key equivalent (so it
    // works correctly once you've switched the host layout yourself
    // first), non-Cyrillic characters still type via the normal US
    // mapping regardless of this setting (so a mixed string like a
    // Cyrillic domain with a literal "." in it needs the SAME layout
    // to correctly interpret both parts -- not something this device
    // tries to compensate for in manual mode).
    bool cyrillic_auto_switch_layout = false;
};

struct SecuritySettings
{
    uint8_t pin_length = 6; // 4-6 per REQUIREMENTS 12.3
    bool auto_lock_enabled = true;
    uint32_t auto_lock_timeout_s = 30;
    uint32_t web_ui_permissions = WEB_UI_VIEW_ACCOUNTS;

    // Combination-lock-style PIN entry (widgets::PinEntry's own
    // Config::dial_mode -- see that widget's header for the full
    // interaction model): rotate one direction to spin a digit,
    // REVERSING direction confirms it and advances to the next digit,
    // whose own spin direction is the opposite of the one just
    // confirmed (alternating every digit, like a real combination
    // dial). false (the default) keeps the original behavior: rotate
    // to spin, OkShort confirms and advances. Applies to both
    // LockScreen (unlocking) and SetupPinScreen (setting/changing the
    // PIN) for a consistent feel -- the way you dial it in is the way
    // you set it.
    bool pin_entry_dial_mode = false;

    // Only meaningful when pin_entry_dial_mode is true: whether the
    // LAST digit also confirms via direction-reversal, same as every
    // other digit (true), or requires an explicit OkShort instead
    // (false) -- a deliberate, unambiguous "I'm done" action for the
    // most security-critical confirmation, for anyone who'd rather
    // not rely on a rotation gesture there.
    bool dial_last_digit_reverses = true;

    // From KeyKeeper 1.90's own "secretword" feature: an optional
    // path-prefix all Web UI/REST routes require
    // (http://IP/<secret_word>/...) when non-empty -- a low-effort
    // deterrent, not real authentication (still no TLS, still visible
    // to anyone who captures the traffic; see components/web's own
    // security note). Empty means disabled -- matches 1.90's own
    // "" == no secret word convention. QuickScreen's OkShort
    // ("Print URL") types the resulting full address, prefix included,
    // via USB HID -- also matching 1.90's Main-button-click behavior.
    char secret_word[33]{};
};

struct GuiSettings
{
    bool animations_enabled = true;
    // No touchscreen on this board (REQUIREMENTS section 6: touch is
    // not required) -- off by default, reserved for a future board
    // revision or a touch-capable panel.
    bool gestures_enabled = false;
    // Not yet consumed by components/input -- reserved.
    uint8_t encoder_sensitivity = 1;
    bool hints_enabled = true;
};

struct WifiSettings
{
    WifiMode mode = WifiMode::Disabled;
    char sta_ssid[33] = "";     // 802.11 SSID: max 32 bytes + null terminator
    char sta_password[65] = ""; // WPA2 passphrase: max 63 chars + null terminator
    char ap_ssid[33] = "KeyKeeper2";
    char ap_password[65] = ""; // empty = open AP

    // Whether entering AccessPoint mode also starts wifi::captive_dns
    // (DNS-hijack) -- see that component's own file comment for the
    // full mechanism. Defaults ON: this is what makes AP mode
    // self-explanatory to connect to (a phone/laptop's own "sign in
    // to this network" prompt pops up automatically) rather than
    // requiring the person to already know to type in the device's
    // IP themselves. Off is for anyone who'd rather their AP behave
    // like a plain, unmodified access point -- e.g. if a specific
    // client's own captive-portal detection misbehaves against it.
    bool captive_portal_enabled = true;
};

// From KeyKeeper 1.90's own password generator, redesigned: 1.90 used
// a single free-text "allowed characters" string (passchars) typed in
// via its web UI. On THIS device's encoder+PinEntry/TextEntry input
// model, four independent toggles are a much better fit than typing
// a custom character-set string one character-wheel spin at a time --
// see components/password_gen's own README for the full comparison
// and the character sets actually used.
struct PasswordGenSettings
{
    uint8_t length = 16; // 4-64, see password_gen::MIN_LENGTH/MAX_LENGTH
    bool include_uppercase = true;
    bool include_lowercase = true;
    bool include_digits = true;
    bool include_symbols = true;
};

struct AllSettings
{
    GeneralSettings general;
    UsbSettings usb;
    SecuritySettings security;
    GuiSettings gui;
    WifiSettings wifi;
    PasswordGenSettings password_gen;
};

} // namespace settings
