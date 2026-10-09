#include "blelink/blelink.hpp"

#include <atomic>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "blelink/frag.hpp"

extern "C" void ble_store_config_init(void);

namespace blelink {

namespace {

constexpr char TAG[] = "blelink";

// ---- GATT identifiers (little-endian byte order, as NimBLE wants). Same values as tools/ble_link_test.
const ble_uuid128_t kSvcUuid = BLE_UUID128_INIT(0x01, 0x00, 0x4b, 0x4b, 0x32, 0x6b, 0x65, 0x79,
                                                0x8e, 0x1a, 0x5d, 0x2c, 0x7f, 0x3b, 0xa1, 0xc0);
const ble_uuid128_t kRxUuid = BLE_UUID128_INIT(0x02, 0x00, 0x4b, 0x4b, 0x32, 0x6b, 0x65, 0x79,
                                               0x8e, 0x1a, 0x5d, 0x2c, 0x7f, 0x3b, 0xa1, 0xc0);  // central -> peripheral
const ble_uuid128_t kTxUuid = BLE_UUID128_INIT(0x03, 0x00, 0x4b, 0x4b, 0x32, 0x6b, 0x65, 0x79,
                                               0x8e, 0x1a, 0x5d, 0x2c, 0x7f, 0x3b, 0xa1, 0xc0);  // peripheral -> central

// Advertising payload: flags + the service UUID + 3 bytes of manufacturer data {0xFF, 0xFF, flags}.
constexpr uint8_t kAdvPairingOpen = 0x01;

// Connection parameters the central asks for: 15..30 ms keeps the round trip near 50 ms.
constexpr uint16_t kConnItvlMin = 12;  // x 1.25 ms
constexpr uint16_t kConnItvlMax = 24;
constexpr uint16_t kConnSupervision = 400;  // x 10 ms = 4 s

constexpr size_t kQueueDepth = 8;

struct Msg {
    uint16_t len;
    uint8_t data[blelink::kMaxMsg];
};

enum class Mode : uint8_t { Idle, Pairing, Any, Addr };

// ---- state shared between the owner task and the NimBLE host task
Role g_role = Role::Peripheral;
std::atomic<bool> g_started{false};
std::atomic<bool> g_synced{false};
std::atomic<bool> g_ready{false};      // link usable for messages
std::atomic<uint16_t> g_conn{BLE_HS_CONN_HANDLE_NONE};
std::atomic<uint16_t> g_mtu{23};
std::atomic<uint32_t> g_epoch{0};
std::atomic<Mode> g_mode{Mode::Idle};  // central
std::atomic<bool> g_pairing_open{false};  // peripheral
std::atomic<bool> g_busy{false};       // central: scanning or connecting
uint8_t g_own_addr_type = 0;
uint16_t g_rx_handle = 0;  // central: remote RX value handle
uint16_t g_tx_handle = 0;  // peripheral: own TX value handle; central: remote TX value handle
uint16_t g_svc_start = 0, g_svc_end = 0;
uint8_t g_msg_id = 0;

SemaphoreHandle_t g_mu = nullptr;  // guards g_target, g_peer
ble_addr_t g_target = {};          // Mode::Addr
ble_addr_t g_peer = {};            // address of the current/last connection (central)
ble_addr_t g_connecting = {};

QueueHandle_t g_rxq = nullptr;
blelink::Reassembler g_reasm;  // host task only

size_t att_payload()
{
    const uint16_t m = g_mtu.load();
    return m > 23 ? m - 3 : 20;
}

void set_ready(bool r)
{
    if (g_ready.exchange(r) != r) {
        if (g_rxq != nullptr) {
            xQueueReset(g_rxq);  // nothing from an old link may reach a new session
        }
        g_reasm.reset();
        g_epoch.fetch_add(1);
    }
}

int gap_event(ble_gap_event* ev, void*);
void apply_mode();

// =============================================================================================
// Peripheral
// =============================================================================================
int rx_access(uint16_t, uint16_t, ble_gatt_access_ctxt* ctxt, void*)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t buf[256];
    uint16_t n = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof buf, &n) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (g_reasm.push(buf, n)) {
        static Msg m;  // host task only
        m.len = static_cast<uint16_t>(g_reasm.size());
        std::memcpy(m.data, g_reasm.data(), m.len);
        if (xQueueSend(g_rxq, &m, 0) != pdTRUE) {
            ESP_LOGW(TAG, "receive queue full: message dropped");
        }
    }
    return 0;
}

int tx_access(uint16_t, uint16_t, ble_gatt_access_ctxt*, void*)
{
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

// All fields are listed on purpose: ESP-IDF builds with -Werror=missing-field-initializers.
const ble_gatt_chr_def kChrs[] = {
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
const ble_gatt_svc_def kSvcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &kSvcUuid.u, .includes = nullptr, .characteristics = kChrs},
    {},
};

