#pragma once

#include "kkproto/messages.hpp"
#include "kkproto/noise.hpp"
#include "kkproto/pairing.hpp"

#include <cstddef>
#include <cstdint>

namespace kk::link {

// =============================================================================
// link::Endpoint -- one end of the vault <-> dongle link, as a state machine.
//
// It knows nothing about the wire (UART, BLE) and nothing about storage: bytes come in through
// on_frame() (one complete Noise message / transport frame per call, framing is the caller's job),
// bytes go out through Io::send(), and everything the application has to react to arrives as an
// Event. Time is passed in (milliseconds, wrap-around safe) so the same code runs on a PC test.
//
// Two roles, because the two ends do different things:
//
//   Dongle  (responder): waits. While the pairing window is open it accepts a Noise XX pairing;
//           otherwise it accepts a Noise IK session, but only from the vault whose key it trusts.
//   Vault   (initiator): starts a pairing (XX) or keeps a session to the trusted dongle (IK),
//           and restarts the attempt by itself when the other end does not answer.
//
// Pairing: XX handshake -> both sides show the same 6-digit code -> each user confirms on their own
// device (PairConfirm travels both ways inside the new session) -> PairingDone with the peer's key.
// The application stores that key and hands it back with set_trusted_peer() on the next boot.
//
// Session: IK handshake (needs only the stored key) -> Hello / HelloAck -> Linked. A lost frame
// breaks the Noise counters for good, so every failure ends the session and the vault simply
// opens a new one (IK is one round trip). Ping / Pong detect a dead link.
//
// Not thread-safe: call everything from one task.
// =============================================================================

enum class Role : uint8_t { Vault, Dongle };

enum class State : uint8_t {
    Idle,       ///< nothing in progress
    Pairing,    ///< XX handshake running
    Confirming, ///< XX finished, the code is showing, waiting for the users
    Connecting, ///< IK handshake / Hello running
    Linked,     ///< session works: send_message() / Io::message()
};

enum class Event : uint8_t {
    PairingStarted,      ///< a pairing attempt began (dongle: accepted one; vault: sent the first message)
    CodeReady,           ///< code() is valid: show it
    PairingDone,         ///< both users confirmed: store peer_static()
    PairingRejected,     ///< a user said no, or nobody confirmed in time
    PairingFailed,       ///< vault: the dongle did not answer (attempt limit reached)
    PairingWindowClosed, ///< dongle: nobody paired within the window
    Connecting,          ///< vault: starting a session attempt
    Linked,              ///< session works
    LinkLost,            ///< session ended (peer left, silence, decryption error)
    Retry,               ///< vault: restarted an attempt (diagnostics)
};

const char* event_name(Event e);
const char* state_name(State s);

/// What the application provides.
class Io {
public:
    virtual ~Io() = default;
    /// One complete frame to put on the wire (the wire adds its own framing / CRC).
    virtual void send(const uint8_t* data, size_t n) = 0;
    virtual void event(Event e) = 0;
    /// An application message received while Linked (TypeKeys, Result, State, Abort, ...). body is
    /// valid only during the call. Hello / HelloAck / Ping / Pong / Bye / PairConfirm never get here.
    virtual void message(const msg::Header& header, const uint8_t* body)
    {
        (void)header;
        (void)body;
    }
};

struct Params {
    uint32_t retry_ms = 1000;            ///< vault: restart an attempt after this long without progress
    uint8_t max_pair_attempts = 8;       ///< vault: give up pairing after this many handshakes
    uint32_t handshake_timeout_ms = 5000; ///< dongle: drop a half-finished handshake after this long
    uint32_t confirm_timeout_ms = 60000; ///< both: the users must confirm the code within this time
    uint32_t ping_ms = 2000;             ///< vault: send a Ping this often while Linked
    uint32_t silence_ms = 6500;          ///< both: no valid message for this long ends the session
};

enum class Fail : uint8_t { None, Handshake, WrongPeer, Version, NoReply };
const char* fail_name(Fail f);

class Endpoint {
public:
    static constexpr size_t kBufLen = 256;

    Endpoint(Role role, Io& io, const noise::KeyPair& static_key, const Params& params = Params());

    // ---- configuration
    /// The peer whose key is trusted (the paired dongle / vault). nullptr forgets it and ends a session.
    void set_trusted_peer(const uint8_t pk[noise::kDhLen]);
    void forget_peer() { set_trusted_peer(nullptr); }
    bool has_trusted_peer() const { return has_trusted_; }
    const uint8_t* trusted_peer() const { return has_trusted_ ? trusted_ : nullptr; }
    /// Shown in HelloAck (dongle).
    void set_device_info(uint8_t fw_major, uint8_t fw_minor)
    {
        fw_major_ = fw_major;
        fw_minor_ = fw_minor;
    }
    void set_usb_mounted(bool mounted) { usb_mounted_ = mounted; }

