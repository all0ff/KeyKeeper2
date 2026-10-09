# components/usb

## Role

USB-HID keyboard emulation -- lets the device "type" a vault entry's
login, password, URL, or TOTP code directly into whatever computer
it's plugged into, as if from a physical keyboard. No software on the
host side, no driver to install; this is the whole point of the
device as a hardware password manager (nothing the host OS could
keylog or clipboard-snoop, since nothing is ever copied through the
host's own clipboard).

## Radio dongle path (stage 4a; not yet used by the vault firmware)

The same `TypingPlan` can be played by a radio dongle instead of the USB cable:

- **`plan_wire`** -- `plan_to_wire()` turns a plan into `kk::msg::Event`s (waits above 255 ms are split,
  holds are clamped, characters without a key are only counted); `wire_to_plan()` is the way back.
- **`wire_executor`** -- the dongle's side: runs a `TypeKeys` batch with the rules of `run_plan()`
  (stop at the first failure, still close an open Cyrillic run), answers with a `Result`.
- **`dongle_sink`** -- `DongleSink : OutputSink`: batches (<= 32 events, <= 2.5 s), waits for each
  `Result`, maps it back to a `RunResult`, closes a Cyrillic run left open by a failure.

All three are pure C++ (they need only `kkproto/messages.hpp` / `messages.cpp`) and are covered by
`test_host/test_wire.cpp`, which compares them with `run_plan()` on random plans with injected failures
(`MUTATION=1 ./run_host_tests.sh` also runs `mutation_check_wire.py`). They are not in this
component's `CMakeLists.txt` yet: they need `CONFIG_KEYKEEPER_KKPROTO`, and the vault firmware will
get them together with the link service. The dongle project (`dongle/`) compiles them directly.

## Structure, bottom to top

- **`hid_keyboard`** -- the actual TinyUSB HID keyboard device: USB
  descriptors, `tud_hid_*` callbacks, and `send_key()`/`release_all()`.
  Every report goes through one `send_report()` choke point that
  retries on a busy endpoint (`tud_hid_ready()`) and then genuinely
  **waits for `tud_hid_report_complete_cb()`** before considering a
  report sent -- a real, confirmed-on-hardware bug (SD-card HID
  tracing: every report returned "accepted" with zero retries, yet
  the host only visibly registered a fraction of them) turned out to
  be exactly what TinyUSB's own docs say to watch for:
  `tud_hid_keyboard_report()` returning `true` only means the report
  was copied into TinyUSB's own buffer, not that it reached the host.
  `is_connected()` only checks `tud_mounted()` (genuine connection),
  not also `tud_hid_ready()` (the endpoint's instantaneous busy/free
  state) -- requiring both used to make a continuously-connected
  device briefly read as "not connected" whenever the endpoint
  happened to be mid-report, confirmed as a real source of spurious
  status, not a theoretical one.
- **`keycode_map`** -- ASCII/control-character -> USB HID usage ID +
  modifier, US QWERTY layout (matches what `widgets::TextEntry` on the
  UI side can actually produce for that range). `'\n'` maps to
  `keycode::ENTER`, `'\t'` to `keycode::TAB` -- both ordinary table
  entries, not special-cased elsewhere; anything downstream that wants
  a literal Enter or Tab typed just needs that character in the
  string.
- **`cyrillic_layout`** -- best-effort Cyrillic typing without the
  host needing a Cyrillic-aware driver of its own: sends the host's
  own layout-switch hotkey (Alt+Shift, Windows' default) before a run
  of Cyrillic characters and again after, typing each one via its
  physical ЙЦУКЕН-key equivalent in between. Depends on the receiving
  computer actually having a Russian layout installed and that hotkey
  actually being its switch shortcut -- see this header's own file
  comment for exactly what breaks (wrong characters, not just missing
  ones) when that doesn't hold, and `settings::UsbSettings::cyrillic_auto_switch_layout`
  (off by default) for the on-device toggle.
- **`TypeEngine`** -- turns a UTF-8 string into keystrokes, routing each
  character through `keycode_map` or `cyrillic_layout` as appropriate.
  It is a thin facade over a **plan / execute split** (see the next
  bullet), with the same public API as before.
  `type_string()`'s return value is a **character** count, not a byte
  count -- `usb_service.cpp`'s own `count_chars()` helper exists
  specifically because comparing that against `std::string::size()`
  (bytes) directly is wrong for any Cyrillic content (2 bytes/char in
  UTF-8), and silently was, for a while. Unsupported characters are
  skipped but still **counted** as sent -- that is what makes
  `usb_service.cpp` report "Typed OK" for them, and the split keeps it.
- **Punctuation on the Russian layout** -- with the automatic layout
  switch OFF the person puts the host on the Russian layout by hand, so
  for a text that contains Cyrillic the planner types `. , ? " ; : /` on the
  keys the standard Windows Russian layout puts them on (the US "." key
  is "ю" there; the Russian "." is the key a US keyboard labels "/").
  Digits and `! % * ( ) - _ = +` already share keys. Latin letters and
  `@ # $ ^ & [ ] { } < > | ~ '` plus the backtick have no key on that layout:
  they are typed as before, `TypingPlan::unavailable_on_russian_layout` counts
  them and a warning is logged -- only the automatic switch can type them.
  With the automatic switch on, or for text without Cyrillic, nothing changes.
  `PlanOptions::russian_layout_punctuation = false` restores the old behaviour.
  Not verified from a published source: the slash (typed as Shift plus the
  backslash key) and the backslash on its own key. The macOS Russian layout
  differs; this targets Windows.
- **Plan / execute split** -- `plan_events()` (`typing_plan.hpp`) is a
  pure function from text to a list of `HidEvent` steps (key presses,
  pacing pauses, the optional Alt+Shift layout hotkey); it has no USB,
  FreeRTOS, settings or logging, so it is unit-tested on a PC.
  `run_plan()` (`typing_runner.hpp`) plays a plan through a small `KeyIo`
  interface with the old rules: stop at the first failure, but still send
  the layout-switch-back of a Cyrillic run that was already opened.
  An `OutputSink` (`output_sink.hpp`) decides where a plan goes -- today
  only `UsbCableSink` (the cable); a radio-dongle sink is the next one
  and needs nothing above this interface to change.
  Tests: `test_host/run_host_tests.sh` (g++ only, no ESP-IDF). It runs
  unit tests and a differential test that compares the new code against
  a verbatim copy of the original `type_engine.cpp` (`test_host/oracle/`,
  delete it once the refactor is settled on hardware) over tens of
  thousands of random inputs, including USB failures mid-typing.
- **`usb::Service`** (`usb_service.hpp`/`.cpp`) -- the public facade
  UI screens actually call: `print_field()` for a vault entry's Login/
  Password/URL/OTP, `type_string()` for a raw string (QuickScreen's
  own shortcut password, printing one specific recovery code). Both
  are non-blocking -- they spawn a FreeRTOS task so the UI stays
  responsive while typing proceeds -- and both go through a single
  `typing_in_progress` atomic flag that refuses to start a second
  typing task while one is already running, rather than letting two
  race on the same shared `TypeEngine` instance and the same USB
  endpoint (confirmed on real hardware: repeated fast presses used to
  do exactly that).

  `print_field(entry, Field::Password)` is where
  `settings::UsbSettings::typing_order` actually takes effect
  (`Settings -> USB -> Print Sequence` on-device) --
  Login+Tab+Password+Enter / Password Only / Password+Enter. This is
  specifically the `Password` case; `Field::Login` on its own always
  just types the login, no Tab or Enter added regardless of this
  setting. `Field::LoginAndPassword` exists in the enum but nothing
  currently calls it with that value -- `typing_order`'s
  `LoginTabPasswordEnter` option covers the same ground by building
  the combined string directly in the `Password` case instead.

## Known gap

`settings::UsbSettings::delay_before_typing_ms` /
`delay_between_chars_ms` / `delay_between_fields_ms` exist as settings
(saved, shown on the on-device USB settings screen) but aren't read
anywhere in `components/usb/src/*.cpp` -- `TypeEngine::Timing`'s
actual values always come from its own hardcoded defaults
(10ms press, 10ms inter-key, 0ms chunk pause), never from these
settings. `TypeEngine`'s own header comment currently claims "All
timing parameters are configurable via settings" -- that's aspirational,
not yet true. Wiring these through is a real, standalone piece of
follow-up work, not a one-line fix (`Timing` would need to be built
from `settings::all().usb` at each call site instead of using its
struct defaults).
