#include "ui/async_pin_check.hpp"

#include "security/lock_manager.hpp"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace ui {

namespace {
constexpr char TAG[] = "ui.async_pin";

constexpr uint32_t TASK_STACK_SIZE = 6144; // PSA/PBKDF2 needs real stack headroom
constexpr uint8_t TASK_PRIORITY = 3;
constexpr uint32_t POLL_PERIOD_MS = 100;
} // namespace

struct AsyncPinCheck::SharedState
{
    Kind kind;
    char pin[9]{};     // new_pin for SetPin, the single PIN for Unlock/Verify
    char old_pin[9]{}; // only used for SetPin
    bool has_old_pin = false;

    // Single-writer-then-single-reader, no mutex: the worker task
    // writes result then done (in that order); the poll timer only
    // ever reads once done is observed true. abandoned is the
    // opposite direction (poll side -> worker side), same pattern.
    volatile bool done = false;
    volatile bool abandoned = false;
    security::pin::VerifyResult result = security::pin::VerifyResult::WrongPin;

    ResultCallback callback = nullptr;
    void* callback_ctx = nullptr;
};

void AsyncPinCheck::task_entry(void* arg)
{
    SharedState* state = static_cast<SharedState*>(arg);

    security::pin::VerifyResult result;
    switch (state->kind) {
        case Kind::Unlock:
            result = security::lock::unlock(state->pin);
            break;
        case Kind::Verify:
            result = security::pin::verify(state->pin);
            break;
        case Kind::SetPin:
        default: {
            const bool ok = security::pin::set_pin(state->pin, state->has_old_pin ? state->old_pin : nullptr);
            // set_pin() returns bool, not a VerifyResult -- map to the
            // same Success/WrongPin the other two kinds use so every
            // caller can share one callback signature. Don't read
            // anything more specific than success/failure into this.
            result = ok ? security::pin::VerifyResult::Success : security::pin::VerifyResult::WrongPin;
            break;
        }
    }

    // Never let either PIN outlive this task, successful or not.
    std::memset(state->pin, 0, sizeof(state->pin));
    std::memset(state->old_pin, 0, sizeof(state->old_pin));

    if (state->abandoned) {
        // The AsyncPinCheck that started this was destroyed before we
        // finished -- nobody is polling `done` anymore. We're the
        // last owner of `state`, so we free it.
        delete state;
        vTaskDelete(nullptr);
        return;
    }

    state->result = result;
    state->done = true; // must be the last field written

    vTaskDelete(nullptr);
}

void AsyncPinCheck::timer_callback(lv_timer_t* timer)
{
    AsyncPinCheck* self = static_cast<AsyncPinCheck*>(lv_timer_get_user_data(timer));
    SharedState* state = self->state_;

    if (state == nullptr || !state->done) {
        return;
    }

    lv_timer_del(self->poll_timer_);
    self->poll_timer_ = nullptr;
    self->running_ = false;
    self->state_ = nullptr;

    const security::pin::VerifyResult result = state->result;
    const ResultCallback callback = state->callback;
    void* const ctx = state->callback_ctx;
    delete state;

    if (callback != nullptr) {
        callback(result, ctx);
    }
}

void AsyncPinCheck::launch(SharedState* state, ResultCallback on_done, void* ctx)
{
    state->callback = on_done;
    state->callback_ctx = ctx;

    state_ = state;
    running_ = true;

    const BaseType_t created =
        xTaskCreate(task_entry, "pin_check", TASK_STACK_SIZE, state, TASK_PRIORITY, nullptr);

    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pin_check task");
        delete state;
        state_ = nullptr;
        running_ = false;
        if (on_done != nullptr) {
            on_done(security::pin::VerifyResult::WrongPin, ctx);
        }
        return;
    }

    poll_timer_ = lv_timer_create(timer_callback, POLL_PERIOD_MS, this);
}

void AsyncPinCheck::start(Kind kind, const char* pin, ResultCallback on_done, void* ctx)
{
    if (running_) {
        ESP_LOGW(TAG, "start() called while a check is already running -- ignoring");
        return;
    }

    auto* state = new SharedState();
    state->kind = kind;
    std::strncpy(state->pin, pin, sizeof(state->pin) - 1);

    launch(state, on_done, ctx);
}

void AsyncPinCheck::start_set_pin(const char* new_pin, const char* old_pin, ResultCallback on_done, void* ctx)
{
    if (running_) {
        ESP_LOGW(TAG, "start_set_pin() called while a check is already running -- ignoring");
        return;
    }

    auto* state = new SharedState();
    state->kind = Kind::SetPin;
    std::strncpy(state->pin, new_pin, sizeof(state->pin) - 1);
    if (old_pin != nullptr) {
        state->has_old_pin = true;
        std::strncpy(state->old_pin, old_pin, sizeof(state->old_pin) - 1);
    }

    launch(state, on_done, ctx);
}

AsyncPinCheck::~AsyncPinCheck()
{
    if (poll_timer_ != nullptr) {
        lv_timer_del(poll_timer_);
        poll_timer_ = nullptr;
    }

    if (state_ != nullptr) {
        if (state_->done) {
            // Worker already finished, nobody consumed the result --
            // safe to free directly, the worker task won't touch it
            // again.
            delete state_;
        } else {
            // Still in flight -- hand off ownership to the worker
            // task, which will free it once done (see task_entry()).
            state_->abandoned = true;
        }
        state_ = nullptr;
    }
}

} // namespace ui
