// BLE link test: one source, two roles (Kconfig BLE_TEST_ROLE).
//   peripheral  = future dongle: advertises a GATT service, echoes every message back.
//   central     = future vault: scans, connects, negotiates MTU, subscribes, sends messages of
//                 various lengths (1..250 bytes, fragmented by ble_frag.hpp), checks the echo,
//                 prints round-trip statistics. After a disconnect it reconnects by itself.
// No link-layer pairing/bonding on purpose: KeyKeeper's own Noise channel provides the security.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ble_frag.hpp"

extern "C" void ble_store_config_init(void);

static const char* TAG = "ble_test";

#if CONFIG_BLE_TEST_ROLE_CENTRAL
static constexpr bool kCentral = true;
#else
static constexpr bool kCentral = false;
#endif

// ---- GATT identifiers (random 128-bit UUIDs, little-endian byte order as NimBLE wants) ----
static const ble_uuid128_t kSvcUuid = BLE_UUID128_INIT(0x01, 0x00, 0x4b, 0x4b, 0x32, 0x6b, 0x65, 0x79,
                                                       0x8e, 0x1a, 0x5d, 0x2c, 0x7f, 0x3b, 0xa1, 0xc0);
static const ble_uuid128_t kRxUuid = BLE_UUID128_INIT(0x02, 0x00, 0x4b, 0x4b, 0x32, 0x6b, 0x65, 0x79,
                                                      0x8e, 0x1a, 0x5d, 0x2c, 0x7f, 0x3b, 0xa1, 0xc0);  // central -> peripheral (write w/o response)
static const ble_uuid128_t kTxUuid = BLE_UUID128_INIT(0x03, 0x00, 0x4b, 0x4b, 0x32, 0x6b, 0x65, 0x79,
                                                      0x8e, 0x1a, 0x5d, 0x2c, 0x7f, 0x3b, 0xa1, 0xc0);  // peripheral -> central (notify)

// ---- shared state ----
static volatile uint16_t g_conn = BLE_HS_CONN_HANDLE_NONE;
static volatile uint16_t g_mtu = 23;
static volatile bool g_ready = false;  // link usable for messages
static uint16_t g_rx_handle = 0;       // central: remote RX value handle; peripheral: unused
static uint16_t g_tx_handle = 0;       // peripheral: own TX value handle; central: remote TX value handle
static uint8_t g_own_addr_type;
static uint8_t g_msg_id = 0;
static kk::ble::Reassembler g_reasm;

struct Msg {
    uint16_t len;
    uint8_t data[kk::ble::kMaxMsg];
};

static size_t att_payload() {
    const uint16_t m = g_mtu;
    return m > 23 ? m - 3 : 20;
}

// Sends one message as fragments (task context, not the NimBLE host task). true = all queued.
static bool send_message(const uint8_t* msg, size_t n) {
    kk::ble::Fragmenter f(msg, n, att_payload(), g_msg_id++);
    uint8_t buf[256];
    size_t len;
    while (f.next(buf, &len)) {
        for (int attempt = 0;; ++attempt) {
            if (!g_ready) return false;
            int rc;
            if (kCentral) {
                rc = ble_gattc_write_no_rsp_flat(g_conn, g_rx_handle, buf, len);
            } else {
                os_mbuf* om = ble_hs_mbuf_from_flat(buf, len);
                if (!om) { rc = BLE_HS_ENOMEM; }
                else { rc = ble_gatts_notify_custom(g_conn, g_tx_handle, om); }
            }
            if (rc == 0) break;
            if (rc != BLE_HS_ENOMEM || attempt > 400) {
                ESP_LOGW(TAG, "send fragment failed rc=%d", rc);
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(2));  // controller queue is full: wait for a connection event
        }
    }
    return true;
}

static void start_advertising();
static void start_scan();

// =============================================================================================
// PERIPHERAL
// =============================================================================================
static QueueHandle_t g_echo_q;
static uint32_t g_echoed = 0;

static int rx_access(uint16_t, uint16_t, ble_gatt_access_ctxt* ctxt, void*) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    uint8_t buf[256];
    uint16_t n = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof buf, &n) != 0) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (g_reasm.push(buf, n)) {
        static Msg m;  // host task only
        m.len = (uint16_t)g_reasm.size();
        std::memcpy(m.data, g_reasm.data(), m.len);
        xQueueSend(g_echo_q, &m, 0);
    }
    return 0;
}

static int tx_access(uint16_t, uint16_t, ble_gatt_access_ctxt*, void*) { return BLE_ATT_ERR_READ_NOT_PERMITTED; }

