#pragma once

#include "power/power.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// =============================================================================
// power::internal::Manager
//
// PRIVATE. Owns the background task that watches for input activity,
// tracks the idle timer, runs the callback registry, and performs the
// actual esp_sleep configuration and transitions. power.cpp is a thin
// wrapper that forwards to internal::instance().
// =============================================================================

namespace power::internal {

class Manager
{
public:
    bool init(const Config& cfg);
    bool is_initialized() const { return task_handle_ != nullptr; }

    State state() const { return state_; }

    void notify_activity();

    int register_callback(Callback cb, void* ctx);
    void unregister_callback(int handle);

    void request_sleep();
    [[noreturn]] void request_shutdown();

private:
    static constexpr size_t MAX_CALLBACKS = 8;
    static constexpr uint32_t TASK_POLL_MS = 200;
    static constexpr uint32_t TASK_STACK_SIZE = 3072;
    static constexpr uint8_t TASK_PRIORITY = 3;

    struct CallbackSlot
    {
        Callback cb = nullptr;
        void* ctx = nullptr;
        bool used = false;
    };

    static void task_trampoline(void* arg);
    void task();

    void fire_callbacks(State new_state);

    /**
     * @brief Fire callbacks, enter light sleep, block until woken, fire
     *        callbacks for the return to Active.
     *
     * Serialized by transition_mutex_ so a request_sleep() call from
     * one task can never overlap with the background task's own
     * idle-timeout-triggered sleep.
     */
    void transition_to_light_sleep();

    [[noreturn]] void transition_to_deep_sleep();

    void configure_wake_sources_light_sleep();
    void configure_wake_sources_deep_sleep();

    Config cfg_{};
    State state_ = State::Active;

    uint32_t last_activity_ms_ = 0;

    CallbackSlot callbacks_[MAX_CALLBACKS]{};

    TaskHandle_t task_handle_ = nullptr;
    SemaphoreHandle_t transition_mutex_ = nullptr;
};

/**
 * @brief Return the single Manager instance owned by the power component.
 */
Manager& instance();

} // namespace power::internal
