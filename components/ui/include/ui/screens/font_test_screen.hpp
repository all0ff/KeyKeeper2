#pragma once

#include "ui/screen.hpp"

namespace ui::screens {

class FontTestScreen final : public Screen
{
public:
    const char* title() const override { return "Font Test"; }
    const char* footer_hint() const override { return "BACK  Return"; }

    void initialize(lv_obj_t* content_parent) override;
    bool on_input(InputAction action) override;

private:
    lv_obj_t* content_parent_ = nullptr;
};

} // namespace ui::screens
