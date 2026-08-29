#include "vault_service.hpp"
#include "logger.hpp"
#include <cstring>
#include <cstdio>

namespace keykeeper {

static const char* TAG = "Vault";

VaultService& VaultService::instance() {
    static VaultService vs;
    return vs;
}

bool VaultService::init() {
    KK_LOGI(TAG, "Initializing vault...");
    // Load categories
    categories_ = {
        {1, "Internet"},
        {2, "Work"},
        {3, "Banking"},
        {4, "Servers"},
        {5, "Social"},
    };
    initialized_ = true;
    KK_LOGI(TAG, "Vault initialized");
    return true;
}

std::vector<AccountInfo> VaultService::loadAccountList() {
    std::vector<AccountInfo> list;
    std::vector<FileInfo> files;
    if (StorageService::instance().listDir("/vault/accounts", files) != ESP_OK) {
        return list;
    }
    
    for (const auto& f : files) {
        if (f.name.size() < 4 || f.name.substr(f.name.size()-4) != ".acc") continue;
        uint32_t id = static_cast<uint32_t>(std::stoul(f.name.substr(0, 6)));
        
        std::vector<uint8_t> data;
        if (StorageService::instance().readFile("/vault/accounts/" + f.name, data) != ESP_OK) continue;
        
        Account acc;
        if (!deserializeAccount(data, acc)) continue;
        
        acc.info.id = id;
        list.push_back(acc.info);
    }
    return list;
}

std::optional<Account> VaultService::loadAccount(uint32_t id) {
    char path[64];
    snprintf(path, sizeof(path), "/vault/accounts/%06lu.acc", id);
    
    std::vector<uint8_t> data;
    if (StorageService::instance().readFile(path, data) != ESP_OK) {
        return std::nullopt;
    }
    
    Account acc;
    if (!deserializeAccount(data, acc)) return std::nullopt;
    acc.info.id = id;
    return acc;
}

bool VaultService::saveAccount(const Account& acc) {
    uint32_t id = acc.info.id;
    if (id == 0) id = generateId();
    
    char path[64];
    snprintf(path, sizeof(path), "/vault/accounts/%06lu.acc", id);
    
    std::vector<uint8_t> data;
    if (!serializeAccount(acc, data)) return false;
    
    return StorageService::instance().writeFile(path, data) == ESP_OK;
}

bool VaultService::deleteAccount(uint32_t id) {
    char path[64];
    snprintf(path, sizeof(path), "/vault/accounts/%06lu.acc", id);
    return StorageService::instance().deleteFile(path) == ESP_OK;
}

std::vector<AccountInfo> VaultService::search(const std::string& query) {
    auto all = loadAccountList();
    std::vector<AccountInfo> result;
    for (const auto& a : all) {
        if (a.title.find(query) != std::string::npos) {
            result.push_back(a);
        }
    }
    return result;
}

bool VaultService::backup(const std::string& path) {
    // TODO: implement .kkp archive creation
    return true;
}

bool VaultService::restore(const std::string& path) {
    return true;
}

std::vector<std::pair<uint32_t, std::string>> VaultService::getCategories() {
    return categories_;
}

void VaultService::setCategory(uint32_t id, const std::string& name) {
    categories_.push_back({id, name});
}

bool VaultService::serializeAccount(const Account& acc, std::vector<uint8_t>& out) {
    // Simple binary format: version(1) + flags(1) + catId(4) + fav(1) + titleLen(1) + title + urlLen(2) + url + ...
    out.clear();
    out.push_back(1); // version
    out.push_back(0); // flags
    out.push_back(static_cast<uint8_t>(acc.info.categoryId));
    out.push_back(acc.info.favorite ? 1 : 0);
    
    auto appendStr = [&out](const std::string& s, uint16_t maxLen) {
        uint16_t len = static_cast<uint16_t>(std::min(s.size(), static_cast<size_t>(maxLen)));
        out.push_back(static_cast<uint8_t>(len & 0xFF));
        out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        out.insert(out.end(), s.begin(), s.begin() + len);
    };
    
    appendStr(acc.info.title, MAX_TITLE_LEN);
    appendStr(acc.details.url, MAX_URL_LEN);
    appendStr(acc.details.username, MAX_USERNAME_LEN);
    appendStr(acc.details.password, MAX_PASSWORD_LEN);
    appendStr(acc.details.totpSecret, MAX_TOTP_SECRET_LEN);
    appendStr(acc.details.comment, MAX_COMMENT_LEN);
    return true;
}

bool VaultService::deserializeAccount(const std::vector<uint8_t>& data, Account& out) {
    if (data.size() < 5) return false;
    size_t pos = 0;
    uint8_t version = data[pos++];
    if (version != 1) return false;
    pos++; // flags
    out.info.categoryId = data[pos++];
    out.info.favorite = data[pos++] != 0;
    
    auto readStr = [&data, &pos](std::string& s) -> bool {
        if (pos + 2 > data.size()) return false;
        uint16_t len = data[pos] | (data[pos+1] << 8);
        pos += 2;
        if (pos + len > data.size()) return false;
        s.assign(reinterpret_cast<const char*>(&data[pos]), len);
        pos += len;
        return true;
    };
    
    if (!readStr(out.info.title)) return false;
    if (!readStr(out.details.url)) return false;
    if (!readStr(out.details.username)) return false;
    if (!readStr(out.details.password)) return false;
    if (!readStr(out.details.totpSecret)) return false;
    if (!readStr(out.details.comment)) return false;
    return true;
}

uint32_t VaultService::generateId() {
    // Simple ID generation based on timestamp
    return static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
}

} // namespace keykeeper