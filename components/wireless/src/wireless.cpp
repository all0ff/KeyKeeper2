#include "wireless/wireless.hpp"

#include "bsp/pins.hpp"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "sdkconfig.h"

#if CONFIG_WIRELESS_TRANSPORT_BLE && !CONFIG_BT_NIMBLE_ENABLED
#error "CONFIG_WIRELESS_TRANSPORT_BLE needs NimBLE: set CONFIG_BT_ENABLED=y and CONFIG_BT_NIMBLE_ENABLED=y (see components/blelink/README.md)"
#endif

#include "blelink/blelink.hpp"
#include "kkproto/crypto_port.hpp"
#include "kkproto/link.hpp"
#include "kkproto/messages.hpp"
#include "usb/dongle_sink.hpp"
#include "usb/usb_service.hpp"

#include "link_frame.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace wireless {

namespace {

constexpr char TAG[] = "wireless";

// ---- transport: BLE (CONFIG_WIRELESS_TRANSPORT_BLE) or a UART cable between two boards
#if CONFIG_WIRELESS_TRANSPORT_BLE
constexpr bool BLE = true;
#else
constexpr bool BLE = false;
#endif
constexpr uart_port_t UART = UART_NUM_1;
constexpr int BAUD = 460800;
constexpr size_t UART_RX_BUFFER = 1024;
constexpr TickType_t POLL_TICKS = 2; // ~20 ms at HZ=100, 2 ms at HZ=1000: the UART read blocks for it

// BLE: how long a pairing may look for a dongle with an open window before it gives up.
constexpr uint32_t PAIRING_SEARCH_MS = 60000;

constexpr uint8_t FW_MAJOR = 0;
constexpr uint8_t FW_MINOR = 1;

constexpr uint32_t TASK_STACK = 10240;
constexpr UBaseType_t TASK_PRIO = 4;

// ---- storage
constexpr char NS[] = "kklink";
constexpr char KEY_SECRET[] = "sk";
constexpr char KEY_PEER[] = "peer";
constexpr char KEY_BADDR[] = "baddr"; // BLE address of the paired dongle (type + 6 bytes)
constexpr char KEY_ENABLED[] = "en";

bool nvs_load_blob(const char* key, uint8_t* out, size_t len = 32)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    uint8_t tmp[32];
    size_t n = len;
    if (len > sizeof tmp) {
        nvs_close(h);
        return false;
    }
    const esp_err_t e = nvs_get_blob(h, key, tmp, &n);
    nvs_close(h);
    if (e != ESP_OK || n != len) {
        return false;
    }
    std::memcpy(out, tmp, len);
    return true;
}

bool nvs_save_blob(const char* key, const uint8_t* in, size_t len = 32)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t e = nvs_set_blob(h, key, in, len);
    if (e == ESP_OK) {
        e = nvs_commit(h);
    }
    nvs_close(h);
    return e == ESP_OK;
}

bool nvs_forget_key(const char* key)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t e = nvs_erase_key(h, key);
    if (e == ESP_ERR_NVS_NOT_FOUND) {
        e = ESP_OK;
    }
    if (e == ESP_OK) {
        e = nvs_commit(h);
    }
    nvs_close(h);
    return e == ESP_OK;
}

bool nvs_load_enabled()
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    uint8_t v = 0;
    const esp_err_t e = nvs_get_u8(h, KEY_ENABLED, &v);
    nvs_close(h);
    return e == ESP_OK && v != 0;
}

bool nvs_save_enabled(bool on)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t e = nvs_set_u8(h, KEY_ENABLED, on ? 1 : 0);
    if (e == ESP_OK) {
        e = nvs_commit(h);
    }
    nvs_close(h);
    return e == ESP_OK;
}

uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void hex8(const uint8_t* pk, char out[17])
{
    static const char* H = "0123456789abcdef";
    for (int i = 0; i < 8; ++i) {
        out[2 * i] = H[pk[i] >> 4];
        out[2 * i + 1] = H[pk[i] & 15];
    }
    out[16] = '\0';
}

// ---- the link
class Link final : public kk::link::Io, public usb::DongleChannel {
public:
    Link() : sink_(*this) {}

