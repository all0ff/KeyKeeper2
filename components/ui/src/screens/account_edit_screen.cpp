#include "ui/screens/account_edit_screen.hpp"

#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "vault/vault.hpp"

#include "esp_log.h"

namespace ui::screens {

namespace {

constexpr char TAG[] = "ui.account_edit";

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 20;

bool is_plausible_base32(const std::string& s)
{
    if (s.empty()) {
        return true; // no OTP configured -- valid
    }
    if (s.size() < 8) {
        return false;
    }
    for (char c : s) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7') || c == '=';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string to_upper(const std::string& s)
{
    std::string out = s;
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return out;
}

} // namespace

AccountEditScreen::AccountEditScreen(uint32_t entry_id) : entry_id_(entry_id) {}

const char* AccountEditScreen::title() const
{
    return (entry_id_ == vault::INVALID_ID) ? "New Entry" : "Edit Entry";
}

const char* AccountEditScreen::footer_hint() const
{
    if (mode_ == Mode::EditField) {
        return "OK  Add char    Hold OK  Done    BACK  Erase";
    }
    return "OK  Open    BACK  Cancel (unsaved changes lost)";
}

void AccountEditScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;

    if (entry_id_ != vault::INVALID_ID) {
        vault::get_entry(entry_id_, entry_);
    }

    build_rows(content_parent_);
}

void AccountEditScreen::on_show()
{
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
}

void AccountEditScreen::build_rows(lv_obj_t* parent)
{
    for (size_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t* label = lv_label_create(parent);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, ROW_Y_START + static_cast<lv_coord_t>(ROW_SPACING * i));
        row_labels_[i] = label;
    }

    const theme::Palette& pal = theme::current();
    status_label_ = lv_label_create(parent);
    lv_obj_set_style_text_color(status_label_, pal.secondary_text, 0);
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_rows();
}

const char* AccountEditScreen::field_label(FieldId field) const
{
    switch (field) {
        case FieldId::Login:     return "Name/Username";
        case FieldId::Password:  return "Password";
        case FieldId::Url:       return "URL";
        case FieldId::OtpSecret: return "OTP Secret";
        case FieldId::Notes:     return "Notes";
        case FieldId::Save:      return "Save";
        case FieldId::Count:     return "";
    }
    return "";
}

size_t AccountEditScreen::field_max_length(FieldId field) const
{
    switch (field) {
        case FieldId::Login:     return vault::MAX_LOGIN_LEN;
        case FieldId::Password:  return vault::MAX_PASSWORD_LEN;
        case FieldId::Url:       return vault::MAX_URL_LEN;
        case FieldId::OtpSecret: return vault::MAX_TOTP_SECRET_LEN;
        case FieldId::Notes:     return vault::MAX_NOTES_LEN;
        default:                 return 64;
    }
}

void AccountEditScreen::render_rows()
{
    const theme::Palette& pal = theme::current();

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const bool is_selected = (i == selected_row_);
        lv_obj_set_style_text_color(row_labels_[i], is_selected ? pal.accent : pal.primary_text, 0);
        const char* prefix = is_selected ? "> " : "";

        const auto field = static_cast<FieldId>(i);
        switch (field) {
            case FieldId::Login:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                       entry_.login.empty() ? "(empty)" : entry_.login.c_str());
                break;
            case FieldId::Password:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                       entry_.password.empty() ? "(empty)" : "********");
                break;
            case FieldId::Url:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                       entry_.url.empty() ? "(empty)" : entry_.url.c_str());
                break;
            case FieldId::OtpSecret:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                       entry_.totp_secret.empty() ? "(empty)" : "(set)");
                break;
            case FieldId::Notes:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                       entry_.notes.empty() ? "(empty)" : entry_.notes.c_str());
                break;
            case FieldId::Save:
                lv_label_set_text_fmt(row_labels_[i], "%s%s", prefix, field_label(field));
                break;
            case FieldId::Count:
                break;
        }
    }

    if (ROW_COUNT > 0) {
        lv_obj_scroll_to_view(row_labels_[selected_row_], LV_ANIM_ON);
    }
}

