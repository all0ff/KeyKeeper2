// UART link wiring test.
//
// Flash the SAME firmware to both boards and connect them: TX of one to RX of the other (crossed),
// plus GND. Each board sends a numbered ping every N ms and checks everything it receives. Once a
// second it prints one line saying whether the link works in both directions, and if not, what to
// check. All the logic (framing, counters, verdict) is in link_frame.hpp / link_stats.hpp and is
// unit-tested on the PC; this file only moves bytes between the UART and that logic.
//
// With a jumper from TX to RX on ONE board, the board hears itself and says "LOOPBACK": that
// checks the pins and the UART peripheral without a second board.

#include <cinttypes>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "link_frame.hpp"
#include "link_stats.hpp"

namespace {

const char* TAG = "link";

constexpr uart_port_t UART = static_cast<uart_port_t>(CONFIG_UART_LINK_TEST_UART_NUM);
constexpr gpio_num_t TX_PIN = static_cast<gpio_num_t>(CONFIG_UART_LINK_TEST_TX_PIN);
constexpr gpio_num_t RX_PIN = static_cast<gpio_num_t>(CONFIG_UART_LINK_TEST_RX_PIN);
constexpr int BAUD = CONFIG_UART_LINK_TEST_BAUD;
constexpr size_t PAYLOAD_LEN = CONFIG_UART_LINK_TEST_PAYLOAD_LEN;
constexpr int64_t TX_PERIOD_US = static_cast<int64_t>(CONFIG_UART_LINK_TEST_PERIOD_MS) * 1000;
constexpr int64_t STAT_PERIOD_US = 1000000;
constexpr int HINT_REPEAT_S = 10;  // repeat the explanation this often while the verdict stays the same

// Static, not on the task stack.
uint8_t s_rx[128];
uint8_t s_payload[uartlink::MAX_PAYLOAD];
uint8_t s_frame[uartlink::MAX_FRAME];

bool init_uart()
{
    uart_config_t cfg = {};
    cfg.baud_rate = BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(UART, 1024, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }
    err = uart_param_config(UART, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        return false;
    }
    err = uart_set_pin(UART, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        return false;
    }
    // An idle UART line is high. The pull-up keeps RX quiet while the other board is off or unplugged.
    gpio_set_pull_mode(RX_PIN, GPIO_PULLUP_ONLY);
    return true;
}

}  // namespace

extern "C" void app_main(void)
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    const uint16_t own_id = static_cast<uint16_t>((mac[4] << 8) | mac[5]);

    ESP_LOGI(TAG, "UART link test. This board id = %04X (last two bytes of its MAC)", own_id);
    ESP_LOGI(TAG, "UART%d  TX=GPIO%d  RX=GPIO%d  %d baud  payload %u bytes  one ping per %d ms",
             static_cast<int>(UART), static_cast<int>(TX_PIN), static_cast<int>(RX_PIN), BAUD,
             static_cast<unsigned>(PAYLOAD_LEN), CONFIG_UART_LINK_TEST_PERIOD_MS);
    ESP_LOGI(TAG, "Wire TX of one board to RX of the other (crossed) and connect GND. "
                  "Run the same firmware on both boards.");

    if (!init_uart()) {
        ESP_LOGE(TAG, "UART setup failed, stopping. Check the pin numbers in menuconfig.");
        vTaskSuspend(nullptr);
    }

    uartlink::Parser parser;
    uartlink::PeerTracker peer;
    uint32_t tx_seq = 0;
    uint32_t rx_bytes = 0;
    uint32_t own_frames = 0;    // frames carrying our own id: TX wired to RX on this board
    uint32_t bad_payload = 0;   // good CRC but wrong content (should stay 0)

    // Snapshot at the previous statistics line, to judge "what happened during the last second".
    uint32_t prev_bytes = 0;
    uint32_t prev_ok = 0;
    uint32_t prev_own = 0;
    uartlink::Verdict prev_verdict = uartlink::Verdict::NoData;
    bool have_prev_verdict = false;
    int seconds = 0;
    int seconds_since_hint = 0;

    const int64_t start = esp_timer_get_time();
    int64_t next_tx = start;
    int64_t next_stat = start + STAT_PERIOD_US;

    for (;;) {
        const int n = uart_read_bytes(UART, s_rx, sizeof s_rx, pdMS_TO_TICKS(5));
        if (n > 0) {
            rx_bytes += static_cast<uint32_t>(n);
            parser.feed(s_rx, static_cast<size_t>(n), [&](const uint8_t* payload, size_t len) {
                uartlink::Ping ping;
                if (uartlink::decode_ping(payload, len, ping) != uartlink::PingResult::Ok) {
                    ++bad_payload;
                    return;
                }
                if (ping.id == own_id) {
                    ++own_frames;
                    return;
                }
                peer.on_ping(ping);
            });
        }

        const int64_t now = esp_timer_get_time();

        if (now >= next_tx) {
            uartlink::Ping me;
            me.id = own_id;
            me.seq = tx_seq;
            me.echo = peer.have ? peer.last_seq : uartlink::NO_ECHO;
            const size_t plen = uartlink::encode_ping(me, PAYLOAD_LEN, s_payload);
            const size_t flen = uartlink::encode(s_payload, plen, s_frame, sizeof s_frame);
            if (flen != 0) {
                uart_write_bytes(UART, s_frame, flen);
                ++tx_seq;
            }
            next_tx += TX_PERIOD_US;
            if (next_tx < now) {
                next_tx = now + TX_PERIOD_US;  // we fell behind (should not happen): do not burst
            }
        }

        if (now >= next_stat) {
            next_stat += STAT_PERIOD_US;
            ++seconds;

            const uartlink::Verdict v = uartlink::classify(
                own_frames - prev_own, rx_bytes - prev_bytes, peer.ok - prev_ok, peer.sees_me(tx_seq));
            prev_own = own_frames;
            prev_bytes = rx_bytes;
            prev_ok = peer.ok;

            ESP_LOGI(TAG,
                     "[%3ds] tx=%" PRIu32 " | rx ok=%" PRIu32 " crc_err=%" PRIu32 " junk=%" PRIu32 " lost=%" PRIu32
                     " bytes=%" PRIu32 " | peer=%04X | %s",
                     seconds, tx_seq, peer.ok, parser.bad_crc, parser.junk_bytes, peer.lost, rx_bytes,
                     peer.have ? static_cast<unsigned>(peer.id) : 0u, uartlink::verdict_tag(v));

            if (bad_payload != 0) {
                ESP_LOGW(TAG, "%" PRIu32 " frames had a good CRC but wrong content: please report this", bad_payload);
            }
            if (peer.restarts != 0) {
                ESP_LOGI(TAG, "the other board restarted or was replaced %" PRIu32 " time(s)", peer.restarts);
            }

            ++seconds_since_hint;
            if (!have_prev_verdict || v != prev_verdict || seconds_since_hint >= HINT_REPEAT_S) {
                if (v == uartlink::Verdict::Ok) {
                    ESP_LOGI(TAG, "%s", uartlink::verdict_hint(v));
                } else {
                    ESP_LOGW(TAG, "%s", uartlink::verdict_hint(v));
                }
                seconds_since_hint = 0;
            }
            prev_verdict = v;
            have_prev_verdict = true;
        }
    }
}
