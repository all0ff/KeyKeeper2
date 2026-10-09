#include "blelink/blelink.hpp"

// Built when NimBLE is not enabled: every call does nothing, start() fails.
namespace blelink {
bool start(Role, const char*) { return false; }
bool started() { return false; }
bool send(const uint8_t*, size_t) { return false; }
int recv(uint8_t*, size_t, TickType_t) { return 0; }
bool connected() { return false; }
uint32_t epoch() { return 0; }
void disconnect() {}
void set_pairing_open(bool) {}
void scan_pairing() {}
void scan_any() {}
void scan_addr(const Addr&) {}
void stop() {}
bool peer_addr(Addr*) { return false; }
} // namespace blelink
