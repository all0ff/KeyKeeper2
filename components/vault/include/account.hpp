#pragma once
#include <string>
#include <cstdint>
#include <vector>

namespace keykeeper {

constexpr size_t MAX_TITLE_LEN = 64;
constexpr size_t MAX_URL_LEN = 256;
constexpr size_t MAX_USERNAME_LEN = 128;
constexpr size_t MAX_PASSWORD_LEN = 512;
constexpr size_t MAX_COMMENT_LEN = 1024;
constexpr size_t MAX_TOTP_SECRET_LEN = 128;
constexpr size_t MAX_ACCOUNTS = 4096;

struct AccountInfo {
    uint32_t id{0};
    std::string title;
    uint32_t categoryId{0};
    bool favorite{false};
    uint32_t modified{0};
};

struct AccountDetails {
    std::string url;
    std::string username;
    std::string password;
    std::string totpSecret;
    std::string comment;
};

struct Account {
    AccountInfo info;
    AccountDetails details;
};

} // namespace keykeeper