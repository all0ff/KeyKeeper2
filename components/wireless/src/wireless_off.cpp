// Stub used when the protocol library is not built (CONFIG_KEYKEEPER_KKPROTO off): the menu entry still
// works and says that wireless typing is not in this build.
#include "wireless/wireless.hpp"

namespace wireless {

bool init() { return true; }
void start_radio() {}
void set_peer_language(uint8_t) {}
bool supported() { return false; }
bool enabled() { return false; }
bool set_enabled(bool) { return false; }
Status status()
{
    Status s;
    s.phase = Phase::Unsupported;
    return s;
}
bool start_pairing() { return false; }
void confirm(bool) {}
void cancel_pairing() {}
bool forget() { return false; }
void clear_outcome() {}

} // namespace wireless
