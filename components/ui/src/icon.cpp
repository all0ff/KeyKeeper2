#include "ui/icon.hpp"

#include "lvgl.h"

namespace ui::icon {

const char* symbol(IconId id)
{
    switch (id) {
        case IconId::Folder:     return LV_SYMBOL_DIRECTORY;
        case IconId::Search:     return LV_SYMBOL_GPS; // closest available; no magnifier glyph in LVGL's default symbol font
        case IconId::Settings:   return LV_SYMBOL_SETTINGS;
        case IconId::Usb:        return LV_SYMBOL_USB;
        case IconId::WiFi:       return LV_SYMBOL_WIFI;
        case IconId::Backup:     return LV_SYMBOL_UPLOAD;
        case IconId::Restore:    return LV_SYMBOL_DOWNLOAD;
        case IconId::Import:     return LV_SYMBOL_DOWNLOAD;
        case IconId::Export:     return LV_SYMBOL_UPLOAD;
        case IconId::Lock:       return LV_SYMBOL_EYE_CLOSE; // no padlock glyph; see README placeholder note
        case IconId::Unlock:     return LV_SYMBOL_EYE_OPEN;
        case IconId::Password:   return LV_SYMBOL_KEYBOARD;
        case IconId::Edit:       return LV_SYMBOL_EDIT;
        case IconId::Delete:     return LV_SYMBOL_TRASH;
        case IconId::Save:       return LV_SYMBOL_SAVE;
        case IconId::Cancel:     return LV_SYMBOL_CLOSE;
        case IconId::Warning:    return LV_SYMBOL_WARNING;
        case IconId::Error:      return LV_SYMBOL_CLOSE;
        case IconId::Success:    return LV_SYMBOL_OK;
        case IconId::Info:       return LV_SYMBOL_LIST;
        case IconId::Brightness: return LV_SYMBOL_IMAGE; // no brightness/sun glyph; see README
        case IconId::SdCard:     return LV_SYMBOL_SD_CARD;
        case IconId::About:      return LV_SYMBOL_WARNING; // no dedicated "info/about" glyph distinct from Warning

        // No reasonable LVGL symbol equivalent -- blank until the real
        // custom icon set exists.
        case IconId::Vault:
        case IconId::Favorite:
        case IconId::User:
        case IconId::Url:
        case IconId::Globe:
        case IconId::Otp:
        case IconId::Language:
        case IconId::Theme:
        case IconId::Flash:
            return "";
    }

    return "";
}

} // namespace ui::icon
