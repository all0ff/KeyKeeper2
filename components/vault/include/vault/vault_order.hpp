#pragma once

#include <cstdint>
#include <vector>

namespace vault::order {

// Persisted, user-chosen display order for vault entries -- separate
// from vault.db itself (stored in its own NVS namespace, keyed by
// entry ID, not by position in vault.db), so reordering never
// touches the actual encrypted vault data. Shared between every
// surface that lists accounts -- the Web UI's own drag-to-reorder
// feature and the device's own vault_list_screen both read/write
// through this single source of truth, so a reorder made on one
// shows up on the other, including across a reboot.
//
// Originally implemented as a Web-only concern (web::reorder, now a
// thin pass-through to this module -- see web_reorder_state.cpp's own
// comment) before the device's own UI grew the same need.

/// Initialize the account order state. Safe to call more than once.
bool init();

/// Returns the persisted order, normalized against the currently
/// existing vault IDs: stale IDs (deleted entries) are dropped, and
/// any current entry missing from the stored order (never ordered
/// yet, or created after the order was last saved) is appended in
/// the vault's own native order. Always returns exactly one entry
/// per currently-existing vault entry.
std::vector<uint32_t> get_order();

/// Replaces the persisted order. The supplied IDs must contain every
/// current vault entry exactly once -- anything else is rejected
/// (returns false) rather than silently partially applied.
bool set_order(const std::vector<uint32_t>& ids);

} // namespace vault::order
