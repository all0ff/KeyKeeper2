#pragma once

#include "event_bus/event.hpp"

// =============================================================================
// event_bus
//
// Central pub/sub event system. A dedicated dispatcher task owns a
// FreeRTOS queue; publish() enqueues, and the dispatcher task calls
// every matching subscriber's handler synchronously as it drains the
// queue.
//
// Handlers run on the dispatcher task, not the publisher's task and
// not an ISR -- keep them fast, and don't call publish() from inside
// a handler for the same category you're handling (that's fine for
// different categories, but re-entrant same-category publish from a
// handler risks feedback loops the bus does nothing to prevent).
// =============================================================================

namespace event_bus {

using Handler = void (*)(const Event& event, void* ctx);

/**
 * @brief Initialize the bus and start its dispatcher task.
 *
 * Safe to call once; a second call is a no-op that returns true.
 */
bool init();

bool is_initialized();

/**
 * @brief Publish a fully-populated event onto the bus.
 *
 * Non-blocking: if the internal queue is full (a subscriber has
 * fallen behind), the oldest queued event is dropped to make room,
 * the same policy components/input already uses for its own queue.
 * timestamp_ms is overwritten with the current time regardless of
 * what the caller set.
 *
 * @return true if the event was enqueued.
 */
bool publish(Event event);

/**
 * @brief Convenience overload: build an Event from its parts and
 *        publish it.
 */
bool publish(Category category, uint32_t id, Payload payload = {});

/**
 * @brief Subscribe to every event published under one category.
 *
 * @return A handle to pass to unsubscribe(), or -1 if the subscriber
 *         table is full.
 */
int subscribe(Category category, Handler handler, void* ctx);

void unsubscribe(int handle);

} // namespace event_bus
