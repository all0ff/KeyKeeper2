#pragma once

#include "ui/screen.hpp"
#include "ui/widgets/text_entry.hpp"

#include "settings/settings_types.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::UsbSettingsScreen
//
// docs/GUI.md 14's USB Settings: Password Shortcut, Print Delays,
// Print Sequence -- maps directly onto settings::UsbSettings
// (settings_types.hpp): default_password, the three delay_*_ms
// fields, and typing_order.
//
// Auto-switch layout (CyrillicAutoSwitch) added later, not part of
// GUI.md 14's original list -- toggles
// settings::UsbSettings::cyrillic_auto_switch_layout, see that
// field's own comment for the full reasoning (defaults off: manual
// host-side layout switching, after a real test found the automatic
// switch-back unreliable).
//
// Password Shortcut uses widgets::TextEntry (masked, like a password
// field elsewhere in this UI) rather than a dedicated widget --
// same reasoning as AccountEditScreen's Password field.
//
// Print Sequence (TypingOrder) has no considered UX name mapping for
// its three values beyond what settings_types.hpp already documents
// as placeholders (REQUIREMENTS 12.2 only named the field, not its
// options) -- shown here as literally what they do
// (Login+Tab+Password+Enter / Password Only / Password+Enter).
//
// Print Delays: no documented sensible min/max/step anywhere -- 50 ms
// steps, 0-5000 ms range, both placeholders.
// =============================================================================

namespace ui::screens {

class UsbSettingsScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Row : uint8_t
    {
        PasswordShortcut,
        DelayBeforeTyping,
        DelayBetweenChars,
        DelayBetweenFields,
        PrintSequence,
        CyrillicAutoSwitch,
        Save,
    };
    static constexpr size_t ROW_COUNT = 7;

    enum class Mode : uint8_t
    {
        Browse,
        Adjust,
        EditText,
    };

    void build_rows(lv_obj_t* parent);
    void render_rows();
    void move_selection(int32_t delta);
    void adjust_value(int32_t delta);
    void activate();
    void enter_edit_password();
    void exit_edit_password(bool commit);
    void save();

    const char* typing_order_label() const;

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;

    size_t selected_row_ = 0;
    Mode mode_ = Mode::Browse;

    // Working copy -- persisted via settings::set_usb() on Save.
    char password_shortcut_[32]{};
    uint16_t delay_before_typing_ms_ = 0;
    uint16_t delay_between_chars_ms_ = 0;
    uint16_t delay_between_fields_ms_ = 0;
    settings::TypingOrder typing_order_ = settings::TypingOrder::LoginTabPasswordEnter;
    bool cyrillic_auto_switch_layout_ = false;

    widgets::TextEntry text_entry_;
};

} // namespace ui::screens
