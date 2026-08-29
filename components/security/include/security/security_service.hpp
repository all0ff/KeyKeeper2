#pragma once

// =============================================================================
// security -- SecurityService
//
// Thin orchestrator, per docs/ARCHITECTURE.md's SecurityService role
// ("координация внутренних менеджеров"). Does not duplicate
// pin/lock/session/permission's own APIs -- callers use
// security::pin::, security::lock::, security::session::,
// security::permission:: directly (each is a free-standing namespace,
// not a class instance you get from here). This file only enforces
// correct init() ordering, since lock::init() requires pin/session to
// already be up, and permission::init() requires lock/session.
// =============================================================================

namespace security {

/**
 * @brief Initialize pin, session, lock, and permission, in that order.
 *
 * Must be called after storage::init(), settings::init(), input::init(),
 * and event_bus::init().
 */
bool init();

bool is_initialized();

} // namespace security
