// Unit tests for plan_events() and run_plan(). Expected values are written by hand
// from the USB HID usage table and the old TypeEngine's documented behaviour.
#include "usb/keycode_map.hpp"
#include "usb/typing_plan.hpp"
#include "usb/typing_runner.hpp"

#include <cstdio>
#include <string>
#include <vector>

using usb::HidEvent;
using Kind = usb::HidEvent::Kind;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static usb::TypingPlan plan(const std::string& text, bool auto_switch = false, uint32_t press = 10, uint32_t inter = 10, uint32_t chunk = 0)
{
    usb::PlanOptions o;
    o.timing.press_ms = press; o.timing.inter_ms = inter; o.timing.chunk_ms = chunk;
    o.auto_switch_layout = auto_switch;
    return usb::plan_events(text, o);
}

static void test_ascii()
{
    const auto p = plan("aB\t\n");
    CHECK(p.events.size() == 4);
    CHECK(p.events[0].kind == Kind::Key && p.events[0].keycode == 0x04 && p.events[0].modifier == 0);
    CHECK(p.events[0].hold_ms == 10 && p.events[0].gap_ms == 10 && p.events[0].chars == 1);
    CHECK(p.events[1].keycode == 0x05 && p.events[1].modifier == usb::modifier::LEFT_SHIFT);   // 'B' = Shift + b
    CHECK(p.events[2].keycode == usb::keycode::TAB);
    CHECK(p.events[3].keycode == usb::keycode::ENTER);
    CHECK(plan("").events.empty());
    CHECK(plan("a", false, 7, 0).events[0].hold_ms == 7 && plan("a", false, 7, 0).events[0].gap_ms == 0);
}

static void test_unsupported()
{
    const auto p = plan("a\xC3\xA9" "b");                       // "aéb": é is two bytes, ONE character
    CHECK(p.events.size() == 3);
    CHECK(p.events[1].kind == Kind::SkipLatin && p.events[1].chars == 1);
}

static void test_cyrillic_plain()
{
    // ж = U+0436 -> physical key ';' (HID 0x33); Ж adds Shift on top
    const auto p = plan("\xD0\xB6\xD0\x96");
    CHECK(p.events.size() == 2);                                // no hotkeys: auto-switch is off
    CHECK(p.events[0].keycode == 0x33 && p.events[0].modifier == 0);
    CHECK(p.events[1].keycode == 0x33 && p.events[1].modifier == usb::modifier::LEFT_SHIFT);
}

static void test_cyrillic_switch_runs()
{
    // a ж ж b : ONE run -> one switch before, one after
    const auto p = plan("a\xD0\xB6\xD0\xB6" "b", true);
    CHECK(p.events.size() == 6);
    CHECK(p.events[0].kind == Kind::Key);
    CHECK(p.events[1].kind == Kind::LayoutOn);
    CHECK(p.events[1].modifier == (usb::modifier::LEFT_ALT | usb::modifier::LEFT_SHIFT) && p.events[1].keycode == 0);
    CHECK(p.events[1].pre_ms == 60 && p.events[1].hold_ms == 50 && p.events[1].gap_ms == 80);
    CHECK(p.events[2].kind == Kind::Key && p.events[3].kind == Kind::Key);
    CHECK(p.events[4].kind == Kind::LayoutOff && p.events[4].pre_ms == 60 && p.events[4].gap_ms == 80);
    CHECK(p.events[5].kind == Kind::Key);
    // two separate runs
    const auto q = plan("\xD0\xB6" "a\xD0\xB6", true);
    int on = 0, off = 0;
    for (const auto& e : q.events) { on += e.kind == Kind::LayoutOn; off += e.kind == Kind::LayoutOff; }
    CHECK(on == 2 && off == 2);
}

static void test_chunk_pacing()
{
    std::string a(33, 'a');
    const auto p = plan(a, false, 10, 10, 7);
    CHECK(p.events.size() == 34);
    CHECK(p.events[32].kind == Kind::Pause && p.events[32].gap_ms == 7);   // after the 32nd character
    CHECK(p.events[33].kind == Kind::Key);
    CHECK(plan(a, false, 10, 10, 0).events.size() == 33);
    std::string cyr;                                                         // 32 Cyrillic letters = 64 bytes
    for (int i = 0; i < 32; ++i) cyr += "\xD0\xB6";
    const auto c = plan(cyr, false, 10, 10, 5);
    CHECK(c.events.size() == 33 && c.events[32].kind == Kind::Pause);        // counted in characters, not bytes
}

// ---- executor ----------------------------------------------------------------------------
struct RecIo final : usb::KeyIo {
    std::string trace; int sends = 0, fail_at = -1; bool is_ready = true; int ready_for = -1;
    bool ready() override { return is_ready && (ready_for < 0 || sends < ready_for); }
    bool send_key(uint8_t kc, uint8_t m, uint32_t h) override
    {
        trace += "K" + std::to_string(kc) + "/" + std::to_string(m) + "/" + std::to_string(h) + ";";
        return !(fail_at >= 0 && sends++ == fail_at) ? true : false;
    }
    void delay_ms(uint32_t ms) override { trace += "D" + std::to_string(ms) + ";"; }
    const char* last_error() const override { return "io failure"; }
};

static void test_runner()
{
    { // success
        RecIo io; const auto r = usb::run_plan(plan("ab"), io);
        CHECK(r.ok && r.chars_sent == 2 && r.error == nullptr);
        CHECK(io.trace == "K4/0/10;D10;K5/0/10;D10;");
    }
    { // failure INSIDE a Cyrillic run: the switch back is still sent, the tail is not
        RecIo io; io.fail_at = 2;                       // sends: 0=LayoutOn, 1=ж, 2=ж (fails)
        const auto r = usb::run_plan(plan("\xD0\xB6\xD0\xB6\xD0\xB6" "a", true), io);
        CHECK(!r.ok && r.chars_sent == 1);
        CHECK(std::string(r.error) == "io failure");
        // ...LayoutOn, ж ok, ж FAILS, LayoutOff (+ its delays), nothing for the 3rd ж or 'a'
        CHECK(io.trace == "D60;K0/6/50;D80;K51/0/10;D10;K51/0/10;D60;K0/6/50;D80;");
    }
    { // LayoutOn itself fails: no switch back
        RecIo io; io.fail_at = 0;
        const auto r = usb::run_plan(plan("\xD0\xB6", true), io);
        CHECK(!r.ok && r.chars_sent == 0);
        CHECK(io.trace == "D60;K0/6/50;");
    }
    { // unplugged: "USB not connected"
        RecIo io; io.is_ready = false;
        const auto r = usb::run_plan(plan("a"), io);
        CHECK(!r.ok && r.chars_sent == 0 && std::string(r.error) == "USB not connected" && io.trace.empty());
    }
    { // unsupported character: counted, not a failure, error text kept
        RecIo io; const auto r = usb::run_plan(plan("\xC3\xA9"), io);
        CHECK(r.ok && r.chars_sent == 1 && std::string(r.error) == "Unsupported character" && io.trace.empty());
    }
    { // chunk pause goes out between the 32nd and 33rd character
        RecIo io; const auto r = usb::run_plan(plan(std::string(33, 'a'), false, 10, 0, 7), io);
        CHECK(r.ok && r.chars_sent == 33);
        CHECK(io.trace.find("D7;") != std::string::npos);
    }
}

int main()
{
    test_ascii(); test_unsupported(); test_cyrillic_plain(); test_cyrillic_switch_runs(); test_chunk_pacing(); test_runner();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