    bool init()
    {
        mu_ = xSemaphoreCreateMutex();
        done_ = xSemaphoreCreateBinary();
        if (mu_ == nullptr || done_ == nullptr) {
            return false;
        }

        if (!nvs_load_blob(KEY_SECRET, key_.sk) || !kk::crypto::x25519_public(key_.sk, key_.pk)) {
            ESP_LOGW(TAG, "no stored link key: generating one");
            if (!kk::crypto::x25519_keygen(key_.sk, key_.pk) || !nvs_save_blob(KEY_SECRET, key_.sk)) {
                ESP_LOGE(TAG, "could not create or store the link key");
                return false;
            }
        }
        {
            char k[17];
            hex8(key_.pk, k);
            ESP_LOGI(TAG, "this device's link key starts %s", k);
        }

        kk::link::Params params;
        // Curve25519 costs 0.1-0.2 s per operation on this chip, a handshake message needs 3-5 of them: the
        // vault must wait long for a reply before it starts over (a restart discards the answer to the
        // previous try). Verified on hardware with the dongle prototype.
        params.retry_ms = 4000;
        params.handshake_timeout_ms = 8000;
        ep_ = new kk::link::Endpoint(kk::link::Role::Vault, *this, key_, params);
        ep_->set_device_info(FW_MAJOR, FW_MINOR);

        uint8_t peer[32];
        if (nvs_load_blob(KEY_PEER, peer)) {
            ep_->set_trusted_peer(peer);
            char k[17];
            hex8(peer, k);
            std::memcpy(peer_hex_, k, sizeof k);
            ESP_LOGI(TAG, "paired with a dongle whose key starts %s", k);
        } else {
            ESP_LOGI(TAG, "no dongle paired");
        }
        paired_ = ep_->has_trusted_peer();

        want_enabled_ = nvs_load_enabled();
        cmd_.apply_enabled = true; // the link task starts the UART (and connects) if the switch was left on
        usb::set_output(want_enabled_ ? &sink_ : nullptr, "Dongle not connected");
        publish();

        if (xTaskCreate(&Link::task_entry, "kk_link", TASK_STACK, this, TASK_PRIO, nullptr) != pdPASS) {
            ESP_LOGE(TAG, "could not start the link task");
            return false;
        }
        return true;
    }

    // ---- API (any task)
    void set_peer_language(uint8_t lang) { lang_ = lang; }

    void start_radio()
    {
        lock();
        radio_ok_ = true;
        cmd_.apply_enabled = true;
        unlock();
    }

    bool enabled() const { return want_enabled_.load(); }

    bool set_enabled(bool on)
    {
        if (!nvs_save_enabled(on)) {
            ESP_LOGE(TAG, "could not store the wireless switch");
            return false;
        }
        lock();
        want_enabled_ = on;
        cmd_.apply_enabled = true;
        unlock();
        usb::set_output(on ? &sink_ : nullptr, "Dongle not connected");
        return true;
    }

    Status status()
    {
        lock();
        const Status s = status_;
        unlock();
        return s;
    }

    bool start_pairing()
    {
        if (!enabled() && !set_enabled(true)) {
            return false;
        }
        lock();
        cmd_.pair = true;
        status_.outcome = Outcome::None;
        unlock();
        return true;
    }

    void confirm(bool accept)
    {
        lock();
        cmd_.confirm = accept ? 1 : 2;
        unlock();
    }

    void cancel_pairing()
    {
        lock();
        cmd_.cancel = true;
        unlock();
    }

    bool forget()
    {
        if (!nvs_forget_key(KEY_PEER)) {
            ESP_LOGE(TAG, "could not erase the stored dongle key");
            return false;
        }
        lock();
        cmd_.forget = true;
        status_.outcome = Outcome::None;
        unlock();
        return true;
    }

    void clear_outcome()
    {
        lock();
        status_.outcome = Outcome::None;
        unlock();
    }

    // ---- usb::DongleChannel (called from the typing task)
    bool linked() override { return a_linked_.load(); }
    bool usb_ready() override { return a_usb_.load(); }

