#pragma once

#include "ui/async_pin_check.hpp"
#include "ui/screen.hpp"
#include "ui/widgets/keyboard.hpp"

#include "security/pin_manager.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::WirelessScreen -- Settings -> USB -> Wireless typing
//
// Typing through the radio dongle (components/wireless):
//   Wireless typing  On/Off      -- default Off. While On, every Print action goes to the dongle.
//   Pair dongle...               -- asks for the PIN again, then pairs with a dongle whose pairing window is
//                                   open (hold BOOT on the dongle first) and shows a 6-digit code that has to
//                                   be confirmed on BOTH devices.
//   Forget dongle                -- asks to confirm.
// A status line shows the link: Off / Not paired / Looking for the dongle / Connected (PC ready or not).
//
// Texts are English literals for now (not in the i18n tables yet).
// =============================================================================

namespace ui::screens {

class WirelessScreen : public Screen
{
public:
    ~WirelessScreen() override;

    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    void on_hide() override;
    bool on_input(InputAction action) override;

private:
    enum class Mode : uint8_t
    {
        Browse,   ///< the three rows and the status line
        PinEntry, ///< re-entering the PIN before pairing
        Pairing,  ///< looking for the dongle / comparing the code / result
    };

    enum class Row : uint8_t
    {
        Switch,
        Pair,
        Forget,
    };
    static constexpr size_t ROW_COUNT = 3;

    void build_browse();
    void build_pin_entry(const char* error);
    void build_pairing();
    void render_rows();
    void refresh_status();      ///< status line (Browse) / pairing view (Pairing), called by the timer
    void activate();
    void begin_pin_entry();
    void on_pin_complete();
    static void on_pin_checked(security::pin::VerifyResult result, void* ctx);
    void handle_pin_result(security::pin::VerifyResult result);
    void leave_pairing();
    void start_timer();
    void stop_timer();
    static void timer_cb(lv_timer_t* t);

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* row_labels_[ROW_COUNT]{};
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* code_label_ = nullptr;
    lv_obj_t* info_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    Mode mode_ = Mode::Browse;
    size_t selected_row_ = 0;
    bool confirm_forget_ = false;
    bool checking_ = false;
    uint32_t result_ticks_ = 0; ///< counts timer ticks since a pairing result appeared
    uint32_t idle_ticks_ = 0;   ///< ticks in Pairing mode without the link task reporting a pairing state

    widgets::PinEntry pin_entry_;
    AsyncPinCheck async_check_;
};

} // namespace ui::screens
