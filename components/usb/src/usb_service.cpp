#include "usb/usb_service.hpp"
#include "usb/hid_keyboard.hpp"

#include "security/permission_manager.hpp"
#include "rtc_time/rtc_time.hpp"
#include "settings/settings.hpp"
#include "totp/totp.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <string>

namespace usb {

namespace {

constexpr char TAG[] = "usb.svc";

constexpr uint32_t TYPE_TASK_STACK = 4096;
constexpr uint8_t  TYPE_TASK_PRIO  = 3;

TypeEngine engine;
// std::atomic<const char*>, not a plain const char* (audit finding
// Q-08): set_status() is called from several places that DON'T go
// through the typing_in_progress-serialized type_task() at all --
// print_field()'s own early returns (permission denied, not
// connected, empty field) run directly on whatever task called
// print_field() in the first place, which can race a type_task()
// that's mid-flight finishing up and setting its own final status at
// the same moment. The underlying TypeEngine `engine` above doesn't
// need the same treatment: typing_in_progress already guarantees at
// most one type_task() -- the only thing that ever calls engine's own
// methods -- runs at a time, so engine's internal state was already
// effectively serialized by that earlier fix. status_msg specifically
// wasn't covered by it.
std::atomic<const char*> status_msg{""};
// Guards against a real, confirmed-on-hardware bug: pressing "Print
// Password" repeatedly, about once a second, used to spawn a NEW
// type_task() each time -- but one full run (the typed text, plus any
// trailing Tab/Enter the configured typing order adds) can take long
// enough that a fast-enough repeat press started a SECOND task while
// the FIRST one was still mid-flight, both calling into the same
// shared `engine` above and the same underlying USB HID endpoint from
// two different FreeRTOS tasks concurrently -- neither is written to
// be safe against that. std::atomic, not a plain bool, because it's
// genuinely set from one task (the UI task calling spawn_type_task())
// and cleared from another (the typing task itself).
std::atomic<bool> typing_in_progress{false};

void set_status(const char* msg)
{
    status_msg = msg;
    ESP_LOGI(TAG, "%s", msg);
}

// std::string::size() is BYTES; TypeEngine::type_string() reports how
// many CHARACTERS it typed. Identical for ASCII, different for
// Cyrillic (2 bytes each in UTF-8) -- comparing them directly made
// "all typed" false for any non-ASCII login/password/secret word,
// silently reporting "Partially typed" for a fully-typed field (and,
// before this component grew a configurable typing order, would have
// skipped a trailing Enter/Tab the same way).
size_t count_chars(const std::string& text)
{
    size_t n = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) {
            ++n;
        }
    }
    return n;
}

struct TypeTaskParams {
    std::string text;
    TypeEngine::Timing timing;
};

void type_task(void* arg)
{
    auto* params = static_cast<TypeTaskParams*>(arg);
    const size_t sent = engine.type_string(params->text, params->timing);
    // Accept either unit -- see count_chars()'s own comment.
    const bool text_complete = (sent == count_chars(params->text)) || (sent == params->text.size());

    if (text_complete) {
        set_status("Typed OK");
    } else if (sent > 0) {
        set_status("Partially typed");
    } else {
        set_status(engine.last_error());
    }

    delete params;
    // Cleared LAST, right before the task actually ends -- a new
    // request arriving between set_status() above and this line
    // should still see "busy" and be refused, not slip through into
    // the same kind of race this flag exists to prevent.
    typing_in_progress.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

void spawn_type_task(const std::string& text, const TypeEngine::Timing& timing = TypeEngine::Timing{})
{
    // Atomic check-and-set (not a separate if-check then store --
    // that would itself be a smaller version of the exact race this
    // flag exists to close) -- refuses to start a second task while
    // one is already running rather than letting them race. expected
    // starts false; compare_exchange_strong only succeeds (and only
    // then flips it to true) if that's still the case.
    bool expected = false;
    if (!typing_in_progress.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        set_status("Already typing -- try again in a moment");
        return;
    }

    auto* params = new TypeTaskParams{text, timing};
    const BaseType_t created = xTaskCreate(
        type_task, "usb_type", TYPE_TASK_STACK, params, TYPE_TASK_PRIO, nullptr);

    if (created != pdPASS) {
        set_status("Failed to start typing task");
        delete params;
        // Task never actually started -- nothing will ever clear the
        // flag on this attempt's behalf, so this call site has to.
        typing_in_progress.store(false, std::memory_order_release);
    }
}

} // namespace

bool init()
{
    if (!hid::init()) {
        return false;
    }
    ESP_LOGI(TAG, "USB service initialised");
    return true;
}

bool is_connected()
{
    return hid::is_connected();
}

const char* last_status()
{
    return status_msg;
}

void type_string(const std::string& text)
{
    if (!is_connected()) {
        set_status("USB not connected");
        return;
    }
    spawn_type_task(text);
}

void print_field(const vault::VaultEntry& entry, Field field)
{
    security::permission::Operation operation =
        security::permission::Operation::PrintPassword;

    if (field == Field::Otp) {
        operation = security::permission::Operation::PrintOtp;
    }

    const security::permission::Result permission =
        security::permission::check(operation);

    if (permission != security::permission::Result::Allowed) {
        set_status("USB print not allowed");
        return;
    }

    if (!is_connected()) {
        set_status("USB not connected");
        return;
    }

    std::string text;
    switch (field) {
        case Field::Url:
            text = entry.url;
            break;
        case Field::Login:
            text = entry.login;
            break;
        case Field::Password:
            // "Print Password" is the one action people actually use
            // day to day, so it's this case -- not the unused
            // Field::LoginAndPassword below -- that follows the
            // configured typing order for real. Settings -> USB ->
            // Print Sequence names each option after exactly what it
            // types; this is that description made to actually do
            // what it says, not the row that ships the setting but
            // leaves it disconnected from typing.
            switch (settings::all().usb.typing_order) {
                case settings::TypingOrder::LoginTabPasswordEnter:
                    text = entry.login + "\t" + entry.password + "\n";
                    break;
                case settings::TypingOrder::PasswordOnly:
                    text = entry.password;
                    break;
                case settings::TypingOrder::PasswordEnter:
                    text = entry.password + "\n";
                    break;
            }
            break;
        case Field::Otp: {
            char code[8];
            if (totp::generate(entry.totp_secret, code, sizeof(code))) {
                text = code;
            } else {
                // See totp::generate()'s own comment for why this
                // fails cleanly rather than falling back to a
                // placeholder -- no Wi-Fi Station time sync yet this
                // boot, or the secret itself isn't valid Base32.
                set_status(rtc_time::is_synced() ? "Invalid TOTP secret" : "No time sync -- connect WiFi first");
                return;
            }
            break;
        }
        case Field::LoginAndPassword:
            text = entry.login + "\t" + entry.password;
            break;
    }

    if (text.empty()) {
        set_status("Field is empty");
        return;
    }

    spawn_type_task(text);
}

} // namespace usb
