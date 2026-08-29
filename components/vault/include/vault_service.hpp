#pragma once
#include "account.hpp"
#include "storage_service.hpp"
#include <vector>
#include <optional>

namespace keykeeper {

class VaultService {
public:
    static VaultService& instance();
    
    bool init();
    
    // Account operations
    std::vector<AccountInfo> loadAccountList();
    std::optional<Account> loadAccount(uint32_t id);
    bool saveAccount(const Account& acc);
    bool deleteAccount(uint32_t id);
    
    // Search
    std::vector<AccountInfo> search(const std::string& query);
    
    // Backup/Import/Export
    bool backup(const std::string& path);
    bool restore(const std::string& path);
    
    // Categories
    std::vector<std::pair<uint32_t, std::string>> getCategories();
    void setCategory(uint32_t id, const std::string& name);

private:
    VaultService() = default;
    bool initialized_{false};
    
    bool serializeAccount(const Account& acc, std::vector<uint8_t>& out);
    bool deserializeAccount(const std::vector<uint8_t>& data, Account& out);
    uint32_t generateId();
    
    std::vector<std::pair<uint32_t, std::string>> categories_;
};

} // namespace keykeeper