// Pairing test: the whole conversation between two boards as a state machine.
//
// Pure C++ on top of kkproto (no ESP-IDF), so the same code is unit-tested on the PC and runs on the
// boards. The caller supplies a way to send one "link payload" (one kkproto message, carried by
// uartlink framing) and feeds in every payload that arrives.
//
//   initiator                                    responder
//   ---------                                    ---------
//   Noise XX msg1 (32 B)  -------------------->
//                         <-------------------  msg2 (96 B)
//   msg3 (64 B)           -------------------->            both now show the 6-digit code
//   HELLO (encrypted)     -------------------->
//                         <-------------------  HELLO_ACK (encrypted)         session works
//
// Robustness for a wire with no handshake of its own:
//  * The initiator restarts the whole handshake when nothing answers (the other board may still be
//    booting) or when a later message was lost, up to `max_attempts` times.
//  * A responder accepts a fresh msg1 in ANY phase (the initiator restarted or was pressed again).
//  * Both boards started as initiator at once: the one whose msg1 compares lower yields.
//
// A 32-byte payload is always taken for msg1. That is fine here, because nothing else sent in this test
// has that size.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "kkproto/messages.hpp"
#include "kkproto/noise.hpp"
#include "kkproto/pairing.hpp"

namespace pairtest {

constexpr size_t kMsg1Len = 32;
constexpr size_t kMsg2Len = 96;
constexpr size_t kMsg3Len = 64;
constexpr size_t kBufLen = 256;

enum class Phase : uint8_t { Idle, InitWaitReply, InitWaitAck, RespWaitFinal, RespWaitHello, Paired, Failed };
enum class Fail : uint8_t { None, Handshake, NoReply, Transport, BadHello };
enum class Event : uint8_t {
    SentMsg1,
    GotMsg1,
    SentMsg2,
    GotMsg2,
    SentMsg3,
    GotMsg3,
    SasReady,
    SentHello,
    GotHello,
    SentHelloAck,
    SessionOk,
    Retry,
    Yield,
    Dropped,
    Failed
};

inline const char* event_name(Event e)
{
    switch (e) {
    case Event::SentMsg1: return "sent msg1";
    case Event::GotMsg1: return "got msg1";
    case Event::SentMsg2: return "sent msg2";
    case Event::GotMsg2: return "got msg2";
    case Event::SentMsg3: return "sent msg3";
    case Event::GotMsg3: return "got msg3";
    case Event::SasReady: return "code ready";
    case Event::SentHello: return "sent HELLO";
    case Event::GotHello: return "got HELLO";
    case Event::SentHelloAck: return "sent HELLO_ACK";
    case Event::SessionOk: return "session OK";
    case Event::Retry: return "retry";
    case Event::Yield: return "other board also started: yielding";
    case Event::Dropped: return "dropped half-finished pairing";
    case Event::Failed: return "FAILED";
    }
    return "?";
}

inline const char* phase_name(Phase p)
{
    switch (p) {
    case Phase::Idle: return "idle";
    case Phase::InitWaitReply: return "waiting for reply";
    case Phase::InitWaitAck: return "waiting for HELLO_ACK";
    case Phase::RespWaitFinal: return "waiting for msg3";
    case Phase::RespWaitHello: return "waiting for HELLO";
    case Phase::Paired: return "paired";
    case Phase::Failed: return "failed";
    }
    return "?";
}

inline const char* fail_name(Fail f)
{
    switch (f) {
    case Fail::None: return "none";
    case Fail::Handshake: return "handshake did not verify";
    case Fail::NoReply: return "no reply from the other board";
    case Fail::Transport: return "encrypted message did not verify";
    case Fail::BadHello: return "unexpected HELLO content";
    }
    return "?";
}

class Io {
public:
    virtual ~Io() = default;
    virtual void send(const uint8_t* payload, size_t n) = 0;  // one link payload
    virtual void event(Event ev) = 0;                         // for logging and timing
};

class Peer {
public:
    struct Params {
        uint32_t retry_ms = 1000;           // initiator: restart if no progress for this long
        uint8_t max_attempts = 8;           // initiator: give up after this many handshakes
        uint32_t responder_timeout_ms = 5000;  // responder: drop a half-finished pairing after this long
    };

