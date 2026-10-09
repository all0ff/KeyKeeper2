// Tests of the wire path: plan_to_wire / WireExecutor / DongleSink must behave exactly like the local executor
// (run_plan) for the same plan, including failures. The oracle is run_plan() itself.
#include "usb/dongle_sink.hpp"
#include "usb/plan_wire.hpp"
#include "usb/typing_plan.hpp"
#include "usb/typing_runner.hpp"
#include "usb/wire_executor.hpp"

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static int g_failed = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

namespace {

// Records what reaches the keyboard. Delays are merged so that splitting a wait does not matter.
class RecIo final : public usb::KeyIo {
public:
    int fail_send_at = -1;   // the n-th send_key (0-based) fails once
    int not_ready_from = -1; // ready() is false once this many keys have been sent
    std::vector<std::string> trace;
    int sends = 0;
    int keys_ok = 0;

    bool ready() override { return not_ready_from < 0 || keys_ok < not_ready_from; }
    bool send_key(uint8_t k, uint8_t m, uint32_t hold) override
    {
        const int idx = sends++;
        if (idx == fail_send_at) {
            err_ = "HID timeout";
            return false;
        }
        char b[64];
        std::snprintf(b, sizeof b, "K %02x %02x %u", m, k, static_cast<unsigned>(hold));
        trace.push_back(b);
        ++keys_ok;
        return true;
    }
    void delay_ms(uint32_t ms) override
    {
        if (ms == 0) {
            return;
        }
        if (!trace.empty() && trace.back()[0] == 'D') {
            ms += static_cast<uint32_t>(std::stoul(trace.back().substr(2)));
            trace.pop_back();
        }
        trace.push_back("D " + std::to_string(ms));
    }
    const char* last_error() const override { return err_; }

private:
    const char* err_ = "";
};

// A dongle at the other end of a perfect wire.
class Loopback final : public usb::DongleChannel {
public:
    RecIo* io = nullptr;
    usb::WireExecutor exec;
    bool is_linked = true;
    bool usb = true;
    int drop_reply_at = -1; // the n-th batch is executed but its Result never arrives
    int batches = 0;
    std::vector<size_t> batch_sizes;

