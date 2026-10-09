#include "ui/screens/wireless_screen.hpp"

#include "display/fonts.hpp"
#include "ui/localization.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "settings/settings.hpp"
#include "wireless/wireless.hpp"

#include "esp_log.h"
#include "sdkconfig.h"

#include <cstdio>

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.wireless";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 22;
constexpr uint32_t TIMER_MS = 250;
constexpr uint32_t RESULT_SHOW_TICKS = 6; // ~1.5 s of "Paired!" before going back
// start_pairing() only queues the command for the link task; until it has run, the status still says "not
// pairing". Give it this many ticks (~3 s) before concluding that pairing ended without a result.
constexpr uint32_t START_GRACE_TICKS = 12;

} // namespace

WirelessScreen::~WirelessScreen()
{
    // Normally on_hide() already stopped it; this covers a screen destroyed without being hidden first.
    stop_timer();
}

const char* WirelessScreen::title() const
{
    return "Wireless typing";
}

const char* WirelessScreen::footer_hint() const
{
    // The footer is read when the screen is shown, so it names the keys of the whole screen, not of one state.
    return "OK: select   BACK: return";
}

void WirelessScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;
    build_browse();
}

void WirelessScreen::on_show()
{
    start_timer();
}

void WirelessScreen::on_hide()
{
    stop_timer();
}

void WirelessScreen::start_timer()
{
    if (timer_ == nullptr) {
        timer_ = lv_timer_create(&WirelessScreen::timer_cb, TIMER_MS, this);
    }
}

void WirelessScreen::stop_timer()
{
    if (timer_ != nullptr) {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }
}

void WirelessScreen::timer_cb(lv_timer_t* t)
{
    static_cast<WirelessScreen*>(lv_timer_get_user_data(t))->refresh_status();
}

// ---- Browse

void WirelessScreen::build_browse()
{
    mode_ = Mode::Browse;
    confirm_forget_ = false;
    lv_obj_clean(content_parent_);
    status_label_ = nullptr;
    code_label_ = nullptr;
    info_label_ = nullptr;

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_set_style_text_font(label, &keykeeper_cyrillic_16, 0);
        lv_obj_set_width(label, LV_PCT(96));
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_font(status_label_, &keykeeper_cyrillic_16, 0);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_obj_set_width(status_label_, LV_PCT(96));
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL);
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    render_rows();
    refresh_status();
}

void WirelessScreen::render_rows()
{
    const theme::Palette& pal = theme::current();
    const wireless::Status s = wireless::status();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool selected = (i == selected_row_);
        lv_obj_set_style_text_color(row_labels_[i], selected ? pal.accent : pal.primary_text, 0);
        const char* prefix = selected ? "> " : "";
        switch (static_cast<Row>(i)) {
            case Row::Switch:
                lv_label_set_text_fmt(row_labels_[i], "%sWireless typing: %s", prefix, s.enabled ? "On" : "Off");
                break;
            case Row::Pair:
                lv_label_set_text_fmt(row_labels_[i], "%s%s", prefix, s.paired ? "Pair another dongle..." : "Pair dongle...");
                break;
            case Row::Forget:
                if (confirm_forget_) {
                    lv_label_set_text_fmt(row_labels_[i], "%sForget the dongle? OK = yes", prefix);
                } else if (s.paired) {
                    lv_label_set_text_fmt(row_labels_[i], "%sForget dongle (%.8s)", prefix, s.peer);
                } else {
                    lv_label_set_text_fmt(row_labels_[i], "%sForget dongle (none paired)", prefix);
                }
                break;
        }
    }
    if (row_labels_[selected_row_] != nullptr) {
        lv_obj_scroll_to_view(row_labels_[selected_row_], LV_ANIM_ON);
    }
}