    Peer(Io& io, const kk::noise::KeyPair& static_key, const Params& params)
        : io_(io), s_(static_key), prm_(params)
    {
        kk::pairing::make_prologue(prologue_);
    }

    Phase phase() const { return phase_; }
    Fail fail_reason() const { return fail_; }
    bool is_initiator() const { return initiator_; }
    uint8_t attempts() const { return attempts_; }
    /// 6 digits plus NUL once the handshake is complete, otherwise nullptr.
    const char* code() const { return sas_ok_ ? sas_ : nullptr; }
    /// The other board's static public key once it is known, otherwise nullptr.
    const uint8_t* peer_static() const { return have_peer_ ? peer_pk_ : nullptr; }

    void reset()
    {
        hs_.clear();
        transport_.clear();
        phase_ = Phase::Idle;
        fail_ = Fail::None;
        sas_ok_ = false;
        have_peer_ = false;
        attempts_ = 0;
        seq_ = 0;
    }

    void start_as_initiator(uint32_t now_ms)
    {
        attempts_ = 1;
        begin_initiator(now_ms);
    }

    void on_payload(const uint8_t* d, size_t n, uint32_t now_ms)
    {
        last_rx_ = now_ms;
        if (n == kMsg1Len && phase_ != Phase::InitWaitReply) {
            accept_msg1(d, now_ms);
            return;
        }
        switch (phase_) {
        case Phase::InitWaitReply:
            if (n == kMsg1Len) {
                // Both boards started as initiator. The one with the lower msg1 yields; the other ignores.
                if (std::memcmp(d, msg1_, kMsg1Len) < 0) {
                    io_.event(Event::Yield);
                    accept_msg1(d, now_ms);
                }
                return;
            }
            if (n == kMsg2Len) {
                handle_msg2(d, now_ms);
            }
            return;
        case Phase::RespWaitFinal:
            if (n == kMsg3Len) {
                handle_msg3(d, now_ms);
            }
            return;
        case Phase::InitWaitAck:
        case Phase::RespWaitHello:
        case Phase::Paired:
            handle_transport(d, n, now_ms);
            return;
        case Phase::Idle:
        case Phase::Failed:
            return;
        }
    }

    void tick(uint32_t now_ms)
    {
        switch (phase_) {
        case Phase::InitWaitReply:
        case Phase::InitWaitAck:
            if (now_ms - last_tx_ >= prm_.retry_ms) {
                if (attempts_ >= prm_.max_attempts) {
                    fail(Fail::NoReply);
                } else {
                    ++attempts_;
                    io_.event(Event::Retry);
                    begin_initiator(now_ms);
                }
            }
            return;
        case Phase::RespWaitFinal:
        case Phase::RespWaitHello:
            if (now_ms - last_rx_ >= prm_.responder_timeout_ms) {
                io_.event(Event::Dropped);
                const uint8_t keep = attempts_;
                reset();
                attempts_ = keep;
            }
            return;
        case Phase::Idle:
        case Phase::Paired:
        case Phase::Failed:
            return;
        }
    }

private:
    Io& io_;
    kk::noise::KeyPair s_;
    Params prm_;
    uint8_t prologue_[kk::pairing::kPrologueLen] = {};

    kk::noise::HandshakeState hs_;
    kk::noise::Transport transport_;
    Phase phase_ = Phase::Idle;
    Fail fail_ = Fail::None;
    bool initiator_ = false;
    uint8_t attempts_ = 0;
    uint16_t seq_ = 0;
    uint32_t last_tx_ = 0;
    uint32_t last_rx_ = 0;
    uint8_t msg1_[kMsg1Len] = {};
    char sas_[7] = {};
    bool sas_ok_ = false;
    uint8_t peer_pk_[32] = {};
    bool have_peer_ = false;
    uint8_t buf_[kBufLen] = {};

