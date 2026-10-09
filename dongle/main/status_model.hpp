#pragma once

#include "kkproto/link.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

// =============================================================================
// status_model -- what the little screen shows, as a pure function of what the link is doing.
//
// The firmware fills a Snapshot, describe() returns the four lines and a colour, ui.cpp draws them.
// No LVGL, no hardware: tested on a PC, so every state has been looked at in text before it is ever
// flashed. Text is English on purpose (the small built-in fonts have no Cyrillic).
// =============================================================================

namespace dongle {

enum class Notice : uint8_t { None, Paired, Rejected, Failed, WindowClosed, Forgotten, Text };
enum class Tone : uint8_t { Normal, Good, Warn, Bad };

struct Snapshot {
    kk::link::Role role = kk::link::Role::Dongle;
    kk::link::State state = kk::link::State::Idle;
    bool paired = false;          ///< a peer is stored
    bool window_open = false;     ///< dongle: accepting a pairing
    uint32_t window_left_s = 0;
    const char* code = nullptr;   ///< 6 digits while Confirming
    bool local_confirmed = false;
    bool remote_confirmed = false;
    Notice notice = Notice::None; ///< a short message about what just happened
    char text[48] = {};           ///< the message for Notice::Text
    bool text_ok = true;          ///< Notice::Text is good news (green) or bad (red)
    int usb = -1;                 ///< dongle: -1 not known, 0 not plugged into a PC, 1 plugged
    uint32_t held_s = 0;          ///< how long the button has been held (shown while it is)
    uint32_t long_s = 3;          ///< hold time that counts as a long press
};

struct Screen {
    char title[24] = {};
    char big[16] = {};  ///< the verification code ("123 456") or empty
    char status[48] = {};
    char hint[64] = {};
    Tone tone = Tone::Normal;
};

inline void put(char* dst, size_t cap, const char* src)
{
    std::snprintf(dst, cap, "%s", src);
}

inline Screen describe(const Snapshot& s)
{
    using kk::link::Role;
    using kk::link::State;
    Screen o;
    put(o.title, sizeof o.title, s.role == Role::Dongle ? "KeyKeeper dongle" : "Vault (simulator)");
    const bool dongle = s.role == Role::Dongle;

    if (s.state == State::Confirming && s.code != nullptr && std::strlen(s.code) == 6) {
        std::snprintf(o.big, sizeof o.big, "%.3s %.3s", s.code, s.code + 3);
        o.tone = Tone::Warn;
        if (s.local_confirmed) {
            put(o.status, sizeof o.status, "Waiting for the other device");
            put(o.hint, sizeof o.hint, "Hold BOOT: cancel");
        } else {
            put(o.status, sizeof o.status, "Same code on both?");
            put(o.hint, sizeof o.hint, "BOOT: yes   Hold BOOT: no");
        }
        return o;
    }

    switch (s.state) {
    case State::Linked:
        put(o.status, sizeof o.status, "Connected");
        if (dongle && s.usb >= 0) {
            put(o.hint, sizeof o.hint, s.usb > 0 ? "USB: ready   Hold BOOT: forget" : "USB: no PC   Hold BOOT: forget");
        } else {
            put(o.hint, sizeof o.hint, dongle ? "Hold BOOT: forget pairing" : "BOOT: type test   Hold: forget");
        }
        o.tone = Tone::Good;
        break;
    case State::Connecting:
        put(o.status, sizeof o.status, "Connecting...");
        put(o.hint, sizeof o.hint, "Hold BOOT: forget pairing");
        o.tone = Tone::Normal;
        break;
    case State::Pairing:
        put(o.status, sizeof o.status, "Pairing...");
        put(o.hint, sizeof o.hint, dongle ? "BOOT: cancel" : "");
        o.tone = Tone::Warn;
        break;
    case State::Confirming: // no code to show (should not happen): say so rather than show nothing
        put(o.status, sizeof o.status, "Pairing...");
        o.tone = Tone::Warn;
        break;
    case State::Idle:
        if (dongle && s.window_open) {
            std::snprintf(o.status, sizeof o.status, "Waiting for the vault (%us)", static_cast<unsigned>(s.window_left_s));
            put(o.hint, sizeof o.hint, "BOOT: cancel");
            o.tone = Tone::Warn;
        } else if (s.paired) {
            put(o.status, sizeof o.status, dongle ? "Paired. Waiting for the vault" : "Paired. Not connected");
            put(o.hint, sizeof o.hint, "BOOT: pair again   Hold: forget");
        } else {
            put(o.status, sizeof o.status, "Not paired");
            put(o.hint, sizeof o.hint, "BOOT: start pairing");
        }
        break;
    }

    // A notice about what just happened replaces the status line for a few seconds.
    switch (s.notice) {
    case Notice::None: break;
    case Notice::Paired: put(o.status, sizeof o.status, "Paired!"); o.tone = Tone::Good; break;
    case Notice::Rejected: put(o.status, sizeof o.status, "Pairing cancelled"); o.tone = Tone::Bad; break;
    case Notice::Failed: put(o.status, sizeof o.status, "No answer. Pairing failed"); o.tone = Tone::Bad; break;
    case Notice::WindowClosed: put(o.status, sizeof o.status, "Pairing time is over"); o.tone = Tone::Bad; break;
    case Notice::Forgotten: put(o.status, sizeof o.status, "Pairing forgotten"); o.tone = Tone::Bad; break;
    case Notice::Text: put(o.status, sizeof o.status, s.text); o.tone = s.text_ok ? Tone::Good : Tone::Bad; break;
    }

    if (s.held_s > 0) {
        std::snprintf(o.hint, sizeof o.hint, "Keep holding... %u/%us", static_cast<unsigned>(s.held_s),
                      static_cast<unsigned>(s.long_s));
    }
    return o;
}

} // namespace dongle