void AccountEditScreen::move_selection(int32_t delta)
{
    int32_t index = static_cast<int32_t>(selected_row_) + delta;
    const int32_t count = static_cast<int32_t>(ROW_COUNT);
    if (index < 0) {
        index = count - 1;
    }
    if (index >= count) {
        index = 0;
    }
    selected_row_ = static_cast<size_t>(index);

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, "");
    }
    render_rows();
}

void AccountEditScreen::enter_edit_mode()
{
    const auto field = static_cast<FieldId>(selected_row_);
    if (field == FieldId::Save) {
        try_save();
        return;
    }

    editing_field_ = field;
    mode_ = Mode::EditField;

    lv_obj_clean(content_parent_);

    const theme::Palette& pal = theme::current();
    edit_header_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(edit_header_label_, pal.secondary_text, 0);
    lv_label_set_text_fmt(edit_header_label_, "Editing: %s", field_label(field));
    lv_obj_align(edit_header_label_, LV_ALIGN_TOP_LEFT, 4, 4);

    const std::string* current_value = nullptr;
    switch (field) {
        case FieldId::Login:     current_value = &entry_.login; break;
        case FieldId::Password:  current_value = &entry_.password; break;
        case FieldId::Url:       current_value = &entry_.url; break;
        case FieldId::OtpSecret: current_value = &entry_.totp_secret; break;
        case FieldId::Notes:     current_value = &entry_.notes; break;
        default: break;
    }

    widgets::TextEntry::Config cfg{};
    cfg.max_length = field_max_length(field);
    cfg.mask = (field == FieldId::Password);
    cfg.initial_value = (current_value != nullptr) ? current_value->c_str() : "";

    text_entry_.init(content_parent_, cfg);
    lv_obj_align(text_entry_.root(), LV_ALIGN_TOP_LEFT, 4, 24);
}

void AccountEditScreen::exit_edit_mode_ui()
{
    mode_ = Mode::SelectField;
    lv_obj_clean(content_parent_);
    build_rows(content_parent_);
}

void AccountEditScreen::apply_edited_field()
{
    std::string value = text_entry_.text();

    // Base32 is conventionally uppercase; TextEntry lets you type
    // either case, so normalize on commit for this field only.
    if (editing_field_ == FieldId::OtpSecret) {
        value = to_upper(value);
    }

    switch (editing_field_) {
        case FieldId::Login:     entry_.login = value; break;
        case FieldId::Password:  entry_.password = value; break;
        case FieldId::Url:       entry_.url = value; break;
        case FieldId::OtpSecret: entry_.totp_secret = value; break;
        case FieldId::Notes:     entry_.notes = value; break;
        default: break;
    }
}

void AccountEditScreen::try_save()
{
    if (entry_.login.empty()) {
        lv_label_set_text(status_label_, "Name/Username cannot be empty");
        return;
    }

    if (!is_plausible_base32(entry_.totp_secret)) {
        lv_label_set_text(status_label_, "OTP secret: invalid Base32 format");
        return;
    }

    if (!vault::validate(entry_)) {
        lv_label_set_text(status_label_, "A field is too long");
        return;
    }

    bool ok;
    if (entry_id_ == vault::INVALID_ID) {
        const uint32_t new_id = vault::create_entry(entry_);
        ok = (new_id != vault::INVALID_ID);
    } else {
        entry_.id = entry_id_;
        ok = vault::update_entry(entry_);
    }

    if (ok) {
        ESP_LOGI(TAG, "Entry saved");
        manager().pop();
    } else {
        lv_label_set_text(status_label_, "Save failed");
    }
}

bool AccountEditScreen::on_input(InputAction action)
{
    if (mode_ == Mode::EditField) {
        const bool consumed = text_entry_.on_input(action);

        if (text_entry_.is_finished()) {
            apply_edited_field();
            exit_edit_mode_ui();
            return true;
        }

        if (!consumed) {
            // BackShort with nothing typed in this field -- cancel
            // editing it (discard) rather than leaving the whole
            // screen.
            exit_edit_mode_ui();
            return true;
        }

        return true;
    }

    switch (action) {
        case InputAction::RotateLeft:
            move_selection(-1);
            return true;

        case InputAction::RotateRight:
            move_selection(+1);
            return true;

        case InputAction::OkShort:
            enter_edit_mode();
            return true;

        case InputAction::BackShort:
            return false; // pop, discarding unsaved changes -- see header comment

        default:
            return false;
    }
}

} // namespace ui::screens
