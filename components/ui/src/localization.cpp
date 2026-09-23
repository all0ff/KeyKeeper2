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
    const char* checking;
    const char* unlock_title;
    const char* lock_screen_footer;
    const char* locked_out_try_later;
    const char* wipe_error;
    const char* wrong_pin_left_fmt;
    const char* wrong_pin_until_wipe_fmt;
    const char* enter_new_pin;
    const char* confirm_pin;
    const char* setup_pin_title;
    const char* setup_pin_footer;
    const char* ok_retry;
    const char* pins_do_not_match;
    const char* failed_to_save_pin_length;
    const char* failed_to_save_pin;
    const char* unlock_failed_after_pin_set;
    const char* change_pin_row_fmt;
    const char* duress_pin_row_fmt;
    const char* configured;
    const char* not_set_value;
    const char* factory_reset_confirm_row_fmt;
    const char* factory_reset_row_fmt;
    const char* auto_lock_off_row_fmt;
    const char* auto_lock_min_row_fmt;
    const char* web_ui_view_row_fmt;
    const char* allowed_value;
    const char* off_value;
    const char* pin_entry_row_fmt;
    const char* dial_value;
    const char* standard_value;
    const char* dial_last_digit_row_fmt;
    const char* reverse_value;
    const char* ok_short_value;
    const char* save_row_fmt;
    const char* duress_pin_set;
    const char* duress_pin_setup_cancelled;
    const char* duress_pin_setup_failed;
    const char* duress_pins_did_not_match;
    const char* factory_reset_failed;
    const char* pin_change_cancelled;
    const char* pin_change_failed;
    const char* pin_changed;
    const char* pins_did_not_match;
    const char* reset_complete_restarting;
    const char* confirm_erase_everything;
    const char* security_title;
    const char* adjust_footer;
    const char* changing_pin_footer;
    const char* enter_current_pin;
    const char* confirm_new_pin;
    const char* enter_duress_pin;
    const char* confirm_duress_pin;
    const char* too_many_failed_attempts;
    const char* wifi_title;
    const char* mode_disabled;
    const char* mode_station;
    const char* mode_access_point;
    const char* wifi_idle;
    const char* wifi_connecting;
    const char* wifi_connected;
    const char* wifi_disconnected;
    const char* wifi_ap_running;
    const char* wifi_failed;
    const char* wifi_status_fmt;
    const char* wifi_status_ap_fmt;
    const char* mode_row_fmt;
    const char* station_ssid_row_fmt;
    const char* station_password_row_fmt;
    const char* ap_ssid_row_fmt;
    const char* ap_password_row_fmt;
    const char* captive_portal_row_fmt;
    const char* secret_word_row_fmt;
    const char* save_apply_row_fmt;
    const char* on_value;
    const char* editing_ap_password;
    const char* editing_ap_ssid;
    const char* editing_secret_word;
    const char* editing_station_password;
    const char* editing_station_ssid;
    const char* saved_but_failed_to_apply;
    const char* wifi_settings_saved_applying;
    const char* editing_password_shortcut;
    const char* login_tab_password_enter;
    const char* password_only_value;
    const char* password_enter_value;
    const char* usb_settings_saved;
    const char* usb_title;
    const char* password_shortcut_row_fmt;
    const char* delay_before_typing_row_fmt;
    const char* delay_between_chars_row_fmt;
    const char* delay_between_fields_row_fmt;
    const char* print_sequence_row_fmt;
    const char* auto_switch_layout_row_fmt;
    const char* empty_value;
    const char* open_value;
    const char* disabled_value;
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
    "> %s -- confirm?", "> %s (%uKB) -- confirm?",
    "Checking...", "Unlock", "ROTATE Digit  OK Next  Hold OK Done  BACK Erase",
    "Locked out, try later", "Wipe error",
    "Wrong PIN, %u left", "Wrong PIN! %u attempts until vault wipe",
    "Enter new PIN", "Confirm PIN", "Setup PIN",
    "ROTATE Digit OK Next Hold OK Done BACK Erase", "OK  Retry",
    "PINs do not match!", "Failed to save PIN length setting",
    "Failed to save PIN", "Unlock failed after PIN set",
    "%sChange PIN", "%sDuress PIN: %s", "Configured", "Not set",
    "%sFactory Reset (confirm?)", "%sFactory Reset",
    "%sAuto Lock: off", "%sAuto Lock: %lu min",
    "%sWeb UI View: %s", "Allowed", "Off",
    "%sPIN Entry: %s", "Dial", "Standard",
    "%sDial Last Digit: %s", "Reverse", "OkShort",
    "%sSave",
    "Duress PIN set", "Duress PIN setup cancelled", "Duress PIN setup failed",
    "Duress PINs did not match", "Factory reset failed",
    "PIN change cancelled", "PIN change failed", "PIN changed", "PINs did not match",
    "Reset complete. Restarting...",
    "This erases EVERYTHING. Press OK again to confirm.",
    "Security", "ROTATE  Change    OK/BACK  Confirm",
    "ROTATE Digit OK Next Hold OK Done BACK Erase/Cancel",
    "Enter current PIN", "Confirm new PIN", "Enter duress PIN", "Confirm duress PIN",
    "Too many failed attempts",
    "WiFi", "Disabled", "Station", "Access Point",
    "Idle", "Connecting...", "Connected", "Disconnected", "AP running", "Failed",
    "%s: %s", "%s: %s (%u client%s)",
    "%sMode: %s", "%sStation SSID: %s", "%sStation Password: %s",
    "%sAP SSID: %s", "%sAP Password: %s", "%sCaptive Portal: %s", "%sSecret Word: %s",
    "%sSave & Apply", "on",
    "Editing: AP Password", "Editing: AP SSID", "Editing: Secret Word",
    "Editing: Station Password", "Editing: Station SSID",
    "Saved, but failed to apply", "WiFi settings saved, applying...",
    "Editing: Password Shortcut", "Login+Tab+Password+Enter", "Password Only", "Password+Enter",
    "USB settings saved", "USB",
    "%sPassword Shortcut: %s", "%sDelay Before Typing: %ums", "%sDelay Between Chars: %ums",
    "%sDelay Between Fields: %ums", "%sPrint Sequence: %s", "%sAuto-switch layout: %s", "(empty)", "(open)", "(disabled)"
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
    "> %s -- подтвердить?", "> %s (%uKB) -- подтвердить?",
    "Проверка...", "Разблокировка", "ПОВОРОТ Цифра  OK Далее  Удержание OK Готово  НАЗАД Стереть",
    "Заблокировано, попробуйте позже", "Ошибка стирания",
    "Неверный PIN, осталось %u", "Неверный PIN! Ещё %u попыток до стирания хранилища",
    "Введите новый PIN", "Подтвердите PIN", "Установка PIN",
    "ПОВОРОТ Цифра OK Далее Удержание OK Готово НАЗАД Стереть", "OK  Повторить",
    "PIN не совпадают!", "Не удалось сохранить длину PIN",
    "Не удалось сохранить PIN", "Ошибка разблокировки после установки PIN",
    "%sСменить PIN", "%sPIN под принуждением: %s", "Настроен", "Не задан",
    "%sСброс до заводских (подтвердить?)", "%sСброс до заводских",
    "%sАвто-блокировка: выкл.", "%sАвто-блокировка: %lu мин",
    "%sДоступ из веб: %s", "Разрешён", "Выкл.",
    "%sНабор PIN: %s", "Лимбовый", "Обычный",
    "%sПоследняя цифра: %s", "Разворот", "OkShort",
    "%sСохранить",
    "PIN под принуждением установлен", "Настройка PIN под принуждением отменена",
    "Ошибка настройки PIN под принуждением",
    "PIN под принуждением не совпадают", "Ошибка сброса до заводских",
    "Смена PIN отменена", "Ошибка смены PIN", "PIN изменён", "PIN не совпадают",
    "Сброс завершён. Перезагрузка...",
    "Это сотрёт ВСЁ содержимое. Нажмите OK ещё раз для подтверждения.",
    "Безопасность", "ПОВОРОТ  Изменить    OK/НАЗАД  Подтвердить",
    "ПОВОРОТ Цифра OK Далее Удержание OK Готово НАЗАД Стереть/Отмена",
    "Введите текущий PIN", "Подтвердите новый PIN", "Введите PIN под принуждением", "Подтвердите PIN под принуждением",
    "Слишком много неудачных попыток",
    "WiFi", "Выключен", "Клиент", "Точка доступа",
    "Простой", "Подключение...", "Подключено", "Отключено", "Точка доступа активна", "Ошибка",
    "%s: %s", "%s: %s (клиентов: %u)",
    "%sРежим: %s", "%sSSID клиента: %s", "%sПароль клиента: %s",
    "%sSSID точки доступа: %s", "%sПароль точки доступа: %s", "%sCaptive-портал: %s", "%sСекретное слово: %s",
    "%sСохранить и применить", "вкл.",
    "Изменение: пароль точки доступа", "Изменение: SSID точки доступа", "Изменение: секретное слово",
    "Изменение: пароль клиента", "Изменение: SSID клиента",
    "Сохранено, но не удалось применить", "Настройки WiFi сохранены, применяются...",
    "Изменение: сочетание для пароля", "Логин+Tab+Пароль+Enter", "Только пароль", "Пароль+Enter",
    "Настройки USB сохранены", "USB",
    "%sСочетание для пароля: %s", "%sЗадержка перед вводом: %uмс", "%sЗадержка между символами: %uмс",
    "%sЗадержка между полями: %uмс", "%sПоследовательность печати: %s", "%sАвто-раскладка (кириллица): %s", "(пусто)", "(открыта)", "(выключено)"
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
        case Key::Checking: return s.checking;
        case Key::UnlockTitle: return s.unlock_title;
        case Key::LockScreenFooter: return s.lock_screen_footer;
        case Key::LockedOutTryLater: return s.locked_out_try_later;
        case Key::WipeError: return s.wipe_error;
        case Key::WrongPinLeftFmt: return s.wrong_pin_left_fmt;
        case Key::WrongPinUntilWipeFmt: return s.wrong_pin_until_wipe_fmt;
        case Key::EnterNewPin: return s.enter_new_pin;
        case Key::ConfirmPin: return s.confirm_pin;
        case Key::SetupPinTitle: return s.setup_pin_title;
        case Key::SetupPinFooter: return s.setup_pin_footer;
        case Key::OkRetry: return s.ok_retry;
        case Key::PinsDoNotMatch: return s.pins_do_not_match;
        case Key::FailedToSavePinLength: return s.failed_to_save_pin_length;
        case Key::FailedToSavePin: return s.failed_to_save_pin;
        case Key::UnlockFailedAfterPinSet: return s.unlock_failed_after_pin_set;
        case Key::ChangePinRowFmt: return s.change_pin_row_fmt;
        case Key::DuressPinRowFmt: return s.duress_pin_row_fmt;
        case Key::Configured: return s.configured;
        case Key::NotSetValue: return s.not_set_value;
        case Key::FactoryResetConfirmRowFmt: return s.factory_reset_confirm_row_fmt;
        case Key::FactoryResetRowFmt: return s.factory_reset_row_fmt;
        case Key::AutoLockOffRowFmt: return s.auto_lock_off_row_fmt;
        case Key::AutoLockMinRowFmt: return s.auto_lock_min_row_fmt;
        case Key::WebUiViewRowFmt: return s.web_ui_view_row_fmt;
        case Key::AllowedValue: return s.allowed_value;
        case Key::OffValue: return s.off_value;
        case Key::PinEntryRowFmt: return s.pin_entry_row_fmt;
        case Key::DialValue: return s.dial_value;
        case Key::StandardValue: return s.standard_value;
        case Key::DialLastDigitRowFmt: return s.dial_last_digit_row_fmt;
        case Key::ReverseValue: return s.reverse_value;
        case Key::OkShortValue: return s.ok_short_value;
        case Key::SaveRowFmt: return s.save_row_fmt;
        case Key::DuressPinSet: return s.duress_pin_set;
        case Key::DuressPinSetupCancelled: return s.duress_pin_setup_cancelled;
        case Key::DuressPinSetupFailed: return s.duress_pin_setup_failed;
        case Key::DuressPinsDidNotMatch: return s.duress_pins_did_not_match;
        case Key::FactoryResetFailed: return s.factory_reset_failed;
        case Key::PinChangeCancelled: return s.pin_change_cancelled;
        case Key::PinChangeFailed: return s.pin_change_failed;
        case Key::PinChanged: return s.pin_changed;
        case Key::PinsDidNotMatch: return s.pins_did_not_match;
        case Key::ResetCompleteRestarting: return s.reset_complete_restarting;
        case Key::ConfirmEraseEverything: return s.confirm_erase_everything;
        case Key::SecurityTitle: return s.security_title;
        case Key::AdjustFooter: return s.adjust_footer;
        case Key::ChangingPinFooter: return s.changing_pin_footer;
        case Key::EnterCurrentPin: return s.enter_current_pin;
        case Key::ConfirmNewPin: return s.confirm_new_pin;
        case Key::EnterDuressPin: return s.enter_duress_pin;
        case Key::ConfirmDuressPin: return s.confirm_duress_pin;
        case Key::TooManyFailedAttempts: return s.too_many_failed_attempts;
        case Key::WifiTitle: return s.wifi_title;
        case Key::ModeDisabled: return s.mode_disabled;
        case Key::ModeStation: return s.mode_station;
        case Key::ModeAccessPoint: return s.mode_access_point;
        case Key::WifiIdle: return s.wifi_idle;
        case Key::WifiConnecting: return s.wifi_connecting;
        case Key::WifiConnected: return s.wifi_connected;
        case Key::WifiDisconnected: return s.wifi_disconnected;
        case Key::WifiApRunning: return s.wifi_ap_running;
        case Key::WifiFailed: return s.wifi_failed;
        case Key::WifiStatusFmt: return s.wifi_status_fmt;
        case Key::WifiStatusApFmt: return s.wifi_status_ap_fmt;
        case Key::ModeRowFmt: return s.mode_row_fmt;
        case Key::StationSsidRowFmt: return s.station_ssid_row_fmt;
        case Key::StationPasswordRowFmt: return s.station_password_row_fmt;
        case Key::ApSsidRowFmt: return s.ap_ssid_row_fmt;
        case Key::ApPasswordRowFmt: return s.ap_password_row_fmt;
        case Key::CaptivePortalRowFmt: return s.captive_portal_row_fmt;
        case Key::SecretWordRowFmt: return s.secret_word_row_fmt;
        case Key::SaveApplyRowFmt: return s.save_apply_row_fmt;
        case Key::OnValue: return s.on_value;
        case Key::EditingApPassword: return s.editing_ap_password;
        case Key::EditingApSsid: return s.editing_ap_ssid;
        case Key::EditingSecretWord: return s.editing_secret_word;
        case Key::EditingStationPassword: return s.editing_station_password;
        case Key::EditingStationSsid: return s.editing_station_ssid;
        case Key::SavedButFailedToApply: return s.saved_but_failed_to_apply;
        case Key::WifiSettingsSavedApplying: return s.wifi_settings_saved_applying;
        case Key::EditingPasswordShortcut: return s.editing_password_shortcut;
        case Key::LoginTabPasswordEnter: return s.login_tab_password_enter;
        case Key::PasswordOnlyValue: return s.password_only_value;
        case Key::PasswordEnterValue: return s.password_enter_value;
        case Key::UsbSettingsSaved: return s.usb_settings_saved;
        case Key::UsbTitle: return s.usb_title;
        case Key::PasswordShortcutRowFmt: return s.password_shortcut_row_fmt;
        case Key::DelayBeforeTypingRowFmt: return s.delay_before_typing_row_fmt;
        case Key::DelayBetweenCharsRowFmt: return s.delay_between_chars_row_fmt;
        case Key::DelayBetweenFieldsRowFmt: return s.delay_between_fields_row_fmt;
        case Key::PrintSequenceRowFmt: return s.print_sequence_row_fmt;
        case Key::AutoSwitchLayoutRowFmt: return s.auto_switch_layout_row_fmt;
        case Key::EmptyValue: return s.empty_value;
        case Key::OpenValue: return s.open_value;
        case Key::DisabledValue: return s.disabled_value;
    }

    return "";
}

} // namespace ui::i18n
