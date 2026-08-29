#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <optional>

namespace keykeeper {

enum class LockState { Locked, Unlocked };

class SecurityService {
public:
    static SecurityService& instance();
    
    bool init();
    
    // PIN management
    bool setPin(const std::vector<uint8_t>& pin);
    bool verifyPin(const std::vector<uint8_t>& pin);
    bool hasPin() const;
    
    // Lock state
    void lock();
    void unlock();
    LockState state() const { return state_; }
    
    // Encryption helpers (AES-256 via mbedTLS)
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data);
    
    // Auto-lock
    void activity();
    bool checkAutoLock(uint32_t timeoutSec);

private:
    SecurityService() = default;
    LockState state_{LockState::Locked};
    std::vector<uint8_t> pinHash_;
    uint32_t lastActivity_{0};
    
    static constexpr size_t AES_KEY_SIZE = 32;
    static constexpr size_t AES_IV_SIZE = 16;
};

} // namespace keykeeper