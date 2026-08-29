# components/ui

## What's in this slice

- `screens/usb_settings_screen.*` -- Password Shortcut (masked
  `widgets::TextEntry`), Print Delays (before typing / between chars /
  between fields, 50ms steps, 0-5000ms placeholder range), Print
  Sequence (`settings::TypingOrder`, cycled). Completes
  `settings::UsbSettings`.
- `screens/system_info_screen.*` -- read-only: firmware version
  (`esp_app_get_description()`, ESP-IDF's built-in `git describe`
  version -- there's no separate project version string anywhere in
  this codebase), vault DB format version
  (`vault::VAULT_FORMAT_VERSION`), device name/revision
  (`bsp::board_name()`/`board_revision()`), free heap, internal +
  microSD storage usage. Not a `settings::` consumer at all -- just a
  snapshot, refreshed on every `on_show()`.
- `screens/settings_screen.cpp`'s `USB`/`System` items now really
  push these instead of a placeholder message -- **all four Settings
  Screens sections from GUI.md 14 exist now.**

## Earlier slice (General/Security Settings)

- `screens/settings_screen.*` -- docs/GUI.md section 14's hub
  (General/USB/Security/System).
- `screens/general_settings_screen.*` -- Language, Theme (only
  `Theme::Dark` exists, so cycling it is a no-op kept for later),
  Brightness (live-previewed via `display::set_brightness()` as you
  adjust, reverted on `on_hide()` if not saved), Screen Timeout.
  **GUI.md lists "Auto Lock" under General, but the field
  (`auto_lock_enabled`) actually lives in
  `settings::SecuritySettings`** -- the toggle is on
  SecuritySettingsScreen instead, where the data is, not a bug.
- `screens/security_settings_screen.*` -- Change PIN (a real 3-step
  flow: current PIN if one exists -> new PIN -> confirm, via
  `widgets::PinEntry`; applies immediately via
  `security::pin::set_pin()`, not deferred to Save), Auto Lock
  enabled/timeout, and a single-toggle simplification of
  `web_ui_permissions` (Security Options in GUI.md is a 4-bit mask;
  only "allow viewing accounts" is exposed here, not a considered
  policy, just what fits one row -- see the header comment).
- `screens/main_menu.cpp`'s `Settings` item now really pushes
  `SettingsScreen` instead of a placeholder message.

## Earlier slice (Account Edit)


- `screens/account_edit_screen.*` -- docs/GUI.md section 12, the Edit
  Screen. Create (from `VaultListScreen`, hold OK) or edit (from
  `AccountViewScreen`'s Edit action) an entry, field by field, using
  the new `widgets::TextEntry`. **Name/Category/Favorite aren't
  editable -- `VaultEntry` has no such fields**; GUI.md's "Name must
  not be empty" validation is applied to `login` (shown as
  "Name/Username") instead, the same consolidation already used for
  display in VaultListScreen/AccountViewScreen. OTP secret gets a
  basic Base32 sanity check and is upper-cased on commit. Save calls
  `vault::validate()` then `vault::create_entry()`/`update_entry()`
  -- this screen never touches storage directly. **BACK from the
  field list discards any unsaved changes with no confirmation** (no
  dialog widget exists yet) -- a real, flagged (not solved) data-loss
  risk for an in-progress edit.
- `widgets/text_entry.*` -- `TextEntry`: general alphanumeric entry
  via the encoder (~90-character wheel: space/lower/upper/digits/
  symbols), reusable across fields via `set_mask()`/`set_max_length()`
  + `reset()`. Optional masking for password-type fields. **Known,
  flagged UX limitation**: no fast-path through the character set
  (no bank-jump shortcut) -- stepping to e.g. digits means rotating
  past every letter first.
- `screens/account_view_screen.cpp`'s `Edit` action now really pushes
  `AccountEditScreen` instead of a placeholder message.
- `screens/vault_list_screen.cpp`: holding OK now pushes
  `AccountEditScreen` to create a new entry -- the only entry point
  into it for a fresh entry, since there's no dedicated "+ Add" row.

## Earlier slice (Account View)

- `screens/account_view_screen.*` -- docs/GUI.md section 11, the
  Account Screen. Shows non-empty fields (URL/Username/Password-
  masked/OTP-indicator/Notes -- **Name/Category/Favorite skipped,
  `VaultEntry` has none of those fields**, same situation as
  VaultListScreen) plus an action list (Reveal Password, Print URL/
  Username/Password/OTP, Edit, Delete). **Delete is fully real** --
  calls `vault::delete_entry()`, gated by a press-twice confirm since
  no dialog widget exists yet. The Print* actions' actual effect is
  still a placeholder (no USB HID OutputChannel).
- `screens/vault_list_screen.cpp`'s entry selection now really pushes
  `AccountViewScreen` instead of a placeholder message.

## Earlier slice (Vault List)

- `screens/vault_list_screen.*` -- docs/GUI.md section 10, the Vault
  screen. Lists `vault::VaultEntry` records (login as the row's name,
  an `[OTP]` tag when `totp_secret` is non-empty). **GUI.md also
  mentions category and a favorite flag per row -- `VaultEntry` has
  neither field, so they're simply not shown; this isn't a bug, that
  data doesn't exist in the model.** Capped at 32 rows (placeholder
  limit, see the header comment).
