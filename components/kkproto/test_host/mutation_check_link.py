#!/usr/bin/env python3
"""Mutation check of the link::Endpoint tests (host). Each mutation plants a realistic bug in a COPY of
src/link.cpp; test_link must go red. A surviving mutation is a gap in the tests or an equivalent mutant.
usage: python3 test_host/mutation_check_link.py      (from components/kkproto)
"""
import os, shutil, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
F = 'src/link.cpp'
MUTS = [
 ("IK: any peer key accepted (trusted-key check removed)", "if (rs == nullptr || std::memcmp(rs, trusted_, noise::kDhLen) != 0) {", "if (rs == nullptr) {"),
 ("pairing completes without the remote user's confirmation", "if (state_ != State::Confirming || !local_ok_ || !remote_ok_ || !have_peer_) {", "if (state_ != State::Confirming || !local_ok_ || !have_peer_) {"),
 ("pairing completes without the local user's confirmation", "if (state_ != State::Confirming || !local_ok_ || !remote_ok_ || !have_peer_) {", "if (state_ != State::Confirming || !remote_ok_ || !have_peer_) {"),
 ("dongle pairs while its window is closed", "if (!window_open_ || n != kXxMsg1) {", "if (n != kXxMsg1) {"),
 ("[equivalent: the key comparison right after rejects the same frames] dongle accepts a session with no trusted vault", "if (!has_trusted_ || n != kIkMsg1) {", "if (n != kIkMsg1) {"),
 ("window closes under users who are still confirming", "if (window_open_ && state_ == State::Idle && reached(now_ms, window_deadline_)) {", "if (window_open_ && reached(now_ms, window_deadline_)) {"),
 ("vault never retries a session attempt", "if (reached(now_ms, last_tx_ + prm_.retry_ms)) {\n            if (want_link_ && has_trusted_) {", "if (false) {\n            if (want_link_ && has_trusted_) {"),
 ("pairing attempt limit off by one", "if (attempts_ >= prm_.max_pair_attempts) {", "if (attempts_ > prm_.max_pair_attempts) {"),
 ("dongle never notices a silent link", "case State::Linked:\n            if (reached(now_ms, last_rx_ + prm_.silence_ms)) {\n                end_session(true);\n            }\n            break;", "case State::Linked:\n            break;"),
 ("vault never notices a silent link", "        if (reached(now_ms, last_rx_ + prm_.silence_ms)) {\n            end_session(true);\n            break;\n        }\n", ""),
 ("code not derived from the handshake hash", "pairing::sas_code(hs_.handshake_hash())", "pairing::sas_code(s_.pk)"),
 ("peer key recorded wrongly", "std::memcpy(peer_pk_, rs, noise::kDhLen);", "std::memcpy(peer_pk_, s_.pk, noise::kDhLen);"),
 ("a Bye does not reject a pairing", "            end_session(false);\n            window_open_ = false;\n            io_.event(Event::PairingRejected);\n        } else if (state_ == State::Linked", "            end_session(false);\n            window_open_ = false;\n        } else if (state_ == State::Linked"),
 ("forgetting the peer leaves the session running", "        want_link_ = false;\n        if (state_ == State::Linked || state_ == State::Connecting) {\n            end_session(true);\n        }\n", "        want_link_ = false;\n"),
 ("reject does not tell the other end", "        if (have_tr_) {\n            seal_send(msg::Type::Bye, seq_++, nullptr, 0);\n        }\n        end_session(false);\n        window_open_ = false;\n        io_.event(Event::PairingRejected);", "        end_session(false);\n        window_open_ = false;\n        io_.event(Event::PairingRejected);"),
 ("a new handshake cannot replace a running session", "if (have_tr_ && try_transport(d, n, now_ms)) {\n        return; // a valid transport message (a failed check did not advance the counter)\n    }", "if (have_tr_) {\n        try_transport(d, n, now_ms);\n        return;\n    }"),
 ("Ping is not answered", "seal_send(msg::Type::Pong, h.seq, nullptr, 0);", "(void)0;"),
 ("received messages do not count as life signs", "    last_rx_ = now_ms;\n    handle_message(h, body, now_ms);", "    handle_message(h, body, now_ms);"),
 ("protocol version not checked (dongle)", "if (!msg::decode_hello(body, h.len, &v) || v.version != msg::kProtocolVersion) {", "if (!msg::decode_hello(body, h.len, &v)) {"),
 ("stale reply makes the vault give up (pairing)", "        // Most likely a reply to an EARLIER attempt (the dongle answered every first message that\n        // was waiting for it). Do not give up: this attempt is spoiled, the retry timer starts a new one.\n        return;", "        fail(Fail::Handshake);\n        drop_handshake();\n        return;"),
 ("application messages delivered while not linked", "    default:\n        if (state_ == State::Linked) {\n            io_.message(h, body);\n        }", "    default:\n        {\n            io_.message(h, body);\n        }"),
 ("internal message types may be sent by the application", "    case msg::Type::Abort:\n        break;\n    default:\n        return false; // protocol-internal messages are not the application's to send", "    case msg::Type::Abort:\n    default:\n        break;"),
]

def have(h):
    return subprocess.run(['g++', '-x', 'c++', '-E', '-'], input='#include <%s>\n' % h, text=True,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
if have('sodium.h'):
    PORT, LIB = 'platform/sodium/crypto_port_sodium.cpp', ['-lsodium']
elif have('openssl/evp.h'):
    PORT, LIB = 'platform/openssl/crypto_port_openssl.cpp', ['-lcrypto']
else:
    sys.exit('no crypto library')
CORE = ['src/noise.cpp', 'src/frames.cpp', 'src/messages.cpp', 'src/pairing.cpp', 'src/selftest.cpp', 'src/link.cpp']

def run_suite(tmp):
    exe = os.path.join(tmp, 'tl')
    cmd = ['g++', '-std=c++17', '-O1', '-Iinclude', '-Itest_host', 'test_host/test_link.cpp'] + CORE + [PORT] + LIB + ['-o', exe]
    c = subprocess.run(cmd, cwd=tmp, capture_output=True, text=True)
    if c.returncode != 0:
        return 'compile-error', c.stderr[-300:]
    r = subprocess.run([exe], cwd=tmp, capture_output=True, text=True, timeout=600)
    return ('green' if r.returncode == 0 else 'red'), r.stdout[-300:]

caught = 0
bad = 0
for name, old, new in MUTS:
    tmp = tempfile.mkdtemp()
    for d in ('src', 'include', 'platform', 'test_host'):
        shutil.copytree(os.path.join(ROOT, d), os.path.join(tmp, d))
    p = os.path.join(tmp, F)
    s = open(p).read()
    if s.count(old) != 1:
        print('INVALID  %s (pattern found %d times)' % (name, s.count(old)))
        bad += 1
        continue
    open(p, 'w').write(s.replace(old, new))
    res, out = run_suite(tmp)
    if res == 'red':
        caught += 1
        print('CAUGHT   ' + name)
    elif res == 'compile-error':
        print('INVALID  %s: %s' % (name, out))
        bad += 1
    elif name.startswith('[equivalent'):
        caught += 1
        print('EQUIVALENT ' + name)
    else:
        print('MISSED   ' + name)
    shutil.rmtree(tmp, ignore_errors=True)
print('%d/%d mutations caught%s' % (caught, len(MUTS), (', %d invalid' % bad) if bad else ''))
sys.exit(0 if caught + bad == len(MUTS) and bad == 0 and caught == len(MUTS) else 1)
