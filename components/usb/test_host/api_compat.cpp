// Compile-only check (never run): the public TypeEngine API is used by usb_service.cpp
// exactly like this. If this stops compiling, usb_service.cpp would stop compiling too.
#include "usb/type_engine.hpp"

#include <memory>
#include <string>

namespace {
struct TypeTaskParams {                  // mirrors usb_service.cpp
    std::string text;
    usb::TypeEngine::Timing timing;
};
[[maybe_unused]] size_t use(usb::TypeEngine& engine, const std::string& text, const usb::TypeEngine::Timing& timing)
{
    auto params = std::make_unique<TypeTaskParams>(TypeTaskParams{text, timing});
    usb::TypeEngine::Timing defaults{};
    const size_t sent = engine.type_string(params->text, params->timing);
    const bool ok = engine.type_char('a', defaults) && engine.can_type();
    return ok ? sent + std::string(engine.last_error()).size() : sent;
}
} // namespace