void WirelessScreen::refresh_status()
{
    const wireless::Status s = wireless::status();

    if (mode_ == Mode::Browse) {
        if (status_label_ == nullptr) {
            return;
        }
        const char* text = "";
        switch (s.phase) {
            case wireless::Phase::Unsupported: text = "Not in this firmware build"; break;
            case wireless::Phase::Off: text = "Off: Print goes to the USB cable"; break;
            case wireless::Phase::NotPaired: text = "No dongle paired"; break;
            case wireless::Phase::Connecting: text = "Looking for the dongle..."; break;
            case wireless::Phase::Linked:
                text = s.dongle_usb_ready ? "Connected, the PC is ready" : "Connected, dongle not in a PC";
                break;
            case wireless::Phase::Pairing: text = "Pairing..."; break;
            case wireless::Phase::Confirming: text = "Pairing..."; break;
        }
        lv_label_set_text(status_label_, text);
        // Keep the switch row's value current if something else changed it.
        render_rows();
        return;
    }

    if (mode_ != Mode::Pairing) {
        return;
    }

    const theme::Palette& pal = theme::current();

    if (s.outcome != wireless::Outcome::None) {
        // The result: show it for a moment (Paired) or until a key (the failures), then back to the rows.
        const char* text = "";
        lv_color_t color = pal.primary_text;
        switch (s.outcome) {
            case wireless::Outcome::Paired: text = "Paired!"; color = pal.success; break;
            case wireless::Outcome::Rejected: text = "Pairing refused"; color = pal.error; break;
            case wireless::Outcome::Failed: text = "Dongle not found. Press BOOT on it first."; color = pal.error; break;
            case wireless::Outcome::None: break;
        }
        lv_label_set_text(code_label_, "");
        lv_label_set_text(info_label_, text);
        lv_obj_set_style_text_color(info_label_, color, 0);
        if (s.outcome == wireless::Outcome::Paired && ++result_ticks_ >= RESULT_SHOW_TICKS) {
            leave_pairing();
        }
        return;
    }

    switch (s.phase) {
        case wireless::Phase::Confirming: {
            idle_ticks_ = 0;
            char code[10];
            std::snprintf(code, sizeof code, "%.3s %.3s", s.code, s.code + 3);
            lv_label_set_text(code_label_, code);
            lv_label_set_text(info_label_, s.local_confirmed ? "Waiting for the dongle..." : "Same code on the dongle?");
            lv_obj_set_style_text_color(info_label_, pal.primary_text, 0);
            break;
        }
        case wireless::Phase::Pairing:
            idle_ticks_ = 0;
            lv_label_set_text(code_label_, "");
            lv_label_set_text(info_label_, "Looking for the dongle...\nPress BOOT on it now.");
            lv_obj_set_style_text_color(info_label_, pal.primary_text, 0);
            break;
        default:
            // Not pairing (yet, or any more) and no result: normally the command is still on its way to the link
            // task; if it stays like this the pairing ended without a result.
            if (++idle_ticks_ >= START_GRACE_TICKS) {
                leave_pairing();
            }
            break;
    }
}

// ---- input

bool WirelessScreen::on_input(InputAction action)
{
    if (mode_ == Mode::PinEntry) {
        if (checking_) {
            return true; // ignore everything while the PIN is being checked (see AsyncPinCheck's comment)
        }
        const bool consumed = pin_entry_.on_input(action);
        if (consumed && pin_entry_.is_complete()) {
            on_pin_complete();
            return true;
        }
        if (!consumed) {
            build_browse(); // BackShort with nothing entered: leave the PIN prompt
            return true;
        }
        return true;
    }

    if (mode_ == Mode::Pairing) {
        const wireless::Status s = wireless::status();
        if (s.outcome != wireless::Outcome::None) {
            if (action == InputAction::OkShort || action == InputAction::BackShort) {
                leave_pairing();
            }
            return true;
        }
        if (s.phase == wireless::Phase::Confirming && !s.local_confirmed && action == InputAction::OkShort) {
            wireless::confirm(true);
            return true;
        }
        if (action == InputAction::BackShort) {
            if (s.phase == wireless::Phase::Confirming) {
                wireless::confirm(false);
            } else {
                wireless::cancel_pairing();
            }
            leave_pairing();
            return true;
        }
        return true;
    }

    switch (action) {
        case InputAction::RotateLeft:
        case InputAction::RotateRight: {
            const int32_t d = action == InputAction::RotateLeft ? -1 : 1;
            int32_t i = static_cast<int32_t>(selected_row_) + d;
            if (i < 0) i = ROW_COUNT - 1;
            if (i >= static_cast<int32_t>(ROW_COUNT)) i = 0;
            selected_row_ = static_cast<size_t>(i);
            confirm_forget_ = false;
            render_rows();
            return true;
        }
        case InputAction::OkShort:
            activate();
            return true;
        case InputAction::BackShort:
            if (confirm_forget_) {
                confirm_forget_ = false;
                render_rows();
                return true;
            }
            return false; // pop
        default:
            return false;
    }
}

void WirelessScreen::activate()
{
    const wireless::Status s = wireless::status();
    switch (static_cast<Row>(selected_row_)) {
        case Row::Switch:
            if (!wireless::supported()) {
                break;
            }
            wireless::set_enabled(!s.enabled);
            break;
        case Row::Pair:
            if (!wireless::supported()) {
                break;
            }
            begin_pin_entry();
            return;
        case Row::Forget:
            if (!s.paired) {
                break;
            }
            if (!confirm_forget_) {
                confirm_forget_ = true;
            } else {
                confirm_forget_ = false;
                wireless::forget();
                ESP_LOGI(TAG, "dongle forgotten from the menu");
            }
            break;
    }
    render_rows();
    refresh_status();
}