    bool type_keys(const uint8_t* body, size_t n, kk::msg::Result* r, uint32_t timeout_ms) override
    {
        if (n == 0 || n > sizeof req_.body) {
            return false;
        }
        lock();
        if (req_.state != Req::None) { // one request at a time (the typing task is serialised already)
            unlock();
            return false;
        }
        std::memcpy(req_.body, body, n);
        req_.len = n;
        req_.state = Req::Pending;
        unlock();
        xSemaphoreTake(done_, 0); // forget a stale signal

        const bool signalled = xSemaphoreTake(done_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
        lock();
        const bool ok = signalled && req_.state == Req::Done;
        if (ok) {
            *r = req_.result;
        }
        req_.state = Req::None; // a Result arriving later no longer matches anything
        unlock();
        return ok;
    }

    // ---- kk::link::Io (link task)
    void send(const uint8_t* data, size_t n) override
    {
        if (!port_up_) {
            return;
        }
        if (BLE) {
            blelink::send(data, n); // BLE keeps message boundaries itself: no framing, no CRC
            return;
        }
        const size_t m = uartlink::encode(data, n, tx_frame_, sizeof tx_frame_);
        if (m != 0) {
            uart_write_bytes(UART, tx_frame_, m);
        }
    }

    void message(const kk::msg::Header& h, const uint8_t* body) override
    {
        if (h.type != kk::msg::Type::Result) {
            return;
        }
        kk::msg::Result r;
        if (!kk::msg::decode_result(body, h.len, &r)) {
            return;
        }
        lock();
        const bool mine = req_.state == Req::Sent && r.seq == req_.seq;
        if (mine) {
            req_.result = r;
            req_.state = Req::Done;
        }
        unlock();
        if (mine) {
            xSemaphoreGive(done_);
        }
    }

    void event(kk::link::Event e) override
    {
        using kk::link::Event;
        ESP_LOGI(TAG, "link: %s", kk::link::event_name(e));
        switch (e) {
        case Event::CodeReady:
            ESP_LOGI(TAG, "pairing code %.3s %.3s", ep_->code(), ep_->code() + 3);
            break;
        case Event::PairingDone: {
            const uint8_t* pk = ep_->peer_static();
            if (pk != nullptr && nvs_save_blob(KEY_PEER, pk)) {
                char k[17];
                hex8(pk, k);
                std::memcpy(peer_hex_, k, sizeof k);
                ESP_LOGI(TAG, "paired; dongle key %s... stored", k);
                pairing_wanted_ = false;
                remember_dongle_address();
                set_outcome(Outcome::Paired);
                connect_after_pairing_ = true;
            } else {
                ESP_LOGE(TAG, "pairing finished but the dongle key could not be stored");
                ep_->forget_peer();
                set_outcome(Outcome::Failed);
            }
            break;
        }
        case Event::PairingRejected:
            set_outcome(Outcome::Rejected);
            end_ble_pairing();
            break;
        case Event::PairingFailed:
            set_outcome(Outcome::Failed);
            end_ble_pairing();
            break;
        case Event::Linked:
            remember_dongle_address(); // a dongle paired before addresses were stored: learn it now
            break;
        case Event::LinkLost: fail_request(); break;
        default: break;
        }
        paired_ = ep_->has_trusted_peer();
        refresh_atomics();
    }

private:
    struct Cmd {
        bool apply_enabled = false;
        bool pair = false;
        uint8_t confirm = 0; // 1 yes, 2 no
        bool cancel = false;
        bool forget = false;
    };

    struct Req {
        enum State : uint8_t { None, Pending, Sent, Done };
        State state = None;
        uint8_t body[224];
        size_t len = 0;
        uint16_t seq = 0;
        kk::msg::Result result;
    };

    void lock() { xSemaphoreTake(mu_, portMAX_DELAY); }
    void unlock() { xSemaphoreGive(mu_); }

    void set_outcome(Outcome o)
    {
        lock();
        status_.outcome = o;
        unlock();
    }

    void refresh_atomics()
    {
        const bool l = ep_->state() == kk::link::State::Linked;
        a_linked_ = l;
        a_usb_ = l && ep_->peer_usb_mounted();
    }

    // A request waiting for a Result that can no longer arrive: wake the typing task.
    void fail_request()
    {
        lock();
        const bool waiting = req_.state == Req::Pending || req_.state == Req::Sent;
        if (waiting) {
            req_.state = Req::None;
        }
        unlock();
        if (waiting) {
            xSemaphoreGive(done_);
        }
    }

    // ---- BLE: which dongle the radio looks for
    // Points the radio at what the current situation needs: a dongle with an open pairing window, the paired
    // dongle (by its stored address, or any dongle for a pairing made before addresses were stored), or nothing.
    void ble_aim()
    {
        if (!BLE) {
            return;
        }
        if (pairing_wanted_) {
            blelink::scan_pairing();
        } else if (!ep_->has_trusted_peer()) {
            blelink::stop();
        } else {
            uint8_t raw[7];
            if (nvs_load_blob(KEY_BADDR, raw, sizeof raw)) {
                blelink::Addr a;
                a.type = raw[0];
                std::memcpy(a.val, raw + 1, 6);
                blelink::scan_addr(a);
            } else {
                blelink::scan_any();
            }
        }
    }

    // Remembers the BLE address of the dongle we are connected to as THE paired dongle's address.
    void remember_dongle_address()
    {
        if (!BLE || !ep_->has_trusted_peer()) {
            return;
        }
        blelink::Addr a;
        if (!blelink::peer_addr(&a)) {
            return;
        }
        uint8_t raw[7];
        raw[0] = a.type;
        std::memcpy(raw + 1, a.val, 6);
        uint8_t old[7];
        if (nvs_load_blob(KEY_BADDR, old, sizeof old) && std::memcmp(old, raw, sizeof raw) == 0) {
            return;
        }
        if (nvs_save_blob(KEY_BADDR, raw, sizeof raw)) {
            ESP_LOGI(TAG, "dongle BLE address stored");
            blelink::scan_addr(a);
        }
    }

    // A pairing ended without success: stop looking for a dongle in pairing mode and go back to the paired one.
    void end_ble_pairing()
    {
        pairing_wanted_ = false;
        if (BLE && port_up_) {
            blelink::disconnect();
            ble_aim();
        }
    }

    // The BLE link came up or went away: the session over it is dead; start the one the situation needs.
    void ble_link_changed()
    {
        fail_request();
        const kk::link::State before = ep_->state();
        ep_->reset();
        refresh_atomics(); // the screen and the typing task must not see "Linked" while the new handshake computes
        publish();
        const uint32_t now = now_ms();
        if (!blelink::connected()) {
            if (pairing_wanted_ && before == kk::link::State::Confirming) {
                ESP_LOGW(TAG, "BLE link lost while the pairing code was showing");
                set_outcome(Outcome::Failed);
                end_ble_pairing();
            }
            return;
        }
        if (pairing_wanted_) {
            ep_->start_pairing(now);
        } else if (ep_->has_trusted_peer()) {
            ep_->connect(now);
        }
    }

    bool port_start()
    {
        if (BLE) {
            if (!blelink::start(blelink::Role::Central, "KeyKeeper vault")) {
                return false;
            }
            ble_epoch_ = blelink::epoch();
            port_up_ = true;
            return true;
        }
        return uart_start();
    }

    void port_stop()
    {
        if (BLE) {
            vTaskDelay(pdMS_TO_TICKS(60)); // let a Bye out
            blelink::stop();
            port_up_ = false;
            return;
        }
        uart_stop();
    }

    bool uart_start()
    {
        uart_config_t cfg = {};
        cfg.baud_rate = BAUD;
        cfg.data_bits = UART_DATA_8_BITS;
        cfg.parity = UART_PARITY_DISABLE;
        cfg.stop_bits = UART_STOP_BITS_1;
        cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        cfg.source_clk = UART_SCLK_DEFAULT;
        if (uart_driver_install(UART, UART_RX_BUFFER, 0, 0, nullptr, 0) != ESP_OK) {
            return false;
        }
        if (uart_param_config(UART, &cfg) != ESP_OK ||
            uart_set_pin(UART, bsp::pins::HEADER_GPIO10, bsp::pins::HEADER_GPIO11, UART_PIN_NO_CHANGE,
                         UART_PIN_NO_CHANGE) != ESP_OK) {
            uart_driver_delete(UART);
            return false;
        }
        gpio_set_pull_mode(bsp::pins::HEADER_GPIO11, GPIO_PULLUP_ONLY);
        port_up_ = true;
        parser_ = uartlink::Parser();
        return true;
    }

    void uart_stop()
    {
        if (!port_up_) {
            return;
        }
        uart_wait_tx_done(UART, pdMS_TO_TICKS(100));
        port_up_ = false;
        uart_driver_delete(UART);
    }

    void apply_commands()
    {
        lock();
        const Cmd c = cmd_;
        cmd_ = Cmd();
        const bool on = want_enabled_.load();
        unlock();

        const uint32_t now = now_ms();

        if (c.forget) {
            ep_->forget_peer(); // ends a session, drops the key
            peer_hex_[0] = '\0';
            pairing_wanted_ = false;
            nvs_forget_key(KEY_BADDR);
            ESP_LOGI(TAG, "dongle forgotten");
        }
        if (c.apply_enabled || c.forget) {
            if (on && !port_up_ && !radio_ok_.load()) {
                // the radio is held back until the UI is up (start_radio): BLE needs ~60 KB of internal RAM
            } else if (on && !port_up_) {
                if (!port_start()) {
                    ESP_LOGE(TAG, "%s setup failed", BLE ? "BLE" : "UART");
                } else if (BLE) {
                    ble_aim(); // the session starts when the BLE link comes up (ble_link_changed)
                } else if (ep_->has_trusted_peer()) {
                    ep_->connect(now);
                }
            } else if (!on && port_up_) {
                ep_->disconnect();
                ep_->tick(now); // let the Bye out
                fail_request();
                pairing_wanted_ = false;
                port_stop();
                ep_->reset();
            } else if (BLE && on && c.forget) {
                ble_aim(); // forgotten: drop the link and stop looking
            }
        }
        if (!on || !port_up_) {
            return;
        }
        const kk::link::State st = ep_->state();
        if (c.cancel && (st == kk::link::State::Pairing || st == kk::link::State::Confirming || pairing_wanted_)) {
            ep_->reject_pairing();
            ep_->reset();
            if (BLE) {
                end_ble_pairing();
            } else if (ep_->has_trusted_peer()) {
                ep_->connect(now);
            }
        }
        if (c.pair) {
            if (BLE) {
                pairing_wanted_ = true;
                pairing_deadline_ = now + PAIRING_SEARCH_MS;
                ep_->reset();
                blelink::scan_pairing(); // the pairing starts when the BLE link to the dongle is up
            } else {
                ep_->start_pairing(now);
            }
        }
        if (c.confirm == 1) {
            ep_->confirm_pairing(now);
        } else if (c.confirm == 2) {
            ep_->reject_pairing();
        }
    }

    void publish()
    {
        Status s;
        const bool on = want_enabled_.load();
        s.enabled = on;
        s.paired = ep_->has_trusted_peer();
        if (!on) {
            s.phase = Phase::Off;
        } else {
            switch (ep_->state()) {
            case kk::link::State::Pairing: s.phase = Phase::Pairing; break;
            case kk::link::State::Confirming: s.phase = Phase::Confirming; break;
            case kk::link::State::Linked: s.phase = Phase::Linked; break;
            case kk::link::State::Connecting: s.phase = Phase::Connecting; break;
            case kk::link::State::Idle:
                if (pairing_wanted_) {
                    s.phase = Phase::Pairing; // BLE: still looking for a dongle with an open pairing window
                } else {
                    s.phase = (s.paired || ep_->wants_link()) ? Phase::Connecting : Phase::NotPaired;
                }
                break;
            }
        }
        s.dongle_usb_ready = s.phase == Phase::Linked && ep_->peer_usb_mounted();
        if (s.phase == Phase::Confirming && ep_->code() != nullptr) {
            std::snprintf(s.code, sizeof s.code, "%s", ep_->code());
            s.local_confirmed = ep_->local_confirmed();
        }
        if (s.paired) {
            std::memcpy(s.peer, peer_hex_, sizeof s.peer);
        }
        lock();
        s.outcome = status_.outcome; // owned by event() / the API
        status_ = s;
        unlock();
    }

    void send_pending_request()
    {
        uint8_t body[sizeof req_.body];
        size_t len = 0;
        lock();
        const bool pending = req_.state == Req::Pending;
        if (pending) {
            len = req_.len;
            std::memcpy(body, req_.body, len);
        }
        unlock();
        if (!pending) {
            return;
        }
        uint16_t seq = 0;
        if (ep_->state() != kk::link::State::Linked ||
            !ep_->send_message(kk::msg::Type::TypeKeys, 0, body, len, &seq)) {
            fail_request();
            return;
        }
        lock();
        if (req_.state == Req::Pending) { // the caller may have given up meanwhile
            req_.seq = seq;
            req_.state = Req::Sent;
        }
        unlock();
    }

    // The vault's menu language goes to the dongle (its screen speaks the same language): once per link, and
    // again whenever the setting changes.
    void send_language()
    {
        if (ep_->state() != kk::link::State::Linked) {
            lang_sent_ = 0xFF;
            return;
        }
        const uint8_t want = lang_.load();
        if (want != 0xFF && want != lang_sent_ && ep_->send_message(kk::msg::Type::Language, 0, &want, 1)) {
            lang_sent_ = want;
        }
    }

    void run()
    {
        for (;;) {
            apply_commands();

            if (!port_up_) {
                publish();
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            if (BLE) {
                if (blelink::epoch() != ble_epoch_) {
                    ble_epoch_ = blelink::epoch();
                    ble_link_changed();
                }
                const int len = blelink::recv(rx_, sizeof rx_, POLL_TICKS);
                if (len > 0) {
                    ep_->on_frame(rx_, static_cast<size_t>(len), now_ms());
                }
                if (pairing_wanted_ && !blelink::connected() && ep_->state() == kk::link::State::Idle &&
                    static_cast<int32_t>(now_ms() - pairing_deadline_) > 0) {
                    ESP_LOGW(TAG, "no dongle with an open pairing window found");
                    set_outcome(Outcome::Failed);
                    end_ble_pairing();
                }
            } else {
                const int n = uart_read_bytes(UART, rx_, sizeof rx_, POLL_TICKS);
                if (n > 0) {
                    const uint32_t now = now_ms();
                    parser_.feed(rx_, static_cast<size_t>(n), [&](const uint8_t* payload, size_t len) {
                        ep_->on_frame(payload, len, now);
                    });
                }
            }
            ep_->tick(now_ms());

            if (connect_after_pairing_) {
                connect_after_pairing_ = false;
                ep_->connect(now_ms());
            }
            refresh_atomics();
            send_language();
            send_pending_request();
            publish();
        }
    }

    static void task_entry(void* arg)
    {
        static_cast<Link*>(arg)->run();
    }

    kk::noise::KeyPair key_;
    kk::link::Endpoint* ep_ = nullptr;
    usb::DongleSink sink_;

    SemaphoreHandle_t mu_ = nullptr;
    SemaphoreHandle_t done_ = nullptr;
    Cmd cmd_;
    Req req_;
    Status status_;
    std::atomic<bool> want_enabled_{false};
    std::atomic<uint8_t> lang_{0xFF}; ///< menu language to tell the dongle (0xFF: not known yet)
    uint8_t lang_sent_ = 0xFF;
    std::atomic<bool> radio_ok_{false}; ///< set by start_radio(): the port may be brought up
    std::atomic<bool> a_linked_{false};
    std::atomic<bool> a_usb_{false};

    // used by the link task only
    bool port_up_ = false;
    bool paired_ = false;
    bool connect_after_pairing_ = false;
    char peer_hex_[17] = {};
    uartlink::Parser parser_;
    uint8_t rx_[256]; // one UART read chunk, or one BLE message
    bool pairing_wanted_ = false; // BLE: a pairing was started and has not ended yet
    uint32_t pairing_deadline_ = 0;
    uint32_t ble_epoch_ = 0;
    uint8_t tx_frame_[uartlink::MAX_FRAME];
};

Link* g_link = nullptr;

} // namespace

bool init()
{
    if (g_link != nullptr) {
        return true;
    }
    Link* l = new Link();
    if (!l->init()) {
        // The task did not start; keep the object (the switch stays off, the menu says so).
        ESP_LOGE(TAG, "wireless typing unavailable");
        usb::set_output(nullptr);
        return false;
    }
    g_link = l;
    return true;
}

void set_peer_language(uint8_t lang)
{
    if (g_link != nullptr) {
        g_link->set_peer_language(lang);
    }
}
void start_radio()
{
    if (g_link != nullptr) {
        g_link->start_radio();
    }
}
bool supported() { return true; }
bool enabled() { return g_link != nullptr && g_link->enabled(); }
bool set_enabled(bool on) { return g_link != nullptr && g_link->set_enabled(on); }
Status status()
{
    if (g_link == nullptr) {
        Status s;
        s.phase = Phase::Unsupported;
        return s;
    }
    return g_link->status();
}
bool start_pairing() { return g_link != nullptr && g_link->start_pairing(); }
void confirm(bool accept)
{
    if (g_link != nullptr) g_link->confirm(accept);
}
void cancel_pairing()
{
    if (g_link != nullptr) g_link->cancel_pairing();
}
bool forget() { return g_link != nullptr && g_link->forget(); }
void clear_outcome()
{
    if (g_link != nullptr) g_link->clear_outcome();
}

} // namespace wireless
