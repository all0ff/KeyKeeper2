#pragma once

#include "ui/screen.hpp"

#include "vault/vault_model.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::AccountViewScreen
//
// docs/GUI.md section 11 ("Account Screen"). Pushed from
// VaultListScreen with the selected entry's id.
//
// Displayed fields per GUI.md 11: Name, URL, Username, Password, OTP,
// Notes, Category, Favorite -- "empty fields are not shown". As with
// VaultListScreen, vault::VaultEntry (vault_model.hpp) has no Name,
// Category, or Favorite field at all, so those three are simply never
// shown here; not a bug, that data doesn't exist. "Username" is
// VaultEntry::login. OTP shows only a "configured" indicator -- no
// code is generated (see vault_model.hpp's file comment: no
// trustworthy time source yet).
//
// Password is masked by default with a fixed-width placeholder (not
// matching the real length, to avoid leaking that) and a "Reveal
// Password" action to show it in the clear -- a deliberate,
// safety-leaning reading of GUI.md 11, which just says "Password" is
// a displayed field without specifying masking either way.
//
// Available Actions per GUI.md 11: Print URL/Username/Password/OTP,
// Edit, Delete. Print* actions run a real security::permission::check()
// but the actual USB HID typing is a logged placeholder (no
// OutputChannel implementation exists -- see interfaces::channels).
// Edit pushes ui::screens::AccountEditScreen. Delete is
// REAL -- calls vault::delete_entry() -- gated by a two-step confirm
// (press the Delete action twice) since no confirmation dialog widget
// exists yet.
//
// There's no separate PermissionManager operation for "Print URL" or
// "Print Username" (REQUIREMENTS 9.3 only names Print Password/Print
// OTP among the print-related gated ops) -- both reuse
// Operation::PrintPassword's gate here as a placeholder, same
// decision already made in QuickScreen for its own Print URL action.
// =============================================================================

namespace ui::screens {

class AccountViewScreen : public Screen
{
public:
    explicit AccountViewScreen(uint32_t entry_id);

    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Action : uint8_t
    {
        RevealPassword,
        PrintUrl,
        PrintUsername,
        PrintPassword,
        PrintOtp,
        Edit,
        Delete,
    };

    void reload();
    lv_coord_t build_fields(lv_obj_t* parent);
    void build_actions(lv_obj_t* parent, lv_coord_t y_start);
    void render_actions();
    void update_password_label();
    void move_selection(int32_t delta);
    void activate();
    const char* action_name(Action action) const;

    uint32_t entry_id_;
    vault::VaultEntry entry_{};
    bool loaded_ = false;

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* password_value_label_ = nullptr;
    bool password_revealed_ = false;

    static constexpr size_t MAX_ACTIONS = 7;
    Action available_actions_[MAX_ACTIONS]{};
    size_t action_count_ = 0;
    lv_obj_t* action_labels_[MAX_ACTIONS]{};
    size_t selected_action_ = 0;

    bool delete_confirm_pending_ = false;

    lv_obj_t* status_label_ = nullptr;
};

} // namespace ui::screens
