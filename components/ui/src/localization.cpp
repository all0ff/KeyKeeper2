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
};

constexpr Strings EN = {
    "Main Menu", "Accounts", "Settings", "Backup", "Lock", "About", "Font Test",
    "General", "Language", "Theme", "Brightness", "Screen Timeout", "Save",
    "ROTATE  Select", "OK  Open", "BACK  Return", "ROTATE  Change",
    "OK/BACK  Confirm", "OK  Open    BACK  Cancel", "English", "Russian", "off",
    "Save failed", "Dark", "Light"
};

constexpr Strings RU = {
    "Главное меню", "Учётные записи", "Настройки", "Резервная копия", "Заблокировать", "О программе", "Тест шрифтов",
    "Общие", "Язык", "Тема", "Яркость", "Тайм-аут экрана", "Сохранить",
    "ПОВОРОТ  Выбор", "OK  Открыть", "НАЗАД  Возврат", "ПОВОРОТ  Изменить",
    "OK/НАЗАД  Подтвердить", "OK  Открыть    НАЗАД  Отмена", "English", "Русский", "выкл.",
    "Ошибка сохранения", "Тёмная", "Светлая"
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
    }

    return "";
}

} // namespace ui::i18n
