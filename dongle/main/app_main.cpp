// KeyKeeper2 dongle firmware (and, with CONFIG_DONGLE_ROLE_VAULT_SIM, the vault simulator that stands in for
// the vault until the real vault firmware speaks the link).
//
// All behaviour lives in code that is tested on a PC:
//   kkproto/link.hpp   the link state machine (pairing, sessions, reconnect)
//   actions.hpp        what a BOOT press means in each situation
//   status_model.hpp   what the screen says
//   button.hpp         short / long press
// This file only connects them to the UART, the BOOT button, the screen, NVS and the log.

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp/bsp.hpp"
#include "display/display.hpp"
#include "display/lvgl_port.hpp"
#include "kkproto/crypto_port.hpp"
#include "kkproto/link.hpp"

#include "actions.hpp"
#include "button.hpp"
#include "link_frame.hpp"
#include "status_model.hpp"
#include "store.hpp"
#include "ui.hpp"

namespace {

const char* TAG = "dongle";

#if CONFIG_DONGLE_ROLE_VAULT_SIM
constexpr kk::link::Role ROLE = kk::link::Role::Vault;
#else
constexpr kk::link::Role ROLE = kk::link::Role::Dongle;
#endif

constexpr uart_port_t UART = static_cast<uart_port_t>(CONFIG_DONGLE_UART_NUM);
constexpr gpio_num_t TX_PIN = static_cast<gpio_num_t>(CONFIG_DONGLE_TX_PIN);
constexpr gpio_num_t RX_PIN = static_cast<gpio_num_t>(CONFIG_DONGLE_RX_PIN);
constexpr int BAUD = CONFIG_DONGLE_BAUD;
constexpr gpio_num_t BOOT_PIN = GPIO_NUM_0;  // the BOOT button, active low

constexpr uint32_t NOTICE_MS = 4000;
constexpr uint32_t LONG_PRESS_MS = 3000;
constexpr uint8_t FW_MAJOR = 0;
constexpr uint8_t FW_MINOR = 1;

// Wait up to ~5 ms for bytes. At a 100 Hz tick pdMS_TO_TICKS(5) would be 0 (a non-blocking read and a
// loop that starves the idle task), so never less than one tick.
constexpr TickType_t POLL_TICKS = (pdMS_TO_TICKS(5) > 0) ? pdMS_TO_TICKS(5) : 1;

// Static, not on the task stack.
uint8_t s_rx[128];
uint8_t s_frame[uartlink::MAX_FRAME];

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void hex8(const uint8_t* p, char out[17])
{
    static const char* d = "0123456789abcdef";
    for (int i = 0; i < 8; ++i) {
        out[2 * i] = d[p[i] >> 4];
        out[2 * i + 1] = d[p[i] & 15];
    }
    out[16] = '\0';
}

// ---------------------------------------------------------------------------------------------

class App : public kk::link::Io {
public:
    kk::link::Endpoint* ep = nullptr;
    dongle::Notice notice = dongle::Notice::None;
    uint32_t notice_until = 0;
    uint32_t window_end = 0;
    bool connect_after_pairing = false;

    // kk::link::Io
    void send(const uint8_t* data, size_t n) override
    {
        const size_t m = uartlink::encode(data, n, s_frame, sizeof s_frame);
        if (ep->state() != kk::link::State::Linked) {
            ESP_LOGI(TAG, "tx %u B (state %s)", static_cast<unsigned>(n), kk::link::state_name(ep->state()));
        }
        if (m != 0) {
            uart_write_bytes(UART, s_frame, m);
        }
    }

    void event(kk::link::Event e) override
    {
        using kk::link::Event;
        ESP_LOGI(TAG, "link: %s", kk::link::event_name(e));
        if (ep->fail_reason() != kk::link::Fail::None) {
            ESP_LOGW(TAG, "link: last failure: %s", kk::link::fail_name(ep->fail_reason()));
        }
        switch (e) {
        case Event::CodeReady:
            ESP_LOGI(TAG, "==== PAIRING CODE: %.3s %.3s ====", ep->code(), ep->code() + 3);
            break;
        case Event::PairingDone: {
            const uint8_t* pk = ep->peer_static();
            if (pk != nullptr && dongle::store::save_peer(pk)) {
                char k[17];
                hex8(pk, k);
                ESP_LOGI(TAG, "paired; peer key %s... stored", k);
            } else {
                ESP_LOGE(TAG, "pairing finished but the peer key could not be stored");
            }
            set_notice(dongle::Notice::Paired);
            connect_after_pairing = true;
            break;
        }
        case Event::PairingRejected: set_notice(dongle::Notice::Rejected); break;
        case Event::PairingFailed: set_notice(dongle::Notice::Failed); break;
        case Event::PairingWindowClosed: set_notice(dongle::Notice::WindowClosed); break;
        case Event::Linked:
            notice = dongle::Notice::None;
            break;
        default: break;
        }
    }

