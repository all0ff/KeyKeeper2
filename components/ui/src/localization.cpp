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
    const char* field_too_long;
    const char* edit_entry_title;
    const char* new_entry_title;
    const char* editing_fmt;
    const char* entry_saved;
    const char* field_favorite;
    const char* name_username_cannot_be_empty;
    const char* field_name_username;
    const char* yes;
    const char* no;
    const char* edit_footer_typing;
    const char* edit_footer_view_unsaved;
    const char* edit_footer_otp_secret;
    const char* field_otp_secret;
    const char* otp_secret_invalid_base32;
    const char* password_generated;
    const char* password_generation_failed;
    const char* field_password;
    const char* backup_title;
    const char* create_backup;
    const char* restore_backup_title;
    const char* export_vault;
    const char* import_vault;
    const char* refresh_sd_card;
    const char* format_sd_card;
    const char* backup_footer_select_confirm;
    const char* backup_footer_import;
    const char* backup_footer_run;
    const char* backup_created_fmt;
    const char* backup_create_failed_locked_no_sd;
    const char* exported_fmt;
    const char* export_failed_locked_no_sd;
    const char* sd_card_mounted_size_fmt;
    const char* sd_card_mounted;
    const char* sd_card_unreadable;
    const char* no_sd_card_detected;
    const char* confirm_erase_card;
    const char* sd_card_formatted;
    const char* format_failed;
    const char* no_backups_found;
    const char* confirm_overwrite_vault;
    const char* restore_failed;
    const char* restored_restarting;
    const char* no_import_files;
    const char* imported_skipped_fmt;
    const char* confirm_backup_action_fmt;
    const char* confirm_backup_file_action_fmt;
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
    "View Recovery Codes", "View Seed Phrase", "Entry not found", "* Favorite", "> %s (confirm?)",
    "A field is too long", "Edit Entry", "New Entry", "Editing: %s", "Entry saved",
    "Favorite", "Name/Username cannot be empty", "Name/Username", "Yes", "No",
    "OK  Add char    Hold OK  Done    BACK  Erase    Hold BACK  Switch set",
    "OK  Open    BACK  Cancel (unsaved changes lost)",
    "OK  Open    Hold OK  Generate    BACK  Cancel",
    "OTP Secret", "OTP secret: invalid Base32 format",
    "Password generated", "Password generation failed", "Password",
    "Backup", "Create Backup", "Restore Backup", "Export Vault", "Import Vault",
    "Refresh SD Card", "Format SD Card",
    "OK  Select/Confirm    BACK  Return", "OK  Import    BACK  Return", "OK  Run    BACK  Return",
    "Created: %s", "Backup failed (locked or no SD card?)",
    "Exported: %s", "Export failed (locked or no SD card?)",
    "SD card mounted: %u / %u MB", "SD card mounted",
    "Card found but unreadable -- try Format SD Card", "No SD card detected",
    "This erases everything on the card. Press OK again to confirm.",
    "SD card formatted and mounted", "Format failed -- no card, or a hardware fault",
    "No backups found",
    "This will overwrite the current vault. Press OK again to confirm.",
    "Restore failed", "Restored. Restarting...",
    "No files in /sdcard/vault/import", "Imported %u, skipped %u",
    "> %s -- confirm?", "> %s (%uKB) -- confirm?"
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
    "Коды восстановления", "Просмотр seed-фразы", "Запись не найдена", "* Избранное", "> %s (подтвердить?)",
    "Поле слишком длинное", "Изменить запись", "Новая запись", "Редактирование: %s", "Запись сохранена",
    "Избранное", "Имя/логин не может быть пустым", "Имя/Логин", "Да", "Нет",
    "OK  Добавить символ    Удержание OK  Готово    НАЗАД  Стереть    Удержание НАЗАД  Набор",
    "OK  Открыть    НАЗАД  Отмена (несохранённые изменения будут потеряны)",
    "OK  Открыть    Удержание OK  Сгенерировать    НАЗАД  Отмена",
    "OTP-секрет", "OTP-секрет: неверный формат Base32",
    "Пароль сгенерирован", "Не удалось сгенерировать пароль", "Пароль",
    "Резервная копия", "Создать резервную копию", "Восстановить из копии", "Экспорт хранилища", "Импорт хранилища",
    "Обновить SD-карту", "Форматировать SD-карту",
    "OK  Выбор/Подтвердить    НАЗАД  Возврат", "OK  Импорт    НАЗАД  Возврат", "OK  Выполнить    НАЗАД  Возврат",
    "Создан: %s", "Ошибка резервного копирования (заблокировано или нет SD-карты?)",
    "Экспортировано: %s", "Ошибка экспорта (заблокировано или нет SD-карты?)",
    "SD-карта подключена: %u / %u МБ", "SD-карта подключена",
    "Карта найдена, но не читается -- попробуйте Format SD Card", "SD-карта не обнаружена",
    "Это сотрёт всё содержимое карты. Нажмите OK ещё раз для подтверждения.",
    "SD-карта отформатирована и подключена", "Ошибка форматирования -- нет карты или аппаратный сбой",
    "Резервных копий не найдено",
    "Это перезапишет текущее хранилище. Нажмите OK ещё раз для подтверждения.",
    "Ошибка восстановления", "Восстановлено. Перезагрузка...",
    "Нет файлов в /sdcard/vault/import", "Импортировано %u, пропущено %u",
    "> %s -- подтвердить?", "> %s (%uKB) -- подтвердить?"
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
        case Key::FieldTooLong: return s.field_too_long;
        case Key::EditEntryTitle: return s.edit_entry_title;
        case Key::NewEntryTitle: return s.new_entry_title;
        case Key::EditingFmt: return s.editing_fmt;
        case Key::EntrySaved: return s.entry_saved;
        case Key::FieldFavorite: return s.field_favorite;
        case Key::NameUsernameCannotBeEmpty: return s.name_username_cannot_be_empty;
        case Key::FieldNameUsername: return s.field_name_username;
        case Key::Yes: return s.yes;
        case Key::No: return s.no;
        case Key::EditFooterTyping: return s.edit_footer_typing;
        case Key::EditFooterViewUnsaved: return s.edit_footer_view_unsaved;
        case Key::EditFooterOtpSecret: return s.edit_footer_otp_secret;
        case Key::FieldOtpSecret: return s.field_otp_secret;
        case Key::OtpSecretInvalidBase32: return s.otp_secret_invalid_base32;
        case Key::PasswordGenerated: return s.password_generated;
        case Key::PasswordGenerationFailed: return s.password_generation_failed;
        case Key::FieldPassword: return s.field_password;
        case Key::BackupTitle: return s.backup_title;
        case Key::CreateBackup: return s.create_backup;
        case Key::RestoreBackupTitle: return s.restore_backup_title;
        case Key::ExportVault: return s.export_vault;
        case Key::ImportVault: return s.import_vault;
        case Key::RefreshSdCard: return s.refresh_sd_card;
        case Key::FormatSdCard: return s.format_sd_card;
        case Key::BackupFooterSelectConfirm: return s.backup_footer_select_confirm;
        case Key::BackupFooterImport: return s.backup_footer_import;
        case Key::BackupFooterRun: return s.backup_footer_run;
        case Key::BackupCreatedFmt: return s.backup_created_fmt;
        case Key::BackupCreateFailedLockedNoSd: return s.backup_create_failed_locked_no_sd;
        case Key::ExportedFmt: return s.exported_fmt;
        case Key::ExportFailedLockedNoSd: return s.export_failed_locked_no_sd;
        case Key::SdCardMountedSizeFmt: return s.sd_card_mounted_size_fmt;
        case Key::SdCardMounted: return s.sd_card_mounted;
        case Key::SdCardUnreadable: return s.sd_card_unreadable;
        case Key::NoSdCardDetected: return s.no_sd_card_detected;
        case Key::ConfirmEraseCard: return s.confirm_erase_card;
        case Key::SdCardFormatted: return s.sd_card_formatted;
        case Key::FormatFailed: return s.format_failed;
        case Key::NoBackupsFound: return s.no_backups_found;
        case Key::ConfirmOverwriteVault: return s.confirm_overwrite_vault;
        case Key::RestoreFailed: return s.restore_failed;
        case Key::RestoredRestarting: return s.restored_restarting;
        case Key::NoImportFiles: return s.no_import_files;
        case Key::ImportedSkippedFmt: return s.imported_skipped_fmt;
        case Key::ConfirmBackupActionFmt: return s.confirm_backup_action_fmt;
        case Key::ConfirmBackupFileActionFmt: return s.confirm_backup_file_action_fmt;
    }

    return "";
}

} // namespace ui::i18n
