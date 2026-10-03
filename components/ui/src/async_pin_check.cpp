#include "ui/async_pin_check.hpp"

#include "power/power.hpp"
#include "security/lock_manager.hpp"
#include "vault/vault_repository.hpp"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <cstring>
#include <memory>

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

    // Single-writer-then-single-reader, cross-task (worker task on one
    // core, LVGL poll timer running on whichever core ui_manager's
    // main loop is pinned to -- ESP32-S3 is dual-core, so these can
    // genuinely be two different cores): the worker writes result
    // then done (in that order); the poll timer only ever reads once
    // done is observed true. abandoned is the opposite direction
    // (poll side -> worker side), same pattern.
    //
    // std::atomic<bool>, not a plain volatile bool (the earlier code
    // here) -- volatile guarantees neither atomicity nor a memory
    // barrier in C++; it only blocks the COMPILER from reordering or
    // caching the access in registers, which says nothing about what
    // the OTHER CORE's cache sees or when. A write becoming visible
    // "quickly enough in practice" on this chip's particular cache
    // coherency behavior is not the same thing as the C++ standard
    // actually guaranteeing it.
    //
    // Left at std::atomic<bool>'s own default operations (operator=
    // and the implicit bool conversion used everywhere below,
    // unchanged from the plain-bool version) rather than hand-picking
    // memory_order_release/acquire at each call site -- those default
    // to memory_order_seq_cst, the strongest ordering, which is
    // exactly as correct as a hand-tuned acquire/release pairing here
    // and categorically simpler to get right. A flag polled once
    // every 100ms has no meaningful performance reason to trade that
    // safety margin away.
    std::atomic<bool> done{false};
    std::atomic<bool> abandoned{false};
    security::pin::VerifyResult result = security::pin::VerifyResult::WrongPin;

    ResultCallback callback = nullptr;
    void* callback_ctx = nullptr;
};

void AsyncPinCheck::task_entry(void* arg)
{
    // Reclaims ownership from the raw pointer launch() had to hand
    // xTaskCreate() (a C API -- plain void*). This function has TWO
    // possible owners of `state` by the time it's done (see the
    // abandoned-check below) -- in BOTH branches, ownership is
    // settled with an explicit reset()/release() before
    // vTaskDelete(nullptr) at the bottom, never left to this
    // unique_ptr's own destructor at scope exit. vTaskDelete(nullptr)
    // deletes THIS task from within itself and never actually returns
    // to this stack frame the ordinary way a C++ function does, so a
    // destructor that would only run at the closing brace never gets
    // the chance to -- true of EITHER exit path here, not just one.
    std::unique_ptr<SharedState> owned_state(static_cast<SharedState*>(arg));
    SharedState* state = owned_state.get();

    security::pin::VerifyResult result = security::pin::VerifyResult::WrongPin;

    switch (state->kind) {
        case Kind::Unlock:
            result = security::lock::unlock(state->pin);
            break;

        case Kind::Verify:
            result = security::pin::verify(state->pin);
            break;

        case Kind::SetPin: {
            const bool ok = security::pin::set_pin(state->pin, state->has_old_pin ? state->old_pin : nullptr);
            // set_pin() returns bool, not a VerifyResult -- map to
            // Success/WrongPin so every Kind can share one callback
            // signature. Don't read anything more specific than
            // success/failure into this.
            result = ok ? security::pin::VerifyResult::Success : security::pin::VerifyResult::WrongPin;
            if (ok && vault::repository::is_loaded()) {
                // A successful set_pin() has ALREADY re-keyed
                // security::vault_key internally (pin_manager.cpp's
                // own store_new_pin(), see its comment) -- but that
                // alone only changes what key the NEXT save uses.
                // vault.db on disk right now is still encrypted under
                // the OLD key, and the in-memory entries this
                // Repository is holding were decrypted under it too.
                // Re-persisting right here, immediately, re-encrypts
                // those SAME already-decrypted entries under the NEW
                // key and overwrites the file -- without this, the
                // vault would become unreadable the next time it's
                // loaded (wrong key for what's actually on disk),
                // which would be silent data loss, not just an
                // inconvenience. Skipped only when nothing is loaded
                // yet (this device's very first PIN setup, before any
                // vault.db exists at all) -- nothing to re-encrypt.
                if (!vault::repository::persist_now()) {
                    ESP_LOGE(TAG, "Failed to re-encrypt vault.db under the new PIN -- old vault.db may now be "
                                  "unreadable; retry changing the PIN, or restore a backup");
                }
            }
            break;
        }

        case Kind::SetPinAfterVerify: {
            const bool ok = security::pin::set_pin_after_verify(state->pin);
            result = ok ? security::pin::VerifyResult::Success : security::pin::VerifyResult::WrongPin;
            if (ok && vault::repository::is_loaded()) {
                // Same reasoning as Kind::SetPin above.
                if (!vault::repository::persist_now()) {
                    ESP_LOGE(TAG, "Failed to re-encrypt vault.db under the new PIN -- old vault.db may now be "
                                  "unreadable; retry changing the PIN, or restore a backup");
                }
            }
            break;
        }

        case Kind::SetDuressPin: {
            const bool ok = security::pin::set_duress_pin(state->pin, state->old_pin);
            result = ok ? security::pin::VerifyResult::Success : security::pin::VerifyResult::WrongPin;
            break;
        }

        case Kind::SetDuressPinAfterVerify: {
            const bool ok = security::pin::set_duress_pin_after_verify(state->pin, state->old_pin);
            result = ok ? security::pin::VerifyResult::Success : security::pin::VerifyResult::WrongPin;
            break;
        }
    }

    // Never let either PIN outlive this task, successful or not.
    std::memset(state->pin, 0, sizeof(state->pin));
    std::memset(state->old_pin, 0, sizeof(state->old_pin));

    if (state->abandoned) {
        // The AsyncPinCheck that started this was destroyed before we
        // finished (or launch() itself marked this abandoned right
        // after starting us -- see that function's own comment on a
        // failed lv_timer_create()) -- nobody is polling `done`
        // anymore either way. We're the last owner of `state`, so we
        // free it -- explicitly, right here, not left to
        // owned_state's own destructor (see this function's opening
        // comment on why that wouldn't actually run before
        // vTaskDelete(nullptr) below).
        owned_state.reset();
        vTaskDelete(nullptr);
        return;
    }

    state->result = result;
    state->done = true; // must be the last field written

    // Ownership passes to whichever of timer_callback() or
    // ~AsyncPinCheck() observes `done` first (see each of their own
    // comments) -- release(), not reset(): the memory must survive
    // this task ending, just no longer be this unique_ptr's to free.
    owned_state.release();
    vTaskDelete(nullptr);
}

