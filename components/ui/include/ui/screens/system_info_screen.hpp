#pragma once

#include "ui/screen.hpp"

// =============================================================================
// ui::screens::SystemInfoScreen
//
// docs/GUI.md 14's System Settings: firmware version, DB version,
// device info, free memory, storage info. Entirely read-only -- not a
// settings:: consumer at all, just a snapshot of live values pulled
// from other components' own accessors:
//   - firmware version: esp_app_get_description()->version, ESP-IDF's
//     built-in app version (auto-derived from `git describe` at build
//     time -- there is no separate project-defined version string
//     anywhere in this codebase).
//   - DB (vault) format version: vault::VAULT_FORMAT_VERSION.
//   - device info: bsp::board_name() / bsp::board_revision().
//   - free memory: esp_get_free_heap_size().
//   - storage info: storage::fs::get_usage() (internal LittleFS,
//     always available) and storage::sd::get_usage() (microSD, only
//     if storage::status().sdcard_present).
// Refreshed every time the screen becomes active (on_show()), so free
// memory/storage usage aren't stale if you left and came back.
// =============================================================================

namespace ui::screens {

class SystemInfoScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    void refresh();

    lv_obj_t* content_parent_ = nullptr;
};

} // namespace ui::screens
