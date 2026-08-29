#pragma once

#include "ui/screen.hpp"
#include "ui/widgets/menu.hpp"

// =============================================================================
// ui::screens::MainMenuScreen
//
// docs/GUI.md section 9: Vault, Favorites, Categories, Search,
// Settings, Backup, System -- reached after a successful unlock.
//
// PLACEHOLDER: every single one of these 7 sections is a screen that
// doesn't exist yet (VaultList, Settings screens, Backup, Search, ...
// are all future ui/ slices). Activating any item currently shows a
// transient "coming soon" status message rather than navigating
// anywhere -- honest about scope rather than pretending a section
// works. Replace each case in main_menu_screen.cpp's on_input() with
// a real manager().push(...) as its screen gets built.
// =============================================================================

namespace ui::screens {

class MainMenuScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    bool on_input(InputAction action) override;

private:
    widgets::MenuList menu_;
    lv_obj_t* status_label_ = nullptr;
};

} // namespace ui::screens
