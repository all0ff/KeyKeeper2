# components/settings

## Where every field came from

All fields in `settings_types.hpp` are taken from
`docs/REQUIREMENTS.md` section 12 (12.1 General, 12.2 USB, 12.3
Security, 12.4 GUI), not invented. Two places that document only names
a field without pinning down its exact shape -- flagged as
placeholders, please review before relying on them:

- **`UsbSettings::typing_order`** -- REQUIREMENTS 12.2 just says
  "последовательность печати" (typing/print sequence). The 3-option
  `TypingOrder` enum (`LoginTabPasswordEnter`, `PasswordOnly`,
  `PasswordEnter`) is a reasonable placeholder shape, not a spec.
- **`SecuritySettings::web_ui_permissions`** -- REQUIREMENTS 12.3 just
  says "разрешения Web UI" (Web UI permissions). The 4-bit
  `WebUiPermission` bitmask (`VIEW_ACCOUNTS`/`EDIT_ACCOUNTS`/
  `EXPORT_DATA`/`CHANGE_SETTINGS`) is a placeholder guess at what a
  Web UI permission set might need, not a spec.

Everything else (language, theme, brightness, display-off timeout,
auto-lock, PIN length, animations/gestures/hints, per-field USB typing
delays) maps directly to a named requirement.

## Design decisions

- **The PIN value is never stored here** -- only
  `SecuritySettings::pin_length` (a preference: how many digits the
  PIN should be). The PIN itself is a secret and belongs to
  `components/security` (not built yet), not to settings/NVS.
- **Direct NVS access is off-limits from here on**, per
  `docs/ARCHITECTURE.md` ("Прямой доступ к NVS из других компонентов
  запрещён" / "Настройки изменяются только через SettingsService").
  From this component onward, anything that's a user-facing setting
  should go through `settings::`, not `storage::nvs::` directly.
  `storage::nvs` itself still exists for components with their own
  genuinely private key-value needs unrelated to user settings.
- **Each of the 4 sections is its own NVS blob**, so changing one
  doesn't rewrite the others. `init()` treats a missing key OR a
  size-mismatched blob (e.g. after a firmware update changes a
  struct's layout) as "use the default for this section only" --
  logged, not fatal.
- **`event_bus::SystemEventId::SettingsChanged`** was added to
  `components/event_bus/event.hpp` for this (small, additive change --
  see docs/SOFTWARE.md's EventBus example list, which names
  `SettingsChanged` explicitly). Every successful `set_*()` publishes
  it with the changed `Section` as payload.
