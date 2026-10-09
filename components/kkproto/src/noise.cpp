#include "kkproto/noise.hpp"

#include <cstring>

namespace kk::noise {

namespace {

void secure_zero(void* p, size_t n)
{
    volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
    while (n-- > 0) {
        *v++ = 0;
    }
}

bool all_zero(const uint8_t* p, size_t n)
{
    uint8_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc = static_cast<uint8_t>(acc | p[i]);
    }
    return acc == 0;
}

// Both names are 31 bytes (< HASHLEN), so Noise pads them with zeros instead of hashing.
constexpr char kNameXX[] = "Noise_XX_25519_ChaChaPoly_SHA256";
constexpr char kNameIK[] = "Noise_IK_25519_ChaChaPoly_SHA256";

/// HKDF with two outputs, built from HMAC-SHA256 exactly as the Noise spec defines it.
void hkdf2(const uint8_t ck[kHashLen], const uint8_t* ikm, size_t ikm_len, uint8_t out1[kHashLen],
           uint8_t out2[kHashLen])
{
    uint8_t temp[kHashLen];
    crypto::hmac_sha256(ck, kHashLen, ikm, ikm_len, nullptr, 0, temp);
    const uint8_t one = 1, two = 2;
    crypto::hmac_sha256(temp, kHashLen, &one, 1, nullptr, 0, out1);
    crypto::hmac_sha256(temp, kHashLen, out1, kHashLen, &two, 1, out2);
    secure_zero(temp, sizeof temp);
}

enum class Tok : uint8_t { E, S, EE, ES, SE, SS };
struct MsgPattern {
    const Tok* tokens;
    size_t count;
};

constexpr Tok XX0[] = {Tok::E};
constexpr Tok XX1[] = {Tok::E, Tok::EE, Tok::S, Tok::ES};
constexpr Tok XX2[] = {Tok::S, Tok::SE};
constexpr MsgPattern kXX[] = {{XX0, 1}, {XX1, 4}, {XX2, 2}};

constexpr Tok IK0[] = {Tok::E, Tok::ES, Tok::S, Tok::SS};
constexpr Tok IK1[] = {Tok::E, Tok::EE, Tok::SE};
constexpr MsgPattern kIK[] = {{IK0, 4}, {IK1, 3}};

const MsgPattern* messages_of(Pattern p, size_t* count)
{
    if (p == Pattern::XX) {
        *count = sizeof kXX / sizeof kXX[0];
        return kXX;
    }
    *count = sizeof kIK / sizeof kIK[0];
    return kIK;
}

} // namespace

const char* status_name(Status s)
{
    switch (s) {
        case Status::Ok: return "ok";
        case Status::BadArgument: return "bad argument";
        case Status::BufferTooSmall: return "buffer too small";
        case Status::BadMessage: return "bad message";
        case Status::DecryptFailed: return "decrypt failed";
        case Status::BadDhResult: return "bad DH result";
        case Status::WrongState: return "wrong state";
        case Status::NonceExhausted: return "nonce exhausted";
        case Status::RngFailed: return "rng failed";
    }
    return "?";
}

// ----------------------------------------------------------------------------- CipherState

void CipherState::clear()
{
    secure_zero(k_, sizeof k_);
    has_key_ = false;
    n_ = 0;
}

void CipherState::init_key(const uint8_t key[crypto::kKeyLen])
{
    std::memcpy(k_, key, sizeof k_);
    has_key_ = true;
    n_ = 0;
}

Status CipherState::encrypt(const uint8_t* ad, size_t ad_len, const uint8_t* in, size_t n, uint8_t* out, size_t cap,
                            size_t* out_len)
{
    if (!has_key_) {
        if (cap < n) {
            return Status::BufferTooSmall;
        }
        if (n > 0) {
            std::memmove(out, in, n);
        }
        *out_len = n;
        return Status::Ok;
    }
    if (n_ == UINT64_MAX) {
        return Status::NonceExhausted; // 2^64-1 is reserved by the spec
    }
    if (cap < n + kTagLen) {
        return Status::BufferTooSmall;
    }
    if (!crypto::aead_seal(k_, n_, ad, ad_len, in, n, out)) {
        return Status::BadArgument;
    }
    ++n_;
    *out_len = n + kTagLen;
    return Status::Ok;
}

