/*
 * Real application entry point -- replaces the old smoke-test main.
 * Everything that used to be inlined here (bsp/display/input/power/
 * storage/event_bus/settings/security/vault/ui init, in the right
 * order, with per-stage error reporting) now lives in
 * components/app_system -- see app_system.hpp for the full
 * initialization order diagram and system_state.hpp for the runtime
 * state machine.
 */

#include "app_system/app_system.hpp"

extern "C" void app_main(void)
{
    app_system::init();

    // Nothing else to do here: every subsystem that needs to keep
    // running (input, power, event_bus, ui, ...) owns its own
    // FreeRTOS task already. app_main can return.
}
