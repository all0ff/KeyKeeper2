#pragma once

#include <cstdint>

// =============================================================================
// security::permission -- PermissionManager
//
// Per docs/REQUIREMENTS.md 9.3: gates the six sensitive operations
// listed there. This is a higher-level check meant to be called
// BEFORE a GUI screen/vault operation even starts (e.g. before
// showing the "change PIN" screen at all) -- it's complementary to,
// not a replacement for, security::pin::set_pin()'s own internal
// re-verification of the old PIN when a change is actually submitted.
// =============================================================================

namespace security::permission {

enum class Operation : uint8_t
{
    PrintPassword,
    PrintOtp,
    ExportVault,
    ChangePin,
    RestoreBackup,
    WebLogin,
};

enum class Result : uint8_t
{
    Allowed,
    DeniedLocked,        ///< Device is not Unlocked.
    DeniedNoSession,      ///< No active session (shouldn't normally happen if Unlocked).
    DeniedWebUiDisabled,  ///< Operation::WebLogin specifically, per settings::web_ui_permissions.
};

/**
 * @brief Must be called after security::lock::init() and
 *        security::session::init().
 */
bool init();

bool is_initialized();

Result check(Operation op);

bool is_allowed(Operation op);

} // namespace security::permission
