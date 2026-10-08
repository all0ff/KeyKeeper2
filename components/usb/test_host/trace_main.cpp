// Differential test driver. Built twice -- against the ORIGINAL type_engine.cpp and
// against the new one -- and the two outputs must be byte-identical.
#include "fake_hid.hpp"
#include "settings/settings.hpp"
#include "usb/type_engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
struct Rng {
    uint64_t s;
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<uint32_t>(s >> 11); }
    uint32_t below(uint32_t n) { return next() % n; }
};

void put_utf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
}

std::string random_text(Rng& r)
{
    static const uint32_t extra[] = {0xE9, 0xDF, 0x20AC, 0x1F600, 0x402, 0x4D8, 0x3A9, 0x4E2D, 0x452, 0x460};
    std::string t;
    const uint32_t len = r.below(5) == 0 ? r.below(100) : r.below(40);
    for (uint32_t i = 0; i < len; ++i) {
        const uint32_t k = r.below(100);
        if (k < 45) put_utf8(t, 0x20 + r.below(0x5F));                    // printable ASCII
        else if (k < 50) put_utf8(t, "\t\n\r"[r.below(3)]);              // Tab / Enter
        else if (k < 80) put_utf8(t, 0x410 + r.below(0x40));             // А-я
        else if (k < 83) put_utf8(t, r.below(2) ? 0x401 : 0x451);        // Ё ё
        else if (k < 92) put_utf8(t, extra[r.below(10)]);               // unsupported, multi-byte
        else if (k < 96) t += static_cast<char>(0x80 + r.below(0x80));    // stray byte: malformed UTF-8
        else put_utf8(t, r.below(0x20));                                  // control characters
    }
    return t;
}

std::string hex(const std::string& s)
{
    std::string h; char b[4];
    for (unsigned char c : s) { std::snprintf(b, sizeof b, "%02x", c); h += b; }
    return h;
}
} // namespace

int main(int argc, char** argv)
{
    const int cases = argc > 1 ? std::atoi(argv[1]) : 20000;
    Rng r{0x9E3779B97F4A7C15ull};
    static const uint32_t T[] = {0, 1, 5, 10, 25, 50};
    for (int i = 0; i < cases; ++i) {
        std::string text = random_text(r);
        usb::TypeEngine::Timing timing;
        timing.press_ms = T[r.below(6)];
        timing.inter_ms = T[r.below(6)];
        timing.chunk_ms = r.below(3) == 0 ? 0 : T[1 + r.below(5)];

        g_hid.reset();
        settings::all().usb.cyrillic_auto_switch_layout = r.below(2) != 0;
        // INTENDED behaviour change: with Cyrillic text and no automatic layout switch, ". , ? \" ; : /" are
        // typed on the keys the Russian layout puts them on (test_ru_punct.cpp covers that against an
        // independent model of the layout). Here those characters are removed from exactly such texts so
        // that everything ELSE must still match the original code byte for byte.
        if (!settings::all().usb.cyrillic_auto_switch_layout) {
            bool has_cyrillic = false;
            for (unsigned char c : text) has_cyrillic = has_cyrillic || c == 0xD0 || c == 0xD1; // lead bytes of U+0400..U+047F
            if (has_cyrillic) {
                std::string kept;
                for (char c : text) if (std::string(".,?\";:/").find(c) == std::string::npos) kept += c;
                text = kept;
            }
        }
        const uint32_t fault = r.below(10);
        if (fault == 0) g_hid.connected_at_start = false;
        else if (fault <= 3) g_hid.fail_send_at = static_cast<int>(r.below(60));
        else if (fault == 4) g_hid.connected_for_sends = static_cast<int>(r.below(60));

        usb::TypeEngine engine;
        const size_t sent = engine.type_string(text, timing);
        std::printf("%d|%s|%u,%u,%u|auto=%d|fail=%d|disc=%d|start=%d => sent=%zu err=\"%s\" trace=%s\n",
                    i, hex(text).c_str(), timing.press_ms, timing.inter_ms, timing.chunk_ms,
                    settings::all().usb.cyrillic_auto_switch_layout ? 1 : 0, g_hid.fail_send_at,
                    g_hid.connected_for_sends, g_hid.connected_at_start ? 1 : 0, sent,
                    engine.last_error(), g_hid.trace.c_str());

        // type_char (single raw byte) too, on the same fault setup
        g_hid.reset();
        if (fault == 0) g_hid.connected_at_start = false;
        else if (fault <= 3) g_hid.fail_send_at = static_cast<int>(r.below(2));
        usb::TypeEngine e2;
        const char c = static_cast<char>(r.below(256));
        const bool ok = e2.type_char(c, timing);
        std::printf("%d|char=%02x => ok=%d err=\"%s\" trace=%s\n", i, static_cast<unsigned char>(c), ok ? 1 : 0, e2.last_error(), g_hid.trace.c_str());
    }
    return 0;
}