Status CipherState::decrypt(const uint8_t* ad, size_t ad_len, const uint8_t* in, size_t n, uint8_t* out, size_t cap,
                            size_t* out_len)
{
    if (!has_key_) {
        if (cap < n) {
            return Status::BufferTooSmall;
        }
        if (n > 0) {
            std::memmove(out, in, n);
        }
        *out_len = n;
        return Status::Ok;
    }
    if (n_ == UINT64_MAX) {
        return Status::NonceExhausted;
    }
    if (n < kTagLen) {
        return Status::DecryptFailed;
    }
    if (cap < n - kTagLen) {
        return Status::BufferTooSmall;
    }
    if (!crypto::aead_open(k_, n_, ad, ad_len, in, n, out)) {
        return Status::DecryptFailed; // counter deliberately NOT advanced
    }
    ++n_;
    *out_len = n - kTagLen;
    return Status::Ok;
}

// ----------------------------------------------------------------------------- HandshakeState

void HandshakeState::clear()
{
    secure_zero(&ss_, sizeof ss_);
    secure_zero(&s_, sizeof s_);
    secure_zero(&e_, sizeof e_);
    secure_zero(&test_e_, sizeof test_e_);
    secure_zero(rs_, sizeof rs_);
    secure_zero(re_, sizeof re_);
    has_test_e_ = has_s_ = has_e_ = has_rs_ = has_re_ = false;
    initialised_ = false;
    // complete_ / failed_ / h_final_ intentionally survive: the hash stays readable after split().
}

void HandshakeState::ss_init(const char* name)
{
    const size_t len = std::strlen(name);
    std::memset(ss_.h, 0, sizeof ss_.h);
    std::memcpy(ss_.h, name, len); // len <= HASHLEN: pad with zeros (spec 5.2)
    std::memcpy(ss_.ck, ss_.h, sizeof ss_.ck);
    ss_.cs.clear();
}

void HandshakeState::mix_hash(const uint8_t* data, size_t n)
{
    crypto::sha256(ss_.h, kHashLen, data, n, ss_.h); // h = HASH(h || data); ports allow in==out for this call
}

void HandshakeState::mix_key(const uint8_t* ikm, size_t n)
{
    uint8_t temp_k[kHashLen];
    hkdf2(ss_.ck, ikm, n, ss_.ck, temp_k);
    ss_.cs.init_key(temp_k);
    secure_zero(temp_k, sizeof temp_k);
}

Status HandshakeState::encrypt_and_hash(const uint8_t* in, size_t n, uint8_t* out, size_t cap, size_t* out_len)
{
    const Status st = ss_.cs.encrypt(ss_.h, kHashLen, in, n, out, cap, out_len);
    if (st != Status::Ok) {
        return st;
    }
    mix_hash(out, *out_len);
    return Status::Ok;
}

Status HandshakeState::decrypt_and_hash(const uint8_t* in, size_t n, uint8_t* out, size_t cap, size_t* out_len)
{
    const Status st = ss_.cs.decrypt(ss_.h, kHashLen, in, n, out, cap, out_len);
    if (st != Status::Ok) {
        return st;
    }
    mix_hash(in, n);
    return Status::Ok;
}

Status HandshakeState::dh_mix(const KeyPair& local, const uint8_t* remote_pk)
{
    uint8_t shared[kDhLen];
    if (!crypto::x25519(shared, local.sk, remote_pk) || all_zero(shared, sizeof shared)) {
        secure_zero(shared, sizeof shared);
        return Status::BadDhResult;
    }
    mix_key(shared, sizeof shared);
    secure_zero(shared, sizeof shared);
    return Status::Ok;
}

