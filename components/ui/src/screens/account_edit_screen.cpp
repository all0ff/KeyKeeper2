#include "ui/screens/account_edit_screen.hpp"

#include "display/fonts.hpp"
#include "ui/localization.hpp"
#include "ui/theme.hpp"
#include "ui/ui_manager.hpp"

#include "vault/vault.hpp"
#include "password_gen/password_gen.hpp"
#include "settings/settings.hpp"

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
    return (entry_id_ == vault::INVALID_ID) ? i18n::tr(i18n::Key::NewEntryTitle) : i18n::tr(i18n::Key::EditEntryTitle);
}

const char* AccountEditScreen::footer_hint() const
{
    if (mode_ == Mode::EditField) {
        return i18n::tr(i18n::Key::EditFooterTyping);
    }
    if (static_cast<FieldId>(selected_row_) == FieldId::Password) {
        return i18n::tr(i18n::Key::EditFooterOtpSecret);
    }
    return i18n::tr(i18n::Key::EditFooterViewUnsaved);
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
        // Rows show field VALUES (login/password/url/notes/category --
        // all user-entered, could be Cyrillic) alongside their fixed
        // English labels -- see widgets::TextEntry's own comment for
        // why this is set per-label, not as a global default theme font.
        lv_obj_set_style_text_font(label, &keykeeper_cyrillic_16, 0);
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
        case FieldId::Login:     return i18n::tr(i18n::Key::FieldNameUsername);
        case FieldId::Password:  return i18n::tr(i18n::Key::FieldPassword);
        case FieldId::Url:       return i18n::tr(i18n::Key::FieldUrl);
        case FieldId::OtpSecret: return i18n::tr(i18n::Key::FieldOtpSecret);
        case FieldId::Notes:     return i18n::tr(i18n::Key::FieldNotes);
        case FieldId::Category:  return i18n::tr(i18n::Key::FieldCategory);
        case FieldId::Favorite:  return i18n::tr(i18n::Key::FieldFavorite);
        case FieldId::Save:      return i18n::tr(i18n::Key::Save);
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
        case FieldId::Category:  return vault::MAX_CATEGORY_LEN;
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
                if (entry_.password.empty()) {
                    lv_label_set_text_fmt(row_labels_[i], "%s%s: (empty)", prefix, field_label(field));
                } else if (password_revealed_) {
                    lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                           entry_.password.c_str());
                } else {
                    lv_label_set_text_fmt(row_labels_[i], "%s%s: ********", prefix, field_label(field));
                }
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
            case FieldId::Category:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                       entry_.category.empty() ? "(none)" : entry_.category.c_str());
                break;
            case FieldId::Favorite:
                lv_label_set_text_fmt(row_labels_[i], "%s%s: %s", prefix, field_label(field),
                                       entry_.favorite ? i18n::tr(i18n::Key::Yes) : i18n::tr(i18n::Key::No));
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

    if (static_cast<FieldId>(selected_row_) != FieldId::Password) {
        password_revealed_ = false;
    }

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

    password_revealed_ = false; // re-mask -- TextEntry masks Password too, see below

    if (field == FieldId::Favorite) {
        // Plain boolean toggle -- no text entry involved, flip it in
        // place and stay in the row list.
        entry_.favorite = !entry_.favorite;
        render_rows();
        return;
    }

    editing_field_ = field;
    mode_ = Mode::EditField;

    lv_obj_clean(content_parent_);

    const theme::Palette& pal = theme::current();
    edit_header_label_ = lv_label_create(content_parent_);
    lv_obj_set_style_text_color(edit_header_label_, pal.secondary_text, 0);
    // Same bump as the value being typed below it (see
    // widgets::TextEntry's own comment), plus the same width+scroll
    // safety net as ui_manager.cpp's own footer_label_ (see that
    // file's comment for the full reasoning) -- at this size, a
    // longer field name plus the "Editing: " prefix can genuinely run
    // past 320px, especially in Russian.
    lv_obj_set_style_text_font(edit_header_label_, &keykeeper_cyrillic_24, 0);
    lv_obj_set_width(edit_header_label_, LV_PCT(96));
    lv_label_set_long_mode(edit_header_label_, LV_LABEL_LONG_SCROLL);
    lv_label_set_text_fmt(edit_header_label_, i18n::tr(i18n::Key::EditingFmt), field_label(field));
    lv_obj_align(edit_header_label_, LV_ALIGN_TOP_LEFT, 4, 4);

    const std::string* current_value = nullptr;
    switch (field) {
        case FieldId::Login:     current_value = &entry_.login; break;
        case FieldId::Password:  current_value = &entry_.password; break;
        case FieldId::Url:       current_value = &entry_.url; break;
        case FieldId::OtpSecret: current_value = &entry_.totp_secret; break;
        case FieldId::Notes:     current_value = &entry_.notes; break;
        case FieldId::Category:  current_value = &entry_.category; break;
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
        case FieldId::Category:  entry_.category = value; break;
        default: break;
    }
}

void AccountEditScreen::try_save()
{
    if (entry_.login.empty()) {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::NameUsernameCannotBeEmpty));
        return;
    }

    if (!is_plausible_base32(entry_.totp_secret)) {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::OtpSecretInvalidBase32));
        return;
    }

    if (!vault::validate(entry_)) {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::FieldTooLong));
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
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::SaveFailed));
    }
}

void AccountEditScreen::generate_password()
{
    char buf[password_gen::MAX_LENGTH + 1];
    if (!password_gen::generate(settings::all().password_gen, buf, sizeof(buf))) {
        if (status_label_ != nullptr) {
            lv_label_set_text(status_label_, i18n::tr(i18n::Key::PasswordGenerationFailed));
        }
        return;
    }

    entry_.password = buf;
    password_revealed_ = true; // see this member's own comment
    render_rows();

    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, i18n::tr(i18n::Key::PasswordGenerated));
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

        case InputAction::OkLong:
            if (static_cast<FieldId>(selected_row_) == FieldId::Password) {
                generate_password();
                return true;
            }
            return false;

        case InputAction::BackShort:
            return false; // pop, discarding unsaved changes -- see header comment

        default:
            return false;
    }
}

} // namespace ui::screens
