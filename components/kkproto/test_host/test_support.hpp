#pragma once
// Small helpers shared by the host tests (host only: uses the standard library freely).
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;

inline int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        if (!(cond)) {                                                                               \
            ++g_failed;                                                                              \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                              \
        }                                                                                            \
    } while (0)
#define CHECK_MSG(cond, ...)                                                                         \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        if (!(cond)) {                                                                               \
            ++g_failed;                                                                              \
            std::printf("FAIL %s:%d  %s  -- ", __FILE__, __LINE__, #cond);                           \
            std::printf(__VA_ARGS__);                                                                \
            std::printf("\n");                                                                       \
        }                                                                                            \
    } while (0)

inline Bytes from_hex(const std::string& h)
{
    Bytes out;
    if (h == "-") return out;
    auto nib = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0)); };
    for (size_t i = 0; i + 1 < h.size(); i += 2) out.push_back(static_cast<uint8_t>((nib(h[i]) << 4) | nib(h[i + 1])));
    return out;
}
inline std::string to_hex(const uint8_t* p, size_t n)
{
    std::string s; char b[3];
    for (size_t i = 0; i < n; ++i) { std::snprintf(b, sizeof b, "%02x", p[i]); s += b; }
    return s;
}
inline std::string to_hex(const Bytes& v) { return to_hex(v.data(), v.size()); }

inline int finish(const char* name)
{
    std::printf("%s: %d checks, %d failed\n", name, g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
