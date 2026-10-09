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
// flashed. Text is English or Russian (Snapshot::ru; ui.cpp draws it with the project's Cyrillic fonts).
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
    char text[96] = {};           ///< the message for Notice::Text
    bool text_ok = true;          ///< Notice::Text is good news (green) or bad (red)
    int usb = -1;                 ///< dongle: -1 not known, 0 not plugged into a PC, 1 plugged
    uint32_t held_s = 0;          ///< how long the button has been held (shown while it is)
    uint32_t long_s = 3;          ///< hold time that counts as a long press
    bool ru = false;              ///< show Russian text
    char id[17] = {};             ///< this device's key prefix in hex, as the vault lists it under "Forget dongle"
};

struct Screen {
    char title[40] = {};
    char big[16] = {};  ///< the verification code ("123 456") or empty
    char status[96] = {};
    char hint[96] = {};
    Tone tone = Tone::Normal;
};

inline void put(char* dst, size_t cap, const char* src)
{
    std::snprintf(dst, cap, "%s", src);
}

/// English or Russian, by the snapshot's language.
inline const char* pick(const Snapshot& s, const char* en, const char* ru)
{
    return s.ru ? ru : en;
}

inline Screen describe(const Snapshot& s)
{
    using kk::link::Role;
    using kk::link::State;
    Screen o;
    const bool dongle = s.role == Role::Dongle;
    if (dongle && s.id[0] != '\0') {
        std::snprintf(o.title, sizeof o.title, "KeyKeeper %s", s.id);
    } else {
        put(o.title, sizeof o.title, dongle ? "KeyKeeper dongle" : pick(s, "Vault (simulator)", "Хранилище (симулятор)"));
    }
    auto say = [&](char* dst, size_t cap, const char* en, const char* ru) { put(dst, cap, pick(s, en, ru)); };

    if (s.state == State::Confirming && s.code != nullptr && std::strlen(s.code) == 6) {
        std::snprintf(o.big, sizeof o.big, "%.3s %.3s", s.code, s.code + 3);
        o.tone = Tone::Warn;
        if (s.local_confirmed) {
            say(o.status, sizeof o.status, "Waiting for the other device", "Ждём второе устройство");
            say(o.hint, sizeof o.hint, "Hold BOOT: cancel", "Удерж. BOOT: отмена");
        } else {
            say(o.status, sizeof o.status, "Same code on both?", "Код совпадает на обоих?");
            say(o.hint, sizeof o.hint, "BOOT: yes   Hold BOOT: no", "BOOT: да   Удерж. BOOT: нет");
        }
        return o;
    }

    switch (s.state) {
    case State::Linked:
        say(o.status, sizeof o.status, "Connected", "Подключено");
        if (dongle && s.usb >= 0) {
            if (s.usb > 0) {
                say(o.hint, sizeof o.hint, "USB: ready   Hold BOOT: forget", "USB: готов   Удерж. BOOT: забыть");
            } else {
                say(o.hint, sizeof o.hint, "USB: no PC   Hold BOOT: forget", "USB: нет ПК   Удерж. BOOT: забыть");
            }
        } else if (dongle) {
            say(o.hint, sizeof o.hint, "Hold BOOT: forget pairing", "Удерж. BOOT: забыть пару");
        } else {
            say(o.hint, sizeof o.hint, "BOOT: type test   Hold: forget", "BOOT: тест печати   Удерж.: забыть");
        }
        o.tone = Tone::Good;
        break;
    case State::Connecting:
        say(o.status, sizeof o.status, "Connecting...", "Подключение...");
        say(o.hint, sizeof o.hint, "Hold BOOT: forget pairing", "Удерж. BOOT: забыть пару");
        o.tone = Tone::Normal;
        break;
    case State::Pairing:
        say(o.status, sizeof o.status, "Pairing...", "Сопряжение...");
        if (dongle) {
            say(o.hint, sizeof o.hint, "BOOT: cancel", "BOOT: отмена");
        }
        o.tone = Tone::Warn;
        break;
    case State::Confirming: // no code to show (should not happen): say so rather than show nothing
        say(o.status, sizeof o.status, "Pairing...", "Сопряжение...");
        o.tone = Tone::Warn;
        break;
    case State::Idle:
        if (dongle && s.window_open) {
            std::snprintf(o.status, sizeof o.status, s.ru ? "Ожидание хранилища (%u с)" : "Waiting for the vault (%us)",
                          static_cast<unsigned>(s.window_left_s));
            say(o.hint, sizeof o.hint, "BOOT: cancel", "BOOT: отмена");
            o.tone = Tone::Warn;
        } else if (s.paired) {
            if (dongle) {
                say(o.status, sizeof o.status, "Paired. Waiting for the vault", "Сопряжено. Ждём хранилище");
            } else {
                say(o.status, sizeof o.status, "Paired. Not connected", "Сопряжено. Нет связи");
            }
            say(o.hint, sizeof o.hint, "BOOT: pair again   Hold: forget", "BOOT: заново   Удерж.: забыть");
        } else {
            say(o.status, sizeof o.status, "Not paired", "Не сопряжено");
            say(o.hint, sizeof o.hint, "BOOT: start pairing", "BOOT: начать сопряжение");
        }
        break;
    }

    // A notice about what just happened replaces the status line for a few seconds.
    switch (s.notice) {
    case Notice::None: break;
    case Notice::Paired: say(o.status, sizeof o.status, "Paired!", "Сопряжено!"); o.tone = Tone::Good; break;
    case Notice::Rejected: say(o.status, sizeof o.status, "Pairing cancelled", "Сопряжение отменено"); o.tone = Tone::Bad; break;
    case Notice::Failed:
        say(o.status, sizeof o.status, "No answer. Pairing failed", "Нет ответа. Сопряжение не удалось");
        o.tone = Tone::Bad;
        break;
    case Notice::WindowClosed: say(o.status, sizeof o.status, "Pairing time is over", "Время сопряжения вышло"); o.tone = Tone::Bad; break;
    case Notice::Forgotten: say(o.status, sizeof o.status, "Pairing forgotten", "Пара забыта"); o.tone = Tone::Bad; break;
    case Notice::Text: put(o.status, sizeof o.status, s.text); o.tone = s.text_ok ? Tone::Good : Tone::Bad; break;
    }

    if (s.held_s > 0) {
        std::snprintf(o.hint, sizeof o.hint, s.ru ? "Держите... %u/%u с" : "Keep holding... %u/%us",
                      static_cast<unsigned>(s.held_s), static_cast<unsigned>(s.long_s));
    }
    return o;
}

} // namespace dongle
