#pragma once

#include "ui/screen.hpp"

#include "lvgl.h"

#include <cstdint>

// =============================================================================
// ui::widgets::PinEntry
//
// Numeric PIN entry via a single rotary encoder + OK/BACK -- there is
// no touchscreen and no physical numpad on this board (bsp/pins.hpp).
//
// Two interaction styles, per Config::dial_mode:
//
// STANDARD (dial_mode = false, the original/default): one digit at a
// time, rotate to spin the current digit 0-9, OkShort confirms it and
// advances to the next slot, BackShort steps back one slot.
//
// DIAL (dial_mode = true -- settings::SecuritySettings::
// pin_entry_dial_mode, see that field's own comment): mimics a real
// combination lock. Each digit slot has an EXPECTED spin direction
// that strictly alternates starting with RotateRight for digit 0
// (RotateRight, RotateLeft, RotateRight, ...) -- rotating a notch in
// the expected direction spins that slot's value by one (wrapping
// 0-9), same as Standard's spin. The moment a notch comes in going
// the OPPOSITE way, that single notch does two things at once: it
// CONFIRMS the current slot's spun value (same effect as Standard's
// OkShort) AND, since the opposite direction is exactly what the next
// slot expects, it also counts as that next slot's first spin notch
// -- so reversing direction is both "confirm" and "start dialing the
// next digit" in one continuous motion, nothing wasted, matching how
// a physical dial actually feels. cfg_.dial_last_reverses controls
// whether this reversal-confirms behavior also applies to the FINAL
// digit (true, fully consistent with every other digit) or whether
// the last digit instead requires an explicit OkShort like Standard
// mode does (false) -- see that field's own comment for why someone
// might want the more deliberate final confirmation. BackShort always
// steps back one slot in both modes, restoring that slot's
// previously-spun value so backing up and re-dialing isn't
// destructive of anything except the slot(s) actually backed past.
// =============================================================================

namespace ui::widgets {

class PinEntry
{
public:
    struct Config
    {
        uint8_t length = 6; ///< Maximum number of digits displayed.
        uint8_t min_length = 4; ///< Minimum number of digits required to finish.
        bool finish_on_short = false; ///< Finish automatically after the last digit on OkShort.

        ///< Digits required. See settings::all().security.pin_length.

        /// Confirmed digits show as a mask dot rather than the actual
        /// digit, standard PIN-entry UX. The digit currently being
        /// spun (not yet confirmed) is always shown in the clear --
        /// the user needs to see what they're dialing in.
        bool mask_confirmed = true;

        /// See this file's own header comment -- combination-lock
        /// style entry instead of Standard's rotate+OkShort. Mirrors
        /// settings::SecuritySettings::pin_entry_dial_mode; the
        /// caller reads that setting and passes it through here
        /// rather than this widget reading settings:: itself.
        bool dial_mode = false;

        /// Only consulted when dial_mode is true -- see this file's
        /// own header comment. Mirrors
        /// settings::SecuritySettings::dial_last_digit_reverses.
        bool dial_last_reverses = true;
    };

    void init(lv_obj_t* parent, const Config& cfg);

    /**
     * @brief Feed one input action to the widget.
     *
     * @return true if this action was consumed by the widget (rotate,
     *         confirm a digit, step back a digit) -- the owning
     *         screen shouldn't apply its own handling for it. false
     *         only for BackShort with is_empty() true (nothing left
     *         to step back), letting the screen decide (typically:
     *         leave the screen via the default pop navigation).
     *
     * This does NOT by itself mean the PIN is complete -- check
     * is_complete() after calling this to know when to read pin().
     */
    bool on_input(InputAction action);

    /// True once all `length` digits have been entered and confirmed.
    bool is_complete() const { return finished_; }

    /// Null-terminated entered PIN. Valid after is_complete() is true;
    /// call reset() once you're done reading it so the digits don't
    /// sit in RAM longer than needed.
    const char* pin() const { return buffer_; }

    /// True if no digit has been confirmed yet (cursor at the first
    /// slot) -- lets the owning screen decide whether a BackShort
    /// should step back one digit (handled here) or leave the screen
    /// entirely (owning screen's job, via UiManager::pop()).
    bool is_empty() const { return cursor_ == 0; }

    /// Access the underlying LVGL container (for lv_obj_align, etc.)
    lv_obj_t* root() const { return container_; }

    /// Clears all entered digits and the on-screen display, back to
    /// the first slot.
    void reset();

    static constexpr uint8_t MAX_LENGTH = 8;  ///< Public: external screens need buffer sizing

private:
    void render();

    lv_obj_t* container_ = nullptr;
    lv_obj_t* digit_labels_[MAX_LENGTH]{};

    Config cfg_{};
    uint8_t cursor_ = 0;
    uint8_t spin_value_ = 0;
    bool finished_ = false;
    char buffer_[MAX_LENGTH + 1]{};
};

} // namespace ui::widgets
