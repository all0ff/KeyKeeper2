#include "usb/typing_plan.hpp"

#include "usb/cyrillic_layout.hpp"
#include "usb/keycode_map.hpp"

namespace usb {

namespace {

/**
 * @brief Decode ONE UTF-8 code point starting at text[pos].
 *
 * @return Number of bytes consumed (1-4), or 0 if pos is already at
 *         the end. Malformed/truncated sequences degrade to
 *         consuming just the one lead byte, decoded as itself -- this
 *         project only ever produces well-formed UTF-8 (from
 *         widgets::TextEntry or vault storage, both closed loops), so
 *         this is a safety fallback, not a real code path.
 *
 * (Moved verbatim from the old type_engine.cpp.)
 */
size_t utf8_decode(const std::string& text, size_t pos, uint32_t& out_codepoint)
{
    const size_t remaining = text.size() - pos;
    const auto b0 = static_cast<unsigned char>(text[pos]);

    size_t seq_len = 1;
    uint32_t codepoint = b0;

    if ((b0 & 0x80) == 0x00) {
        seq_len = 1;
        codepoint = b0;
    } else if ((b0 & 0xE0) == 0xC0 && remaining >= 2) {
        seq_len = 2;
        codepoint = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0 && remaining >= 3) {
        seq_len = 3;
        codepoint = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0 && remaining >= 4) {
        seq_len = 4;
        codepoint = b0 & 0x07;
    } else {
        out_codepoint = b0;
        return 1;
    }

    for (size_t i = 1; i < seq_len; ++i) {
        const auto cb = static_cast<unsigned char>(text[pos + i]);
        if ((cb & 0xC0) != 0x80) {
            out_codepoint = b0;
            return 1;
        }
        codepoint = (codepoint << 6) | (cb & 0x3F);
    }

    out_codepoint = codepoint;
    return seq_len;
}

class Planner {
public:
    Planner(const PlanOptions& options, TypingPlan& plan) : opt_(options), plan_(plan) {}

    /// A non-Cyrillic character. Like the old type_char(), this takes the RAW
    /// first byte of the sequence, so a multi-byte non-Cyrillic character
    /// (e.g. "é") maps to nothing and becomes a SkipLatin.
    void latin(char c)
    {
        const KeyMapping km = ascii_to_hid(c);
        HidEvent ev;
        ev.chars = 1;
        if (km.keycode == keycode::NONE) {
            ev.kind = HidEvent::Kind::SkipLatin;
        } else {
            ev.kind = HidEvent::Kind::Key;
            ev.modifier = km.modifier;
            ev.keycode = km.keycode;
            ev.hold_ms = opt_.timing.press_ms;
            ev.gap_ms = opt_.timing.inter_ms;
        }
        plan_.events.push_back(ev);
    }

    /// One Cyrillic letter, typed as the physical key the YCUKEN layout puts
    /// it on (through the ordinary ascii_to_hid() table).
    void cyrillic(uint32_t codepoint)
    {
        char physical_key = '\0';
        bool uppercase = false;
        cyrillic::cyrillic_physical_key(codepoint, physical_key, uppercase);

        HidEvent ev;
        ev.chars = 1;
        if (physical_key == '\0') {
            ev.kind = HidEvent::Kind::SkipCyrillic;
        } else {
            KeyMapping km = ascii_to_hid(physical_key);
            if (uppercase) {
                km.modifier |= modifier::LEFT_SHIFT;
            }
            ev.kind = HidEvent::Kind::Key;
            ev.modifier = km.modifier;
            ev.keycode = km.keycode;
            ev.hold_ms = opt_.timing.press_ms;
            ev.gap_ms = opt_.timing.inter_ms;
        }
        plan_.events.push_back(ev);
    }

    /// Alt+Shift (bare modifiers, no regular key) -- the classic Windows
    /// layout-switch hotkey.
    //
    // PRE: held slightly longer than a normal character press elsewhere
    // (HOLD) -- OS layout-switch hotkey handlers are sometimes picky about
    // very brief modifier-only taps. The PRE delay gives the host a moment to
    // fully process the PREVIOUS keystroke's release before the modifier-only
    // report goes out: a real, confirmed failure mode was the switch-back
    // after a Cyrillic run not registering (wrong characters typed afterward,
    // not just missing ones). SETTLE gives the host time to actually apply
    // the new layout before the first character of the run.
    void layout_hotkey(HidEvent::Kind kind)
    {
        HidEvent ev;
        ev.kind = kind;
        ev.modifier = modifier::LEFT_ALT | modifier::LEFT_SHIFT;
        ev.keycode = keycode::NONE;
        ev.pre_ms = LAYOUT_SWITCH_PRE_DELAY_MS;
        ev.hold_ms = LAYOUT_SWITCH_HOLD_MS;
        ev.gap_ms = LAYOUT_SWITCH_SETTLE_MS;
        plan_.events.push_back(ev);
    }

    /// Called after every character: counts it and adds the pacing pause after
    /// each 32 characters (counted in CHARACTERS, not bytes).
    void character_done()
    {
        ++char_index_;
        if (opt_.timing.chunk_ms > 0 && char_index_ % 32 == 0) {
            HidEvent ev;
            ev.kind = HidEvent::Kind::Pause;
            ev.gap_ms = opt_.timing.chunk_ms;
            plan_.events.push_back(ev);
        }
    }

    bool auto_switch() const { return opt_.auto_switch_layout; }

private:
    const PlanOptions& opt_;
    TypingPlan& plan_;
    size_t char_index_ = 0;
};

} // namespace

TypingPlan plan_events(const std::string& text, const PlanOptions& options)
{
    TypingPlan plan;
    plan.events.reserve(text.size() + 4);
    Planner planner(options, plan);

    size_t pos = 0;
    while (pos < text.size()) {
        uint32_t codepoint = 0;
        const size_t consumed = utf8_decode(text, pos, codepoint);
        if (consumed == 0) {
            break; // cannot happen (pos < size guarantees a byte); safety only
        }

        if (cyrillic::is_cyrillic(codepoint)) {
            // Group the WHOLE run of consecutive Cyrillic characters under one
            // layout switch, not one per letter (see cyrillic_layout.hpp).
            if (planner.auto_switch()) {
                planner.layout_hotkey(HidEvent::Kind::LayoutOn);
            }

            while (pos < text.size()) {
                uint32_t run_codepoint = 0;
                const size_t run_consumed = utf8_decode(text, pos, run_codepoint);
                if (run_consumed == 0 || !cyrillic::is_cyrillic(run_codepoint)) {
                    break;
                }
                planner.cyrillic(run_codepoint);
                pos += run_consumed;
                planner.character_done();
            }

            if (planner.auto_switch()) {
                planner.layout_hotkey(HidEvent::Kind::LayoutOff);
            }
            continue; // pos already advanced past the whole run
        }

        planner.latin(text[pos]);
        pos += consumed;
        planner.character_done();
    }

    return plan;
}

} // namespace usb
