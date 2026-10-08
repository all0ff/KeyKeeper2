#pragma once

#include "usb/typing_plan.hpp"

#include <cstddef>
#include <cstdint>

namespace usb {

// =============================================================================
// Local executor for a TypingPlan.
//
// KeyIo is the smallest surface the executor needs: "is the output usable",
// "press a key", "wait", "why did it fail". The USB cable implements it with
// hid:: and vTaskDelay (cable_sink.cpp); tests implement it with a recorder.
// This file has no ESP-IDF dependency.
// =============================================================================

class KeyIo {
public:
    virtual ~KeyIo() = default;
    virtual bool ready() = 0;
    virtual bool send_key(uint8_t keycode, uint8_t modifier, uint32_t hold_ms) = 0;
    virtual void delay_ms(uint32_t ms) = 0;
    virtual const char* last_error() const = 0;
};

struct RunResult {
    /// false if the run was aborted by a failure. Unsupported characters are
    /// NOT failures (they are skipped and counted, as TypeEngine always did).
    bool ok = true;
    /// Characters credited so far -- what TypeEngine::type_string() returns.
    size_t chars_sent = 0;
    /// The last error text set during the run, or nullptr if none was set.
    const char* error = nullptr;
};

/// Plays |plan| through |io|, step by step, with the same rules the old
/// monolithic TypeEngine had:
///  - before every Key and every Skip step: if !io.ready(), abort with
///    "USB not connected" (nothing is checked before the layout hotkeys);
///  - the first failing step aborts the run; its error is io.last_error();
///  - LayoutOff is the exception: if a Cyrillic run was already opened (its
///    LayoutOn succeeded), LayoutOff is still sent after an abort, its result
///    ignored (but its error text, if any, is still recorded as last error);
///    if LayoutOn itself failed, nothing more is sent;
///  - a SkipLatin step sets the error text "Unsupported character" without
///    failing; the text stays as the last error unless a later step replaces it.
RunResult run_plan(const TypingPlan& plan, KeyIo& io);

} // namespace usb
