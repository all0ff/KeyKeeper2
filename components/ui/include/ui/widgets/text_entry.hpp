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
// variable length instead of a fixed digit count. Characters are
// grouped into CLASSES (lowercase Latin + space, uppercase Latin,
// digits, symbols, lowercase Cyrillic, uppercase Cyrillic):
//   - Rotate: spin the character at the current position through the
//     CURRENT class only (e.g. just the 27 lowercase-Latin-plus-space
//     entries, not all ~150 across every class) -- this is both what
//     makes reaching Cyrillic practical at all, and a real speedup
//     for plain Latin text too, versus the old single ~90-entry wheel.
//   - OkShort: confirm the current character, append it, advance.
//   - OkLong: finish editing -- see is_finished().
//   - BackShort: remove the last confirmed character. If there is
//     nothing to remove (buffer empty), returns false so the owning
//     screen can decide what "leave with nothing typed" means (e.g.
//     cancel editing this field).
//   - BackLong: switch to the NEXT character class, wrapping around.
//     Previously unhandled by this widget (fell through to
//     `default: return false`) -- callers that relied on THAT as an
//     accidental "cancel editing" trigger (see
//     ui::screens::AccountEditScreen's on_input(), which cancels on
//     any unconsumed action) will now see BackLong switch class
//     instead of canceling. This is a deliberate behavior change --
//     BackLong never had a documented role here before.
//
// UTF-8: buffer_/text() hold raw UTF-8 bytes (Cyrillic is 2 bytes per
// character in UTF-8; everything else this widget offers is 1 byte).
// max_length_ is a BYTE budget, not a character count -- it's meant
// to match a vault field's own storage limit (e.g.
// vault::MAX_LOGIN_LEN), and enforcing it in bytes is what actually
// guarantees vault::validate() never rejects what this widget
// produces. This does mean an all-Cyrillic string hits its limit at
// roughly HALF as many characters as an all-Latin one would -- an
// intentional, storage-driven trade-off, not an oversight.
//
// KNOWN UX LIMITATION: even scoped to one class, stepping through
// e.g. 33 Cyrillic letters one detent at a time is still slow. There
// is no faster path (hold-to-accelerate, ...) in this version --
// flagged, not solved, given the hardware constraint (one encoder, no
// touch, no physical keyboard).
//
// Capacity is capped at MAX_BUFFER (128 BYTES) regardless of what a
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
        size_t max_length = 64; ///< BYTE budget -- see the class comment. Clamped to MAX_BUFFER - 1 regardless.
        const char* initial_value = "";
        bool mask = false; ///< Confirmed characters render as '*' (one per CHARACTER, not per byte -- e.g. password fields).
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

    /// Raw UTF-8 bytes, null-terminated.
    const char* text() const { return buffer_; }
    bool is_empty() const { return length_ == 0; }

    /// Clears the buffer (or loads initial_value if given) and resets
    /// is_finished() to false. Call this every time the widget is
    /// repurposed for a (possibly different) field. initial_value is
    /// trusted to already be well-formed UTF-8 (it only ever comes
    /// from this same widget's own past output, via vault storage) --
    /// if it needs truncating to fit max_length_/MAX_BUFFER, the cut
    /// is moved back to the nearest character boundary rather than
    /// splitting a multi-byte character in half.
    void reset(const char* initial_value = "");

    lv_obj_t* root() const { return value_label_; }

private:
    void render();
    void advance_to_next_class();

    lv_obj_t* value_label_ = nullptr;

    static constexpr size_t MAX_BUFFER = 129; // BYTES, not characters -- see the class comment
    char buffer_[MAX_BUFFER]{};
    size_t length_ = 0;    // bytes currently in buffer_
    size_t max_length_ = 64; // byte budget

    bool mask_ = false;
    bool finished_ = false;

    size_t class_index_ = 0;
    size_t index_in_class_ = 0;
};

} // namespace ui::widgets