    // ---- dongle commands
    /// Accept a pairing for window_ms. Ends an existing session.
    void open_pairing(uint32_t now_ms, uint32_t window_ms);
    void close_pairing();
    bool pairing_open() const { return window_open_; }

    // ---- vault commands
    /// Start a pairing with whatever dongle has its window open. Ends an existing session.
    void start_pairing(uint32_t now_ms);
    /// Keep a session to the trusted dongle: connects now and again after every loss, until disconnect().
    bool connect(uint32_t now_ms);
    /// Say goodbye (Bye) and stop reconnecting.
    void disconnect();
    bool wants_link() const { return want_link_; }

    // ---- both
    /// The user accepted the code on this device.
    void confirm_pairing(uint32_t now_ms);
    /// The user refused the code (or cancelled): tells the other end and drops the pairing.
    void reject_pairing();
    /// Drop everything in progress, back to Idle (the trusted peer stays).
    void reset();

    /// Seals and sends an application message. Only while Linked.
    bool send_message(msg::Type type, uint8_t flags, const uint8_t* body, size_t body_len);

    // ---- the wire
    void on_frame(const uint8_t* data, size_t n, uint32_t now_ms);
    void tick(uint32_t now_ms);

    // ---- observers
    Role role() const { return role_; }
    State state() const { return state_; }
    Fail fail_reason() const { return fail_; }
    /// 6 digits + NUL while Confirming (and until the next pairing), else nullptr.
    const char* code() const { return have_code_ ? code_ : nullptr; }
    /// Peer's static key: after the XX handshake and while a session runs, else nullptr.
    const uint8_t* peer_static() const { return have_peer_ ? peer_pk_ : nullptr; }
    bool local_confirmed() const { return local_ok_; }
    bool remote_confirmed() const { return remote_ok_; }
    uint8_t attempts() const { return attempts_; }
    /// Ping round trip of the last answered Ping (vault), milliseconds; 0 if none yet.
    uint32_t last_rtt_ms() const { return rtt_ms_; }

private:
    Io& io_;
    Role role_;
    Params prm_;
    noise::KeyPair s_;
    uint8_t prologue_[pairing::kPrologueLen];

    uint8_t trusted_[noise::kDhLen] = {};
    bool has_trusted_ = false;
    uint8_t fw_major_ = 0, fw_minor_ = 0;
    bool usb_mounted_ = false;

    State state_ = State::Idle;
    Fail fail_ = Fail::None;
    noise::HandshakeState hs_;
    noise::Transport tr_;
    bool have_tr_ = false;
    bool hello_sent_ = false;

    char code_[7] = {};
    bool have_code_ = false;
    uint8_t peer_pk_[noise::kDhLen] = {};
    bool have_peer_ = false;
    bool local_ok_ = false, remote_ok_ = false;

    bool window_open_ = false;
    uint32_t window_deadline_ = 0;
    bool want_link_ = false;
    uint8_t attempts_ = 0;

    uint32_t last_tx_ = 0;   ///< vault: when the current attempt (or ping) was sent
    uint32_t last_rx_ = 0;   ///< last valid message / handshake progress
    uint32_t confirm_deadline_ = 0;
    uint32_t last_ping_ = 0;
    uint16_t seq_ = 0;
    uint32_t ping_sent_at_ = 0;
    uint16_t ping_seq_ = 0;
    bool ping_out_ = false;
    uint32_t rtt_ms_ = 0;

    uint8_t buf_[kBufLen] = {};      ///< outgoing frame
    uint8_t rx_plain_[kBufLen] = {}; ///< decrypted incoming message (body pointers point in here)
    uint8_t tx_plain_[kBufLen] = {}; ///< outgoing message before sealing (separate: a handler may reply)

    // handshake
    bool init_hs(noise::Pattern p, noise::Role r); ///< (re)initialises hs_ only; the session is untouched
    void begin_xx_initiator(uint32_t now_ms);
    void begin_ik_initiator(uint32_t now_ms);
    void dongle_accept_xx_msg1(const uint8_t* d, size_t n, uint32_t now_ms);
    void dongle_accept_ik_msg1(const uint8_t* d, size_t n, uint32_t now_ms);
    void dongle_xx_msg3(const uint8_t* d, size_t n, uint32_t now_ms);
    void vault_xx_msg2(const uint8_t* d, size_t n, uint32_t now_ms);
    void vault_ik_msg2(const uint8_t* d, size_t n, uint32_t now_ms);
    bool finish_xx(uint32_t now_ms);

    // transport
    bool try_transport(const uint8_t* d, size_t n, uint32_t now_ms);
    void handle_message(const msg::Header& h, const uint8_t* body, uint32_t now_ms);
    bool seal_send(msg::Type type, uint16_t seq, const uint8_t* body, size_t body_len);

    // helpers
    void end_session(bool notify);  ///< wipes hs_ and the session, state -> Idle; LinkLost if it was Linked and notify
    void drop_handshake();          
    void maybe_pairing_done();
    void enter_idle();
    void fail(Fail f);
};

} // namespace kk::link