// All fields are listed on purpose: ESP-IDF builds with -Werror=missing-field-initializers.
static const ble_gatt_chr_def kChrs[] = {
    {.uuid = &kRxUuid.u,
     .access_cb = rx_access,
     .arg = nullptr,
     .descriptors = nullptr,
     .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE,
     .min_key_size = 0,
     .val_handle = nullptr,
     .cpfd = nullptr},
    {.uuid = &kTxUuid.u,
     .access_cb = tx_access,
     .arg = nullptr,
     .descriptors = nullptr,
     .flags = BLE_GATT_CHR_F_NOTIFY,
     .min_key_size = 0,
     .val_handle = &g_tx_handle,
     .cpfd = nullptr},
    {},
};
static const ble_gatt_svc_def kSvcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &kSvcUuid.u, .includes = nullptr, .characteristics = kChrs},
    {},
};

static void echo_task(void*) {
    static Msg m;
    uint32_t last_echoed = 0;
    for (;;) {
        if (xQueueReceive(g_echo_q, &m, pdMS_TO_TICKS(5000)) != pdTRUE) {
            // heartbeat: shows whether the link is alive and whether messages still arrive
            ESP_LOGI(TAG, "alive: link=%s echoed=%u (+%u in 5 s) reasm: msgs=%u drops=%u", g_ready ? "UP" : "down", (unsigned)g_echoed,
                     (unsigned)(g_echoed - last_echoed), (unsigned)g_reasm.stats().messages, (unsigned)g_reasm.stats().dropped);
            last_echoed = g_echoed;
            continue;
        }
        if (send_message(m.data, m.len)) ++g_echoed;
        if (g_echoed % 50 == 0 && g_echoed) ESP_LOGI(TAG, "echoed %u messages (att payload %u)", (unsigned)g_echoed, (unsigned)att_payload());
    }
}

// =============================================================================================
// CENTRAL
// =============================================================================================
static SemaphoreHandle_t g_echo_sem;
static Msg g_echo_msg;  // last complete message from the peripheral
static uint16_t g_svc_start, g_svc_end;
static int64_t g_t_disc;  // time of last disconnect / start, for the "link up in" report

static int gap_event(ble_gap_event* ev, void*);

