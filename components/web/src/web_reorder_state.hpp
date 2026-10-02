#pragma once

#include <cstdint>
#include <vector>

namespace web::reorder {

// Thin pass-through to vault::order (components/vault/include/vault/
// vault_order.hpp) -- this used to be its own, Web-only persisted
// order, separate from the device UI's own display order. The two
// are now the same shared state, owned by vault:: since both web::
// and ui:: already depend on it; kept as web::reorder:: here, rather
// than having web_reorder_routes.cpp call vault::order:: directly, so
// this header/the routes file didn't need to change at all.

/// Initialize the account order state. Safe to call more than once.
bool init();

/// Returns the persisted order, normalized against the currently existing vault IDs.
std::vector<uint32_t> get_order();

/// Replaces the persisted order. The supplied IDs must contain every current
/// vault entry exactly once.
bool set_order(const std::vector<uint32_t>& ids);

} // namespace web::reorder
