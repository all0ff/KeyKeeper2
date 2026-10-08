// Runs the official Noise test vectors (cacophony, snow, noise-c) through HandshakeState on BOTH
// sides: every handshake and transport ciphertext must match byte for byte, and so must the
// final handshake hash. Linked against whichever crypto_port the build selects.
#include "kkproto/noise.hpp"
#include "test_support.hpp"

#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

using namespace kk;
using namespace kk::noise;

struct Vector {
    std::string id, protocol;
    std::map<std::string, std::string> f;
    std::vector<std::pair<Bytes, Bytes>> msgs; // payload, ciphertext
};

static std::vector<Vector> load(const char* path)
{
    std::ifstream in(path);
    std::vector<Vector> out;
    std::string line;
    Vector cur;
    bool open = false;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        if (key == "vector") { cur = Vector(); open = true; ss >> cur.id >> cur.protocol; }
        else if (key == "end") { out.push_back(cur); open = false; }
        else if (key == "msg" && open) { std::string a, b; ss >> a >> b; cur.msgs.push_back({from_hex(a), from_hex(b)}); }
        else if (open) { std::string v; ss >> v; cur.f[key] = v; }
    }
    return out;
}

static bool key_pair(const Vector& v, const char* name, KeyPair* kp)
{
    auto it = v.f.find(name);
    if (it == v.f.end()) return false;
    const Bytes sk = from_hex(it->second);
    if (sk.size() != 32) return false;
    std::memcpy(kp->sk, sk.data(), 32);
    return crypto::x25519_public(kp->sk, kp->pk);
}

static void run(const Vector& v)
{
    const Pattern pattern = v.protocol.find("_IK_") != std::string::npos ? Pattern::IK : Pattern::XX;
    const size_t hs_msgs = pattern == Pattern::XX ? 3 : 2;
    KeyPair is{}, ie{}, rs{}, re{};
    CHECK_MSG(key_pair(v, "init_static", &is) && key_pair(v, "init_ephemeral", &ie) && key_pair(v, "resp_static", &rs) &&
                  key_pair(v, "resp_ephemeral", &re), "%s: keys", v.id.c_str());
    const Bytes prologue = from_hex(v.f.at("init_prologue"));
    CHECK(prologue == from_hex(v.f.at("resp_prologue")));

    HandshakeState ini, res;
    HandshakeState::Config ci, cr;
    ci.pattern = cr.pattern = pattern;
    ci.role = Role::Initiator; cr.role = Role::Responder;
    ci.prologue = cr.prologue = prologue.data();
    ci.prologue_len = cr.prologue_len = prologue.size();
    ci.s = &is; cr.s = &rs;
    ci.test_e = &ie; cr.test_e = &re;
    if (pattern == Pattern::IK) ci.rs = rs.pk; // initiator knows the responder's static public key
    CHECK(ini.init(ci) == Status::Ok && res.init(cr) == Status::Ok);

    Transport ti, tr;
    for (size_t i = 0; i < v.msgs.size(); ++i) {
        const Bytes& payload = v.msgs[i].first;
        const Bytes& expect = v.msgs[i].second;
        const bool from_initiator = (i % 2 == 0);
        Bytes wire(expect.size() + 64), back(payload.size() + 64);
        size_t wn = 0, bn = 0;
        if (i < hs_msgs) {
            HandshakeState& tx = from_initiator ? ini : res;
            HandshakeState& rx = from_initiator ? res : ini;
            const Status w = tx.write_message(payload.data(), payload.size(), wire.data(), wire.size(), &wn);
            CHECK_MSG(w == Status::Ok, "%s msg %zu write: %s", v.id.c_str(), i, status_name(w));
            CHECK_MSG(Bytes(wire.begin(), wire.begin() + wn) == expect, "%s msg %zu ciphertext differs", v.id.c_str(), i);
            const Status r = rx.read_message(wire.data(), wn, back.data(), back.size(), &bn);
            CHECK_MSG(r == Status::Ok, "%s msg %zu read: %s", v.id.c_str(), i, status_name(r));
            CHECK_MSG(Bytes(back.begin(), back.begin() + bn) == payload, "%s msg %zu payload differs", v.id.c_str(), i);
            if (i + 1 == hs_msgs) {
                CHECK(ini.complete() && res.complete());
                CHECK(std::memcmp(ini.handshake_hash(), res.handshake_hash(), 32) == 0);
                auto h = v.f.find("handshake_hash");
                if (h != v.f.end()) CHECK_MSG(to_hex(ini.handshake_hash(), 32) == h->second, "%s handshake hash", v.id.c_str());
                CHECK(ini.remote_static() != nullptr && std::memcmp(ini.remote_static(), rs.pk, 32) == 0);
                CHECK(res.remote_static() != nullptr && std::memcmp(res.remote_static(), is.pk, 32) == 0);
                CHECK(ini.split(&ti) == Status::Ok && res.split(&tr) == Status::Ok);
            }
        } else {
            Transport& tx = from_initiator ? ti : tr;
            Transport& rx = from_initiator ? tr : ti;
            const Status w = tx.seal(payload.data(), payload.size(), wire.data(), wire.size(), &wn);
            CHECK_MSG(w == Status::Ok, "%s msg %zu seal", v.id.c_str(), i);
            CHECK_MSG(Bytes(wire.begin(), wire.begin() + wn) == expect, "%s transport msg %zu ciphertext differs", v.id.c_str(), i);
            const Status r = rx.open(wire.data(), wn, back.data(), back.size(), &bn);
            CHECK_MSG(r == Status::Ok && Bytes(back.begin(), back.begin() + bn) == payload, "%s transport msg %zu open", v.id.c_str(), i);
        }
    }
}

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "vectors/noise_vectors.txt";
    const auto vectors = load(path);
    CHECK_MSG(!vectors.empty(), "no vectors loaded from %s", path);
    size_t xx = 0, ik = 0;
    for (const auto& v : vectors) {
        run(v);
        (v.protocol.find("_IK_") != std::string::npos ? ik : xx)++;
    }
    std::printf("vectors: %zu XX + %zu IK\n", xx, ik);
    return finish("noise official vectors");
}
