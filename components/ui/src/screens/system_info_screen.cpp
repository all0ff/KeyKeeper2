#include "ui/screens/system_info_screen.hpp"

#include "ui/theme.hpp"

#include "bsp/bsp.hpp"
#include "storage/filesystem.hpp"
#include "storage/sdcard.hpp"
#include "storage/storage.hpp"
#include "vault/vault_repository.hpp"

#include "esp_app_desc.h"
#include "esp_system.h"

#include <cstdio>

namespace ui::screens {

namespace {

constexpr lv_coord_t ROW_Y_START = 4;
constexpr lv_coord_t ROW_SPACING = 16;

} // namespace

const char* SystemInfoScreen::title() const
{
    return "System";
}

const char* SystemInfoScreen::footer_hint() const
{
    return "BACK  Return";
}

void SystemInfoScreen::initialize(lv_obj_t* content_parent)
{
    content_parent_ = content_parent;
    refresh();
}

void SystemInfoScreen::on_show()
{
    refresh();
}

void SystemInfoScreen::refresh()
{
    lv_obj_clean(content_parent_);

    const theme::Palette& pal = theme::current();
    lv_coord_t y = ROW_Y_START;

    auto add_row = [&](const char* text) {
        lv_obj_t* label = lv_label_create(content_parent_);
        lv_obj_set_style_text_color(label, pal.primary_text, 0);
        lv_label_set_text(label, text);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, y);
        y += ROW_SPACING;
    };

    char buf[64];

    const esp_app_desc_t* app_desc = esp_app_get_description();
    std::snprintf(buf, sizeof(buf), "Firmware: %s", app_desc != nullptr ? app_desc->version : "unknown");
    add_row(buf);

    std::snprintf(buf, sizeof(buf), "Vault DB format: v%u",
                   static_cast<unsigned>(vault::repository::VAULT_FORMAT_VERSION));
    add_row(buf);

    std::snprintf(buf, sizeof(buf), "Device: %s (rev %s)", bsp::board_name(), bsp::board_revision());
    add_row(buf);

    std::snprintf(buf, sizeof(buf), "Free heap: %u KB",
                   static_cast<unsigned>(esp_get_free_heap_size() / 1024));
    add_row(buf);

    size_t fs_total = 0;
    size_t fs_used = 0;
    if (storage::fs::get_usage(fs_total, fs_used)) {
        std::snprintf(buf, sizeof(buf), "Internal storage: %u / %u KB",
                       static_cast<unsigned>(fs_used / 1024), static_cast<unsigned>(fs_total / 1024));
    } else {
        std::snprintf(buf, sizeof(buf), "Internal storage: unavailable");
    }
    add_row(buf);

    if (storage::is_initialized() && storage::status().sdcard_present) {
        uint64_t sd_total = 0;
        uint64_t sd_used = 0;
        if (storage::sd::get_usage(sd_total, sd_used)) {
            // %llu risks a picolibc/newlib-nano 64-bit printf gap on
            // this target -- narrow to MB (unsigned, 32-bit) first,
            // safe well past any microSD size in practical use.
            const unsigned used_mb = static_cast<unsigned>(sd_used / (1024 * 1024));
            const unsigned total_mb = static_cast<unsigned>(sd_total / (1024 * 1024));
            std::snprintf(buf, sizeof(buf), "microSD: %u / %u MB", used_mb, total_mb);
        } else {
            std::snprintf(buf, sizeof(buf), "microSD: present, usage unavailable");
        }
    } else {
        std::snprintf(buf, sizeof(buf), "microSD: not inserted");
    }
    add_row(buf);
}

bool SystemInfoScreen::on_input(InputAction action)
{
    (void)action;
    return false; // read-only -- BackShort pops via default nav, everything else ignored
}

} // namespace ui::screens
