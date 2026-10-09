// g++ -std=c++17 -Wall -Wextra -I../include test_frag.cpp -o test_frag && ./test_frag
#include "blelink/frag.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace blelink;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::vector<std::vector<uint8_t>> split(const std::vector<uint8_t>& m, size_t att, uint8_t id) {
    std::vector<std::vector<uint8_t>> out;
    Fragmenter f(m.data(), m.size(), att, id);
    uint8_t buf[512]; size_t len;
    while (f.next(buf, &len)) { CHECK(len <= att); out.emplace_back(buf, buf + len); }
    return out;
}

int main() {
    // round trip for many sizes and MTUs
    for (size_t att : {20u, 23u, 100u, 185u, 244u}) {
        for (size_t n = 0; n <= kMaxMsg; ++n) {
            std::vector<uint8_t> m(n);
            for (size_t i = 0; i < n; ++i) m[i] = (uint8_t)(i * 7 + n);
            auto fr = split(m, att, (uint8_t)n);
            CHECK(!fr.empty());
            Reassembler r; int got = 0;
            for (auto& f : fr) if (r.push(f.data(), f.size())) ++got;
            CHECK(got == 1);
            CHECK(r.size() == n);
            CHECK(n == 0 || std::memcmp(r.data(), m.data(), n) == 0);
            CHECK(r.stats().dropped == 0);
        }
    }
    // fragment counts
    CHECK(split(std::vector<uint8_t>(18), 20, 1).size() == 1);
    CHECK(split(std::vector<uint8_t>(19), 20, 1).size() == 2);
    CHECK(split(std::vector<uint8_t>(250), 244, 1).size() == 2);
    // too big / unusable
    { uint8_t b[300]; size_t l; Fragmenter f(b, 257, 244, 0); CHECK(!f.next(b, &l)); }
    { uint8_t b[8]; size_t l; Fragmenter f(b, 4, 2, 0); CHECK(!f.next(b, &l)); }
    // missing first fragment -> dropped
    {
        std::vector<uint8_t> m(100, 0x55);
        auto fr = split(m, 20, 5);
        Reassembler r;
        CHECK(!r.push(fr[1].data(), fr[1].size()));
        CHECK(r.stats().dropped == 1);
    }
    // id change mid-message -> dropped, next message still fine
    {
        std::vector<uint8_t> a(60, 1), b(60, 2);
        auto fa = split(a, 20, 1), fb = split(b, 20, 2);
        Reassembler r;
        CHECK(!r.push(fa[0].data(), fa[0].size()));
        CHECK(!r.push(fb[1].data(), fb[1].size()));
        CHECK(r.stats().dropped == 1);
        bool ok = false;
        for (auto& f : fb) ok = r.push(f.data(), f.size());
        CHECK(ok && r.size() == 60 && r.data()[0] == 2);
    }
    // new FIRST while active -> old one counted dropped, new one delivered
    {
        std::vector<uint8_t> a(60, 1), b(10, 2);
        auto fa = split(a, 20, 1), fb = split(b, 20, 2);
        Reassembler r;
        r.push(fa[0].data(), fa[0].size());
        CHECK(r.push(fb[0].data(), fb[0].size()));
        CHECK(r.stats().dropped == 1 && r.size() == 10);
    }
    // too short fragment, overflow
    {
        Reassembler r; uint8_t x[1] = {0};
        CHECK(!r.push(x, 1)); CHECK(r.stats().dropped == 1);
        uint8_t big[244]; big[0] = 1; big[1] = kFirst;
        CHECK(!r.push(big, sizeof big));          // 242
        big[1] = 0;
        CHECK(!r.push(big, sizeof big));          // 484 > 256 -> drop
        CHECK(r.stats().dropped == 2);
    }
    std::printf(fails ? "FAILED (%d)\n" : "OK\n", fails);
    return fails ? 1 : 0;
}
