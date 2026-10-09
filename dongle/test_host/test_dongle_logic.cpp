// Host tests for the dongle's pure logic: button detector, button actions, status screen text.
#include "actions.hpp"
#include "button.hpp"
#include "status_model.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                                      \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) {                                                                   \
            ++g_failed;                                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                  \
        }                                                                                \
    } while (0)

using namespace dongle;
using kk::link::Role;
using kk::link::State;

namespace {

// Feeds a press of `hold_ms` starting at t0, polling every 10 ms; returns what came out (Short/Long/None counts).
struct Out {
    int shorts = 0, longs = 0;
    uint32_t long_at = 0;
};
Out press(ButtonDetector& b, uint32_t t0, uint32_t hold_ms, uint32_t tail_ms = 100)
{
    Out o;
    for (uint32_t t = 0; t <= hold_ms + tail_ms; t += 10) {
        const bool low = t < hold_ms;
        const Press p = b.feed(low, t0 + t);
        if (p == Press::Short) ++o.shorts;
        if (p == Press::Long) {
            ++o.longs;
            o.long_at = t;
        }
    }
    return o;
}

void test_button()
{
    {
        ButtonDetector b;
        Out o = press(b, 1000, 200);
        CHECK(o.shorts == 1 && o.longs == 0);
    }
    {
        ButtonDetector b;
        Out o = press(b, 1000, 10);  // contact bounce
        CHECK(o.shorts == 0 && o.longs == 0);
    }
    {
        ButtonDetector b;
        Out o = press(b, 1000, 2990);  // just under the long press
        CHECK(o.shorts == 1 && o.longs == 0);
    }
    {
        ButtonDetector b;
        Out o = press(b, 1000, 5000);  // held for long: one Long, fired at the threshold, none on release
        CHECK(o.shorts == 0 && o.longs == 1 && o.long_at >= 3000 && o.long_at <= 3010);
    }
    {
        ButtonDetector b;
        Out a = press(b, 1000, 200);
        Out c = press(b, 5000, 4000);
        Out d = press(b, 20000, 100);
        CHECK(a.shorts == 1 && c.longs == 1 && c.shorts == 0 && d.shorts == 1);
    }
    {
        ButtonDetector b(3000, 30);
        const uint32_t near_wrap = 0xFFFFFFFFu - 1000;  // the millisecond clock wraps during the press
        Out o = press(b, near_wrap, 4000);
        CHECK(o.longs == 1);
        ButtonDetector c;
        Out s = press(c, near_wrap, 200);
        CHECK(s.shorts == 1);
    }
    {
        ButtonDetector b;
        CHECK(b.held_ms(0) == 0 && !b.is_down());
        b.feed(true, 100);
        CHECK(b.is_down() && b.held_ms(1100) == 1000);
    }
}

void test_actions()
{
    auto act = [](Role r, State st, bool paired, bool window, bool local, Press p) {
        Situation s;
        s.role = r;
        s.state = st;
        s.paired = paired;
        s.window_open = window;
        s.local_confirmed = local;
        return action_for(s, p);
    };
    // nothing pressed
    CHECK(act(Role::Dongle, State::Idle, false, false, false, Press::None) == Action::None);
    // dongle, idle
    CHECK(act(Role::Dongle, State::Idle, false, false, false, Press::Short) == Action::OpenPairing);
    CHECK(act(Role::Dongle, State::Idle, true, false, false, Press::Short) == Action::OpenPairing);
    CHECK(act(Role::Dongle, State::Idle, false, true, false, Press::Short) == Action::CancelPairing);
    CHECK(act(Role::Dongle, State::Idle, true, false, false, Press::Long) == Action::Forget);
    CHECK(act(Role::Dongle, State::Idle, false, false, false, Press::Long) == Action::None);
    // vault simulator, idle
    CHECK(act(Role::Vault, State::Idle, false, false, false, Press::Short) == Action::StartPairing);
    CHECK(act(Role::Vault, State::Idle, true, false, false, Press::Short) == Action::StartPairing);
    CHECK(act(Role::Vault, State::Idle, true, false, false, Press::Long) == Action::Forget);
    // handshake running
    for (Role r : {Role::Dongle, Role::Vault}) {
        CHECK(act(r, State::Pairing, false, true, false, Press::Short) == Action::CancelPairing);
        CHECK(act(r, State::Pairing, false, true, false, Press::Long) == Action::CancelPairing);
    }
    // code showing
    for (Role r : {Role::Dongle, Role::Vault}) {
        CHECK(act(r, State::Confirming, false, true, false, Press::Short) == Action::Confirm);
        CHECK(act(r, State::Confirming, false, true, true, Press::Short) == Action::None);  // already confirmed
        CHECK(act(r, State::Confirming, false, true, false, Press::Long) == Action::Reject);
        CHECK(act(r, State::Confirming, false, true, true, Press::Long) == Action::Reject);
    }
    // a working link: a short press must never disturb it; a long press forgets
    for (Role r : {Role::Dongle, Role::Vault}) {
        for (State st : {State::Connecting, State::Linked}) {
            const bool test_press = r == Role::Vault && st == State::Linked; // the simulator types a test text
            CHECK(act(r, st, true, false, false, Press::Short) == (test_press ? Action::TypeTest : Action::None));
            CHECK(act(r, st, true, false, false, Press::Long) == Action::Forget);
        }
    }
    CHECK(act(Role::Dongle, State::Linked, true, false, false, Press::Short) == Action::None);
}

bool all_fit(const Screen& s)
{
    return std::strlen(s.title) < sizeof s.title - 1 && std::strlen(s.big) < sizeof s.big - 1 &&
           std::strlen(s.status) < sizeof s.status - 1 && std::strlen(s.hint) < sizeof s.hint - 1;
}

void test_screens()
{
    Snapshot s;
    // unpaired dongle
    Screen o = describe(s);
    CHECK(std::strcmp(o.title, "KeyKeeper dongle") == 0);
    CHECK(std::strcmp(o.status, "Not paired") == 0 && std::strcmp(o.hint, "BOOT: start pairing") == 0);
    CHECK(o.big[0] == '\0');
    // window open
    s.window_open = true;
    s.window_left_s = 42;
    o = describe(s);
    CHECK(std::strcmp(o.status, "Waiting for the vault (42s)") == 0 && o.tone == Tone::Warn);
    // handshake
    s.state = State::Pairing;
    o = describe(s);
    CHECK(std::strcmp(o.status, "Pairing...") == 0);
    // code
    s.state = State::Confirming;
    s.code = "004217";
    o = describe(s);
    CHECK(std::strcmp(o.big, "004 217") == 0);
    CHECK(std::strcmp(o.status, "Same code on both?") == 0);
    CHECK(std::strcmp(o.hint, "BOOT: yes   Hold BOOT: no") == 0);
    s.local_confirmed = true;
    o = describe(s);
    CHECK(std::strcmp(o.big, "004 217") == 0 && std::strcmp(o.status, "Waiting for the other device") == 0);
    // a code that is not 6 digits never reaches the screen as a code
    s.code = "12";
    o = describe(s);
    CHECK(o.big[0] == '\0');
    s.code = nullptr;
    o = describe(s);
    CHECK(o.big[0] == '\0');
    // paired, idle / connecting / linked
    s = Snapshot();
    s.paired = true;
    o = describe(s);
    CHECK(std::strcmp(o.status, "Paired. Waiting for the vault") == 0);
    s.state = State::Connecting;
    CHECK(std::strcmp(describe(s).status, "Connecting...") == 0);
    s.state = State::Linked;
    o = describe(s);
    CHECK(std::strcmp(o.status, "Connected") == 0 && o.tone == Tone::Good);
    // vault simulator wording
    s = Snapshot();
    s.role = Role::Vault;
    o = describe(s);
    CHECK(std::strcmp(o.title, "Vault (simulator)") == 0);
    s.paired = true;
    CHECK(std::strcmp(describe(s).status, "Paired. Not connected") == 0);
    // the dongle shows whether a PC is behind its USB port
    {
        Snapshot d;
        d.role = Role::Dongle;
        d.paired = true;
        d.state = State::Linked;
        CHECK(std::strstr(describe(d).hint, "forget pairing") != nullptr); // unknown: the old hint
        d.usb = 1;
        CHECK(std::strstr(describe(d).hint, "USB: ready") != nullptr);
        d.usb = 0;
        CHECK(std::strstr(describe(d).hint, "USB: no PC") != nullptr);
        CHECK(std::strlen(describe(d).hint) < 63);
    }
    // a free-text notice (typing results) and the simulator's hint
    s = Snapshot();
    s.role = Role::Vault;
    s.paired = true;
    s.state = State::Linked;
    CHECK(std::strstr(describe(s).hint, "type test") != nullptr);
    s.notice = Notice::Text;
    std::snprintf(s.text, sizeof s.text, "Typed 18 characters");
    s.text_ok = true;
    o = describe(s);
    CHECK(std::strcmp(o.status, "Typed 18 characters") == 0 && o.tone == Tone::Good);
    s.text_ok = false;
    CHECK(describe(s).tone == Tone::Bad);
    s = Snapshot();
    s.state = State::Linked;
    CHECK(std::strstr(describe(s).hint, "type test") == nullptr);  // the dongle itself has no test press
    // notices
    s = Snapshot();
    s.notice = Notice::Paired;
    CHECK(std::strcmp(describe(s).status, "Paired!") == 0 && describe(s).tone == Tone::Good);
    s.notice = Notice::Rejected;
    CHECK(std::strcmp(describe(s).status, "Pairing cancelled") == 0 && describe(s).tone == Tone::Bad);
    s.notice = Notice::Failed;
    CHECK(std::strcmp(describe(s).status, "No answer. Pairing failed") == 0);
    s.notice = Notice::WindowClosed;
    CHECK(std::strcmp(describe(s).status, "Pairing time is over") == 0);
    s.notice = Notice::Forgotten;
    CHECK(std::strcmp(describe(s).status, "Pairing forgotten") == 0);
    // holding the button
    s = Snapshot();
    s.paired = true;
    s.held_s = 2;
    CHECK(std::strcmp(describe(s).hint, "Keep holding... 2/3s") == 0);

    // every combination fits its buffer
    const Role roles[] = {Role::Dongle, Role::Vault};
    const State states[] = {State::Idle, State::Pairing, State::Confirming, State::Connecting, State::Linked};
    const Notice notices[] = {Notice::None, Notice::Paired, Notice::Rejected, Notice::Failed, Notice::WindowClosed, Notice::Forgotten};
    for (Role r : roles)
        for (State st : states)
            for (Notice n : notices)
                for (int paired = 0; paired < 2; ++paired)
                    for (int window = 0; window < 2; ++window) {
                        Snapshot t;
                        t.role = r;
                        t.state = st;
                        t.notice = n;
                        t.paired = paired != 0;
                        t.window_open = window != 0;
                        t.window_left_s = 4294967295u;
                        t.held_s = 4294967295u;
                        t.long_s = 4294967295u;
                        t.code = "999999";
                        const Screen x = describe(t);
                        CHECK(all_fit(x));
                        CHECK(x.status[0] != '\0');
                    }
}

}  // namespace

