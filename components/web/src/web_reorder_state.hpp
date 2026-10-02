#pragma once

#include <cstdint>
#include <vector>

namespace web::reorder {

/// Initialize the web-only account order state. Safe to call more than once.
bool init();

/// Returns the persisted order, normalized against the currently existing vault IDs.
std::vector<uint32_t> get_order();

/// Replaces the persisted order. The supplied IDs must contain every current
/// vault entry exactly once. The order is stored separately from vault.db and
/// is intentionally a Web UI presentation preference.
bool set_order(const std::vector<uint32_t>& ids);

} // namespace web::reorder
