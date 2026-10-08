// End-to-end semantics of the typing plan against an INDEPENDENT model of the host.
//
// The model below is a table of what a Windows host prints for each (HID key, Shift) under the
// standard Russian layout and under the US layout -- written separately from the implementation
// (it is keyed by HID code, the implementation is keyed by ASCII key labels). Playing a plan
// through it answers the only question that matters: does the TEXT come out?
#include "usb/cyrillic_layout.hpp"
#include "usb/keycode_map.hpp"
#include "usb/typing_plan.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_MSG(cond, msg) do { ++g_checks; if (!(cond)) { ++g_failed; std::printf("FAIL %s:%d  %s -- %s\n", __FILE__, __LINE__, #cond, (msg).c_str()); } } while (0)

using usb::HidEvent;
using Kind = HidEvent::Kind;

// ------------------------------------------------------------------------------ host model
struct KeyChars { char32_t plain, shifted; };

// Standard Windows Russian layout (ЙЦУКЕН), indexed by HID usage 0x04..0x38.
static KeyChars russian(uint8_t hid)
{
    switch (hid) {
        case 0x04: return {U'ф', U'Ф'}; case 0x05: return {U'и', U'И'}; case 0x06: return {U'с', U'С'};
        case 0x07: return {U'в', U'В'}; case 0x08: return {U'у', U'У'}; case 0x09: return {U'а', U'А'};
        case 0x0A: return {U'п', U'П'}; case 0x0B: return {U'р', U'Р'}; case 0x0C: return {U'ш', U'Ш'};
        case 0x0D: return {U'о', U'О'}; case 0x0E: return {U'л', U'Л'}; case 0x0F: return {U'д', U'Д'};
        case 0x10: return {U'ь', U'Ь'}; case 0x11: return {U'т', U'Т'}; case 0x12: return {U'щ', U'Щ'};
        case 0x13: return {U'з', U'З'}; case 0x14: return {U'й', U'Й'}; case 0x15: return {U'к', U'К'};
        case 0x16: return {U'ы', U'Ы'}; case 0x17: return {U'е', U'Е'}; case 0x18: return {U'г', U'Г'};
        case 0x19: return {U'м', U'М'}; case 0x1A: return {U'ц', U'Ц'}; case 0x1B: return {U'ч', U'Ч'};
        case 0x1C: return {U'н', U'Н'}; case 0x1D: return {U'я', U'Я'};
        case 0x1E: return {U'1', U'!'};  case 0x1F: return {U'2', U'"'};  case 0x20: return {U'3', U'№'};
        case 0x21: return {U'4', U';'};  case 0x22: return {U'5', U'%'};  case 0x23: return {U'6', U':'};
        case 0x24: return {U'7', U'?'};  case 0x25: return {U'8', U'*'};  case 0x26: return {U'9', U'('};
        case 0x27: return {U'0', U')'};
        case 0x28: return {U'\n', U'\n'}; case 0x2B: return {U'\t', U'\t'}; case 0x2C: return {U' ', U' '};
        case 0x2D: return {U'-', U'_'};  case 0x2E: return {U'=', U'+'};
        case 0x2F: return {U'х', U'Х'};  case 0x30: return {U'ъ', U'Ъ'};  case 0x31: return {U'\\', U'/'};
        case 0x33: return {U'ж', U'Ж'};  case 0x34: return {U'э', U'Э'};  case 0x35: return {U'ё', U'Ё'};
        case 0x36: return {U'б', U'Б'};  case 0x37: return {U'ю', U'Ю'};  case 0x38: return {U'.', U','};
        default:   return {U'\uFFFD', U'\uFFFD'};
    }
}

