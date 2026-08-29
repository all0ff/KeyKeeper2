#pragma once

#include <cstdint>

// =============================================================================
// input
//
// Public input subsystem: combines the EC11 rotary encoder and the two
// physical buttons (OK, BACK) into a single stream of semantic events.
//
// input owns a FreeRTOS task that polls Encoder/Button at a fixed rate
// and a queue that other components read from with poll_event(). This
// component does not depend on event_bus (component 7 in the plan,
// not built yet); once event_bus exists, a small adapter can drain
// this queue and republish onto the bus, without input.cpp needing to
// change.
// =============================================================================

namespace input {

enum class EventType : uint8_t
{
    /// Encoder was rotated. Event::encoder_delta holds signed notches.
    EncoderRotate,

    ButtonOkDown,
    ButtonOkClick,
    ButtonOkLongPress,
    ButtonOkRepeat,
    ButtonOkUp,

    ButtonBackDown,
    ButtonBackClick,
    ButtonBackLongPress,
    ButtonBackRepeat,
    ButtonBackUp,
};

struct Event
{
    EventType type;

    /// Signed notches rotated. Only meaningful for EventType::EncoderRotate.
    int32_t encoder_delta = 0;

    /// esp_timer-based millisecond timestamp of when this event was queued.
    uint32_t timestamp_ms = 0;
};

/**
 * @brief Initialize the encoder, both buttons, and start the input task.
 *
 * Must be called after bsp::init(). Safe to call once; a second call
 * is a no-op that returns true.
 *
 * @return true on success.
 */
bool init();

bool is_initialized();

/**
 * @brief Wait up to timeout_ms for the next input event.
 *
 * Pass 0 to poll without blocking. Pass a large value (e.g.
 * portMAX_DELAY-equivalent via UINT32_MAX) to block indefinitely.
 *
 * @return true if an event was written to out, false on timeout.
 */
bool poll_event(Event& out, uint32_t timeout_ms);

/**
 * @brief Discard any events queued but not yet consumed by poll_event().
 *
 * Useful when switching UI screens, so stale input from the previous
 * screen doesn't leak into the new one.
 */
void flush_events();

/**
 * @brief Timestamp (esp_timer-based ms) of the most recent input
 *        activity (any encoder rotation or button transition).
 *
 * Unlike poll_event(), this does NOT consume anything from the event
 * queue -- it is safe to call from any number of components at once.
 * Intended for things like components/power's idle timer, which need
 * to know "was there recent activity" without competing with the
 * real event consumer (the UI) for the same queued events.
 */
uint32_t last_activity_ms();

} // namespace input