void start_advertising()
{
    if (!g_synced || g_conn.load() != BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    ble_gap_adv_stop();  // fields cannot change while advertising (error if not advertising: ignored)

    const uint8_t mfg[3] = {0xFF, 0xFF, static_cast<uint8_t>(g_pairing_open.load() ? kAdvPairingOpen : 0)};
    ble_hs_adv_fields f = {};
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.uuids128 = &kSvcUuid;
    f.num_uuids128 = 1;
    f.uuids128_is_complete = 1;
    f.mfg_data = mfg;
    f.mfg_data_len = sizeof mfg;
    int rc = ble_gap_adv_set_fields(&f);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv fields rc=%d", rc);
        return;
    }
    ble_hs_adv_fields r = {};
    const char* name = ble_svc_gap_device_name();
    r.name = reinterpret_cast<const uint8_t*>(name);
    r.name_len = static_cast<uint8_t>(std::strlen(name));
    r.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&r);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv rsp rc=%d", rc);
        return;
    }
    ble_gap_adv_params ap = {};
    ap.conn_mode = BLE_GAP_CONN_MODE_UND;
    ap.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ap.itvl_min = 0x30;  // 30 ms
    ap.itvl_max = 0x40;  // 40 ms
    rc = ble_gap_adv_start(g_own_addr_type, nullptr, BLE_HS_FOREVER, &ap, gap_event, nullptr);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv start rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising%s", g_pairing_open.load() ? " (pairing window open)" : "");
}

// =============================================================================================
// Central
// =============================================================================================
void connect_to(const ble_addr_t* addr);