// Every state in Russian: fits in the buffers (no cut UTF-8 character), nothing left in English.
static bool valid_utf8(const char* t)
{
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(t); *p;) {
        int n = *p < 0x80 ? 1 : (*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : 0;
        if (n == 0) return false;
        for (int i = 1; i < n; ++i) if ((p[i] & 0xC0) != 0x80) return false;
        p += n;
    }
    return true;
}

static void test_russian()
{
    using kk::link::State;
    const State states[] = {State::Idle, State::Pairing, State::Confirming, State::Connecting, State::Linked};
    const Notice notices[] = {Notice::None, Notice::Paired, Notice::Rejected, Notice::Failed, Notice::WindowClosed, Notice::Forgotten};
    for (State st : states)
        for (int paired = 0; paired < 2; ++paired)
            for (int win = 0; win < 2; ++win)
                for (int usb = -1; usb <= 1; ++usb)
                    for (Notice n : notices)
                        for (int role = 0; role < 2; ++role) {
                            Snapshot s;
                            s.role = role ? kk::link::Role::Vault : kk::link::Role::Dongle;
                            s.state = st; s.paired = paired; s.window_open = win; s.usb = usb; s.notice = n;
                            s.window_left_s = 42; s.code = "123456"; s.ru = true;
                            const Screen o = describe(s);
                            CHECK(valid_utf8(o.title) && valid_utf8(o.status) && valid_utf8(o.hint));
                            CHECK(std::strlen(o.hint) < sizeof o.hint - 1 && std::strlen(o.status) < sizeof o.status - 1);
                            s.held_s = 2; s.long_s = 3;
                            CHECK(valid_utf8(describe(s).hint));
                        }
    Snapshot s;
    s.ru = true;
    CHECK(std::strcmp(describe(s).status, "Не сопряжено") == 0);
    s.paired = true; s.state = State::Linked; s.usb = 1;
    CHECK(std::strcmp(describe(s).status, "Подключено") == 0 && std::strstr(describe(s).hint, "готов") != nullptr);
    s.ru = false;
    CHECK(std::strcmp(describe(s).status, "Connected") == 0);
}

int main()
{
    test_button();
    test_actions();
    test_screens();
    test_russian();
    std::printf("dongle logic: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