Status HandshakeState::init(const Config& cfg)
{
    clear();
    complete_ = failed_ = false;
    msg_index_ = 0;
    if (cfg.s == nullptr || (cfg.prologue == nullptr && cfg.prologue_len != 0)) {
        return Status::BadArgument;
    }
    if (cfg.pattern == Pattern::IK && cfg.role == Role::Initiator && cfg.rs == nullptr) {
        return Status::BadArgument; // IK: the initiator must already know the responder's static key
    }

    pattern_ = cfg.pattern;
    role_ = cfg.role;
    s_ = *cfg.s;
    has_s_ = true;
    if (cfg.rs != nullptr) {
        std::memcpy(rs_, cfg.rs, sizeof rs_);
        has_rs_ = true;
    }
    if (cfg.test_e != nullptr) {
        test_e_ = *cfg.test_e;
        has_test_e_ = true;
    }

    ss_init(pattern_ == Pattern::XX ? kNameXX : kNameIK);
    mix_hash(cfg.prologue, cfg.prologue_len);

    // IK pre-message "<- s": the responder's static key is hashed in by both sides.
    if (pattern_ == Pattern::IK) {
        mix_hash(role_ == Role::Initiator ? rs_ : s_.pk, kDhLen);
    }
    initialised_ = true;
    return Status::Ok;
}

bool HandshakeState::my_turn() const
{
    if (!initialised_ || complete_ || failed_) {
        return false;
    }
    return (msg_index_ % 2 == 0) == (role_ == Role::Initiator);
}

Status HandshakeState::write_message(const uint8_t* payload, size_t payload_len, uint8_t* out, size_t cap,
                                     size_t* out_len)
{
    if (!my_turn()) {
        return Status::WrongState;
    }
    if (payload == nullptr && payload_len != 0) {
        return Status::BadArgument;
    }
    size_t total = 0;
    const MsgPattern* msgs = messages_of(pattern_, &total);
    const MsgPattern& mp = msgs[msg_index_];

    size_t pos = 0;
    Status st = Status::Ok;
    for (size_t i = 0; i < mp.count && st == Status::Ok; ++i) {
        switch (mp.tokens[i]) {
            case Tok::E:
                if (cap - pos < kDhLen) {
                    st = Status::BufferTooSmall;
                    break;
                }
                if (has_test_e_) {
                    e_ = test_e_;
                } else if (!crypto::x25519_keygen(e_.sk, e_.pk)) {
                    st = Status::RngFailed;
                    break;
                }
                has_e_ = true;
                std::memcpy(out + pos, e_.pk, kDhLen);
                pos += kDhLen;
                mix_hash(e_.pk, kDhLen);
                break;
            case Tok::S: {
                size_t n = 0;
                st = encrypt_and_hash(s_.pk, kDhLen, out + pos, cap - pos, &n);
                pos += n;
                break;
            }
            case Tok::EE:
                st = (has_e_ && has_re_) ? dh_mix(e_, re_) : Status::WrongState;
                break;
            case Tok::ES: // initiator: DH(e, rs)   responder: DH(s, re)
                if (role_ == Role::Initiator) {
                    st = (has_e_ && has_rs_) ? dh_mix(e_, rs_) : Status::WrongState;
                } else {
                    st = (has_s_ && has_re_) ? dh_mix(s_, re_) : Status::WrongState;
                }
                break;
            case Tok::SE: // initiator: DH(s, re)   responder: DH(e, rs)
                if (role_ == Role::Initiator) {
                    st = (has_s_ && has_re_) ? dh_mix(s_, re_) : Status::WrongState;
                } else {
                    st = (has_e_ && has_rs_) ? dh_mix(e_, rs_) : Status::WrongState;
                }
                break;
            case Tok::SS:
                st = (has_s_ && has_rs_) ? dh_mix(s_, rs_) : Status::WrongState;
                break;
        }
    }
    if (st == Status::Ok) {
        size_t n = 0;
        st = encrypt_and_hash(payload, payload_len, out + pos, cap - pos, &n);
        pos += n;
    }
    if (st != Status::Ok) {
        failed_ = true; // a failed handshake is abandoned, never retried on the same state
        return st;
    }
    *out_len = pos;
    if (++msg_index_ == total) {
        complete_ = true;
        std::memcpy(h_final_, ss_.h, kHashLen);
    }
    return Status::Ok;
}