    void set_notice(dongle::Notice n)
    {
        notice = n;
        notice_until = now_ms() + NOTICE_MS;
    }
};

bool init_uart()
{
    uart_config_t cfg = {};
    cfg.baud_rate = BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    if (uart_driver_install(UART, 1024, 0, 0, nullptr, 0) != ESP_OK) return false;
    if (uart_param_config(UART, &cfg) != ESP_OK) return false;
    if (uart_set_pin(UART, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return false;
    gpio_set_pull_mode(RX_PIN, GPIO_PULLUP_ONLY);
    return true;
}

void show_fatal(const char* what, const char* detail)
{
    dongle::Screen s;
    std::snprintf(s.title, sizeof s.title, "%s", "KeyKeeper dongle");
    std::snprintf(s.status, sizeof s.status, "%s", what);
    std::snprintf(s.hint, sizeof s.hint, "%s", detail);
    s.tone = dongle::Tone::Bad;
    dongle::ui::show(s);
}

[[noreturn]] void halt()
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "%s firmware %u.%u", ROLE == kk::link::Role::Dongle ? "Dongle" : "Vault simulator",
             static_cast<unsigned>(FW_MAJOR), static_cast<unsigned>(FW_MINOR));

    // 1. Screen first, so that every later failure can be shown on it.
    if (!bsp::init() || !display::init()) {
        ESP_LOGE(TAG, "display initialisation failed");
        halt();
    }
    display::set_backlight(true);
    if (!lvgl_port::init() || !dongle::ui::init()) {
        ESP_LOGE(TAG, "LVGL initialisation failed");
        halt();
    }
    {
        dongle::Screen s;
        std::snprintf(s.title, sizeof s.title, "%s", ROLE == kk::link::Role::Dongle ? "KeyKeeper dongle" : "Vault (simulator)");
        std::snprintf(s.status, sizeof s.status, "%s", "Starting...");
        dongle::ui::show(s);
    }

    // 2. Crypto must work, or nothing below means anything.
    const char* bad = kk::crypto::selftest();
    if (bad != nullptr) {
        ESP_LOGE(TAG, "kkproto self-test FAILED at: %s (check the mbedTLS switches in sdkconfig.defaults)", bad);
        show_fatal("Crypto self-test failed", bad);
        halt();
    }

    // 3. Storage: this device's key (made on first start) and the paired peer, if any.
    if (!dongle::store::init()) {
        ESP_LOGE(TAG, "NVS initialisation failed");
        show_fatal("Storage failed", "NVS init");
        halt();
    }
    static kk::noise::KeyPair key;
    bool have_key = dongle::store::load_secret(key.sk) && kk::crypto::x25519_public(key.sk, key.pk);
    if (!have_key) {
        ESP_LOGW(TAG, "no stored key: generating one");
        if (!kk::crypto::x25519_keygen(key.sk, key.pk) || !dongle::store::save_secret(key.sk)) {
            ESP_LOGE(TAG, "could not create or store the device key");
            show_fatal("Key storage failed", "NVS write");
            halt();
        }
    }
    {
        char k[17];
        hex8(key.pk, k);
        ESP_LOGI(TAG, "this device's public key starts %s", k);
    }
    uint8_t peer[32];
    const bool have_peer = dongle::store::load_peer(peer);

    // 4. The link.
    if (!init_uart()) {
        ESP_LOGE(TAG, "UART setup failed: check the pin numbers in menuconfig");
        show_fatal("UART setup failed", "check menuconfig pins");
        halt();
    }
    gpio_set_direction(BOOT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_PIN, GPIO_PULLUP_ONLY);

    static App app;
    // Curve25519 is slow on this chip (a handshake message costs 0.5..1 s of computing), so the vault must
    // wait long for the reply before it starts over; a restart discards the answer to the previous try.
    kk::link::Params params;
    params.retry_ms = 4000;
    params.handshake_timeout_ms = 8000;
    static kk::link::Endpoint ep(ROLE, app, key, params);
    app.ep = &ep;
    ep.set_device_info(FW_MAJOR, FW_MINOR);
    if (have_peer) {
        ep.set_trusted_peer(peer);
        char k[17];
        hex8(peer, k);
        ESP_LOGI(TAG, "paired with a peer whose key starts %s", k);
    } else {
        ESP_LOGI(TAG, "not paired");
    }
    if (ROLE == kk::link::Role::Vault && have_peer) {
        ep.connect(now_ms());
    }

    static uartlink::Parser parser;
    static dongle::ButtonDetector button(LONG_PRESS_MS);
    ESP_LOGI(TAG, "ready");

    uint32_t last_status_log = now_ms();
    for (;;) {
        const int n = uart_read_bytes(UART, s_rx, sizeof s_rx, POLL_TICKS);
        const uint32_t now = now_ms();
        if (n > 0) {
            parser.feed(s_rx, static_cast<size_t>(n), [&](const uint8_t* payload, size_t len) {
                if (ep.state() != kk::link::State::Linked) {
                    ESP_LOGI(TAG, "rx %u B (state %s)", static_cast<unsigned>(len), kk::link::state_name(ep.state()));
                }
                ep.on_frame(payload, len, now);
            });
        }
        ep.tick(now);

        if (app.connect_after_pairing) {
            app.connect_after_pairing = false;
            if (ROLE == kk::link::Role::Vault) {
                ep.connect(now);
            }
        }
        if (app.notice != dongle::Notice::None && static_cast<int32_t>(now - app.notice_until) >= 0) {
            app.notice = dongle::Notice::None;
        }

        dongle::Situation sit;
        sit.role = ROLE;
        sit.state = ep.state();
        sit.paired = ep.has_trusted_peer();
        sit.window_open = ep.pairing_open();
        sit.local_confirmed = ep.local_confirmed();

        switch (dongle::action_for(sit, button.feed(gpio_get_level(BOOT_PIN) == 0, now))) {
        case dongle::Action::None: break;
        case dongle::Action::OpenPairing:
            app.window_end = now + static_cast<uint32_t>(CONFIG_DONGLE_PAIRING_WINDOW_S) * 1000u;
            ep.open_pairing(now, static_cast<uint32_t>(CONFIG_DONGLE_PAIRING_WINDOW_S) * 1000u);
            app.notice = dongle::Notice::None;
            ESP_LOGI(TAG, "pairing window open for %d s", CONFIG_DONGLE_PAIRING_WINDOW_S);
            break;
        case dongle::Action::CancelPairing:
            if (ROLE == kk::link::Role::Dongle) {
                ep.close_pairing();
            } else {
                ep.reject_pairing();
            }
            app.set_notice(dongle::Notice::Rejected);
            break;
        case dongle::Action::StartPairing:
            app.notice = dongle::Notice::None;
            ep.start_pairing(now);
            break;
        case dongle::Action::Confirm:
            ESP_LOGI(TAG, "code confirmed on this device");
            ep.confirm_pairing(now);
            break;
        case dongle::Action::Reject:
            ESP_LOGI(TAG, "code rejected on this device");
            ep.reject_pairing();
            break;
        case dongle::Action::Forget:
            ESP_LOGI(TAG, "forgetting the pairing");
            ep.forget_peer();
            if (!dongle::store::erase_peer()) {
                ESP_LOGE(TAG, "could not erase the stored peer");
            }
            app.set_notice(dongle::Notice::Forgotten);
            break;
        }

        // Screen
        dongle::Snapshot snap;
        snap.role = ROLE;
        snap.state = ep.state();
        snap.paired = ep.has_trusted_peer();
        snap.window_open = ep.pairing_open();
        const int32_t left = static_cast<int32_t>(app.window_end - now);
        snap.window_left_s = left > 0 ? static_cast<uint32_t>(left) / 1000u : 0;
        snap.code = ep.code();
        snap.local_confirmed = ep.local_confirmed();
        snap.remote_confirmed = ep.remote_confirmed();
        snap.notice = app.notice;
        snap.long_s = LONG_PRESS_MS / 1000;
        {
            dongle::Situation held = sit;
            const uint32_t held_s = button.held_ms(now) / 1000;
            // Only tell the user to keep holding if holding does something here.
            if (held_s >= 1 && dongle::action_for(held, dongle::Press::Long) != dongle::Action::None) {
                snap.held_s = held_s;
            }
        }
        dongle::ui::show(dongle::describe(snap));

        if (static_cast<int32_t>(now - last_status_log) >= 10000) {
            last_status_log = now;
            ESP_LOGI(TAG, "[%s] link: %" PRIu32 " frames ok, %" PRIu32 " bad CRC, %" PRIu32 " junk bytes",
                     kk::link::state_name(ep.state()), parser.frames_ok, parser.bad_crc, parser.junk_bytes);
        }
    }
}