// ---- pairing: PIN first, then the live view

void WirelessScreen::begin_pin_entry()
{
    build_pin_entry(nullptr);
}

void WirelessScreen::build_pin_entry(const char* error)
{
    mode_ = Mode::PinEntry;
    checking_ = false;
    lv_obj_clean(content_parent_);
    status_label_ = nullptr;
    code_label_ = nullptr;
    info_label_ = nullptr;

    const theme::Palette& pal = theme::current();
    lv_obj_t* header = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(header, pal.secondary_text, 0);
    lv_label_set_text(header, "PIN to pair a dongle");
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 4);

    if (error != nullptr) {
        lv_obj_t* err = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(err, pal.warning, 0);
        lv_label_set_text(err, error);
        lv_obj_align(err, LV_ALIGN_TOP_MID, 0, 24);
    }

    widgets::PinEntry::Config cfg{};
    cfg.length = security::pin::stored_pin_length();
    if (cfg.length < 4 || cfg.length > 6) {
        cfg.length = 6;
    }
    cfg.min_length = cfg.length;
    cfg.finish_on_short = true;
#if CONFIG_KEYKEEPER_LITE
    cfg.dial_mode = false;
    cfg.dial_last_reverses = true;
#else
    cfg.dial_mode = settings::all().security.pin_entry_dial_mode;
    cfg.dial_last_reverses = settings::all().security.dial_last_digit_reverses;
#endif
    pin_entry_.init(content_parent_, cfg);
}

void WirelessScreen::on_pin_complete()
{
    // The PIN is copied inside AsyncPinCheck::start(); the check takes seconds (PBKDF2) and must not run
    // inside on_input() (see async_pin_check.hpp).
    checking_ = true;
    async_check_.start(AsyncPinCheck::Kind::Verify, pin_entry_.pin(), &WirelessScreen::on_pin_checked, this);
    pin_entry_.reset();
    build_pin_entry(i18n::tr(i18n::Key::Checking));
    checking_ = true; // build_pin_entry() cleared it
}

void WirelessScreen::on_pin_checked(security::pin::VerifyResult result, void* ctx)
{
    static_cast<WirelessScreen*>(ctx)->handle_pin_result(result);
}

void WirelessScreen::handle_pin_result(security::pin::VerifyResult result)
{
    checking_ = false;
    if (result == security::pin::VerifyResult::Success) {
        if (wireless::start_pairing()) {
            build_pairing();
        } else {
            build_browse();
            if (status_label_ != nullptr) {
                lv_label_set_text(status_label_, "Could not start pairing");
            }
        }
        return;
    }
    if (result == security::pin::VerifyResult::LockedOut) {
        build_pin_entry(i18n::tr(i18n::Key::LockedOutTryLater));
    } else if (result == security::pin::VerifyResult::WipeRequired) {
        build_pin_entry(i18n::tr(i18n::Key::TooManyFailedAttempts));
    } else {
        const uint8_t remaining = security::pin::attempts_remaining();
        char buf[96];
        if (remaining > 0) {
            std::snprintf(buf, sizeof buf, i18n::tr(i18n::Key::WrongPinLeftFmt), static_cast<unsigned>(remaining));
        } else {
            std::snprintf(buf, sizeof buf, i18n::tr(i18n::Key::WrongPinUntilWipeFmt),
                          static_cast<unsigned>(security::pin::attempts_until_wipe()));
        }
        build_pin_entry(buf);
    }
}

void WirelessScreen::build_pairing()
{
    mode_ = Mode::Pairing;
    result_ticks_ = 0;
    idle_ticks_ = 0;
    lv_obj_clean(content_parent_);
    status_label_ = nullptr;

    const theme::Palette& pal = theme::current();
    code_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_font(code_label_, &keykeeper_cyrillic_24, 0);
    lv_obj_set_style_text_color(code_label_, pal.accent, 0);
    lv_label_set_text(code_label_, "");
    lv_obj_align(code_label_, LV_ALIGN_TOP_MID, 0, 8);

    info_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_font(info_label_, &keykeeper_cyrillic_16, 0);
    lv_obj_set_width(info_label_, LV_PCT(96));
    lv_label_set_long_mode(info_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(info_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info_label_, LV_ALIGN_CENTER, 0, 24);
    lv_label_set_text(info_label_, "Looking for the dongle...");
}

void WirelessScreen::leave_pairing()
{
    wireless::clear_outcome();
    build_browse();
}

} // namespace ui::screens
