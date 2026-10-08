#pragma once

#include "kkproto/crypto_port.hpp"

#include <cstddef>
#include <cstdint>

namespace kk::noise {

// =============================================================================
// Noise Protocol Framework (revision 34), the two patterns KeyKeeper2 needs:
//
//   XX   pairing:    -> e   <- e, ee, s, es   -> s, se
//   IK   session:    <- s   ...   -> e, es, s, ss   <- e, ee, se
//
// both as  Noise_<pattern>_25519_ChaChaPoly_SHA256. Written from the spec and
// checked against the official test vectors (see test_host/). No exceptions, no
// heap: all buffers are owned by the caller or live inside the objects.
// =============================================================================

constexpr size_t kDhLen = 32;
constexpr size_t kHashLen = 32;
constexpr size_t kTagLen = 16;

enum class Pattern : uint8_t { XX, IK };
enum class Role : uint8_t { Initiator, Responder };

enum class Status : uint8_t {
    Ok = 0,
    BadArgument,    ///< missing or inconsistent configuration
    BufferTooSmall, ///< caller's output buffer cannot hold the result
    BadMessage,     ///< too short / malformed handshake message
    DecryptFailed,  ///< authentication failure
    BadDhResult,    ///< DH output was all-zero (invalid / low-order public key)
    WrongState,     ///< not this side's turn, already complete, or already failed
    NonceExhausted, ///< 2^64-1 messages sent under one key
    RngFailed,
};

const char* status_name(Status s);

struct KeyPair {
    uint8_t sk[kDhLen];
    uint8_t pk[kDhLen];
};

/// Noise CipherState: a key and a 64-bit message counter.
class CipherState {
public:
    CipherState() = default;
    ~CipherState() { clear(); }
    CipherState(const CipherState&) = default;
    CipherState& operator=(const CipherState&) = default;

    void clear();
    void init_key(const uint8_t key[crypto::kKeyLen]);
    bool has_key() const { return has_key_; }
    uint64_t nonce() const { return n_; }
    void set_nonce(uint64_t n) { n_ = n; }

    /// With no key the plaintext is copied through unchanged (Noise semantics
    /// for the very first handshake payloads). Otherwise out receives
    /// n + kTagLen bytes and the counter advances.
    Status encrypt(const uint8_t* ad, size_t ad_len, const uint8_t* in, size_t n, uint8_t* out, size_t cap,
                   size_t* out_len);
    /// The counter advances only if authentication succeeds (as the spec requires).
    Status decrypt(const uint8_t* ad, size_t ad_len, const uint8_t* in, size_t n, uint8_t* out, size_t cap,
                   size_t* out_len);

private:
    uint8_t k_[crypto::kKeyLen] = {};
    bool has_key_ = false;
    uint64_t n_ = 0;
};

/// The two CipherStates a finished handshake splits into. Messages are
/// authenticated by the implicit counter: a replayed, dropped, duplicated or
/// reordered message fails to decrypt.
class Transport {
public:
    Status seal(const uint8_t* plaintext, size_t n, uint8_t* out, size_t cap, size_t* out_len)
    {
        return send_.encrypt(nullptr, 0, plaintext, n, out, cap, out_len);
    }
    Status open(const uint8_t* ciphertext, size_t n, uint8_t* out, size_t cap, size_t* out_len)
    {
        return recv_.decrypt(nullptr, 0, ciphertext, n, out, cap, out_len);
    }
    void clear()
    {
        send_.clear();
        recv_.clear();
    }

    CipherState send_;
    CipherState recv_;
};

class HandshakeState {
public:
    struct Config {
        Pattern pattern = Pattern::XX;
        Role role = Role::Initiator;
        const uint8_t* prologue = nullptr;
        size_t prologue_len = 0;
        const KeyPair* s = nullptr;    ///< our static key pair (always required)
        const uint8_t* rs = nullptr;   ///< peer static PUBLIC key; required for IK initiators
        const KeyPair* test_e = nullptr; ///< TESTS ONLY: use this instead of a fresh ephemeral key
    };

    HandshakeState() = default;
    ~HandshakeState() { clear(); }
    HandshakeState(const HandshakeState&) = delete;
    HandshakeState& operator=(const HandshakeState&) = delete;

    Status init(const Config& cfg);

    /// True when it is this side's turn to write (and the handshake is not over).
    bool my_turn() const;
    bool complete() const { return complete_; }

    Status write_message(const uint8_t* payload, size_t payload_len, uint8_t* out, size_t cap, size_t* out_len);
    Status read_message(const uint8_t* in, size_t n, uint8_t* payload, size_t payload_cap, size_t* payload_len);

    /// After complete(): the final handshake hash h (channel binding / SAS input).
    const uint8_t* handshake_hash() const { return h_final_; }
    /// The peer's static public key, once it has been received (XX) or was given (IK).
    const uint8_t* remote_static() const { return has_rs_ ? rs_ : nullptr; }

    /// After complete(): the transport cipher states. Wipes the handshake keys.
    Status split(Transport* out);

    void clear();

private:
    struct Symmetric {
        uint8_t ck[kHashLen];
        uint8_t h[kHashLen];
        CipherState cs;
    };

    void ss_init(const char* name);
    void mix_hash(const uint8_t* data, size_t n);
    void mix_key(const uint8_t* ikm, size_t n);
    Status encrypt_and_hash(const uint8_t* in, size_t n, uint8_t* out, size_t cap, size_t* out_len);
    Status decrypt_and_hash(const uint8_t* in, size_t n, uint8_t* out, size_t cap, size_t* out_len);
    Status dh_mix(const KeyPair& local, const uint8_t* remote_pk);

    Pattern pattern_ = Pattern::XX;
    Role role_ = Role::Initiator;
    Symmetric ss_ = {};
    KeyPair s_ = {};
    KeyPair e_ = {};
    uint8_t rs_[kDhLen] = {};
    uint8_t re_[kDhLen] = {};
    KeyPair test_e_ = {};
    bool has_test_e_ = false;
    bool has_s_ = false, has_e_ = false, has_rs_ = false, has_re_ = false;
    bool initialised_ = false, complete_ = false, failed_ = false;
    size_t msg_index_ = 0;
    uint8_t h_final_[kHashLen] = {};
};

} // namespace kk::noise