int on_subscribed(uint16_t conn, const ble_gatt_error* err, ble_gatt_attr*, void*)
{
    if (err->status != 0) {
        ESP_LOGE(TAG, "subscribe failed status=%d", err->status);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    ESP_LOGI(TAG, "link up: mtu=%u att_payload=%u", static_cast<unsigned>(g_mtu.load()), static_cast<unsigned>(att_payload()));
    set_ready(true);
    return 0;
}

int on_chr(uint16_t conn, const ble_gatt_error* err, const ble_gatt_chr* chr, void*)
{
    if (err->status == 0 && chr != nullptr) {
        if (ble_uuid_cmp(&chr->uuid.u, &kRxUuid.u) == 0) g_rx_handle = chr->val_handle;
        if (ble_uuid_cmp(&chr->uuid.u, &kTxUuid.u) == 0) g_tx_handle = chr->val_handle;
        return 0;
    }
    if (err->status != BLE_HS_EDONE || g_rx_handle == 0 || g_tx_handle == 0) {
        ESP_LOGE(TAG, "characteristic discovery failed (status=%d)", err->status);
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    // The CCCD descriptor directly follows the value attribute of a NimBLE notify characteristic.
    const uint8_t on[2] = {0x01, 0x00};
    if (ble_gattc_write_flat(conn, g_tx_handle + 1, on, sizeof on, on_subscribed, nullptr) != 0) {
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

int on_svc(uint16_t conn, const ble_gatt_error* err, const ble_gatt_svc* svc, void*)
{
    if (err->status == 0 && svc != nullptr) {
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
    if (ble_gattc_disc_all_chrs(conn, g_svc_start, g_svc_end, on_chr, nullptr) != 0) {
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

int on_mtu(uint16_t conn, const ble_gatt_error* err, uint16_t mtu, void*)
{
    if (err->status == 0) {
        g_mtu = mtu;
    }
    g_svc_start = g_svc_end = 0;
    if (ble_gattc_disc_svc_by_uuid(conn, &kSvcUuid.u, on_svc, nullptr) != 0) {
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

bool adv_has_service(const ble_hs_adv_fields& f)
{
    for (int i = 0; i < f.num_uuids128; ++i) {
        if (ble_uuid_cmp(&f.uuids128[i].u, &kSvcUuid.u) == 0) {
            return true;
        }
    }
    return false;
}

bool want_device(const ble_gap_disc_desc& d)
{
    ble_hs_adv_fields f;
    if (ble_hs_adv_parse_fields(&f, d.data, d.length_data) != 0 || !adv_has_service(f)) {
        return false;
    }
    switch (g_mode.load()) {
    case Mode::Pairing:
        return f.mfg_data != nullptr && f.mfg_data_len >= 3 && (f.mfg_data[2] & kAdvPairingOpen) != 0;
    case Mode::Any:
        return true;
    case Mode::Addr: {
        xSemaphoreTake(g_mu, portMAX_DELAY);
        const bool same = d.addr.type == g_target.type && std::memcmp(d.addr.val, g_target.val, 6) == 0;
        xSemaphoreGive(g_mu);
        return same;
    }
    case Mode::Idle: break;
    }
    return false;
}

void connect_to(const ble_addr_t* addr)
{
    ble_gap_conn_params cp = {};
    cp.scan_itvl = 0x60;
    cp.scan_window = 0x30;
    cp.itvl_min = kConnItvlMin;
    cp.itvl_max = kConnItvlMax;
    cp.latency = 0;
    cp.supervision_timeout = kConnSupervision;
    cp.min_ce_len = 0;
    cp.max_ce_len = 0;
    xSemaphoreTake(g_mu, portMAX_DELAY);
    g_connecting = *addr;
    xSemaphoreGive(g_mu);
    const int rc = ble_gap_connect(g_own_addr_type, addr, 5000, &cp, gap_event, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "connect rc=%d", rc);
        g_busy = false;
        apply_mode();
    }
}

void start_scan()
{
    if (!g_synced || g_conn.load() != BLE_HS_CONN_HANDLE_NONE || g_mode.load() == Mode::Idle) {
        return;
    }
    if (g_busy.exchange(true)) {
        return;  // already scanning or connecting
    }
    ble_gap_disc_params dp = {};
    dp.passive = 1;
    dp.filter_duplicates = 0;
    dp.itvl = 0x60;    // 60 ms
    dp.window = 0x30;  // 30 ms
    const int rc = ble_gap_disc(g_own_addr_type, BLE_HS_FOREVER, &dp, gap_event, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan rc=%d", rc);
        g_busy = false;
    }
}

// Brings the radio in line with the requested mode (host or owner task).
void apply_mode()
{
    if (g_role == Role::Peripheral) {
        start_advertising();
        return;
    }
    if (g_mode.load() == Mode::Idle) {
        if (g_busy.exchange(false)) {
            ble_gap_disc_cancel();  // whichever of the two is running; the other call just fails
            ble_gap_conn_cancel();
        }
        return;
    }
    start_scan();
}

// =============================================================================================
// GAP events (host task)
// =============================================================================================
int gap_event(ble_gap_event* ev, void*)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_DISC:
        if (g_role == Role::Central && want_device(ev->disc)) {
            ble_gap_disc_cancel();
            ESP_LOGI(TAG, "dongle found, connecting");
            connect_to(&ev->disc.addr);  // g_busy stays set: connecting
        }
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        g_busy = false;
        apply_mode();
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status == 0) {
            g_conn = ev->connect.conn_handle;
            g_mtu = 23;
            g_busy = false;
            if (g_role == Role::Central) {
                xSemaphoreTake(g_mu, portMAX_DELAY);
                g_peer = g_connecting;
                xSemaphoreGive(g_mu);
                if (ble_gattc_exchange_mtu(g_conn.load(), on_mtu, nullptr) != 0) {
                    ble_gap_terminate(g_conn.load(), BLE_ERR_REM_USER_CONN_TERM);
                }
            }
            ESP_LOGI(TAG, "connected");
        } else {
            ESP_LOGW(TAG, "connect failed status=%d", ev->connect.status);
            g_busy = false;
            apply_mode();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "disconnected, reason=%d", ev->disconnect.reason);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        g_busy = false;
        set_ready(false);
        apply_mode();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        apply_mode();
        return 0;

    case BLE_GAP_EVENT_MTU:
        g_mtu = ev->mtu.value;
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (g_role == Role::Peripheral && ev->subscribe.attr_handle == g_tx_handle) {
            if (ev->subscribe.cur_notify != 0) {
                ESP_LOGI(TAG, "link up: att_payload=%u", static_cast<unsigned>(att_payload()));
            }
            set_ready(ev->subscribe.cur_notify != 0);
        }
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        if (g_role == Role::Central && ev->notify_rx.attr_handle == g_tx_handle) {
            uint8_t buf[256];
            const uint16_t n = OS_MBUF_PKTLEN(ev->notify_rx.om);
            if (n <= sizeof buf && os_mbuf_copydata(ev->notify_rx.om, 0, n, buf) == 0 && g_reasm.push(buf, n)) {
                static Msg m;  // host task only
                m.len = static_cast<uint16_t>(g_reasm.size());
                std::memcpy(m.data, g_reasm.data(), m.len);
                if (xQueueSend(g_rxq, &m, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "receive queue full: message dropped");
                }
            }
        }
        return 0;

    default:
        return 0;
    }
}

void on_reset(int reason)
{
    ESP_LOGE(TAG, "host reset, reason=%d", reason);
    g_synced = false;
}

void on_sync()
{
    ESP_LOGW(TAG, "host synced");
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "address rc=%d", rc);
        return;
    }
    g_synced = true;
    apply_mode();
}

void host_task(void*)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

} // namespace

// =============================================================================================
// Public API
// =============================================================================================
bool start(Role role, const char* device_name)
{
    if (g_started.load()) {
        return true;
    }
    g_role = role;
    g_mu = xSemaphoreCreateMutex();
    g_rxq = xQueueCreate(kQueueDepth, sizeof(Msg));
    if (g_mu == nullptr || g_rxq == nullptr) {
        return false;
    }
    const size_t heap_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const esp_err_t init_err = nimble_port_init();
    ESP_LOGW(TAG, "nimble_port_init: %s, internal heap %u -> %u bytes free (BLE took %d)", init_err == ESP_OK ? "ok" : "FAILED",
             static_cast<unsigned>(heap_before), static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<int>(heap_before) - static_cast<int>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed (%d)", static_cast<int>(init_err));
        return false;
    }
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_bonding = 0;  // no link-layer security: the Noise channel above does it
    ble_hs_cfg.sm_sc = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(device_name != nullptr ? device_name : "KeyKeeper");
    if (role == Role::Peripheral) {
        int rc = ble_gatts_count_cfg(kSvcs);
        if (rc == 0) {
            rc = ble_gatts_add_svcs(kSvcs);
        }
        if (rc != 0) {
            ESP_LOGE(TAG, "GATT table rc=%d", rc);
            return false;
        }
    }
    ble_store_config_init();
    nimble_port_freertos_init(host_task);
    g_started = true;
    ESP_LOGW(TAG, "BLE started as %s, internal heap now %u bytes free (lowest ever %u)", role == Role::Central ? "central" : "peripheral",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
    return true;
}

bool started() { return g_started.load(); }

bool send(const uint8_t* msg, size_t n)
{
    if (!g_started || n == 0 || n > blelink::kMaxMsg) {
        return false;
    }
    blelink::Fragmenter f(msg, n, att_payload(), g_msg_id++);
    uint8_t buf[256];
    size_t len;
    while (f.next(buf, &len)) {
        for (int attempt = 0;; ++attempt) {
            if (!g_ready.load()) {
                return false;
            }
            int rc;
            if (g_role == Role::Central) {
                rc = ble_gattc_write_no_rsp_flat(g_conn.load(), g_rx_handle, buf, static_cast<uint16_t>(len));
            } else {
                os_mbuf* om = ble_hs_mbuf_from_flat(buf, static_cast<uint16_t>(len));
                rc = om != nullptr ? ble_gatts_notify_custom(g_conn.load(), g_tx_handle, om) : BLE_HS_ENOMEM;
            }
            if (rc == 0) {
                break;
            }
            if (rc != BLE_HS_ENOMEM || attempt > 400) {
                ESP_LOGW(TAG, "send failed rc=%d", rc);
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(2));  // the controller queue is full: wait for a connection event
        }
    }
    return true;
}

int recv(uint8_t* buf, size_t cap, TickType_t wait)
{
    if (!g_started) {
        vTaskDelay(wait > 0 ? wait : 1);
        return 0;
    }
    static Msg m;  // owner task only
    if (xQueueReceive(g_rxq, &m, wait) != pdTRUE) {
        return 0;
    }
    if (m.len > cap) {
        return 0;
    }
    std::memcpy(buf, m.data, m.len);
    return m.len;
}

bool connected() { return g_ready.load(); }
uint32_t epoch() { return g_epoch.load(); }

void disconnect()
{
    const uint16_t c = g_conn.load();
    if (c != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(c, BLE_ERR_REM_USER_CONN_TERM);
    }
}

void set_pairing_open(bool open)
{
    if (g_pairing_open.exchange(open) != open) {
        apply_mode();  // restarts advertising with the new flag (not while connected)
    }
}

void scan_pairing()
{
    if (g_role != Role::Central) return;
    g_mode = Mode::Pairing;
    disconnect();  // a pairing starts from a fresh connection
    apply_mode();
}

void scan_any()
{
    if (g_role != Role::Central) return;
    g_mode = Mode::Any;
    apply_mode();
}

void scan_addr(const Addr& addr)
{
    if (g_role != Role::Central) return;
    xSemaphoreTake(g_mu, portMAX_DELAY);
    g_target.type = addr.type;
    std::memcpy(g_target.val, addr.val, 6);
    xSemaphoreGive(g_mu);
    g_mode = Mode::Addr;
    apply_mode();
}

void stop()
{
    if (g_role != Role::Central) return;
    g_mode = Mode::Idle;
    apply_mode();
    disconnect();
}

bool peer_addr(Addr* out)
{
    if (!g_ready.load() || out == nullptr) {
        return false;
    }
    xSemaphoreTake(g_mu, portMAX_DELAY);
    out->type = g_peer.type;
    std::memcpy(out->val, g_peer.val, 6);
    xSemaphoreGive(g_mu);
    return true;
}

} // namespace blelink