    void fail(Fail f)
    {
        fail_ = f;
        phase_ = Phase::Failed;
        hs_.clear();
        transport_.clear();
        io_.event(Event::Failed);
    }

    bool init_handshake(kk::noise::Role role)
    {
        hs_.clear();
        transport_.clear();
        sas_ok_ = false;
        have_peer_ = false;
        kk::noise::HandshakeState::Config c;
        c.pattern = kk::noise::Pattern::XX;
        c.role = role;
        c.prologue = prologue_;
        c.prologue_len = sizeof prologue_;
        c.s = &s_;
        return hs_.init(c) == kk::noise::Status::Ok;
    }

    void begin_initiator(uint32_t now_ms)
    {
        initiator_ = true;
        if (!init_handshake(kk::noise::Role::Initiator)) {
            fail(Fail::Handshake);
            return;
        }
        size_t n = 0;
        if (hs_.write_message(nullptr, 0, buf_, sizeof buf_, &n) != kk::noise::Status::Ok || n != kMsg1Len) {
            fail(Fail::Handshake);
            return;
        }
        std::memcpy(msg1_, buf_, kMsg1Len);
        io_.send(buf_, n);
        io_.event(Event::SentMsg1);
        phase_ = Phase::InitWaitReply;
        fail_ = Fail::None;
        last_tx_ = now_ms;
    }

    void accept_msg1(const uint8_t* d, uint32_t now_ms)
    {
        initiator_ = false;
        const uint8_t keep = attempts_;
        if (!init_handshake(kk::noise::Role::Responder)) {
            fail(Fail::Handshake);
            return;
        }
        attempts_ = keep;
        uint8_t scratch[16];
        size_t pn = 0;
        if (hs_.read_message(d, kMsg1Len, scratch, sizeof scratch, &pn) != kk::noise::Status::Ok) {
            reset();
            return;
        }
        io_.event(Event::GotMsg1);
        size_t n = 0;
        if (hs_.write_message(nullptr, 0, buf_, sizeof buf_, &n) != kk::noise::Status::Ok || n != kMsg2Len) {
            fail(Fail::Handshake);
            return;
        }
        io_.send(buf_, n);
        io_.event(Event::SentMsg2);
        phase_ = Phase::RespWaitFinal;
        fail_ = Fail::None;
        last_rx_ = now_ms;
    }

    void handle_msg2(const uint8_t* d, uint32_t now_ms)
    {
        io_.event(Event::GotMsg2);
        uint8_t scratch[16];
        size_t pn = 0;
        if (hs_.read_message(d, kMsg2Len, scratch, sizeof scratch, &pn) != kk::noise::Status::Ok) {
            // Most likely a reply to an EARLIER attempt (the other board answered every queued msg1).
            // Do not give up: this attempt is spoiled, and the retry timer starts a clean one.
            return;
        }
        size_t n = 0;
        if (hs_.write_message(nullptr, 0, buf_, sizeof buf_, &n) != kk::noise::Status::Ok || n != kMsg3Len) {
            fail(Fail::Handshake);
            return;
        }
        io_.send(buf_, n);
        io_.event(Event::SentMsg3);
        if (!finish_handshake()) {
            return;
        }
        // First encrypted message: HELLO.
        const uint8_t body[2] = {kk::msg::kProtocolVersion, 0};
        uint8_t plain[kk::msg::kHeaderLen + 2];
        size_t plain_n = 0;
        size_t out_n = 0;
        if (!kk::msg::encode(kk::msg::Type::Hello, 0, seq_++, body, sizeof body, plain, sizeof plain, &plain_n) ||
            transport_.seal(plain, plain_n, buf_, sizeof buf_, &out_n) != kk::noise::Status::Ok) {
            fail(Fail::Transport);
            return;
        }
        io_.send(buf_, out_n);
        io_.event(Event::SentHello);
        phase_ = Phase::InitWaitAck;
        last_tx_ = now_ms;
    }

