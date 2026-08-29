#include "interfaces/status/system_status.hpp"

#include "bsp/bsp.hpp"
#include "display/display.hpp"
#include "input/input.hpp"
#include "power/power.hpp"
#include "storage/storage.hpp"

namespace interfaces::status {

namespace {

SystemStatus current{};
uint32_t sticky_error_flags = ErrorFlag::ERROR_NONE;

} // namespace

const SystemStatus& refresh()
{
    current.bsp_ready = bsp::is_initialized();
    current.display_ready = display::is_initialized();
    current.input_ready = input::is_initialized();
    current.power_ready = power::is_initialized();
    current.storage_ready = storage::is_initialized();

    current.sdcard_present = current.storage_ready && storage::status().sdcard_present;

    current.error_flags = sticky_error_flags;
    current.has_error = sticky_error_flags != ErrorFlag::ERROR_NONE;

    return current;
}

const SystemStatus& snapshot()
{
    return current;
}

void report_error(ErrorFlag flag)
{
    sticky_error_flags |= static_cast<uint32_t>(flag);
    current.error_flags = sticky_error_flags;
    current.has_error = sticky_error_flags != ErrorFlag::ERROR_NONE;
}

void clear_error(ErrorFlag flag)
{
    sticky_error_flags &= ~static_cast<uint32_t>(flag);
    current.error_flags = sticky_error_flags;
    current.has_error = sticky_error_flags != ErrorFlag::ERROR_NONE;
}

} // namespace interfaces::status
