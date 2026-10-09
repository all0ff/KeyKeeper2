#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace usb {

// =============================================================================
// Typing plan -- WHAT to type, separated from HOW it gets out.
//
// plan_events() turns a UTF-8 string into a flat list of HidEvent steps:
// key presses, pacing pauses and the optional Cyrillic layout-switch hotkey.
// It is a pure function: no USB, no FreeRTOS, no settings, no logging, so it
// runs (and is tested) on a PC. Executing a plan is somebody else's job:
// run_plan() (typing_runner.hpp) plays it through a KeyIo, and an OutputSink
// (output_sink.hpp) decides where it goes -- the USB cable today, a radio
// dongle later.
//
// The plan describes the SUCCESSFUL path only. What happens when a step
// fails (stop, but still send the layout-switch-back of a run that was
// already opened) is the executor's rule, spelled out in typing_runner.hpp.
// =============================================================================

/// Key timing in milliseconds. Same fields and defaults as the old
/// TypeEngine::Timing (which is now an alias of this).
struct TypingTiming {
    uint32_t press_ms = 10; ///< how long a key is held down
    uint32_t inter_ms = 10; ///< delay after each key
    uint32_t chunk_ms = 0;  ///< extra pause after every 32 characters
};

/// One step of a typing plan.
struct HidEvent {
    enum class Kind : uint8_t {
        /// Wait pre_ms (0 for plain keys), press modifier+keycode for hold_ms,
        /// wait gap_ms. Credits `chars` characters when it completes.
        Key,
        /// Wait gap_ms and nothing else (pacing between 32-character chunks).
        Pause,
        /// Layout-switch hotkey that OPENS a Cyrillic run. Wait pre_ms, press
        /// the modifiers for hold_ms, wait gap_ms (settle).
        LayoutOn,
        /// The same hotkey, CLOSING the run. Still sent after a failure inside
        /// the run (but not if LayoutOn itself failed).
        LayoutOff,
        /// A character with no key on a US keyboard: counted as sent, nothing
        /// typed, last_error becomes "Unsupported character".
        SkipLatin,
        /// A Cyrillic code point without a mapping: counted, nothing typed.
        /// Unreachable today (is_cyrillic() only accepts mapped letters); kept
        /// because the old TypeEngine had the same safety branch.
        SkipCyrillic,
    };

    Kind kind = Kind::Key;
    uint8_t modifier = 0; ///< HID modifier bits (usb::modifier::*)
    uint8_t keycode = 0;  ///< HID usage; 0 for a modifier-only hotkey
    uint8_t chars = 0;    ///< characters credited once this event has completed
    uint32_t pre_ms = 0;
    uint32_t hold_ms = 0;
    uint32_t gap_ms = 0;
};

struct PlanOptions {
    TypingTiming timing;
    /// Wrap each run of Cyrillic characters in the Alt+Shift layout-switch
    /// hotkey (settings::UsbSettings::cyrillic_auto_switch_layout).
    bool auto_switch_layout = false;
    /// Without the automatic switch the person puts the host on the Russian layout
    /// by hand, so for a text that CONTAINS Cyrillic every ASCII character is read
    /// by that layout too: type punctuation with the keys the Russian layout puts it
    /// on ("." on the US "/" key, ...). See cyrillic::russian_layout_ascii(). Has no
    /// effect with auto_switch_layout, or for text without Cyrillic. Turn it off to
    /// get the earlier behaviour (every character on its US key).
    bool russian_layout_punctuation = true;
};

struct TypingPlan {
    std::vector<HidEvent> events;
    /// Characters of a plan typed for a host on the Russian layout that NO key of
    /// that layout can produce (Latin letters, @ # $ ^ & [ ] { } < > | ~ ' `). They
    /// are typed as before and come out as other characters; the only way to type
    /// them is the automatic layout switch. Callers use this to warn.
    size_t unavailable_on_russian_layout = 0;
};

/// Layout-switch hotkey timing -- see the comments in typing_plan.cpp for the
/// real failure each value was introduced for.
constexpr uint32_t LAYOUT_SWITCH_HOLD_MS = 100;
constexpr uint32_t LAYOUT_SWITCH_PRE_DELAY_MS = 120;
constexpr uint32_t LAYOUT_SWITCH_SETTLE_MS = 250;

/// Pure: builds the plan for |text|. Never fails; characters it cannot type
/// become Skip events so that the executor's character count matches what
/// TypeEngine::type_string() always returned.
TypingPlan plan_events(const std::string& text, const PlanOptions& options);

} // namespace usb
