#pragma once

#include "settings/settings_types.hpp"

namespace ui::i18n {

enum class Key : uint8_t {
    MainMenu,
    Accounts,
    Settings,
    Backup,
    Lock,
    About,
    FontTest,
    General,
    Language,
    Theme,
    Brightness,
    ScreenTimeout,
    Save,
    RotateSelect,
    OkOpen,
    BackReturn,
    RotateChange,
    OkBackConfirm,
    OkOpenBackCancel,
    English,
    Russian,
    Off,
    SaveFailed,
    Firmware,
    OkOpenBackReturn,
};

void set_language(settings::Language language);
settings::Language language();
const char* tr(Key key);

} // namespace ui::i18n