    void handle_msg3(const uint8_t* d, uint32_t now_ms)
    {
        io_.event(Event::GotMsg3);
        uint8_t scratch[16];
        size_t pn = 0;
        if (hs_.read_message(d, kMsg3Len, scratch, sizeof scratch, &pn) != kk::noise::Status::Ok) {
            fail(Fail::Handshake);
            return;
        }
        if (!finish_handshake()) {
            return;
        }
        phase_ = Phase::RespWaitHello;
        last_rx_ = now_ms;
    }

    // The code must be computed before split(), which wipes the handshake keys.
    bool finish_handshake()
    {
        if (!hs_.complete()) {
            fail(Fail::Handshake);
            return false;
        }
        const uint32_t code = kk::pairing::sas_code(hs_.handshake_hash());
        kk::pairing::sas_text(code, sas_);
        sas_ok_ = true;
        const uint8_t* rs = hs_.remote_static();
        if (rs != nullptr) {
            std::memcpy(peer_pk_, rs, sizeof peer_pk_);
            have_peer_ = true;
        }
        if (hs_.split(&transport_) != kk::noise::Status::Ok) {
            fail(Fail::Handshake);
            return false;
        }
        io_.event(Event::SasReady);
        return true;
    }

    void handle_transport(const uint8_t* d, size_t n, uint32_t now_ms)
    {
        uint8_t plain[kBufLen];
        size_t plain_n = 0;
        if (transport_.open(d, n, plain, sizeof plain, &plain_n) != kk::noise::Status::Ok) {
            // A message that does not verify ends an established session. While the session is still
            // being set up it is ignored (a stale message from an earlier attempt, or noise): the
            // counter does not advance on a failed check, so a valid message can still follow, and
            // the retry timer covers the rest.
            if (phase_ == Phase::Paired) {
                fail(Fail::Transport);
            }
            return;
        }
        kk::msg::Header h;
        const uint8_t* body = nullptr;
        if (kk::msg::parse(plain, plain_n, &h, &body) != kk::msg::ParseStatus::Ok) {
            fail(Fail::BadHello);
            return;
        }
        if (!initiator_ && phase_ == Phase::RespWaitHello && h.type == kk::msg::Type::Hello) {
            kk::msg::Hello hello;
            if (!kk::msg::decode_hello(body, h.len, &hello) || hello.version != kk::msg::kProtocolVersion) {
                fail(Fail::BadHello);
                return;
            }
            io_.event(Event::GotHello);
            kk::msg::HelloAck ack;
            ack.fw_major = 0;
            ack.fw_minor = 1;
            ack.usb_mounted = false;
            uint8_t ack_body[4];
            uint8_t reply[kk::msg::kHeaderLen + 4];
            size_t reply_n = 0;
            size_t out_n = 0;
            if (!kk::msg::encode_hello_ack(ack, ack_body) ||
                !kk::msg::encode(kk::msg::Type::HelloAck, 0, seq_++, ack_body, sizeof ack_body, reply, sizeof reply,
                                 &reply_n) ||
                transport_.seal(reply, reply_n, buf_, sizeof buf_, &out_n) != kk::noise::Status::Ok) {
                fail(Fail::Transport);
                return;
            }
            io_.send(buf_, out_n);
            io_.event(Event::SentHelloAck);
            phase_ = Phase::Paired;
            io_.event(Event::SessionOk);
            return;
        }
        if (initiator_ && phase_ == Phase::InitWaitAck && h.type == kk::msg::Type::HelloAck) {
            kk::msg::HelloAck ack;
            if (!kk::msg::decode_hello_ack(body, h.len, &ack) || ack.version != kk::msg::kProtocolVersion) {
                fail(Fail::BadHello);
                return;
            }
            phase_ = Phase::Paired;
            last_rx_ = now_ms;
            io_.event(Event::SessionOk);
            return;
        }
        // Any other valid message in this phase: not used by this test, ignored.
    }
};

}  // namespace pairtest
