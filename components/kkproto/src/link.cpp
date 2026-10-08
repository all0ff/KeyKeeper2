#include "kkproto/link.hpp"

#include <cstring>

namespace kk::link {

namespace {

constexpr size_t kXxMsg1 = 32;
constexpr size_t kXxMsg2 = 96;
constexpr size_t kXxMsg3 = 64;
constexpr size_t kIkMsg1 = 96;
constexpr size_t kIkMsg2 = 48;

bool reached(uint32_t now, uint32_t deadline)
{
    return static_cast<int32_t>(now - deadline) >= 0;
}

} // namespace

const char* event_name(Event e)
{
    switch (e) {
    case Event::PairingStarted: return "pairing started";
    case Event::CodeReady: return "code ready";
    case Event::PairingDone: return "pairing done";
    case Event::PairingRejected: return "pairing rejected";
    case Event::PairingFailed: return "pairing failed";
    case Event::PairingWindowClosed: return "pairing window closed";
    case Event::Connecting: return "connecting";
    case Event::Linked: return "linked";
    case Event::LinkLost: return "link lost";
    case Event::Retry: return "retry";
    }
    return "?";
}

const char* state_name(State s)
{
    switch (s) {
    case State::Idle: return "idle";
    case State::Pairing: return "pairing";
    case State::Confirming: return "confirming";
    case State::Connecting: return "connecting";
    case State::Linked: return "linked";
    }
    return "?";
}

const char* fail_name(Fail f)
{
    switch (f) {
    case Fail::None: return "none";
    case Fail::Handshake: return "handshake error";
    case Fail::WrongPeer: return "unknown peer";
    case Fail::Version: return "protocol version mismatch";
    case Fail::NoReply: return "no reply";
    }
    return "?";
}

Endpoint::Endpoint(Role role, Io& io, const noise::KeyPair& static_key, const Params& params)
    : io_(io), role_(role), prm_(params), s_(static_key)
{
    pairing::make_prologue(prologue_);
}

// ----------------------------------------------------------------------------- configuration

void Endpoint::set_trusted_peer(const uint8_t pk[noise::kDhLen])
{
    if (pk == nullptr) {
        has_trusted_ = false;
        std::memset(trusted_, 0, sizeof trusted_);
        want_link_ = false;
        if (state_ == State::Linked || state_ == State::Connecting) {
            end_session(true);
        }
        return;
    }
    if (has_trusted_ && std::memcmp(trusted_, pk, noise::kDhLen) == 0) {
        return;
    }
    std::memcpy(trusted_, pk, noise::kDhLen);
    has_trusted_ = true;
    if (state_ == State::Linked || state_ == State::Connecting) {
        end_session(true); // a different peer: the running session belongs to the old one
    }
}

// ----------------------------------------------------------------------------- commands

void Endpoint::open_pairing(uint32_t now_ms, uint32_t window_ms)
{
    if (role_ != Role::Dongle) {
        return;
    }
    end_session(true);
    window_open_ = true;
    window_deadline_ = now_ms + window_ms;
    fail_ = Fail::None;
}

void Endpoint::close_pairing()
{
    window_open_ = false;
    if (state_ == State::Pairing || state_ == State::Confirming) {
        if (state_ == State::Confirming && have_tr_) {
            seal_send(msg::Type::Bye, seq_++, nullptr, 0);
        }
        end_session(false);
    }
}

void Endpoint::start_pairing(uint32_t now_ms)
{
    if (role_ != Role::Vault) {
        return;
    }
    want_link_ = false;
    end_session(true);
    attempts_ = 0;
    fail_ = Fail::None;
    begin_xx_initiator(now_ms);
}

bool Endpoint::connect(uint32_t now_ms)
{
    if (role_ != Role::Vault || !has_trusted_) {
        return false;
    }
    want_link_ = true;
    if (state_ == State::Idle) {
        begin_ik_initiator(now_ms);
    }
    return true;
}

void Endpoint::disconnect()
{
    want_link_ = false;
    if (state_ == State::Linked && have_tr_) {
        seal_send(msg::Type::Bye, seq_++, nullptr, 0);
    }
    if (state_ == State::Linked || state_ == State::Connecting) {
        end_session(true);
    }
}

void Endpoint::confirm_pairing(uint32_t now_ms)
{
    (void)now_ms;
    if (state_ != State::Confirming || local_ok_ || !have_tr_) {
        return;
    }
    if (!seal_send(msg::Type::PairConfirm, seq_++, nullptr, 0)) {
        return;
    }
    local_ok_ = true;
    maybe_pairing_done();
}

void Endpoint::reject_pairing()
{
    if (state_ == State::Confirming) {
        if (have_tr_) {
            seal_send(msg::Type::Bye, seq_++, nullptr, 0);
        }
        end_session(false);
        window_open_ = false;
        io_.event(Event::PairingRejected);
    } else if (state_ == State::Pairing) {
        end_session(false);
        window_open_ = false;
    }
}

void Endpoint::reset()
{
    end_session(false);
}

bool Endpoint::send_message(msg::Type type, uint8_t flags, const uint8_t* body, size_t body_len)
{
    if (state_ != State::Linked || !have_tr_) {
        return false;
    }
    switch (type) {
    case msg::Type::TypeKeys:
    case msg::Type::Result:
    case msg::Type::State:
    case msg::Type::Abort:
        break;
    default:
        return false; // protocol-internal messages are not the application's to send
    }
    size_t n = 0;
    if (!msg::encode(type, flags, seq_++, body, body_len, tx_plain_, sizeof tx_plain_, &n)) {
        return false;
    }
    size_t m = 0;
    if (tr_.seal(tx_plain_, n, buf_, sizeof buf_, &m) != noise::Status::Ok) {
        return false;
    }
    io_.send(buf_, m);
    return true;
}

// ----------------------------------------------------------------------------- helpers

bool Endpoint::init_hs(noise::Pattern p, noise::Role r)
{
    hs_.clear();
    noise::HandshakeState::Config c;
    c.pattern = p;
    c.role = r;
    c.prologue = prologue_;
    c.prologue_len = sizeof prologue_;
    c.s = &s_;
    c.rs = (p == noise::Pattern::IK && r == noise::Role::Initiator) ? trusted_ : nullptr;
    return hs_.init(c) == noise::Status::Ok;
}

void Endpoint::drop_handshake()
{
    hs_.clear();
    state_ = State::Idle;
}

void Endpoint::end_session(bool notify)
{
    const bool was_linked = state_ == State::Linked;
    hs_.clear();
    tr_.clear();
    have_tr_ = false;
    hello_sent_ = false;
    have_code_ = false;
    std::memset(code_, 0, sizeof code_);
    have_peer_ = false;
    local_ok_ = false;
    remote_ok_ = false;
    ping_out_ = false;
    state_ = State::Idle;
    if (notify && was_linked) {
        io_.event(Event::LinkLost);
    }
}

void Endpoint::fail(Fail f)
{
    fail_ = f;
}

bool Endpoint::seal_send(msg::Type type, uint16_t seq, const uint8_t* body, size_t body_len)
{
    if (!have_tr_) {
        return false;
    }
    size_t n = 0;
    if (!msg::encode(type, 0, seq, body, body_len, tx_plain_, sizeof tx_plain_, &n)) {
        return false;
    }
    size_t m = 0;
    if (tr_.seal(tx_plain_, n, buf_, sizeof buf_, &m) != noise::Status::Ok) {
        return false;
    }
    io_.send(buf_, m);
    return true;
}

void Endpoint::maybe_pairing_done()
{
    if (state_ != State::Confirming || !local_ok_ || !remote_ok_ || !have_peer_) {
        return;
    }
    std::memcpy(trusted_, peer_pk_, noise::kDhLen);
    has_trusted_ = true;
    window_open_ = false;
    io_.event(Event::PairingDone); // the application stores peer_static() now
    end_session(false);
}

// ----------------------------------------------------------------------------- handshakes

void Endpoint::begin_xx_initiator(uint32_t now_ms)
{
    tr_.clear();
    have_tr_ = false;
    have_code_ = false;
    local_ok_ = remote_ok_ = false;
    if (!init_hs(noise::Pattern::XX, noise::Role::Initiator)) {
        fail(Fail::Handshake);
        state_ = State::Idle;
        return;
    }
    size_t n = 0;
    if (hs_.write_message(nullptr, 0, buf_, sizeof buf_, &n) != noise::Status::Ok || n != kXxMsg1) {
        fail(Fail::Handshake);
        drop_handshake();
        return;
    }
    ++attempts_;
    state_ = State::Pairing;
    last_tx_ = now_ms;
    last_rx_ = now_ms;
    io_.send(buf_, n);
    io_.event(Event::PairingStarted);
}

void Endpoint::begin_ik_initiator(uint32_t now_ms)
{
    tr_.clear();
    have_tr_ = false;
    hello_sent_ = false;
    if (!has_trusted_ || !init_hs(noise::Pattern::IK, noise::Role::Initiator)) {
        fail(Fail::Handshake);
        state_ = State::Idle;
        return;
    }
    size_t n = 0;
    if (hs_.write_message(nullptr, 0, buf_, sizeof buf_, &n) != noise::Status::Ok || n != kIkMsg1) {
        fail(Fail::Handshake);
        drop_handshake();
        return;
    }
    state_ = State::Connecting;
    last_tx_ = now_ms;
    last_rx_ = now_ms;
    io_.send(buf_, n);
    io_.event(Event::Connecting);
}

void Endpoint::dongle_accept_xx_msg1(const uint8_t* d, size_t n, uint32_t now_ms)
{
    if (!window_open_ || n != kXxMsg1) {
        return;
    }
    tr_.clear();
    have_tr_ = false;
    have_code_ = false;
    local_ok_ = remote_ok_ = false;
    if (!init_hs(noise::Pattern::XX, noise::Role::Responder)) {
        drop_handshake();
        return;
    }
    uint8_t scratch[8];
    size_t pn = 0;
    size_t m = 0;
    if (hs_.read_message(d, n, scratch, sizeof scratch, &pn) != noise::Status::Ok ||
        hs_.write_message(nullptr, 0, buf_, sizeof buf_, &m) != noise::Status::Ok || m != kXxMsg2) {
        fail(Fail::Handshake);
        drop_handshake();
        return;
    }
    state_ = State::Pairing;
    last_rx_ = now_ms;
    // The window is only checked while Idle (see tick()), so a pairing that has begun is never cut
    // short by it: the users can take their time over the code.
    io_.send(buf_, m);
    io_.event(Event::PairingStarted);
}

void Endpoint::dongle_accept_ik_msg1(const uint8_t* d, size_t n, uint32_t now_ms)
{
    if (!has_trusted_ || n != kIkMsg1) {
        return;
    }
    // Run it on a private handshake object and commit only on success, so that a stray or forged
    // frame cannot disturb a session that is working or a pairing that is in progress.
    noise::HandshakeState ik;
    noise::HandshakeState::Config c;
    c.pattern = noise::Pattern::IK;
    c.role = noise::Role::Responder;
    c.prologue = prologue_;
    c.prologue_len = sizeof prologue_;
    c.s = &s_;
    uint8_t scratch[8];
    size_t pn = 0, m = 0;
    if (ik.init(c) != noise::Status::Ok || ik.read_message(d, n, scratch, sizeof scratch, &pn) != noise::Status::Ok) {
        return;
    }
    const uint8_t* rs = ik.remote_static();
    if (rs == nullptr || std::memcmp(rs, trusted_, noise::kDhLen) != 0) {
        fail(Fail::WrongPeer);
        return;
    }
    noise::Transport fresh;
    if (ik.write_message(nullptr, 0, buf_, sizeof buf_, &m) != noise::Status::Ok || m != kIkMsg2 ||
        !ik.complete() || ik.split(&fresh) != noise::Status::Ok) {
        fail(Fail::Handshake);
        return;
    }
    const bool was_linked = state_ == State::Linked;
    hs_.clear();
    tr_ = fresh;
    have_tr_ = true;
    hello_sent_ = false;
    have_code_ = false;
    local_ok_ = remote_ok_ = false;
    std::memcpy(peer_pk_, trusted_, noise::kDhLen);
    have_peer_ = true;
    state_ = State::Connecting;
    last_rx_ = now_ms;
    fail_ = Fail::None;
    io_.send(buf_, m);
    if (was_linked) {
        io_.event(Event::LinkLost);
    }
    io_.event(Event::Connecting);
}

bool Endpoint::finish_xx(uint32_t now_ms)
{
    if (!hs_.complete()) {
        fail(Fail::Handshake);
        drop_handshake();
        return false;
    }
    // The code is computed before split(), which wipes the handshake keys.
    pairing::sas_text(pairing::sas_code(hs_.handshake_hash()), code_);
    have_code_ = true;
    const uint8_t* rs = hs_.remote_static();
    if (rs == nullptr) {
        fail(Fail::Handshake);
        drop_handshake();
        have_code_ = false;
        return false;
    }
    std::memcpy(peer_pk_, rs, noise::kDhLen);
    have_peer_ = true;
    if (hs_.split(&tr_) != noise::Status::Ok) {
        fail(Fail::Handshake);
        drop_handshake();
        have_code_ = false;
        have_peer_ = false;
        return false;
    }
    have_tr_ = true;
    state_ = State::Confirming;
    local_ok_ = remote_ok_ = false;
    confirm_deadline_ = now_ms + prm_.confirm_timeout_ms;
    last_rx_ = now_ms;
    io_.event(Event::CodeReady);
    return true;
}

void Endpoint::dongle_xx_msg3(const uint8_t* d, size_t n, uint32_t now_ms)
{
    uint8_t scratch[8];
    size_t pn = 0;
    if (hs_.read_message(d, n, scratch, sizeof scratch, &pn) != noise::Status::Ok) {
        drop_handshake(); // the vault starts over with a new first message
        return;
    }
    finish_xx(now_ms);
}

void Endpoint::vault_xx_msg2(const uint8_t* d, size_t n, uint32_t now_ms)
{
    uint8_t scratch[8];
    size_t pn = 0;
    if (hs_.read_message(d, n, scratch, sizeof scratch, &pn) != noise::Status::Ok) {
        // Most likely a reply to an EARLIER attempt (the dongle answered every first message that
        // was waiting for it). Do not give up: this attempt is spoiled, the retry timer starts a new one.
        return;
    }
    size_t m = 0;
    if (hs_.write_message(nullptr, 0, buf_, sizeof buf_, &m) != noise::Status::Ok || m != kXxMsg3) {
        fail(Fail::Handshake);
        drop_handshake();
        return;
    }
    io_.send(buf_, m);
    finish_xx(now_ms);
}

void Endpoint::vault_ik_msg2(const uint8_t* d, size_t n, uint32_t now_ms)
{
    uint8_t scratch[8];
    size_t pn = 0;
    if (hs_.read_message(d, n, scratch, sizeof scratch, &pn) != noise::Status::Ok) {
        return; // stale reply to an earlier attempt: the retry timer takes care of it
    }
    if (!hs_.complete() || hs_.split(&tr_) != noise::Status::Ok) {
        fail(Fail::Handshake);
        drop_handshake();
        return;
    }
    have_tr_ = true;
    std::memcpy(peer_pk_, trusted_, noise::kDhLen);
    have_peer_ = true;
    last_rx_ = now_ms;
    uint8_t body[2];
    msg::Hello hello;
    if (!msg::encode_hello(hello, body) || !seal_send(msg::Type::Hello, seq_++, body, sizeof body)) {
        fail(Fail::Handshake);
        end_session(false);
        return;
    }
    hello_sent_ = true;
    last_tx_ = now_ms; // the HelloAck must come within retry_ms, or the whole attempt restarts
}

// ----------------------------------------------------------------------------- the wire

bool Endpoint::try_transport(const uint8_t* d, size_t n, uint32_t now_ms)
{
    size_t pn = 0;
    if (tr_.open(d, n, rx_plain_, sizeof rx_plain_, &pn) != noise::Status::Ok) {
        return false;
    }
    msg::Header h;
    const uint8_t* body = nullptr;
    if (msg::parse(rx_plain_, pn, &h, &body) != msg::ParseStatus::Ok) {
        // Authentic but malformed: the peer is broken. The counters are now out of step anyway.
        fail(Fail::Handshake);
        end_session(true);
        return true;
    }
    last_rx_ = now_ms;
    handle_message(h, body, now_ms);
    return true;
}

void Endpoint::handle_message(const msg::Header& h, const uint8_t* body, uint32_t now_ms)
{
    switch (h.type) {
    case msg::Type::PairConfirm:
        if (state_ == State::Confirming) {
            remote_ok_ = true;
            maybe_pairing_done();
        }
        return;
    case msg::Type::Bye:
        if (state_ == State::Confirming) {
            end_session(false);
            window_open_ = false;
            io_.event(Event::PairingRejected);
        } else if (state_ == State::Linked || state_ == State::Connecting) {
            end_session(true);
        }
        return;
    case msg::Type::Hello:
        if (role_ == Role::Dongle && state_ == State::Connecting) {
            msg::Hello v;
            if (!msg::decode_hello(body, h.len, &v) || v.version != msg::kProtocolVersion) {
                fail(Fail::Version);
                seal_send(msg::Type::Bye, seq_++, nullptr, 0);
                end_session(false);
                return;
            }
            msg::HelloAck ack;
            ack.fw_major = fw_major_;
            ack.fw_minor = fw_minor_;
            ack.usb_mounted = usb_mounted_;
            uint8_t out[4];
            if (!msg::encode_hello_ack(ack, out) || !seal_send(msg::Type::HelloAck, h.seq, out, sizeof out)) {
                end_session(false);
                return;
            }
            state_ = State::Linked;
            last_rx_ = now_ms;
            io_.event(Event::Linked);
        }
        return;
    case msg::Type::HelloAck:
        if (role_ == Role::Vault && state_ == State::Connecting && hello_sent_) {
            msg::HelloAck v;
            if (!msg::decode_hello_ack(body, h.len, &v) || v.version != msg::kProtocolVersion) {
                fail(Fail::Version);
                seal_send(msg::Type::Bye, seq_++, nullptr, 0);
                end_session(false);
                return;
            }
            state_ = State::Linked;
            last_ping_ = now_ms;
            last_rx_ = now_ms;
            fail_ = Fail::None;
            io_.event(Event::Linked);
        }
        return;
    case msg::Type::Ping:
        if (state_ == State::Linked) {
            seal_send(msg::Type::Pong, h.seq, nullptr, 0);
        }
        return;
    case msg::Type::Pong:
        if (state_ == State::Linked && ping_out_ && h.seq == ping_seq_) {
            ping_out_ = false;
            rtt_ms_ = now_ms - ping_sent_at_;
        }
        return;
    default:
        if (state_ == State::Linked) {
            io_.message(h, body);
        }
        return;
    }
}

void Endpoint::on_frame(const uint8_t* d, size_t n, uint32_t now_ms)
{
    if (d == nullptr || n == 0 || n > kBufLen) {
        return;
    }
    if (have_tr_ && try_transport(d, n, now_ms)) {
        return; // a valid transport message (a failed check did not advance the counter)
    }
    if (role_ == Role::Dongle) {
        if (n == kXxMsg1) {
            dongle_accept_xx_msg1(d, n, now_ms);
        } else if (n == kIkMsg1) {
            dongle_accept_ik_msg1(d, n, now_ms);
        } else if (n == kXxMsg3 && state_ == State::Pairing && !have_tr_) {
            dongle_xx_msg3(d, n, now_ms);
        }
        return;
    }
    if (state_ == State::Pairing && n == kXxMsg2 && !have_tr_) {
        vault_xx_msg2(d, n, now_ms);
    } else if (state_ == State::Connecting && n == kIkMsg2 && !have_tr_) {
        vault_ik_msg2(d, n, now_ms);
    }
}

// ----------------------------------------------------------------------------- time

void Endpoint::tick(uint32_t now_ms)
{
    if (role_ == Role::Dongle) {
        if (window_open_ && state_ == State::Idle && reached(now_ms, window_deadline_)) {
            window_open_ = false;
            io_.event(Event::PairingWindowClosed);
        }
        switch (state_) {
        case State::Pairing:
            if (reached(now_ms, last_rx_ + prm_.handshake_timeout_ms)) {
                drop_handshake();
            }
            break;
        case State::Confirming:
            if (reached(now_ms, confirm_deadline_)) {
                reject_pairing();
            }
            break;
        case State::Connecting:
            if (reached(now_ms, last_rx_ + prm_.handshake_timeout_ms)) {
                end_session(false);
            }
            break;
        case State::Linked:
            if (reached(now_ms, last_rx_ + prm_.silence_ms)) {
                end_session(true);
            }
            break;
        case State::Idle:
            break;
        }
        return;
    }

    switch (state_) {
    case State::Pairing:
        if (reached(now_ms, last_tx_ + prm_.retry_ms)) {
            if (attempts_ >= prm_.max_pair_attempts) {
                fail(Fail::NoReply);
                end_session(false);
                io_.event(Event::PairingFailed);
            } else {
                io_.event(Event::Retry);
                begin_xx_initiator(now_ms);
            }
        }
        break;
    case State::Confirming:
        if (reached(now_ms, confirm_deadline_)) {
            reject_pairing();
        }
        break;
    case State::Connecting:
        if (reached(now_ms, last_tx_ + prm_.retry_ms)) {
            if (want_link_ && has_trusted_) {
                io_.event(Event::Retry);
                begin_ik_initiator(now_ms);
            } else {
                end_session(false);
            }
        }
        break;
    case State::Linked:
        if (reached(now_ms, last_rx_ + prm_.silence_ms)) {
            end_session(true);
            break;
        }
        if (reached(now_ms, last_ping_ + prm_.ping_ms)) {
            last_ping_ = now_ms;
            ping_seq_ = seq_++;
            ping_sent_at_ = now_ms;
            ping_out_ = true;
            seal_send(msg::Type::Ping, ping_seq_, nullptr, 0);
        }
        break;
    case State::Idle:
        if (want_link_ && has_trusted_) {
            begin_ik_initiator(now_ms);
        }
        break;
    }
}

} // namespace kk::link
