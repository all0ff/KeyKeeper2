#pragma once

#include "ui/screen.hpp"
#include "ui/widgets/text_entry.hpp"

#include "vault/vault_model.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::AccountEditScreen
//
// docs/GUI.md section 12 ("Edit Screen") -- creates or edits an
// account. Construct with vault::INVALID_ID to create a new entry, or
// an existing entry's id to edit it.
//
// Editable Fields per GUI.md 12: Name, URL, Username, Password, OTP
// Secret, Notes, Category, Favorite. As with every other vault
// screen, vault::VaultEntry (vault_model.hpp) has no Name, Category,
// or Favorite field -- those three are not editable here because
// there is nowhere to store them. GUI.md's validation rule "Name
// must not be empty" is applied to Username (VaultEntry::login)
// instead, since that's the closest thing to an identifying field the
// model actually has.
//
// "OTP must have a valid format" (GUI.md 12) is checked as: empty is
// fine (no OTP configured), non-empty must be valid Base32
// (A-Z, 2-7, optional '=' padding) -- the conventional TOTP secret
// encoding. This does not mean OTP codes are generated; see
// vault_model.hpp's file comment.
//
// One field is edited at a time via widgets::TextEntry (rotate = spin
// through a field-list of rows; OK on a row enters edit mode for it;
// OK-long inside edit mode confirms that field and returns to the row
// list; BACK on an empty field cancels editing that field). "Save"
// is its own row at the bottom -- selecting it runs validation and
// calls vault::create_entry()/vault::update_entry(). Per GUI.md 12
// ("GUI does not save data itself"), this screen never touches
// storage directly, only vault::.
//
// BACK from the row list (nothing being edited) leaves the screen
// WITHOUT saving, discarding any changes made so far -- there is no
// confirmation dialog (none exists yet), so this is a real, silent
// data-loss risk for an in-progress edit. Flagged, not solved.
// =============================================================================

namespace ui::screens {

class AccountEditScreen : public Screen
{
public:
    /// entry_id == vault::INVALID_ID creates a new entry on Save;
    /// otherwise edits that existing entry.
    explicit AccountEditScreen(uint32_t entry_id);

    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    enum class Mode : uint8_t
    {
        SelectField,
        EditField,
    };

    enum class FieldId : uint8_t
    {
        Login,
        Password,
        Url,
        OtpSecret,
        Notes,
        Save,
        Count,
    };

    void build_rows(lv_obj_t* parent);
    void render_rows();
    void enter_edit_mode();
    void exit_edit_mode_ui();
    void apply_edited_field();
    void move_selection(int32_t delta);
    void try_save();

    const char* field_label(FieldId field) const;
    size_t field_max_length(FieldId field) const;

    uint32_t entry_id_;
    vault::VaultEntry entry_{}; // working copy, only touches vault:: on Save

    Mode mode_ = Mode::SelectField;
    size_t selected_row_ = 0;

    widgets::TextEntry text_entry_; // reused across fields, see set_mask()/set_max_length()
    FieldId editing_field_ = FieldId::Login;

    static constexpr size_t ROW_COUNT = static_cast<size_t>(FieldId::Count);
    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* edit_header_label_ = nullptr;

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
};

} // namespace ui::screens
