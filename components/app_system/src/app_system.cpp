#include "app_system/app_system.hpp"

#include "bsp/bsp.hpp"
#include "display/display.hpp"
#include "display/lvgl_port.hpp"
#include "imu/imu.hpp"
#include "event_bus/event_bus.hpp"
#include "input/input.hpp"
#include "interfaces/status/system_status.hpp"
#include "power/power.hpp"
#include "security/lock_manager.hpp"
#include "security/security_service.hpp"
#include "settings/settings.hpp"
#include "storage/storage.hpp"
#include "app_system/logger.hpp"
#include "vault/vault.hpp"
#include "ui/ui.hpp"
#include "usb/usb_service.hpp"
#include "rtc_time/rtc_time.hpp"
#include "wifi/wifi_service.hpp"
#include "web/web_service.hpp"

#include "esp_log.h"

namespace app_system {

// DIAGNOSTIC -- set true, rebuild, and reflash to skip USB HID
// entirely (usb::init() never runs) while investigating anything that
// needs an UNINTERRUPTED serial log across the point where USB
// normally re-enumerates from plain CDC to composite CDC+HID -- that
// re-enumeration is what makes idf.py monitor visibly drop and
// reconnect at "Boot stage: USB" on every normal boot (a real,
// separate, already-understood quirk of this board's single USB
// peripheral serving both roles -- not a bug), which briefly loses
// whatever log lines happen to print right around it. Sending a
// password or TOTP code over USB obviously won't work while this is
// true. Meant to be flipped back to false again once whatever's being
// investigated is done -- not a permanent setting, so it's a
// constexpr here rather than a proper settings::/Kconfig option.
constexpr bool DIAGNOSTIC_SKIP_USB_HID = true;

namespace {

constexpr char TAG[] = "system";

bool g_initialized = false;

void report_failure(
    state::BootStage stage,
    uint32_t error_flag,
    const char* message)
{
    state::set_boot_stage(stage);
    state::report_error(error_flag);

    logger::error(message);
}

void on_power_state_changed(power::State new_state, void* /*ctx*/)
{
    switch (new_state) {
        case power::State::Active:
            if (security::lock::state() == security::lock::State::Locked) {
                state::set_runtime(state::RuntimeState::Locked);
            } else {
                state::set_runtime(state::RuntimeState::Unlocked);
            }
            break;

        case power::State::LightSleep:
            state::set_runtime(state::RuntimeState::LightSleep);
            break;

        case power::State::DeepSleep:
            state::set_runtime(state::RuntimeState::DeepSleep);
            break;
    }

    logger::state(
        static_cast<uint8_t>(state::snapshot().runtime),
        state::snapshot().ready,
        state::snapshot().error_flags
    );
}

void on_system_event(const event_bus::Event& event, void* /*ctx*/)
{
    if (event.category != event_bus::Category::System) {
        return;
    }

    switch (static_cast<event_bus::SystemEventId>(event.id)) {
        case event_bus::SystemEventId::DeviceLocked:
            state::set_runtime(state::RuntimeState::Locked);
            break;

        case event_bus::SystemEventId::DeviceUnlocked:
            state::set_runtime(state::RuntimeState::Unlocked);
            break;

        case event_bus::SystemEventId::ErrorReported:
            state::report_error(event.payload.u32);
            break;

        case event_bus::SystemEventId::ErrorCleared:
            state::clear_error(event.payload.u32);
            break;

        case event_bus::SystemEventId::BootComplete:
        case event_bus::SystemEventId::SettingsChanged:
        case event_bus::SystemEventId::PinChanged:
            break;
    }

    logger::state(
        static_cast<uint8_t>(state::snapshot().runtime),
        state::snapshot().ready,
        state::snapshot().error_flags
    );
}

bool initialize_bsp()
{
    state::set_boot_stage(state::BootStage::Bsp);
    logger::boot_stage("BSP");

    if (!bsp::init()) {
        report_failure(
            state::BootStage::Bsp,
            interfaces::status::ERROR_BSP,
            "BSP initialization failed"
        );
        return false;
    }

    return true;
}

bool initialize_display()
{
    state::set_boot_stage(state::BootStage::Display);
    logger::boot_stage("Display");

    if (!display::init()) {
        report_failure(
            state::BootStage::Display,
            interfaces::status::ERROR_DISPLAY,
            "Display initialization failed"
        );
        return false;
    }

    display::set_backlight(true);
    return true;
}

bool initialize_lvgl()
{
    state::set_boot_stage(state::BootStage::Lvgl);
    logger::boot_stage("LVGL");

    if (!lvgl_port::init()) {
        report_failure(
            state::BootStage::Lvgl,
            interfaces::status::ERROR_DISPLAY,
            "LVGL initialization failed"
        );
        return false;
    }

    return true;
}

bool initialize_input()
{
    state::set_boot_stage(state::BootStage::Input);
    logger::boot_stage("Input");

    if (!input::init()) {
        report_failure(
            state::BootStage::Input,
            interfaces::status::ERROR_INPUT,
            "Input initialization failed"
        );
        return false;
    }

    return true;
}

bool initialize_power()
{
    state::set_boot_stage(state::BootStage::Power);
    logger::boot_stage("Power");

    const settings::SecuritySettings& security_settings =
        settings::all().security;

    power::Config config{};
    config.idle_timeout_ms =
        security_settings.auto_lock_enabled
            ? security_settings.auto_lock_timeout_s * 1000u
            : 0;

    if (!power::init(config)) {
        report_failure(
            state::BootStage::Power,
            interfaces::status::ERROR_POWER,
            "Power initialization failed"
        );
        return false;
    }

    power::register_callback(on_power_state_changed, nullptr);
    return true;
}

bool initialize_storage()
{
    state::set_boot_stage(state::BootStage::Storage);
    logger::boot_stage("Storage");

    if (!storage::init()) {
        report_failure(
            state::BootStage::Storage,
            interfaces::status::ERROR_STORAGE,
            "Storage initialization failed"
        );
        return false;
    }

    return true;
}

bool initialize_event_bus()
{
    state::set_boot_stage(state::BootStage::EventBus);
    logger::boot_stage("EventBus");

    if (!event_bus::init()) {
        report_failure(
            state::BootStage::EventBus,
            0,
            "EventBus initialization failed"
        );
        return false;
    }

    if (event_bus::subscribe(
            event_bus::Category::System,
            on_system_event,
            nullptr) < 0) {
        report_failure(
            state::BootStage::EventBus,
            0,
            "EventBus System subscription failed"
        );
        return false;
    }

    return true;
}

bool initialize_settings()
{
    state::set_boot_stage(state::BootStage::Settings);
    logger::boot_stage("Settings");

    if (!settings::init()) {
        report_failure(
            state::BootStage::Settings,
            0,
            "Settings initialization failed"
        );
        return false;
    }

    display::set_brightness(
        settings::all().general.display_brightness
    );

    return true;
}

bool initialize_wifi()
{
    state::set_boot_stage(state::BootStage::Wifi);
    logger::boot_stage("Wifi");

    if (!wifi::init()) {
        report_failure(
            state::BootStage::Wifi,
            0,
            "WiFi initialization failed"
        );
        return false;
    }

    // rtc_time::init() must come AFTER wifi::init(), not before --
    // real bug, confirmed on real hardware: wifi::init() is what
    // actually calls esp_netif_init() + esp_event_loop_create_default(),
    // and rtc_time::init()'s own esp_event_handler_register() call
    // needs that default event loop to already exist. Called first
    // (the original ordering here), it failed outright every boot
    // ("Failed to register SNTP sync event handler" in the serial
    // log) -- not fatal on its own (TOTP just silently stayed
    // unavailable, matching what totp::generate() reports when
    // rtc_time::is_synced() is false), but a real, now-fixed defect,
    // not a design choice.
    if (!rtc_time::init()) {
        logger::error("rtc_time::init() failed -- TOTP codes will be unavailable");
    }

    // Brings up whatever mode was saved from a previous session
    // (Disabled by default on first boot) -- not a hard failure if
    // this doesn't succeed (e.g. a saved network is out of range):
    // wifi::init() itself already succeeded, and the user can retry
    // or change settings from the WiFi settings screen once one
    // exists.
    if (!wifi::apply_settings()) {
        logger::error("WiFi apply_settings() did not start the configured mode");
    }

    return true;
}

bool initialize_web()
{
    state::set_boot_stage(state::BootStage::Web);
    logger::boot_stage("Web");

    if (!web::init()) {
        report_failure(
            state::BootStage::Web,
            0,
            "Web initialization failed"
        );
        return false;
    }

    // Placed after Security/Vault, not right after WiFi: the login
    // handler's WipeRequired path calls vault::repository::wipe() and
    // security::pin::wipe() directly, so both must already be ready
    // before the HTTP server can possibly receive a login request.
    if (!web::start()) {
        logger::error("Web start() failed -- HTTP server not running");
    }

    return true;
}

bool initialize_security()
{
    state::set_boot_stage(state::BootStage::Security);
    logger::boot_stage("Security");

    if (!security::init()) {
        report_failure(
            state::BootStage::Security,
            0,
            "Security initialization failed"
        );
        return false;
    }

    return true;
}

bool initialize_vault()
{
    state::set_boot_stage(state::BootStage::Vault);
    logger::boot_stage("Vault");

    if (!vault::init()) {
        report_failure(
            state::BootStage::Vault,
            0,
            "Vault initialization failed"
        );
        return false;
    }

    return true;
}

bool initialize_usb()
{
    state::set_boot_stage(state::BootStage::Usb);
    logger::boot_stage("USB");

    if (DIAGNOSTIC_SKIP_USB_HID) {
        ESP_LOGW(TAG, "DIAGNOSTIC_SKIP_USB_HID is true -- usb::init() skipped, "
                       "Print/Generate&Type actions will not work until this is flipped back");
        return true;
    }

    if (!usb::init()) {
        report_failure(
            state::BootStage::Usb,
            0,
            "USB initialization failed"
        );
        return false;
    }

    return true;
}

bool initialize_ui()
{
    state::set_boot_stage(state::BootStage::Ui);
    logger::boot_stage("UI");

    if (!ui::init()) {
        report_failure(
            state::BootStage::Ui,
            0,
            "UI initialization failed"
        );
        return false;
    }

    return true;
}

} // namespace

bool init()
{
    if (g_initialized) {
        return true;
    }

    logger::init();

    ESP_LOGI(TAG, "KeyKeeper2 system initialization started");

    /*
     * BSP
     */
    if (!initialize_bsp()) {
        return false;
    }

    /*
     * Display
     */
    if (!initialize_display()) {
        return false;
    }

    /*
     * LVGL
     */
    if (!initialize_lvgl()) {
        return false;
    }

    /*
     * Input
     */
    if (!initialize_input()) {
        return false;
    }

    /*
     * Storage must be initialized before Settings and Security.
     *
     * Power itself only needs Input, but its final configuration is
     * derived from persisted security settings. Therefore the actual
     * power initialization is intentionally performed after Settings.
     *
     * The physical dependency remains:
     *
     *     Input -> Power
     *
     * while the configuration dependency is:
     *
     *     Storage -> Settings -> Power
     */
    if (!initialize_storage()) {
        return false;
    }

    /*
     * EventBus
     */
    if (!initialize_event_bus()) {
        return false;
    }

    /*
     * Settings
     */
    if (!initialize_settings()) {
        return false;
    }

    // lvgl_port::set_rotation() -- re-enabled here after being
    // reverted (see git history/that function's own comment): the
    // ORIGINAL implementation used LVGL's own software rotation
    // (lv_display_set_rotation()), which caused a confirmed
    // blank/dark-display regression on real hardware, suspected due
    // to this display's partial (40-line) LVGL draw buffer being
    // incompatible with the full-frame buffer software rotation
    // typically needs. Re-implemented to rotate at the PANEL level
    // instead (ST7789's own MADCTL register, via LovyanGFX) -- LVGL's
    // own buffering is untouched either way, so this doesn't carry
    // the same risk. Not yet confirmed on real hardware -- if the
    // screen goes blank again after this specific change, this call
    // (and the two others in GeneralSettingsScreen::save() and
    // imu::'s own auto-rotate task) is exactly what to revert again.
    //
    // IMU + orientation -- folded in right here rather than getting
    // its own BootStage, same reasoning as rtc_time:: (see
    // initialize_wifi()'s own comment) -- lightweight, and tightly
    // coupled to what it's immediately used for (applying the saved
    // settings::GeneralSettings::orientation). Not a hard failure if
    // no IMU is found (see imu::init()'s own comment) -- manual
    // 0/180 orientation still works either way, only Auto becomes
    // unavailable.
    imu::init();
    {
        const settings::Orientation orientation = settings::all().general.orientation;
        if (orientation == settings::Orientation::Auto) {
            if (!imu::start_auto_rotate()) {
                lvgl_port::set_rotation(false);
            }
        } else {
            lvgl_port::set_rotation(orientation == settings::Orientation::Rotate180);
        }
    }

    /*
     * WiFi -- needs settings:: (mode/credentials) and event_bus::
     * (state-change publishing), both already up by this point.
     * Deliberately NOT a hard failure gate for anything after it: a
     * failed connection attempt shouldn't prevent the rest of the
     * device from working (see initialize_wifi()'s own comment).
     */
    if (!initialize_wifi()) {
        return false;
    }

    /*
     * Power
     */
    if (!initialize_power()) {
        return false;
    }

    /*
     * Refresh the unified low-level status after the hardware
     * components are initialized.
     */
    interfaces::status::refresh();

    /*
     * Security
     */
    if (!initialize_security()) {
        return false;
    }

    /*
     * Vault
     */
    if (!initialize_vault()) {
        return false;
    }

    /*
     * Web -- needs security:: and vault:: ready first (the login
     * handler's automatic-wipe path calls into both directly).
     */
    if (!initialize_web()) {
        return false;
    }

    /*
     * USB HID
     */
    if (!initialize_usb()) {
        return false;
    }

    /*
     * UI
     */
    if (!initialize_ui()) {
        return false;
    }

    /*
     * The device must start locked.
     *
     * security::lock::init() already starts in Locked state. We do
     * not call unlock() here and deliberately do not modify that
     * state.
     */
    if (security::lock::state() == security::lock::State::Locked) {
        state::set_runtime(state::RuntimeState::Locked);
    } else {
        state::set_runtime(state::RuntimeState::Unlocked);
    }

    state::set_ready();

    /*
     * set_ready() represents application readiness. The runtime state
     * must remain Locked when the device starts locked.
     */
    state::set_runtime(
        security::lock::state() == security::lock::State::Locked
            ? state::RuntimeState::Locked
            : state::RuntimeState::Unlocked
    );

    g_initialized = true;

    logger::ready();

    const state::Snapshot& current = state::snapshot();

    logger::state(
        static_cast<uint8_t>(current.runtime),
        current.ready,
        current.error_flags
    );

    ESP_LOGI(TAG, "KeyKeeper2 system initialization complete");

    /*
     * Publish BootComplete only after every mandatory component has
     * reached its initialized state.
     */
    if (event_bus::is_initialized()) {
        event_bus::Payload payload{};
        event_bus::publish(
            event_bus::Category::System,
            static_cast<uint32_t>(
                event_bus::SystemEventId::BootComplete
            ),
            payload
        );
    }

    return true;
}

bool is_initialized()
{
    return g_initialized;
}

state::RuntimeState runtime_state()
{
    return state::snapshot().runtime;
}

const state::Snapshot& snapshot()
{
    return state::snapshot();
}

void shutdown()
{
    /*
     * The System layer does not implement sleep/shutdown itself.
     * Power owns the ESP32 power-management mechanism.
     */
    power::request_shutdown();
}

void sleep()
{
    power::request_sleep();
}

} // namespace app_system