void AsyncPinCheck::timer_callback(lv_timer_t* timer)
{
    AsyncPinCheck* self = static_cast<AsyncPinCheck*>(lv_timer_get_user_data(timer));
    SharedState* state = self->state_;

    // Keep the idle/screen-timeout timer from thinking the device has
    // gone idle just because no BUTTON was pressed during the ~10-20s
    // PBKDF2 wait. Ping every poll (100ms) the check is still
    // running, not just once, so even a short display_off_timeout_s
    // can't fire mid-check.
    power::notify_activity();

    if (state == nullptr || !state->done) {
        return;
    }

    lv_timer_del(self->poll_timer_);
    self->poll_timer_ = nullptr;
    self->running_ = false;
    self->state_ = nullptr;

    // Reclaims ownership task_entry() released on the "done, not
    // abandoned" path (see that function's own comment). Safe to let
    // this unique_ptr free the memory via its own destructor at the
    // end of this function -- unlike task_entry(), this is an
    // ordinary LVGL timer callback that returns normally; no
    // vTaskDelete()-style non-return to work around here.
    std::unique_ptr<SharedState> owned_state(state);

    const security::pin::VerifyResult result = state->result;
    const ResultCallback callback = state->callback;
    void* const ctx = state->callback_ctx;

    if (callback != nullptr) {
        callback(result, ctx);
    }
}

void AsyncPinCheck::launch(std::unique_ptr<SharedState> state, ResultCallback on_done, void* ctx)
{
    state->callback = on_done;
    state->callback_ctx = ctx;

    // state_ (an AsyncPinCheck member) stays a plain observing
    // pointer, not itself a unique_ptr -- its own lifecycle is
    // inherently conditional (the destructor either deletes it
    // directly or hands ownership to the worker task depending on
    // `done`, see ~AsyncPinCheck() below), which a member unique_ptr
    // couldn't express any more simply than the raw pointer already
    // does. The unique_ptr here is about THIS function's own local
    // ownership of the allocation on the way to becoming task_entry()'s.
    state_ = state.get();
    running_ = true;

    // xTaskCreate() takes a plain void* (a C API, no smart-pointer
    // overload) -- .get(), not .release(), for this call specifically:
    // state still owns the memory at this point, so if xTaskCreate()
    // fails, letting state run out of scope below frees it
    // automatically (what used to be an explicit `delete state;` on
    // this exact failure path).
    const BaseType_t created =
        xTaskCreate(task_entry, "pin_check", TASK_STACK_SIZE, state.get(), TASK_PRIORITY, nullptr);

    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pin_check task");
        state_ = nullptr;
        running_ = false;
        if (on_done != nullptr) {
            on_done(security::pin::VerifyResult::WrongPin, ctx);
        }
        return;
    }

    // The new task reclaims ownership itself, at the top of
    // task_entry() below -- release(), not reset(), since the memory
    // must survive this call, just no longer be this function's to
    // free.
    state.release();

    poll_timer_ = lv_timer_create(timer_callback, POLL_PERIOD_MS, this);
    if (poll_timer_ == nullptr) {
        // Real, independent defect (found in review, not the
        // original Q-10 new/delete audit item): the worker task is
        // ALREADY running at this point -- xTaskCreate() above
        // succeeded, and there is no way to cancel an in-flight
        // FreeRTOS task cleanly, especially one that may already be
        // mid-PBKDF2. Without this block, the task would finish
        // normally and set state->done = true, but nothing would
        // ever be polling for that (no timer exists), so SharedState
        // would leak permanently, AND running_ -- only ever cleared
        // in timer_callback(), which would also never fire -- would
        // stay true forever, silently blocking this AsyncPinCheck
        // instance from starting any FUTURE check until the whole
        // object is destroyed and recreated. lv_timer_create()
        // failing is rare in practice (LVGL-internal memory
        // exhaustion), but when it does, the fix is to mark this
        // abandoned right away -- the exact same protocol
        // ~AsyncPinCheck() already uses when the UI object itself is
        // destroyed mid-check (see that destructor and task_entry()'s
        // own abandoned branch): the worker, once it finishes, will
        // see abandoned == true and free SharedState itself, with no
        // poll timer ever needed.
        ESP_LOGE(TAG, "Failed to create poll timer -- abandoning this check, worker will self-clean on finish");
        state_->abandoned = true;
        state_ = nullptr;
        running_ = false;
        if (on_done != nullptr) {
            on_done(security::pin::VerifyResult::WrongPin, ctx);
        }
    }
}

