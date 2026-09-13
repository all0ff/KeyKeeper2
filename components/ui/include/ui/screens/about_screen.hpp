#pragma once

#include "ui/screen.hpp"

// =============================================================================
// ui::screens::AboutScreen
//
// A simple "what is this device" screen. NOT part of docs/GUI.md's
// 20-section spec -- there is no dedicated "About Screen" section
// there; "About" is a MainMenu item the project owner added on their
// own. Deliberately kept distinct from SystemInfoScreen (technical
// diagnostics: heap, storage usage, board revision, ...) -- this is
// just name/purpose/firmware version, read-only, no input handling
// beyond BACK.
// =============================================================================

namespace ui::screens {

class AboutScreen : public Screen
{
public:
    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    bool on_input(InputAction action) override;
};

} // namespace ui::screens