// US layout.
static KeyChars us(uint8_t hid)
{
    if (hid >= 0x04 && hid <= 0x1D) return {static_cast<char32_t>('a' + (hid - 0x04)), static_cast<char32_t>('A' + (hid - 0x04))};
    static const char32_t digits[10] = {U'1', U'2', U'3', U'4', U'5', U'6', U'7', U'8', U'9', U'0'};
    static const char32_t symbols[10] = {U'!', U'@', U'#', U'$', U'%', U'^', U'&', U'*', U'(', U')'};
    if (hid >= 0x1E && hid <= 0x27) return {digits[hid - 0x1E], symbols[hid - 0x1E]};
    switch (hid) {
        case 0x28: return {U'\n', U'\n'}; case 0x2B: return {U'\t', U'\t'}; case 0x2C: return {U' ', U' '};
        case 0x2D: return {U'-', U'_'};  case 0x2E: return {U'=', U'+'};  case 0x2F: return {U'[', U'{'};
        case 0x30: return {U']', U'}'};  case 0x31: return {U'\\', U'|'}; case 0x33: return {U';', U':'};
        case 0x34: return {U'\'', U'"'}; case 0x35: return {U'`', U'~'};  case 0x36: return {U',', U'<'};
        case 0x37: return {U'.', U'>'};  case 0x38: return {U'/', U'?'};
        default:   return {U'\uFFFD', U'\uFFFD'};
    }
}

/// What the host prints when the plan is played. |start_russian|: the host layout at the start.
/// The layout hotkey events toggle it (Alt+Shift).
static std::u32string host_prints(const usb::TypingPlan& plan, bool start_russian)
{
    std::u32string out;
    bool ru = start_russian;
    for (const HidEvent& e : plan.events) {
        if (e.kind == Kind::LayoutOn) ru = true;
        else if (e.kind == Kind::LayoutOff) ru = false;
        else if (e.kind == Kind::Key) {
            const KeyChars k = ru ? russian(e.keycode) : us(e.keycode);
            out += (e.modifier & usb::modifier::LEFT_SHIFT) ? k.shifted : k.plain;
        }
    }
    return out;
}

// ------------------------------------------------------------------------------ helpers
static void put(std::string& s, char32_t cp)
{
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
}
static std::string utf8(const std::u32string& u) { std::string s; for (char32_t c : u) put(s, c); return s; }
static std::u32string u32(const char32_t* p) { return std::u32string(p); }

static usb::TypingPlan plan(const std::string& text, bool auto_switch, bool punct = true, uint32_t chunk = 0)
{
    usb::PlanOptions o;
    o.timing.chunk_ms = chunk;
    o.auto_switch_layout = auto_switch;
    o.russian_layout_punctuation = punct;
    return usb::plan_events(text, o);
}

struct Rng {
    uint64_t s;
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<uint32_t>(s >> 11); }
    uint32_t below(uint32_t n) { return next() % n; }
};

static char32_t random_cyrillic(Rng& r)
{
    const uint32_t k = r.below(70);
    if (k == 0) return U'Ё';
    if (k == 1) return U'ё';
    return static_cast<char32_t>(0x410 + r.below(0x40)); // А-я
}

// ------------------------------------------------------------------------------ tests
static void test_the_reported_bug()
{
    // The exact string from the hardware test: a Russian domain typed on the Russian layout.
    const std::string url = utf8(u32(U"яндекс.рф"));
    CHECK(host_prints(plan(url, false, /*punct=*/false), true) == u32(U"яндексюрф")); // the old behaviour: "." came out as "ю"
    CHECK(host_prints(plan(url, false), true) == u32(U"яндекс.рф"));                  // fixed
    // With the automatic switch the "." is typed on the US layout and was always right.
    CHECK(host_prints(plan(url, true), false) == u32(U"яндекс.рф"));
    CHECK(host_prints(plan(url, true, false), false) == u32(U"яндекс.рф"));
}

static void test_each_punctuation_character()
{
    const char32_t ch[] = {U'.', U',', U'?', U'"', U';', U':', U'/'};
    for (char32_t c : ch) {
        std::u32string text = U"я";
        text += c;
        text += U"р";
        const std::u32string got = host_prints(plan(utf8(text), false), true);
        CHECK_MSG(got == text, std::string("character U+") + std::to_string(static_cast<unsigned>(c)));
    }
}

static void test_scope()
{
    // No Cyrillic: nothing changes (host assumed on the US layout), whatever the option.
    for (const char* t : {"a.b,c?d\"e;f:g/h", "user@mail.ru\tpass.word\n", "1.2,3"}) {
        const auto on = plan(t, false, true), off = plan(t, false, false);
        CHECK(on.events.size() == off.events.size());
        bool same = on.events.size() == off.events.size();
        for (size_t i = 0; same && i < on.events.size(); ++i)
            same = on.events[i].keycode == off.events[i].keycode && on.events[i].modifier == off.events[i].modifier;
        CHECK(same);
        CHECK(on.unavailable_on_russian_layout == 0);
    }
    // With the automatic switch nothing changes either, and nothing is "unavailable".
    const std::string mixed = utf8(u32(U"user.name@яндекс.рф, ok?"));
    CHECK(plan(mixed, true).unavailable_on_russian_layout == 0);
    CHECK(host_prints(plan(mixed, true), false) == u32(U"user.name@яндекс.рф, ok?"));
}

