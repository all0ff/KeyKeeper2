#include "ui/localization.hpp"

namespace ui::i18n {

namespace {

settings::Language current_language = settings::Language::English;

struct Strings {
    const char* main_menu;
    const char* accounts;
    const char* settings;
    const char* backup;
    const char* lock;
    const char* about;
    const char* font_test;
    const char* general;
    const char* language;
    const char* theme;
    const char* brightness;
    const char* screen_timeout;
    const char* save;
    const char* rotate_select;
    const char* ok_open;
    const char* back_return;
    const char* rotate_change;
    const char* ok_back_confirm;
    const char* ok_open_back_cancel;
    const char* english;
    const char* russian;
    const char* off;
    const char* save_failed;
    const char* dark;
    const char* light;
    const char* orientation;
    const char* rotate0;
    const char* rotate180;
    const char* auto_;
    const char* all;
    const char* favorites;
    const char* categories;
    const char* search;
    const char* ok_open_back_return;
    const char* vault_title;
    const char* vault_empty;
    const char* rotate_select_ok_open_hold_ok_new_back_return;
    const char* account_title;
    const char* add_to_favorites;
    const char* remove_from_favorites;
    const char* field_category;
    const char* field_notes;
    const char* field_url;
    const char* field_username;
    const char* delete_;
    const char* edit;
    const char* delete_failed;
    const char* update_failed;
    const char* no_unused_codes_left;
    const char* not_allowed;
    const char* otp_value_fmt;
    const char* otp_invalid_secret;
    const char* otp_no_time_sync;
    const char* password_value_fmt;
    const char* password_masked;
    const char* press_ok_again_to_delete;
    const char* print_otp;
    const char* print_password;
    const char* print_recovery_codes;
    const char* print_seed_phrase;
    const char* print_url;
    const char* print_username;
    const char* rotate_scroll_back_return;
    const char* rotate_scroll_ok_reveal_hide_back_return;
    const char* rotate_select_ok_run_back_return;
    const char* recovery_codes_title;
    const char* seed_phrase_title;
    const char* reveal_password;
    const char* view_recovery_codes;
    const char* view_seed_phrase;
    const char* entry_not_found;
    const char* favorite_label;
    const char* confirm_action_fmt;
};

constexpr Strings EN = {
    "Main Menu", "Accounts", "Settings", "Backup", "Lock", "About", "Font Test",
    "General", "Language", "Theme", "Brightness", "Screen Timeout", "Save",
    "ROTATE  Select", "OK  Open", "BACK  Return", "ROTATE  Change",
    "OK/BACK  Confirm", "OK  Open    BACK  Cancel", "English", "Russian", "off",
    "Save failed", "Dark", "Light", "Orientation", "0\u00b0", "180\u00b0", "Auto",
    "All", "Favorites", "Categories", "Search", "OK  Open    BACK  Return",
    "Vault", "Vault is empty", "ROTATE  Select    OK  Open    Hold OK  New    BACK  Return",
    "Account", "Add to Favorites", "Remove from Favorites",
    "Category", "Notes", "URL", "Username", "Delete", "Edit",
    "Delete failed", "Update failed", "No unused codes left", "Not allowed",
    "OTP: %s (%us)", "OTP: invalid secret", "OTP: no time sync (connect WiFi)",
    "Password: %s", "Password: ********", "Press OK again to delete",
    "Print OTP", "Print Password", "Print Recovery Codes", "Print Seed Phrase",
    "Print URL", "Print Username",
    "ROTATE  Scroll    BACK  Return", "ROTATE  Scroll    OK  Reveal/Hide    BACK  Return",
    "ROTATE  Select    OK  Run    BACK  Return",
    "Recovery Codes", "Seed Phrase", "Reveal Password",
    "View Recovery Codes", "View Seed Phrase", "Entry not found", "* Favorite", "> %s (confirm?)"
};

constexpr Strings RU = {
    "Главное меню", "Учётные записи", "Настройки", "Резервная копия", "Заблокировать", "О программе", "Тест шрифтов",
    "Общие", "Язык", "Тема", "Яркость", "Тайм-аут экрана", "Сохранить",
    "ПОВОРОТ  Выбор", "OK  Открыть", "НАЗАД  Возврат", "ПОВОРОТ  Изменить",
    "OK/НАЗАД  Подтвердить", "OK  Открыть    НАЗАД  Отмена", "English", "Русский", "выкл.",
    "Ошибка сохранения", "Тёмная", "Светлая", "Ориентация", "0\u00b0", "180\u00b0", "Авто",
    "Все", "Избранное", "Категории", "Поиск", "OK  Открыть    НАЗАД  Возврат",
    "Хранилище", "Хранилище пусто", "ПОВОРОТ  Выбор    OK  Открыть    Удержание OK  Новая    НАЗАД  Возврат",
    "Запись", "В избранное", "Убрать из избранного",
    "Категория", "Заметки", "URL", "Логин", "Удалить", "Изменить",
    "Ошибка удаления", "Ошибка обновления", "Неиспользованных кодов не осталось", "Не разрешено",
    "OTP: %s (%uс)", "OTP: неверный секрет", "OTP: нет синхронизации времени (подключите WiFi)",
    "Пароль: %s", "Пароль: ********", "Нажмите OK ещё раз для удаления",
    "Напечатать OTP", "Напечатать пароль", "Напечатать коды восстановления", "Напечатать seed-фразу",
    "Напечатать URL", "Напечатать логин",
    "ПОВОРОТ  Прокрутка    НАЗАД  Возврат", "ПОВОРОТ  Прокрутка    OK  Показать/Скрыть    НАЗАД  Возврат",
    "ПОВОРОТ  Выбор    OK  Выполнить    НАЗАД  Возврат",
    "Коды восстановления", "Seed-фраза", "Показать пароль",
    "Коды восстановления", "Просмотр seed-фразы", "Запись не найдена", "* Избранное", "> %s (подтвердить?)"
};

const Strings& strings()
{
    return current_language == settings::Language::Russian ? RU : EN;
}

} // namespace

void set_language(settings::Language language)
{
    current_language = language;
}

settings::Language language()
{
    return current_language;
}

const char* tr(Key key)
{
    const Strings& s = strings();

    switch (key) {
        case Key::MainMenu: return s.main_menu;
        case Key::Accounts: return s.accounts;
        case Key::Settings: return s.settings;
        case Key::Backup: return s.backup;
        case Key::Lock: return s.lock;
        case Key::About: return s.about;
        case Key::FontTest: return s.font_test;
        case Key::General: return s.general;
        case Key::Language: return s.language;
        case Key::Theme: return s.theme;
        case Key::Brightness: return s.brightness;
        case Key::ScreenTimeout: return s.screen_timeout;
        case Key::Save: return s.save;
        case Key::RotateSelect: return s.rotate_select;
        case Key::OkOpen: return s.ok_open;
        case Key::BackReturn: return s.back_return;
        case Key::RotateChange: return s.rotate_change;
        case Key::OkBackConfirm: return s.ok_back_confirm;
        case Key::OkOpenBackCancel: return s.ok_open_back_cancel;
        case Key::English: return s.english;
        case Key::Russian: return s.russian;
        case Key::Off: return s.off;
        case Key::SaveFailed: return s.save_failed;
        case Key::Dark: return s.dark;
        case Key::Light: return s.light;
        case Key::Orientation: return s.orientation;
        case Key::Rotate0: return s.rotate0;
        case Key::Rotate180: return s.rotate180;
        case Key::Auto: return s.auto_;
        case Key::All: return s.all;
        case Key::Favorites: return s.favorites;
        case Key::Categories: return s.categories;
        case Key::Search: return s.search;
        case Key::OkOpenBackReturn: return s.ok_open_back_return;
        case Key::VaultTitle: return s.vault_title;
        case Key::VaultEmpty: return s.vault_empty;
        case Key::RotateSelectOkOpenHoldOkNewBackReturn: return s.rotate_select_ok_open_hold_ok_new_back_return;
        case Key::AccountTitle: return s.account_title;
        case Key::AddToFavorites: return s.add_to_favorites;
        case Key::RemoveFromFavorites: return s.remove_from_favorites;
        case Key::FieldCategory: return s.field_category;
        case Key::FieldNotes: return s.field_notes;
        case Key::FieldUrl: return s.field_url;
        case Key::FieldUsername: return s.field_username;
        case Key::Delete: return s.delete_;
        case Key::Edit: return s.edit;
        case Key::DeleteFailed: return s.delete_failed;
        case Key::UpdateFailed: return s.update_failed;
        case Key::NoUnusedCodesLeft: return s.no_unused_codes_left;
        case Key::NotAllowed: return s.not_allowed;
        case Key::OtpValueFmt: return s.otp_value_fmt;
        case Key::OtpInvalidSecret: return s.otp_invalid_secret;
        case Key::OtpNoTimeSync: return s.otp_no_time_sync;
        case Key::PasswordValueFmt: return s.password_value_fmt;
        case Key::PasswordMasked: return s.password_masked;
        case Key::PressOkAgainToDelete: return s.press_ok_again_to_delete;
        case Key::PrintOtp: return s.print_otp;
        case Key::PrintPassword: return s.print_password;
        case Key::PrintRecoveryCodes: return s.print_recovery_codes;
        case Key::PrintSeedPhrase: return s.print_seed_phrase;
        case Key::PrintUrl: return s.print_url;
        case Key::PrintUsername: return s.print_username;
        case Key::RotateScrollBackReturn: return s.rotate_scroll_back_return;
        case Key::RotateScrollOkRevealHideBackReturn: return s.rotate_scroll_ok_reveal_hide_back_return;
        case Key::RotateSelectOkRunBackReturn: return s.rotate_select_ok_run_back_return;
        case Key::RecoveryCodesTitle: return s.recovery_codes_title;
        case Key::SeedPhraseTitle: return s.seed_phrase_title;
        case Key::RevealPassword: return s.reveal_password;
        case Key::ViewRecoveryCodes: return s.view_recovery_codes;
        case Key::ViewSeedPhrase: return s.view_seed_phrase;
        case Key::EntryNotFound: return s.entry_not_found;
        case Key::FavoriteLabel: return s.favorite_label;
        case Key::ConfirmActionFmt: return s.confirm_action_fmt;
    }

    return "";
}

} // namespace ui::i18n