static int on_subscribed(uint16_t, const ble_gatt_error* err, ble_gatt_attr*, void*) {
    if (err->status != 0) {
        ESP_LOGE(TAG, "subscribe failed status=%d", err->status);
        ble_gap_terminate(g_conn, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    g_reasm.reset();
    g_ready = true;
    ESP_LOGI(TAG, "LINK UP: mtu=%u att_payload=%u, %lld ms since start/disconnect", (unsigned)g_mtu, (unsigned)att_payload(),
             (long long)((esp_timer_get_time() - g_t_disc) / 1000));
    return 0;
}

static int on_chr(uint16_t conn, const ble_gatt_error* err, const ble_gatt_chr* chr, void*) {
    if (err->status == 0 && chr) {
        if (ble_uuid_cmp(&chr->uuid.u, &kRxUuid.u) == 0) g_rx_handle = chr->val_handle;
        if (ble_uuid_cmp(&chr->uuid.u, &kTxUuid.u) == 0) g_tx_handle = chr->val_handle;
        return 0;
    }
    if (err->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "characteristic discovery failed status=%d", err->status);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    if (!g_rx_handle || !g_tx_handle) {
        ESP_LOGE(TAG, "service lacks the expected characteristics");
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    // The CCCD descriptor directly follows the value attribute of a NimBLE notify characteristic.
    const uint8_t on[2] = {0x01, 0x00};
    int rc = ble_gattc_write_flat(conn, g_tx_handle + 1, on, sizeof on, on_subscribed, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "subscribe write rc=%d", rc);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

static int on_svc(uint16_t conn, const ble_gatt_error* err, const ble_gatt_svc* svc, void*) {
    if (err->status == 0 && svc) {
        g_svc_start = svc->start_handle;
        g_svc_end = svc->end_handle;
        return 0;
    }
    if (err->status != BLE_HS_EDONE || g_svc_start == 0) {
        ESP_LOGE(TAG, "service not found (status=%d)", err->status);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    g_rx_handle = g_tx_handle = 0;
    int rc = ble_gattc_disc_all_chrs(conn, g_svc_start, g_svc_end, on_chr, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "disc chrs rc=%d", rc);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

static int on_mtu(uint16_t conn, const ble_gatt_error* err, uint16_t mtu, void*) {
    if (err->status == 0) g_mtu = mtu;
    ESP_LOGI(TAG, "MTU exchange status=%d mtu=%u", err->status, (unsigned)mtu);
    g_svc_start = g_svc_end = 0;
    int rc = ble_gattc_disc_svc_by_uuid(conn, &kSvcUuid.u, on_svc, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "disc svc rc=%d", rc);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

static void connect_to(const ble_addr_t* addr) {
    ble_gap_conn_params cp = {};
    cp.scan_itvl = 0x60;
    cp.scan_window = 0x30;
    cp.itvl_min = CONFIG_BLE_TEST_CONN_ITVL_MIN;
    cp.itvl_max = CONFIG_BLE_TEST_CONN_ITVL_MAX;
    cp.latency = 0;
    cp.supervision_timeout = 400;  // 4 s
    cp.min_ce_len = 0;
    cp.max_ce_len = 0;
    int rc = ble_gap_connect(g_own_addr_type, addr, 5000, &cp, gap_event, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "connect rc=%d", rc);
        start_scan();
    }
}

static bool adv_has_service(const ble_gap_disc_desc& d) {
    ble_hs_adv_fields f;
    if (ble_hs_adv_parse_fields(&f, d.data, d.length_data) != 0) return false;
    for (int i = 0; i < f.num_uuids128; ++i)
        if (ble_uuid_cmp(&f.uuids128[i].u, &kSvcUuid.u) == 0) return true;
    return false;
}

static void start_scan() {
    ble_gap_disc_params dp = {};
    dp.passive = 1;
    dp.filter_duplicates = 0;
    dp.itvl = 0x60;    // 60 ms
    dp.window = 0x30;  // 30 ms
    int rc = ble_gap_disc(g_own_addr_type, BLE_HS_FOREVER, &dp, gap_event, nullptr);
    if (rc != 0) ESP_LOGE(TAG, "scan rc=%d", rc);
}

static void central_test_task(void*) {
    static const uint16_t kSizes[] = {8, 20, 21, 96, 100, 180, 244, 250};
    uint32_t seq = 0, ok = 0, bad = 0, lost = 0;
    int64_t sum = 0, mn = INT64_MAX, mx = 0;
    static uint8_t msg[kk::ble::kMaxMsg];
    for (;;) {
        if (!g_ready) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        const uint16_t n = kSizes[seq % (sizeof kSizes / sizeof kSizes[0])];
        std::memcpy(msg, &seq, 4);
        for (uint16_t i = 4; i < n; ++i) msg[i] = (uint8_t)((seq + i) * 31);
        xSemaphoreTake(g_echo_sem, 0);  // drop a stale echo
        const int64_t t0 = esp_timer_get_time();
        if (send_message(msg, n) && xSemaphoreTake(g_echo_sem, pdMS_TO_TICKS(2000)) == pdTRUE) {
            const int64_t dt = esp_timer_get_time() - t0;
            if (g_echo_msg.len == n && std::memcmp(g_echo_msg.data, msg, n) == 0) {
                ++ok;
                sum += dt;
                if (dt < mn) mn = dt;
                if (dt > mx) mx = dt;
            } else {
                ++bad;
                ESP_LOGW(TAG, "echo MISMATCH seq=%u sent %u got %u bytes", (unsigned)seq, (unsigned)n, (unsigned)g_echo_msg.len);
            }
        } else if (g_ready) {
            ++lost;
            ESP_LOGW(TAG, "echo TIMEOUT seq=%u len=%u", (unsigned)seq, (unsigned)n);
            g_reasm.reset();
        }
        ++seq;
        if (seq % 40 == 0 && ok) {
            ESP_LOGI(TAG, "sent=%u ok=%u bad=%u lost=%u  rtt ms: min=%.1f avg=%.1f max=%.1f  (mtu %u, reasm drops %u)", (unsigned)seq,
                     (unsigned)ok, (unsigned)bad, (unsigned)lost, mn / 1000.0, sum / 1000.0 / ok, mx / 1000.0, (unsigned)g_mtu,
                     (unsigned)g_reasm.stats().dropped);
        }
        if (CONFIG_BLE_TEST_PERIOD_MS) vTaskDelay(pdMS_TO_TICKS(CONFIG_BLE_TEST_PERIOD_MS));
    }
}

// =============================================================================================
// GAP (both roles)
// =============================================================================================
static int gap_event(ble_gap_event* ev, void*) {
    switch (ev->type) {
        case BLE_GAP_EVENT_DISC:
            if (kCentral && adv_has_service(ev->disc)) {
                ble_gap_disc_cancel();
                ESP_LOGI(TAG, "found the peripheral, connecting");
                connect_to(&ev->disc.addr);
            }
            return 0;

        case BLE_GAP_EVENT_CONNECT:
            if (ev->connect.status == 0) {
                g_conn = ev->connect.conn_handle;
                g_mtu = 23;
                ESP_LOGI(TAG, "connected, handle %u", (unsigned)g_conn);
                if (kCentral) {
                    int rc = ble_gattc_exchange_mtu(g_conn, on_mtu, nullptr);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "exchange mtu rc=%d", rc);
                        ble_gap_terminate(g_conn, BLE_ERR_REM_USER_CONN_TERM);
                    }
                }
            } else {
                ESP_LOGW(TAG, "connect failed status=%d", ev->connect.status);
                if (kCentral) start_scan(); else start_advertising();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGW(TAG, "disconnected, reason=%d", ev->disconnect.reason);
            g_ready = false;
            g_conn = BLE_HS_CONN_HANDLE_NONE;
            g_reasm.reset();
            g_t_disc = esp_timer_get_time();
            if (kCentral) start_scan(); else start_advertising();
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            if (!kCentral) start_advertising();
            return 0;

        case BLE_GAP_EVENT_MTU:
            g_mtu = ev->mtu.value;
            ESP_LOGI(TAG, "MTU event: %u", (unsigned)ev->mtu.value);
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            if (!kCentral && ev->subscribe.attr_handle == g_tx_handle) {
                g_reasm.reset();
                g_ready = ev->subscribe.cur_notify != 0;
                ESP_LOGI(TAG, "central %s notifications -> link %s (att payload %u)", g_ready ? "enabled" : "disabled",
                         g_ready ? "UP" : "down", (unsigned)att_payload());
            }
            return 0;

        case BLE_GAP_EVENT_NOTIFY_RX:
            if (kCentral && ev->notify_rx.attr_handle == g_tx_handle) {
                uint8_t buf[256];
                const uint16_t n = OS_MBUF_PKTLEN(ev->notify_rx.om);
                if (n <= sizeof buf && os_mbuf_copydata(ev->notify_rx.om, 0, n, buf) == 0 && g_reasm.push(buf, n)) {
                    g_echo_msg.len = (uint16_t)g_reasm.size();
                    std::memcpy(g_echo_msg.data, g_reasm.data(), g_echo_msg.len);
                    xSemaphoreGive(g_echo_sem);
                }
            }
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            return 0;

        default:
            return 0;
    }
}

static void start_advertising() {
    ble_hs_adv_fields f = {};
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.uuids128 = &kSvcUuid;
    f.num_uuids128 = 1;
    f.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&f);
    if (rc != 0) { ESP_LOGE(TAG, "adv fields rc=%d", rc); return; }

    ble_hs_adv_fields r = {};
    const char* name = ble_svc_gap_device_name();
    r.name = (const uint8_t*)name;
    r.name_len = (uint8_t)std::strlen(name);
    r.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&r);
    if (rc != 0) { ESP_LOGE(TAG, "adv rsp rc=%d", rc); return; }

    ble_gap_adv_params ap = {};
    ap.conn_mode = BLE_GAP_CONN_MODE_UND;
    ap.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ap.itvl_min = 0x30;  // 30 ms
    ap.itvl_max = 0x40;  // 40 ms
    rc = ble_gap_adv_start(g_own_addr_type, nullptr, BLE_HS_FOREVER, &ap, gap_event, nullptr);
    if (rc != 0) { ESP_LOGE(TAG, "adv start rc=%d", rc); return; }
    ESP_LOGI(TAG, "advertising");
}

static void on_reset(int reason) { ESP_LOGE(TAG, "host reset, reason=%d", reason); }

static void on_sync() {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
    if (rc != 0) { ESP_LOGE(TAG, "address rc=%d", rc); return; }
    g_t_disc = esp_timer_get_time();
    if (kCentral) start_scan(); else start_advertising();
}

static void host_task(void*) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "BLE link test, role: %s", kCentral ? "CENTRAL (vault)" : "PERIPHERAL (dongle)");

    ret = nimble_port_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init failed %d", (int)ret); return; }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_bonding = 0;  // no link-layer security: the app-level Noise channel does it
    ble_hs_cfg.sm_sc = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(kCentral ? "KK-vault-test" : "KK-dongle-test");

    if (kCentral) {
        g_echo_sem = xSemaphoreCreateBinary();
        xTaskCreate(central_test_task, "ble_test", 4096, nullptr, 5, nullptr);
    } else {
        int rc = ble_gatts_count_cfg(kSvcs);
        if (rc == 0) rc = ble_gatts_add_svcs(kSvcs);
        if (rc != 0) { ESP_LOGE(TAG, "gatt table rc=%d", rc); return; }
        g_echo_q = xQueueCreate(4, sizeof(Msg));
        xTaskCreate(echo_task, "ble_echo", 4096, nullptr, 5, nullptr);
    }

    ble_store_config_init();
    nimble_port_freertos_init(host_task);
}
