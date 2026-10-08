// Pairing test over the UART link.
//
// Flash the SAME firmware to both boards (wired as in tools/uart_link_test). Press BOOT on ONE board:
// it becomes the initiator and runs a Noise XX pairing with the other board over the wire. Both boards
// then print the same 6-digit code, and exchange one encrypted HELLO / HELLO_ACK to prove the session
// works. All the conversation logic is in pairing_flow.hpp and is unit-tested on the PC; this file only
// connects it to the UART, the BOOT button and the log.
//
// It also answers two questions the PC could not: does kkproto build and pass its self-test with this
// ESP-IDF's mbedTLS, and how long do X25519 and a whole pairing take on the ESP32-S3.

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

#include "kkproto/crypto_port.hpp"
#include "link_frame.hpp"
#include "pairing_flow.hpp"

namespace {

const char* TAG = "pair";

constexpr uart_port_t UART = static_cast<uart_port_t>(CONFIG_PAIRING_TEST_UART_NUM);
constexpr gpio_num_t TX_PIN = static_cast<gpio_num_t>(CONFIG_PAIRING_TEST_TX_PIN);
constexpr gpio_num_t RX_PIN = static_cast<gpio_num_t>(CONFIG_PAIRING_TEST_RX_PIN);
constexpr int BAUD = CONFIG_PAIRING_TEST_BAUD;
constexpr gpio_num_t BOOT_PIN = GPIO_NUM_0;  // the BOOT button, active low

constexpr int64_t STATUS_PERIOD_US = 10 * 1000000;

// Static, not on the task stack.
uint8_t s_rx[128];
uint8_t s_frame[uartlink::MAX_FRAME];

void hex8(const uint8_t* p, char out[17])
{
    static const char* d = "0123456789abcdef";
    for (int i = 0; i < 8; ++i) {
        out[2 * i] = d[p[i] >> 4];
        out[2 * i + 1] = d[p[i] & 15];
    }
    out[16] = '\0';
}

// Integer microseconds -> "12.3" (one decimal), without needing float printf.
struct Ms {
    int whole;
    int tenth;
};
Ms to_ms(int64_t us)
{
    const int64_t t = us / 100;  // tenths of a millisecond
    return {static_cast<int>(t / 10), static_cast<int>(t % 10)};
}

class LinkIo : public pairtest::Io {
public:
    pairtest::Peer* peer = nullptr;
    int64_t attempt_start_us = 0;

    void send(const uint8_t* payload, size_t n) override
    {
        const size_t m = uartlink::encode(payload, n, s_frame, sizeof s_frame);
        if (m != 0) {
            uart_write_bytes(UART, s_frame, m);
        }
    }

    void event(pairtest::Event ev) override
    {
        const int64_t now = esp_timer_get_time();
        if (ev == pairtest::Event::SentMsg1 || ev == pairtest::Event::GotMsg1) {
            attempt_start_us = now;
        }
        const Ms t = to_ms(now - attempt_start_us);
        ESP_LOGI(TAG, "+%4d.%d ms  %s", t.whole, t.tenth, pairtest::event_name(ev));

        if (ev == pairtest::Event::SasReady && peer != nullptr && peer->code() != nullptr) {
            const char* c = peer->code();
            ESP_LOGI(TAG, "==============================================");
            ESP_LOGI(TAG, "   PAIRING CODE:  %.3s %.3s", c, c + 3);
            ESP_LOGI(TAG, "   It must be the SAME on both boards.");
            ESP_LOGI(TAG, "==============================================");
        } else if (ev == pairtest::Event::SessionOk && peer != nullptr && peer->peer_static() != nullptr) {
            char k[17];
            hex8(peer->peer_static(), k);
            ESP_LOGI(TAG, "Session works (encrypted HELLO/HELLO_ACK exchanged). Other board's key starts %s", k);
            ESP_LOGI(TAG, "Pairing took %d.%d ms from the first handshake message (attempt %u).", t.whole, t.tenth,
                     static_cast<unsigned>(peer->attempts()));
        } else if (ev == pairtest::Event::Failed && peer != nullptr) {
            ESP_LOGE(TAG, "Pairing failed: %s. Press BOOT to try again.", pairtest::fail_name(peer->fail_reason()));
        }
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

// Debounced BOOT button: true once per press.
bool boot_pressed()
{
    static int low_polls = 0;
    if (gpio_get_level(BOOT_PIN) == 0) {
        ++low_polls;
    } else {
        low_polls = 0;
    }
    return low_polls == 3;
}

// X25519 timing: what a pairing costs in CPU on this chip.
void measure_x25519()
{
    uint8_t sk[kk::crypto::kKeyLen], pk[kk::crypto::kKeyLen], sk2[kk::crypto::kKeyLen], pk2[kk::crypto::kKeyLen];
    uint8_t out[kk::crypto::kKeyLen];
    constexpr int N = 8;

    int64_t t0 = esp_timer_get_time();
    bool ok = true;
    for (int i = 0; i < N; ++i) ok = kk::crypto::x25519_keygen(sk, pk) && ok;
    const int64_t keygen_us = (esp_timer_get_time() - t0) / N;

    ok = kk::crypto::x25519_keygen(sk2, pk2) && ok;
    t0 = esp_timer_get_time();
    for (int i = 0; i < N; ++i) ok = kk::crypto::x25519(out, sk, pk2) && ok;
    const int64_t dh_us = (esp_timer_get_time() - t0) / N;

    if (!ok) {
        ESP_LOGE(TAG, "X25519 failed during timing");
        return;
    }
    const Ms k = to_ms(keygen_us), d = to_ms(dh_us);
    ESP_LOGI(TAG, "X25519: key generation %d.%d ms, shared secret %d.%d ms (one pairing is about 4 such operations per board)",
             k.whole, k.tenth, d.whole, d.tenth);
}

}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Pairing test over UART%d (TX=GPIO%d RX=GPIO%d, %d baud)", static_cast<int>(UART),
             static_cast<int>(TX_PIN), static_cast<int>(RX_PIN), BAUD);

