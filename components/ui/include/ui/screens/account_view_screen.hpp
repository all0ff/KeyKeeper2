#pragma once

#include "ui/screen.hpp"

#include "vault/vault_model.hpp"

#include <cstdint>

// =============================================================================
// ui::screens::AccountViewScreen
//
// docs/GUI.md section 11 ("Account Screen"). Pushed from
// VaultListScreen with the selected entry's id.
//
// Displayed fields per GUI.md 11: Name, URL, Username, Password, OTP,
// Notes, Category, Favorite -- "empty fields are not shown". As with
// VaultListScreen, "Name" and "Username" both map to VaultEntry::login
// (no separate Name field exists). Category/Favorite (format v2) are
// now real: Category is shown only when non-empty, Favorite as a
// "Yes"/nothing-shown-if-false row plus a "Toggle Favorite" action.
// OTP shows a LIVE, auto-refreshing 6-digit TOTP code (RFC 6238, via
// totp::generate()) plus a seconds-remaining countdown, refreshed
// once a second by otp_refresh_timer_ -- not just a "configured"
// indicator anymore. Requires rtc_time::is_synced() (Wi-Fi Station
// connected and NTP-synced at least once this boot -- see
// rtc_time.hpp's own comment for why this board has no other time
// source); shows a clear "no time sync" message in place of the code
// otherwise, rather than a stale or wrong-looking one.
//
// Available Actions per GUI.md 11: Print URL/Username/Password/OTP,
// Edit, Delete. Print* actions run a real security::permission::check();
// Print OTP types the SAME live code this screen is currently
// showing (usb::print_field() calls totp::generate() itself, so it's
// always the code for the moment you press the action, not
// whatever was on screen when the screen first opened).
//
// Password is masked by default with a fixed-width placeholder (not
// matching the real length, to avoid leaking that) and a "Reveal
// Password" action to show it in the clear -- a deliberate,
// safety-leaning reading of GUI.md 11, which just says "Password" is
// a displayed field without specifying masking either way.
//
// Available Actions per GUI.md 11: Print URL/Username/Password/OTP,
// Edit, Delete. Edit pushes ui::screens::AccountEditScreen. Delete is
// REAL -- calls vault::delete_entry() -- gated by a two-step confirm
// (press the Delete action twice) since no confirmation dialog widget
// exists yet.
//
// There's no separate PermissionManager operation for "Print URL" or
// "Print Username" (REQUIREMENTS 9.3 only names Print Password/Print
// OTP among the print-related gated ops) -- both reuse
// Operation::PrintPassword's gate here as a placeholder, same
// decision already made in QuickScreen for its own Print URL action.
//
// Recovery Codes (format v3, vault::RecoveryCode -- a FIXED list of
// individually one-time-use codes, distinct from totp_secret's
// rotating ones) get two MORE conditional actions when the entry has
// any: "View Recovery Codes" switches this screen into a second,
// read-only scrolling MODE (mode_) listing every code with its
// used/unused status -- device-side support is deliberately NOMINAL
// per the project owner's own framing (view + print only); actually
// generating a set, or marking one used, is web-UI-only (see
// web_vault_routes.cpp's dedicated endpoints and web_app_html.hpp's
// own recovery-codes section) since typing/tapping on a phone or
// laptop is a much better fit for that than this device's single
// rotary knob. "Print Recovery Codes" types every UNUSED code over
// USB, one per line (same Operation::PrintPassword gate as the other
// Print* actions here) -- matching the web UI's own "Copy unused",
// not the full list including already-spent codes.
//
// Seed Phrase (format v4, vault::VaultEntry::seed_phrase -- a crypto
// wallet's BIP-39 mnemonic, MORE sensitive than anything else this
// screen shows, see that field's own comment) gets the SAME
// view/print pattern as Recovery Codes above, one more conditional
// mode (SeedPhraseView) -- but MASKED by default like Password is,
// unlike the recovery-codes list: OK toggles reveal for the whole
// list at once (seed_phrase_revealed_), same "hide by default, one
// press to see it" caution as update_password_label(). Setting or
// replacing the phrase is web-UI-only, same reasoning as recovery
// codes (typing 12-24 exact BIP-39 words via a single rotary knob
// is not a reasonable on-device task, and the real wordlist
// validation -- vault::bip39::validate_seed_phrase() -- runs
// server-side regardless of where the words came from). "Print Seed
// Phrase" types every word over USB space-separated on ONE line
// (matching how wallet software's own "paste your recovery phrase"
// fields expect it), same Operation::PrintPassword gate.
// =============================================================================

namespace ui::screens {

class AccountViewScreen : public Screen
{
public:
    explicit AccountViewScreen(uint32_t entry_id);
    ~AccountViewScreen() override;

    const char* title() const override;
    const char* footer_hint() const override;

    void initialize(lv_obj_t* content_parent) override;
    void on_show() override;
    void on_hide() override;
    bool on_input(InputAction action) override;

private:
    enum class Mode : uint8_t
    {
        Main,
        RecoveryCodesList,
        SeedPhraseView,
    };

    enum class Action : uint8_t
    {
        RevealPassword,
        ToggleFavorite,
        PrintUrl,
        PrintUsername,
        PrintPassword,
        PrintOtp,
        ViewRecoveryCodes,
        PrintRecoveryCodes,
        ViewSeedPhrase,
        PrintSeedPhrase,
        Edit,
        Delete,
    };

    void reload();
    lv_coord_t build_fields(lv_obj_t* parent);
    void build_actions(lv_obj_t* parent, lv_coord_t y_start);
    void render_actions();
    void update_password_label();
    void update_otp_label();
    static void otp_refresh_timer_cb(lv_timer_t* timer);
    void move_selection(int32_t delta);
    void activate();
    const char* action_name(Action action) const;

    void enter_recovery_codes_list();
    void build_recovery_codes_list();
    void render_recovery_codes_list();
    void move_recovery_code_selection(int32_t delta);

    void enter_seed_phrase_view();
    void build_seed_phrase_view();
    void render_seed_phrase_view();
    void move_seed_phrase_selection(int32_t delta);

    uint32_t entry_id_;
    vault::VaultEntry entry_{};
    bool loaded_ = false;

    Mode mode_ = Mode::Main;

    lv_obj_t* content_parent_ = nullptr;
    lv_obj_t* password_value_label_ = nullptr;
    bool password_revealed_ = false;

    lv_obj_t* otp_value_label_ = nullptr;
    lv_timer_t* otp_refresh_timer_ = nullptr;

    static constexpr size_t MAX_ACTIONS = 12;
    Action available_actions_[MAX_ACTIONS]{};
    size_t action_count_ = 0;
    lv_obj_t* action_labels_[MAX_ACTIONS]{};
    size_t selected_action_ = 0;

    bool delete_confirm_pending_ = false;

    // vault::MAX_RECOVERY_CODES caps how many an entry can ever have.
    lv_obj_t* recovery_code_labels_[vault::MAX_RECOVERY_CODES]{};
    size_t selected_recovery_code_ = 0;

    // vault::MAX_SEED_PHRASE_WORDS (24) caps how many an entry can
    // ever have.
    lv_obj_t* seed_word_labels_[vault::MAX_SEED_PHRASE_WORDS]{};
    size_t selected_seed_word_ = 0;
    bool seed_phrase_revealed_ = false;

    lv_obj_t* status_label_ = nullptr;
};

} // namespace ui::screens
