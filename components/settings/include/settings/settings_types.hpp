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

/// Only "Dark Theme" is specified in docs/ARCHITECTURE.md's Theme
/// System section -- extend this enum if/when a second theme is
/// actually designed, don't add speculative options now.
enum class Theme : uint8_t
{
    Dark,
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

/// Which section changed -- carried as the payload of a
/// SystemEventId::SettingsChanged event_bus event.
enum class Section : uint8_t
{
    General,
    Usb,
    Security,
    Gui,
};

struct GeneralSettings
{
    Language language = Language::English;
    Theme theme = Theme::Dark;
    uint8_t display_brightness = 80; // 0-100
    uint32_t display_off_timeout_s = 30;
};

struct UsbSettings
{
    char default_password[32] = "";
    TypingOrder typing_order = TypingOrder::LoginTabPasswordEnter;
    uint16_t delay_before_typing_ms = 500;
    uint16_t delay_between_chars_ms = 10;
    uint16_t delay_between_fields_ms = 100;
};

struct SecuritySettings
{
    uint8_t pin_length = 6; // 4-6 per REQUIREMENTS 12.3
    bool auto_lock_enabled = true;
    uint32_t auto_lock_timeout_s = 30;
    uint32_t web_ui_permissions = WEB_UI_VIEW_ACCOUNTS;
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

struct AllSettings
{
    GeneralSettings general;
    UsbSettings usb;
    SecuritySettings security;
    GuiSettings gui;
};

} // namespace settings
