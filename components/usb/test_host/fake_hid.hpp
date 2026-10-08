#pragma once
#include <string>
// Recorder + fault injection shared by the oracle build, the new build and the unit tests.
struct FakeHid {
    std::string trace;          // "K<kc>/<mod>/<hold>;" per send_key, "D<ms>;" per delay
    int sends = 0;              // send_key calls so far
    int fail_send_at = -1;      // the Nth send_key (0-based) fails; -1 = never
    int connected_for_sends = -1; // is_connected() turns false after this many send_key calls; -1 = always
    bool connected_at_start = true;
    void reset() { *this = FakeHid(); }
};
extern FakeHid g_hid;