static void test_unavailable_count()
{
    // In a Russian-layout context Latin letters and @ # $ ^ & [ ] { } < > | ~ ' ` cannot be produced.
    CHECK(plan(utf8(u32(U"яa@")), false).unavailable_on_russian_layout == 2);
    CHECK(plan(utf8(u32(U"я123.,-_=+!%*()\\ ")), false).unavailable_on_russian_layout == 0);
    const std::u32string symbols = U"#$^&[]{}<>|~'`";   // the count comes from the string itself, not from a hand count
    CHECK(plan(utf8(u32(U"я") + symbols), false).unavailable_on_russian_layout == symbols.size());
    CHECK(symbols.size() == 14);
    CHECK(plan(utf8(u32(U"яabcXYZ")), false).unavailable_on_russian_layout == 6);
    CHECK(plan(utf8(u32(U"я")), false, /*punct=*/false).unavailable_on_russian_layout == 0); // option off: not counted
}

// Random texts: Cyrillic plus everything the Russian layout can type -> the host must print the text.
static void test_manual_mode_round_trip(Rng& r, int iters)
{
    static const char32_t typeable[] = U"0123456789!%*()-_=+\\ .,?\";:/\t\n";
    size_t in_scope = 0;
    for (int i = 0; i < iters; ++i) {
        std::u32string t;
        const uint32_t len = r.below(60);
        bool cyr = false;
        for (uint32_t k = 0; k < len; ++k) {
            if (r.below(100) < 55) { t += random_cyrillic(r); cyr = true; }
            else t += typeable[r.below(sizeof(typeable) / sizeof(typeable[0]) - 1)];
        }
        if (!cyr) continue;
        ++in_scope;
        const uint32_t chunk = r.below(3) == 0 ? 7 : 0;
        const usb::TypingPlan p = plan(utf8(t), false, true, chunk);
        const std::u32string got = host_prints(p, true);
        CHECK_MSG(got == t, "manual mode round trip: " + utf8(t));
        CHECK(p.unavailable_on_russian_layout == 0);
    }
    std::printf("  manual-mode round trips: %zu texts printed back identically by the Russian-layout host model\n", in_scope);
}

// Auto-switch: Cyrillic mixed with ANY printable ASCII must come out, with the host starting on US.
static void test_auto_mode_round_trip(Rng& r, int iters)
{
    size_t n = 0;
    for (int i = 0; i < iters; ++i) {
        std::u32string t;
        const uint32_t len = r.below(60);
        for (uint32_t k = 0; k < len; ++k) {
            if (r.below(100) < 45) t += random_cyrillic(r);
            else if (r.below(10) == 0) t += (r.below(2) ? U'\t' : U'\n');
            else t += static_cast<char32_t>(0x20 + r.below(0x5F));
        }
        ++n;
        const usb::TypingPlan p = plan(utf8(t), true, true, r.below(3) == 0 ? 5 : 0);
        CHECK_MSG(host_prints(p, false) == t, "auto mode round trip: " + utf8(t));
        CHECK(p.unavailable_on_russian_layout == 0);
    }
    std::printf("  auto-switch round trips: %zu mixed texts printed back identically (host starts on US)\n", n);
}

// Every table entry points at a real key.
static void test_table_is_consistent()
{
    for (int c = 0; c < 128; ++c) {
        const usb::cyrillic::RuAsciiKey k = usb::cyrillic::russian_layout_ascii(static_cast<char>(c));
        if (k.kind == usb::cyrillic::RuAsciiKind::Remapped) {
            CHECK(usb::ascii_to_hid(k.physical_key).keycode != usb::keycode::NONE);
        }
    }
}

int main()
{
    test_the_reported_bug();
    test_each_punctuation_character();
    test_scope();
    test_unavailable_count();
    test_table_is_consistent();
    Rng r{0xBADC0FFEE0DDF00Dull};
    test_manual_mode_round_trip(r, 60000);
    test_auto_mode_round_trip(r, 60000);
    std::printf("russian-layout punctuation: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