Status HandshakeState::read_message(const uint8_t* in, size_t n, uint8_t* payload, size_t payload_cap,
                                    size_t* payload_len)
{
    if (!initialised_ || complete_ || failed_ || my_turn()) {
        return Status::WrongState;
    }
    if (in == nullptr && n != 0) {
        return Status::BadArgument;
    }
    size_t total = 0;
    const MsgPattern* msgs = messages_of(pattern_, &total);
    const MsgPattern& mp = msgs[msg_index_];

    size_t pos = 0;
    Status st = Status::Ok;
    for (size_t i = 0; i < mp.count && st == Status::Ok; ++i) {
        switch (mp.tokens[i]) {
            case Tok::E:
                if (n - pos < kDhLen) {
                    st = Status::BadMessage;
                    break;
                }
                std::memcpy(re_, in + pos, kDhLen);
                has_re_ = true;
                pos += kDhLen;
                mix_hash(re_, kDhLen);
                break;
            case Tok::S: {
                const size_t len = ss_.cs.has_key() ? kDhLen + kTagLen : kDhLen;
                if (n - pos < len) {
                    st = Status::BadMessage;
                    break;
                }
                size_t got = 0;
                st = decrypt_and_hash(in + pos, len, rs_, sizeof rs_, &got);
                if (st == Status::Ok) {
                    has_rs_ = true;
                    pos += len;
                }
                break;
            }
            case Tok::EE:
                st = (has_e_ && has_re_) ? dh_mix(e_, re_) : Status::WrongState;
                break;
            case Tok::ES: // as seen from the READER: initiator's token ES means DH(e_i, s_r)
                if (role_ == Role::Initiator) {
                    st = (has_e_ && has_rs_) ? dh_mix(e_, rs_) : Status::WrongState;
                } else {
                    st = (has_s_ && has_re_) ? dh_mix(s_, re_) : Status::WrongState;
                }
                break;
            case Tok::SE:
                if (role_ == Role::Initiator) {
                    st = (has_s_ && has_re_) ? dh_mix(s_, re_) : Status::WrongState;
                } else {
                    st = (has_e_ && has_rs_) ? dh_mix(e_, rs_) : Status::WrongState;
                }
                break;
            case Tok::SS:
                st = (has_s_ && has_rs_) ? dh_mix(s_, rs_) : Status::WrongState;
                break;
        }
    }
    if (st == Status::Ok) {
        st = decrypt_and_hash(in + pos, n - pos, payload, payload_cap, payload_len);
    }
    if (st != Status::Ok) {
        failed_ = true;
        return st;
    }
    if (++msg_index_ == total) {
        complete_ = true;
        std::memcpy(h_final_, ss_.h, kHashLen);
    }
    return Status::Ok;
}

Status HandshakeState::split(Transport* out)
{
    if (!initialised_ || !complete_ || failed_ || out == nullptr) {
        return Status::WrongState;
    }
    uint8_t k1[kHashLen], k2[kHashLen];
    hkdf2(ss_.ck, nullptr, 0, k1, k2);
    out->clear();
    if (role_ == Role::Initiator) {
        out->send_.init_key(k1);
        out->recv_.init_key(k2);
    } else {
        out->send_.init_key(k2);
        out->recv_.init_key(k1);
    }
    secure_zero(k1, sizeof k1);
    secure_zero(k2, sizeof k2);
    clear(); // handshake keys are no longer needed; h_final_ stays
    return Status::Ok;
}

} // namespace kk::noise
