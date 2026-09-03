#pragma once

#include "ui/screen.hpp"

#include "lvgl.h"

#include <cstdint>

// =============================================================================
// ui::widgets::PinEntry
//
// Numeric PIN entry via a single rotary encoder + OK/BACK -- there is
// no touchscreen and no physical numpad on this board (bsp/pins.hpp).
// One digit at a time: rotate to spin the current digit 0-9, OkShort
// confirms it and advances to the next slot, BackShort steps back one
// slot. Once all digits are confirmed, on_input() returns true and
// pin() holds the entered value.
//
// SCOPE NOTE: this is a numeric-only widget for PIN entry
// specifically. General alphanumeric entry (docs/GUI.md's Edit
// Screen) is ui::widgets::TextEntry, a separate widget
// (text_entry.hpp) -- not built here.
// =============================================================================

namespace ui::widgets {

class PinEntry
{
public:
    struct Config
    {
        uint8_t length = 6; ///< Digits required. See settings::all().security.pin_length.

        /// Confirmed digits show as a mask dot rather than the actual
        /// digit, standard PIN-entry UX. The digit currently being
        /// spun (not yet confirmed) is always shown in the clear --
        /// the user needs to see what they're dialing in.
        bool mask_confirmed = true;
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
    bool is_complete() const { return cursor_ >= cfg_.length; }

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
    char buffer_[MAX_LENGTH + 1]{};
};

} // namespace ui::widgets
