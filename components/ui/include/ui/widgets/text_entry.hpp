#pragma once

#include "ui/screen.hpp"

#include "lvgl.h"

#include <cstddef>

// =============================================================================
// ui::widgets::TextEntry
//
// General alphanumeric text entry via a single rotary encoder + OK/
// BACK -- the widget flagged as "not built yet" since PinEntry
// (keyboard.hpp) was written for PIN-only numeric input.
//
// Same core idea as PinEntry, extended to a full character set and
// variable length instead of a fixed digit count:
//   - Rotate: spin the character at the current position through a
//     ~90-character alphabet (space, a-z, A-Z, 0-9, common symbols).
//   - OkShort: confirm the current character, append it, advance.
//   - OkLong: finish editing -- see is_finished().
//   - BackShort: remove the last confirmed character. If there is
//     nothing to remove (buffer empty), returns false so the owning
//     screen can decide what "leave with nothing typed" means (e.g.
//     cancel editing this field).
//
// KNOWN UX LIMITATION: stepping through ~90 characters one detent at
// a time is slow. There is no faster path (grouped rotation, hold-to-
// accelerate, ...) in this version -- flagged, not solved, given the
// hardware constraint (one encoder, no touch, no physical keyboard).
//
// Capacity is capped at MAX_BUFFER (128 chars) regardless of what a
// field's real storage limit is (e.g. vault::MAX_NOTES_LEN is 512) --
// entering hundreds of characters one at a time via a single knob
// isn't practical UX regardless of what the data model could store.
// The owning screen should clamp its configured max_length to
// whichever is smaller.
// =============================================================================

namespace ui::widgets {

class TextEntry
{
public:
    struct Config
    {
        size_t max_length = 64; ///< Clamped to MAX_BUFFER - 1 regardless.
        const char* initial_value = "";
        bool mask = false; ///< Confirmed characters render as '*' (e.g. password fields).
    };

    void init(lv_obj_t* parent, const Config& cfg);

    /// Change masking/length limit for reuse across different fields
    /// without recreating the underlying LVGL object -- call before
    /// reset() when repurposing one TextEntry instance for a new
    /// field (see ui::screens::AccountEditScreen).
    void set_mask(bool mask) { mask_ = mask; }
    void set_max_length(size_t max_length);

    /// See the class comment for what each action does.
    bool on_input(InputAction action);

    /// True after OkLong -- the user is done with this field.
    bool is_finished() const { return finished_; }

    const char* text() const { return buffer_; }
    bool is_empty() const { return length_ == 0; }

    /// Clears the buffer (or loads initial_value if given) and resets
    /// is_finished() to false. Call this every time the widget is
    /// repurposed for a (possibly different) field.
    void reset(const char* initial_value = "");

    lv_obj_t* root() const { return value_label_; }

private:
    void render();

    lv_obj_t* value_label_ = nullptr;

    static constexpr size_t MAX_BUFFER = 129;
    char buffer_[MAX_BUFFER]{};
    size_t length_ = 0;
    size_t max_length_ = 64;

    bool mask_ = false;
    bool finished_ = false;

    size_t alphabet_index_ = 0;
};

} // namespace ui::widgets