- `screens/main_menu.cpp`'s `Vault` item now really pushes
  `VaultListScreen` instead of a placeholder message.

## Earlier slice (Main Menu)

- `screens/main_menu.*` -- docs/GUI.md section 9's Main Menu, written
  by the project owner (not the assistant) and wired in here after
  review: `Vault`/`Settings`/`Lock`/`About` (4 items -- a deliberate
  reduction from GUI.md's 7-item list: Favorites/Categories/Search/
  Backup aren't included). `Lock` and `BackLong` both call
  `security::lock::lock()` directly and work today; `Settings`/
  `About` show a "coming soon" status message, since neither screen
  exists yet.
- `screens/lock_screen.cpp` updated: successful unlock now
  `replace()`s with `MainMenu` (no LockScreen entry left on the
  stack) instead of the earlier `pop()`-back-to-QuickScreen
  placeholder.

## Earlier slice (PIN entry)

- `screens/lock_screen.*` -- docs/GUI.md section 8, PIN entry. Uses
  `widgets::PinEntry`.
- `widgets/keyboard.*` -- `PinEntry`: one-digit-at-a-time numeric entry
  via the single rotary encoder (rotate = spin 0-9, OK = confirm
  digit, BACK = step back one digit). NOT a general text-entry
  widget -- see its header comment for why that's a separate,
  bigger, not-yet-built piece.
- `screens/quick_screen.cpp` updated: `BackShort` while Locked now
  really pushes `LockScreen`, instead of the earlier "coming soon"
  placeholder message.

## Earlier slice (framework + QuickScreen)

The full `ui/` per the build plan is ~3000-6000 lines (10 screens + 5
widgets + framework) -- far too much for one pass. This delivers the
**framework only**, plus one real screen to prove it end-to-end:

- `theme.hpp/.cpp` -- ThemeService (docs/GUI.md 18), one Dark palette.
- `icon.hpp/.cpp` -- IconId placeholder system, see below.
- `screen.hpp` -- base `Screen` class, lifecycle matches
  docs/GUI.md section 4 exactly (Create/Initialize/Show/Active/Hide/
  Destroy).
- `ui_manager.hpp/.cpp` -- `UiManager` (the "ScreenManager"): owns the
  shared Header/Content/Footer chrome (docs/GUI.md 6) and a
  push/pop/replace screen stack. Hide only hides the LVGL tree
  (`LV_OBJ_FLAG_HIDDEN`); Destroy is what actually deletes it -- two
  distinct steps, matching the six-stage lifecycle literally rather
  than rebuilding on every navigation.
- `ui.hpp/.cpp` -- public entry point: builds the chrome, pushes
  `QuickScreen`, starts the input-dispatch task
  (`input::poll_event()` -> `InputAction` -> `UiManager::handle_input()`,
  under `lvgl_port::lock()`).
- `screens/quick_screen.*` -- docs/GUI.md section 7, the home/idle
  screen. See the file-level comment there for two placeholders and
  one open spec conflict (Print actions vs. PermissionManager while
  Locked) -- not silently resolved, flagged for you to decide.

## Not built yet (next slices)

- **Screens**: Backup/Restore, Search, About, Favorites, Categories --
  Main Menu's About item and AccountViewScreen's Print* actions'
  actual USB typing are still "coming soon" placeholders until these
  exist (Favorites/Categories/Search/Backup aren't in the current
  Main Menu at all -- see screens/main_menu.hpp). All four Settings
  Screens sections from GUI.md 14 are done.
- **Widgets**: menu (a reusable encoder-navigable list -- Main Menu/
  VaultListScreen/AccountViewScreen/AccountEditScreen each lay out
  their own rows directly rather than sharing a widget; worth
  factoring out once it gets repetitive enough to be worth it),
  dialog (AccountEditScreen's silent-discard-on-BACK and Delete's
  press-twice confirm would both be better as a real confirmation
  dialog), password_field, status_bar.

## Known placeholders / limitations

- **Icons**: docs/GUI.md 18 calls for a custom monochrome SVG set (31
  icons, no third-party library). No such asset set exists --
  producing one is a design task. `icon.cpp` maps each `IconId` to
  LVGL's own built-in symbol font as a stand-in; 9 IDs
  (Vault/Favorite/User/Url/Globe/Otp/Language/Theme/Flash) have no
  reasonable LVGL symbol and currently render blank.
- **Fonts**: no explicit font is set anywhere (chrome labels, screen
  content) -- everything uses whatever LVGL's default configured font
  is. Not a considered typography pass, just "didn't want to
  hardcode a specific `lv_font_montserrat_NN` symbol that might not
  be enabled in your `menuconfig` and fail to link."
- **Layout constants**: `HEADER_HEIGHT = 22`, `FOOTER_HEIGHT = 20` in
  `ui_manager.cpp` are eyeballed against the 320x172 logical display
  size, not derived from a real design pass.
- **QuickScreen's Print actions**: see the open spec conflict noted in
  `screens/quick_screen.hpp` -- GUI.md's mockup suggests these should
  work from the Locked state as a convenience; as built, they go
  through the same `PermissionManager` gate as everything else and
  are denied while Locked. Kept the safer behavior rather than
  special-casing this screen.
