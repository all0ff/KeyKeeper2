#pragma once

#include <cstdint>

// =============================================================================
// security::session -- SessionManager
//
// Per docs/ARCHITECTURE.md: manages the current user session. Only
// local GUI sessions exist today (no Web UI/OTA/remote access yet),
// so this is intentionally thin -- but it's the architectural
// extension point those will plug into later without requiring
// LockManager/PermissionManager to change.
//
// Single session at a time (no multi-user yet, per REQUIREMENTS 9.4
// listing multi-user as Future Security). begin_session()/
// end_session() are called by security::lock on unlock/lock -- other
// components should treat this as read-only.
// =============================================================================

namespace security::session {

enum class Origin : uint8_t
{
    Local,  ///< The only origin actually used today.
    WebUi,  ///< Reserved, not produced by anything yet.
    Ota,    ///< Reserved, not produced by anything yet.
};

struct Session
{
    bool active = false;
    Origin origin = Origin::Local;
    uint32_t started_ms = 0;
};

bool init();
bool is_initialized();

bool has_active_session();

const Session& current();

/// Called by security::lock on a successful unlock. Not meant to be
/// called from outside the security component.
void begin_session(Origin origin = Origin::Local);

/// Called by security::lock on lock. Not meant to be called from
/// outside the security component.
void end_session();

} // namespace security::session
