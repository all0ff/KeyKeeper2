#!/usr/bin/env python3
"""Mutation check of the kkproto test suite (host, libsodium port).

Each mutation plants a realistic bug in a COPY of the library; the suite must go red.
A surviving mutation is either a gap in the tests or an equivalent mutant -- look at it.
usage: python3 test_host/mutation_check.py      (from components/kkproto)
"""
import os, shutil, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
MUTS = [
 ("HKDF: second output uses 0x03 instead of 0x02", 'src/noise.cpp', "const uint8_t one = 1, two = 2;", "const uint8_t one = 1, two = 3;"),
 ("encrypt_and_hash does not mix the ciphertext into h", 'src/noise.cpp', "        return st;\n    }\n    mix_hash(out, *out_len);\n    return Status::Ok;\n}\n\nStatus HandshakeState::decrypt_and_hash", "        return st;\n    }\n    return Status::Ok;\n}\n\nStatus HandshakeState::decrypt_and_hash"),
 ("SS computed with the wrong key pair (write side)", 'src/noise.cpp', "st = (has_s_ && has_rs_) ? dh_mix(s_, rs_) : Status::WrongState;\n                break;\n        }\n    }\n    if (st == Status::Ok) {\n        size_t n = 0;", "st = (has_s_ && has_re_) ? dh_mix(s_, re_) : Status::WrongState;\n                break;\n        }\n    }\n    if (st == Status::Ok) {\n        size_t n = 0;"),
 ("nonce advances even when authentication fails", 'src/noise.cpp', "return Status::DecryptFailed; // counter deliberately NOT advanced", "{ ++n_; return Status::DecryptFailed; }"),
 ("all-zero DH check disabled", 'src/noise.cpp', "    return acc == 0;", "    return false;"),
 ("prologue not mixed into h", 'src/noise.cpp', "    mix_hash(cfg.prologue, cfg.prologue_len);", "    (void)0;"),
 ("IK pre-message (responder static) not hashed", 'src/noise.cpp', "        mix_hash(role_ == Role::Initiator ? rs_ : s_.pk, kDhLen);", "        (void)0;"),
 ("nonce does not advance after encrypt", 'src/noise.cpp', "    ++n_;\n    *out_len = n + kTagLen;", "    *out_len = n + kTagLen;"),
 ("split: responder keys swapped", 'src/noise.cpp', "        out->send_.init_key(k2);\n        out->recv_.init_key(k1);", "        out->send_.init_key(k1);\n        out->recv_.init_key(k2);"),
 ("Transport::open decrypts with the send key", 'include/kkproto/noise.hpp', "return recv_.decrypt(nullptr, 0, ciphertext, n, out, cap, out_len);", "return send_.decrypt(nullptr, 0, ciphertext, n, out, cap, out_len);"),
 ("SAS: different HMAC message string", 'src/pairing.cpp', "{'k', 'k', '2', '-', 's', 'a', 's'}", "{'k', 'k', '2', '-', 's', 'a', 'x'}"),
 ("SAS: modulo 10^6 replaced by 10^5", 'src/pairing.cpp', "return v % 1000000u;", "return v % 100000u;"),
 ("frames: zero length accepted", 'src/frames.cpp', "if (need_ == 0 || need_ > kMaxPayload) {", "if (need_ > kMaxPayload) {"),
 ("frames: length cap one too high", 'src/frames.cpp', "need_ > kMaxPayload) {", "need_ > kMaxPayload + 1) {"),
 ("events: Key accepts usage 0x03", 'src/messages.cpp', "return e.usage >= 0x04 && e.usage <= 0xE7 && e.hold_ms >= 1;", "return e.usage >= 0x03 && e.usage <= 0xE7 && e.hold_ms >= 1;"),
 ("messages: body length need not match exactly", 'src/messages.cpp', "static_cast<size_t>(len) != n - kHeaderLen", "static_cast<size_t>(len) > n - kHeaderLen"),
 ("batch limit 33 events", 'include/kkproto/messages.hpp', "constexpr size_t kMaxEventsPerBatch = 32;", "constexpr size_t kMaxEventsPerBatch = 33;"),
 # Either selftest check alone already exposes a non-authenticating port (a forged message and a wrong
 # nonce are accepted by it); removing only one therefore survives by design. Remove both:
 ("selftest checks neither forged message nor wrong nonce", 'src/selftest.cpp', "        if (aead_open(key, nonce, ad, sizeof ad, ct, sizeof ct, back)) return \"aead accepts a forged message\";\n        ct[3] ^= 0x01;\n        if (aead_open(key, nonce + 1, ad, sizeof ad, ct, sizeof ct, back)) return \"aead accepts a wrong nonce\";", "        ct[3] ^= 0x01;"),
]
CORE = ['src/noise.cpp', 'src/frames.cpp', 'src/messages.cpp', 'src/pairing.cpp', 'src/selftest.cpp']
SODIUM = 'platform/sodium/crypto_port_sodium.cpp'
SUITE = [  # (name, extra sources, extra flags)
    ('test_noise_vectors', [SODIUM], []), ('test_protocol', [SODIUM], []), ('test_codec', [SODIUM], []),
    ('test_core_checks', ['test_host/broken_ports/leaky_dh_port.cpp'], ['-DLEAKY']),
    ('test_core_checks', ['test_host/broken_ports/no_auth_port.cpp'], ['-DNOAUTH']),
]

def main():
    caught, survived = 0, []
    for name, f, a, b in MUTS:
        d = tempfile.mkdtemp()
        shutil.copytree(ROOT, d + '/k')
        p = d + '/k/' + f
        src = open(p).read()
        assert a in src, f'pattern not found for: {name}'
        open(p, 'w').write(src.replace(a, b, 1))
        red = []
        for i, (t, extra, flags) in enumerate(SUITE):
            exe = f'{d}/{t}_{i}'
            cc = ['g++', '-std=c++17', '-O1', '-w', '-Iinclude', '-Itest_host', *flags, f'test_host/{t}.cpp', *CORE, *extra, '-lsodium', '-o', exe]
            if subprocess.run(cc, cwd=d + '/k', capture_output=True).returncode != 0:
                red.append(t + '(build)'); continue
            if subprocess.run([exe], cwd=d + '/k/test_host', capture_output=True, timeout=600).returncode != 0:
                red.append(t + ''.join(flags))
        ok = bool(red)
        caught += ok
        print(f"  {'caught  ' if ok else 'SURVIVED'}  {name:56s} {', '.join(red)}")
        if not ok: survived.append(name)
        shutil.rmtree(d, ignore_errors=True)
    print(f'\ncaught {caught} of {len(MUTS)}')
    sys.exit(0 if not survived else 1)

if __name__ == '__main__':
    main()