void AsyncPinCheck::start(Kind kind, const char* pin, ResultCallback on_done, void* ctx)
{
    if (running_) {
        ESP_LOGW(TAG, "start() called while a check is already running -- ignoring");
        return;
    }

    auto state = std::make_unique<SharedState>();
    state->kind = kind;
    std::strncpy(state->pin, pin, sizeof(state->pin) - 1);

    launch(std::move(state), on_done, ctx);
}

void AsyncPinCheck::start_set_pin(const char* new_pin, const char* old_pin, ResultCallback on_done, void* ctx)
{
    if (running_) {
        ESP_LOGW(TAG, "start_set_pin() called while a check is already running -- ignoring");
        return;
    }

    auto state = std::make_unique<SharedState>();
    state->kind = Kind::SetPin;
    std::strncpy(state->pin, new_pin, sizeof(state->pin) - 1);
    if (old_pin != nullptr) {
        state->has_old_pin = true;
        std::strncpy(state->old_pin, old_pin, sizeof(state->old_pin) - 1);
    }

    launch(std::move(state), on_done, ctx);
}

void AsyncPinCheck::start_set_pin_after_verify(const char* new_pin, ResultCallback on_done, void* ctx)
{
    if (running_) {
        ESP_LOGW(TAG, "start_set_pin_after_verify() called while a check is already running -- ignoring");
        return;
    }

    auto state = std::make_unique<SharedState>();
    state->kind = Kind::SetPinAfterVerify;
    std::strncpy(state->pin, new_pin, sizeof(state->pin) - 1);
    // No old_pin needed at all -- security::pin::set_pin_after_verify()
    // doesn't take one.

    launch(std::move(state), on_done, ctx);
}

void AsyncPinCheck::start_set_duress_pin(const char* duress_pin, const char* current_pin, ResultCallback on_done,
                                          void* ctx)
{
    if (running_) {
        ESP_LOGW(TAG, "start_set_duress_pin() called while a check is already running -- ignoring");
        return;
    }

    auto state = std::make_unique<SharedState>();
    state->kind = Kind::SetDuressPin;
    std::strncpy(state->pin, duress_pin, sizeof(state->pin) - 1);
    state->has_old_pin = true;
    std::strncpy(state->old_pin, current_pin, sizeof(state->old_pin) - 1);

    launch(std::move(state), on_done, ctx);
}

void AsyncPinCheck::start_set_duress_pin_after_verify(const char* duress_pin, const char* current_pin,
                                                       ResultCallback on_done, void* ctx)
{
    if (running_) {
        ESP_LOGW(TAG, "start_set_duress_pin_after_verify() called while a check is already running -- ignoring");
        return;
    }

    auto state = std::make_unique<SharedState>();
    state->kind = Kind::SetDuressPinAfterVerify;
    std::strncpy(state->pin, duress_pin, sizeof(state->pin) - 1);
    // current_pin's VALUE is still needed (length-match/distinctness
    // checks inside store_duress_pin()), just not re-verified -- same
    // old_pin field, different Kind changes what pin_manager does
    // with it.
    state->has_old_pin = true;
    std::strncpy(state->old_pin, current_pin, sizeof(state->old_pin) - 1);

    launch(std::move(state), on_done, ctx);
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
            // again. A destructor returning normally (this one) has
            // no vTaskDelete()-style non-return to work around, so a
            // brief owning unique_ptr, freeing state_ via its own
            // destructor at the end of this scope, is exactly as
            // correct as the explicit `delete state_;` this replaced
            // -- just consistent with every other ownership-settling
            // point in this file.
            std::unique_ptr<SharedState> owned(state_);
        } else {
            // Still in flight -- hand off ownership to the worker
            // task, which will free it once done (see task_entry()).
            state_->abandoned = true;
        }
        state_ = nullptr;
    }
}

} // namespace ui
