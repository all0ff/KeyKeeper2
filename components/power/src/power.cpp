#include "power/power.hpp"

#include "power_manager.hpp"

namespace power {

bool init(const Config& cfg)
{
    return internal::instance().init(cfg);
}

bool is_initialized()
{
    return internal::instance().is_initialized();
}

State state()
{
    return internal::instance().state();
}

void notify_activity()
{
    internal::instance().notify_activity();
}

int register_callback(Callback cb, void* ctx)
{
    return internal::instance().register_callback(cb, ctx);
}

void unregister_callback(int handle)
{
    internal::instance().unregister_callback(handle);
}

void request_sleep()
{
    internal::instance().request_sleep();
}

void request_shutdown()
{
    internal::instance().request_shutdown();
}

} // namespace power
