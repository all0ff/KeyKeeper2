#include "app_system/system_state.hpp"

namespace app_system::state {

namespace {

Snapshot g_snapshot{};

} // namespace

const Snapshot& snapshot()
{
    return g_snapshot;
}

void set_boot_stage(BootStage stage)
{
    g_snapshot.boot_stage = stage;
}

void set_runtime(RuntimeState runtime)
{
    g_snapshot.runtime = runtime;
}

void set_ready()
{
    g_snapshot.boot_stage = BootStage::Ready;
    g_snapshot.runtime = RuntimeState::Ready;
    g_snapshot.initialized = true;
    g_snapshot.ready = true;
}

void report_error(uint32_t flag)
{
    g_snapshot.error_flags |= flag;
    g_snapshot.runtime = RuntimeState::Error;
    g_snapshot.ready = false;
}

void clear_error(uint32_t flag)
{
    g_snapshot.error_flags &= ~flag;

    if (g_snapshot.error_flags == 0 && g_snapshot.initialized) {
        g_snapshot.ready = true;

        if (g_snapshot.runtime == RuntimeState::Error) {
            g_snapshot.runtime = RuntimeState::Ready;
        }
    }
}

void reset()
{
    g_snapshot = Snapshot{};
}

} // namespace app_system::state