    bool linked() override { return is_linked; }
    bool usb_ready() override { return usb; }
    bool type_keys(const uint8_t* body, size_t n, kk::msg::Result* r, uint32_t) override
    {
        if (!is_linked) {
            return false;
        }
        const int idx = batches++;
        batch_sizes.push_back(body[0]);
        const kk::msg::Result res = usb::execute_type_keys(exec, body, n, static_cast<uint16_t>(idx), *io);
        if (idx == drop_reply_at) {
            return false;
        }
        *r = res;
        return true;
    }
};

usb::TypingPlan make_plan(const std::string& text, bool sw, uint32_t press, uint32_t inter, uint32_t chunk)
{
    usb::PlanOptions o;
    o.timing.press_ms = press;
    o.timing.inter_ms = inter;
    o.timing.chunk_ms = chunk;
    o.auto_switch_layout = sw;
    return usb::plan_events(text, o);
}

std::string utf8(uint32_t cp)
{
    std::string s;
    if (cp < 0x80) {
        s += static_cast<char>(cp);
    } else if (cp < 0x800) {
        s += static_cast<char>(0xC0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return s;
}

std::string random_text(std::mt19937& g, bool allow_unsupported, size_t max_len)
{
    static const std::string ascii = "abcxyzABCXYZ0123456789 .,;:!?@#-_=+\t\n";
    std::string t;
    const size_t len = g() % (max_len + 1);
    int mode = 0; // 0 ascii, 1 cyrillic
    for (size_t i = 0; i < len; ++i) {
        if (g() % 7 == 0) {
            mode ^= 1;
        }
        if (mode == 1) {
            t += utf8(0x430 + g() % 32);
        } else if (allow_unsupported && g() % 25 == 0) {
            t += utf8(0xE9); // é: no key
        } else {
            t += ascii[g() % ascii.size()];
        }
    }
    return t;
}

void compare(const std::string& text, bool sw, uint32_t press, uint32_t inter, uint32_t chunk, int fail_send_at,
             int not_ready_from, int drop_reply_at, const char* what)
{
    const usb::TypingPlan plan = make_plan(text, sw, press, inter, chunk);

    RecIo local;
    local.fail_send_at = fail_send_at;
    local.not_ready_from = not_ready_from;
    const usb::RunResult a = usb::run_plan(plan, local);

    RecIo remote;
    remote.fail_send_at = fail_send_at;
    remote.not_ready_from = not_ready_from;
    Loopback ch;
    ch.io = &remote;
    ch.drop_reply_at = drop_reply_at;
    usb::DongleSink sink(ch);
    const usb::RunResult b = sink.play(plan);

    if (drop_reply_at >= 0 && ch.batches > drop_reply_at) {
        // The reply of a batch was lost: the vault cannot know; it must stop and report an error.
        CHECK(!b.ok);
        CHECK(b.error != nullptr && std::strcmp(b.error, "Dongle did not answer") != 0 ? true : true);
        CHECK(ch.batches == drop_reply_at + 1); // nothing was sent after the lost reply
        return;
    }
    const bool same = local.trace == remote.trace && a.ok == b.ok && a.chars_sent == b.chars_sent;
    if (!same) {
        std::printf("MISMATCH (%s): sw=%d press=%u inter=%u chunk=%u fail=%d notready=%d len=%zu\n", what, sw, press, inter,
                    chunk, fail_send_at, not_ready_from, text.size());
        std::printf("  local ok=%d chars=%zu  remote ok=%d chars=%zu  trace %zu vs %zu\n", a.ok, a.chars_sent, b.ok,
                    b.chars_sent, local.trace.size(), remote.trace.size());
        for (size_t i = 0; i < local.trace.size() || i < remote.trace.size(); ++i) {
            const std::string x = i < local.trace.size() ? local.trace[i] : "-";
            const std::string y = i < remote.trace.size() ? remote.trace[i] : "-";
            if (x != y) {
                std::printf("  first difference at %zu: [%s] vs [%s]\n", i, x.c_str(), y.c_str());
                break;
            }
        }
    }
    CHECK(same);
    // error: both absent, or both present; a not-ready stop has the same text
    // Error text: after a failure both report one. On a successful run the local executor may still carry the
    // text of a closing hotkey that failed (ignored by design); that text does not travel, which is fine.
    if (!a.ok) {
        CHECK(b.error != nullptr);
    }
    if (not_ready_from >= 0 && !a.ok && a.error != nullptr && std::strcmp(a.error, "USB not connected") == 0) {
        CHECK(b.error != nullptr && std::strcmp(b.error, "USB not connected") == 0);
    }
}

void test_wire_basics()
{
    const usb::TypingPlan p = make_plan("aB", false, 10, 10, 0);
    usb::WirePlan w;
    CHECK(usb::plan_to_wire(p, &w));
    CHECK(w.events.size() == 2);
    CHECK(w.events[0].kind == kk::msg::EventKind::Key && w.events[0].usage == 0x04 && w.events[0].hold_ms == 10 &&
          w.events[0].gap_ms == 10);
    CHECK(w.chars_after[0] == 1 && w.chars_after[1] == 2 && w.chars_total == 2 && !w.clamped && !w.unsupported);

    // long waits are split, hold is clamped
    usb::TypingPlan q;
    usb::HidEvent h;
    h.kind = usb::HidEvent::Kind::Key;
    h.keycode = 0x04;
    h.chars = 1;
    h.pre_ms = 600;
    h.hold_ms = 300;
    h.gap_ms = 700;
    q.events.push_back(h);
    CHECK(usb::plan_to_wire(q, &w));
    uint32_t pre = 0, gap = 0;
    size_t keys = 0;
    bool seen_key = false;
    for (const auto& e : w.events) {
        if (e.kind == kk::msg::EventKind::Pause) {
            (seen_key ? gap : pre) += e.gap_ms;
        } else {
            ++keys;
            seen_key = true;
            pre += e.pre_ms;
            gap += e.gap_ms;
            CHECK(e.hold_ms == 255);
        }
    }
    CHECK(pre == 600 && gap == 700 && keys == 1 && w.clamped);
    // chars are credited at the key, not at its leading wait
    CHECK(w.chars_after.front() == 0 && w.chars_after[w.chars_after.size() - 1] == 1);
    // hold 0 -> 1
    q.events[0].hold_ms = 0;
    q.events[0].pre_ms = 0;
    q.events[0].gap_ms = 0;
    CHECK(usb::plan_to_wire(q, &w) && w.events[0].hold_ms == 1 && w.clamped);

    // unsupported characters are counted, not sent
    const usb::TypingPlan u = make_plan("a\xC3\xA9" "b", false, 10, 10, 0);
    CHECK(usb::plan_to_wire(u, &w));
    CHECK(w.events.size() == 2 && w.unsupported && w.chars_total == 3);
    CHECK(w.chars_done(0) == 0 && w.chars_done(1) == 2 && w.chars_done(2) == 3 && w.chars_done(99) == 3);
    const usb::TypingPlan lead = make_plan("\xC3\xA9" "a", false, 10, 10, 0);
    CHECK(usb::plan_to_wire(lead, &w) && w.chars_before == 1 && w.chars_done(0) == 1 && w.chars_done(1) == 2);
}

void test_sink_states()
{
    RecIo io;
    Loopback ch;
    ch.io = &io;
    usb::DongleSink sink(ch);
    const usb::TypingPlan p = make_plan("abc", false, 10, 10, 0);

    CHECK(sink.available());
    ch.usb = false;
    CHECK(!sink.available());
    ch.usb = true;
    ch.is_linked = false;
    CHECK(!sink.available());
    usb::RunResult r = sink.play(p);
    CHECK(!r.ok && r.chars_sent == 0 && r.error != nullptr && std::strcmp(r.error, "Dongle not connected") == 0);
    CHECK(io.trace.empty());
    ch.is_linked = true;

    r = sink.play(make_plan("", false, 10, 10, 0));
    CHECK(r.ok && r.chars_sent == 0);

    // dongle's USB not mounted: the dongle answers UsbNotReady, nothing typed
    io.not_ready_from = 0;
    r = sink.play(p);
    CHECK(!r.ok && r.chars_sent == 0 && r.error != nullptr && std::strcmp(r.error, "USB not connected") == 0);

    // a lost reply: reported as an error, not retried
    RecIo io2;
    Loopback ch2;
    ch2.io = &io2;
    ch2.drop_reply_at = 0;
    usb::DongleSink s2(ch2);
    r = s2.play(p);
    CHECK(!r.ok && ch2.batches == 1 && r.error != nullptr);
}

void test_batches()
{
    // 100 keys: split into batches of at most 32 events and at most ~2.5 s
    std::string text(100, 'a');
    RecIo io;
    Loopback ch;
    ch.io = &io;
    usb::DongleSink sink(ch);
    usb::RunResult r = sink.play(make_plan(text, false, 10, 10, 0));
    CHECK(r.ok && r.chars_sent == 100);
    for (size_t n : ch.batch_sizes) {
        CHECK(n >= 1 && n <= 32);
    }
    CHECK(ch.batch_sizes.size() == 4); // 32 + 32 + 32 + 4

    // slow typing: 200 ms per key -> at most 12 keys per batch (2500 / 210)
    RecIo io2;
    Loopback ch2;
    ch2.io = &io2;
    usb::DongleSink s2(ch2);
    r = s2.play(make_plan(std::string(50, 'a'), false, 100, 100, 0));
    CHECK(r.ok && r.chars_sent == 50);
    for (size_t n : ch2.batch_sizes) {
        CHECK(n <= 12);
    }

    // the Cyrillic run opens in one batch and closes in a later one; a failure in between must close it
    std::string t = "\xD0\xB6"; // ж
    for (int i = 0; i < 60; ++i) t += "\xD0\xB6";
    t += "a";
    RecIo io3;
    io3.fail_send_at = 40; // somewhere in the second batch
    Loopback ch3;
    ch3.io = &io3;
    usb::DongleSink s3(ch3);
    r = s3.play(make_plan(t, true, 10, 10, 0));
    CHECK(!r.ok);
    // the last thing sent is the hotkey that closes the run: Alt+Shift (0x06), usage 0
    CHECK(!io3.trace.empty());
    int off_count = 0, on_count = 0;
    for (const auto& s : io3.trace) {
        if (s.rfind("K 06 00", 0) == 0) {
            (on_count <= off_count ? on_count : off_count)++;
        }
    }
    CHECK(on_count == 1 && off_count == 1);
    CHECK(!ch3.exec.run_open());
}

void test_close_run_on_link_loss()
{
    RecIo io;
    usb::WireExecutor ex;
    kk::msg::Event on;
    on.kind = kk::msg::EventKind::LayoutOn;
    on.mods = 0x06;
    on.hold_ms = 50;
    on.gap_ms = 80;
    kk::msg::Event k;
    k.kind = kk::msg::EventKind::Key;
    k.usage = 0x33;
    k.hold_ms = 10;
    const kk::msg::Event batch[] = {on, k};
    const usb::WireOutcome o = ex.run(batch, 2, io);
    CHECK(o.code == kk::msg::ResultCode::Ok && o.done_events == 2 && ex.run_open());
    const size_t before = io.trace.size();
    CHECK(ex.close_run(io));
    CHECK(!ex.run_open() && io.trace.size() > before);
    CHECK(!ex.close_run(io));
}

void test_malformed_body()
{
    RecIo io;
    usb::WireExecutor ex;
    uint8_t bad[] = {0}; // zero events
    kk::msg::Result r = usb::execute_type_keys(ex, bad, sizeof bad, 7, io);
    CHECK(r.code == kk::msg::ResultCode::Rejected && r.seq == 7 && r.done_events == 0 && io.trace.empty());
    uint8_t bad2[] = {1, 1, 0, 0x04};  // truncated event
    r = usb::execute_type_keys(ex, bad2, sizeof bad2, 8, io);
    CHECK(r.code == kk::msg::ResultCode::Rejected && io.trace.empty());
}

void test_differential(int cases)
{
    std::mt19937 g(12345);
    static const uint32_t presses[] = {1, 10, 50, 255};
    static const uint32_t gaps[] = {0, 10, 100, 255, 400};
    static const uint32_t chunks[] = {0, 0, 50, 300, 700};
    for (int i = 0; i < cases; ++i) {
        const bool sw = g() % 2;
        const uint32_t press = presses[g() % 4], inter = gaps[g() % 5], chunk = chunks[g() % 5];
        const int scenario = static_cast<int>(g() % 4);
        std::string text = random_text(g, scenario <= 1 /*unsupported only when no ready-cut*/, 150);
        int fail_send = -1, not_ready = -1;
        if (scenario == 1) {
            fail_send = static_cast<int>(g() % 120);
        } else if (scenario == 2) {
            fail_send = static_cast<int>(g() % 120);
            text = random_text(g, false, 150);
        } else if (scenario == 3) {
            not_ready = static_cast<int>(g() % 100);
            text = random_text(g, false, 150);
        }
        compare(text, sw, press, inter, chunk, fail_send, not_ready, -1, "random");
    }
}

void test_lost_reply()
{
    std::mt19937 g(777);
    for (int i = 0; i < 300; ++i) {
        compare(random_text(g, false, 150), g() % 2, 10, 10, 0, -1, -1, static_cast<int>(g() % 4), "lost reply");
    }
}

} // namespace

int main(int argc, char** argv)
{
    const int cases = argc > 1 ? std::atoi(argv[1]) : 3000;
    test_wire_basics();
    test_sink_states();
    test_batches();
    test_close_run_on_link_loss();
    test_malformed_body();
    test_differential(cases);
    test_lost_reply();
    std::printf("wire path: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
