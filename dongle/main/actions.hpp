#pragma once

#include "button.hpp"
#include "kkproto/link.hpp"

#include <cstdint>

// =============================================================================
// What a press of the BOOT button means in each situation. A pure function so the whole table can be
// tested on a PC; app_main.cpp only carries the chosen action out.
//
//   Confirming (the code is showing): short = "the codes match", long = "they do not"
//   pairing handshake running:        any press cancels
//   otherwise (Idle / Connecting / Linked):
//        short press   dongle: open (or close) the pairing window   vault: start a pairing
//                      only when nothing is connected or being connected
//        long press    forget the pairing (only if there is one)
// =============================================================================

namespace dongle {

enum class Action : uint8_t { None, OpenPairing, CancelPairing, StartPairing, Confirm, Reject, Forget };

struct Situation {
    kk::link::Role role = kk::link::Role::Dongle;
    kk::link::State state = kk::link::State::Idle;
    bool paired = false;
    bool window_open = false;
    bool local_confirmed = false;
};

inline Action action_for(const Situation& s, Press p)
{
    using kk::link::Role;
    using kk::link::State;
    if (p == Press::None) {
        return Action::None;
    }
    switch (s.state) {
    case State::Confirming:
        if (p == Press::Long) return Action::Reject;
        return s.local_confirmed ? Action::None : Action::Confirm;
    case State::Pairing:
        return Action::CancelPairing;
    case State::Idle:
        if (p == Press::Long) return s.paired ? Action::Forget : Action::None;
        if (s.role == Role::Dongle) return s.window_open ? Action::CancelPairing : Action::OpenPairing;
        return Action::StartPairing;
    case State::Connecting:
    case State::Linked:
        if (p == Press::Long) return s.paired ? Action::Forget : Action::None;
        return Action::None;
    }
    return Action::None;
}

} // namespace dongle
