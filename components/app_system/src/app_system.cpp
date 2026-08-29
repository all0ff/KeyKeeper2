#include "app_system/app_system.hpp"

#include "bsp/bsp.hpp"
#include "display/display.hpp"
#include "display/lvgl_port.hpp"
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

#include "esp_log.h"

namespace app_system {

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