    // 1. Does the crypto the library needs exist in this build, and is it correct?
    const int64_t t0 = esp_timer_get_time();
    const char* bad = kk::crypto::selftest();
    const Ms st = to_ms(esp_timer_get_time() - t0);
    if (bad != nullptr) {
        ESP_LOGE(TAG, "kkproto self-test FAILED at: %s", bad);
        ESP_LOGE(TAG, "This mbedTLS build lacks or breaks that primitive. See sdkconfig.defaults for the switches.");
        vTaskSuspend(nullptr);
    }
    ESP_LOGI(TAG, "kkproto self-test: PASS (%d.%d ms)", st.whole, st.tenth);
    measure_x25519();

    // 2. This board's long-term key. Fresh on every boot: this test does not store anything.
    static kk::noise::KeyPair key;
    if (!kk::crypto::x25519_keygen(key.sk, key.pk)) {
        ESP_LOGE(TAG, "key generation failed");
        vTaskSuspend(nullptr);
    }
    char mykey[17];
    hex8(key.pk, mykey);
    ESP_LOGI(TAG, "This board's key starts %s (the other board will report it as 'other board's key')", mykey);

    if (!init_uart()) {
        ESP_LOGE(TAG, "UART setup failed. Check the pin numbers in menuconfig.");
        vTaskSuspend(nullptr);
    }
    gpio_set_direction(BOOT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_PIN, GPIO_PULLUP_ONLY);

    static LinkIo io;
    pairtest::Peer::Params params;
    params.retry_ms = CONFIG_PAIRING_TEST_RETRY_MS;
    params.max_attempts = CONFIG_PAIRING_TEST_MAX_ATTEMPTS;
    static pairtest::Peer peer(io, key, params);
    io.peer = &peer;

    static uartlink::Parser parser;
    ESP_LOGI(TAG, "Ready. Press BOOT on ONE board to start pairing. The other board just waits.");

    int64_t next_status = esp_timer_get_time() + STATUS_PERIOD_US;
    for (;;) {
        const int n = uart_read_bytes(UART, s_rx, sizeof s_rx, pdMS_TO_TICKS(5));
        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        if (n > 0) {
            parser.feed(s_rx, static_cast<size_t>(n), [&](const uint8_t* payload, size_t len) {
                peer.on_payload(payload, len, now_ms);
            });
        }
        peer.tick(now_ms);

        if (boot_pressed()) {
            ESP_LOGI(TAG, "BOOT pressed: starting pairing as initiator");
            peer.start_as_initiator(now_ms);
        }

        if (esp_timer_get_time() >= next_status) {
            next_status += STATUS_PERIOD_US;
            ESP_LOGI(TAG, "[%s] link: %" PRIu32 " frames ok, %" PRIu32 " bad CRC, %" PRIu32 " junk bytes", pairtest::phase_name(peer.phase()),
                     parser.frames_ok, parser.bad_crc, parser.junk_bytes);
        }
    }
}
