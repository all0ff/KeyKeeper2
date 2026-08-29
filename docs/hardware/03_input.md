# Input

Detailed description of the physical input subsystem used by the **KeyKeeper2** project.

The KeyKeeper2 user interface uses an external **EC11 rotary encoder** and a dedicated **BACK** button connected to the expansion header of the Waveshare ESP32-S3-LCD-1.47B board.

---

# Purpose

This document defines the hardware and software requirements for the KeyKeeper2 input subsystem.

It describes:

* EC11 rotary encoder;
* encoder push button;
* BACK button;
* GPIO assignment;
* electrical input configuration;
* active-low logic;
* encoder event processing;
* button event processing;
* interaction with the GUI;
* input component responsibilities.

The input subsystem provides hardware-independent input events to the rest of the firmware.

---

# Input Devices

The KeyKeeper2 standard input configuration consists of:

| Device              | Function                        |
| ------------------- | ------------------------------- |
| EC11 rotary encoder | Navigation and value adjustment |
| EC11 push button    | OK / selection                  |
| BACK button         | Return / cancel / back          |

The board does not provide a touchscreen, therefore the physical controls are the primary method of user interaction. :contentReference[oaicite:0]{index=0}

---

# GPIO Assignment

The KeyKeeper2 input GPIO allocation is:

| Function            | GPIO  | Configuration   |
| ------------------- | ----- | --------------- |
| Encoder A           | GPIO4 | Input + Pull-up |
| Encoder B           | GPIO5 | Input + Pull-up |
| Encoder Button / OK | GPIO6 | Input + Pull-up |
| BACK Button         | GPIO7 | Input + Pull-up |

This allocation is part of the KeyKeeper2 hardware baseline. :contentReference[oaicite:1]{index=1}

---

# EC11 Rotary Encoder

The EC11 encoder provides two quadrature output signals:

* Phase A;
* Phase B.

The two signals are connected to:

| Encoder Signal | GPIO  |
| -------------- | ----- |
| Phase A        | GPIO4 |
| Phase B        | GPIO5 |

The encoder is used for menu navigation and adjustment of selectable values.

---

# Encoder Push Button

The EC11 encoder includes an integrated push button.

The push button is connected to:

| Function            | GPIO  |
| ------------------- | ----- |
| OK / Encoder Button | GPIO6 |

The button is used as the primary **OK / Select / Confirm** input.

The input is active-low.

---

# BACK Button

A dedicated external BACK button is connected to:

| Function | GPIO  |
| -------- | ----- |
| BACK     | GPIO7 |

The BACK button provides a dedicated navigation command independent of the encoder push button.

Typical uses include:

* returning to the previous screen;
* cancelling an operation;
* leaving a menu;
* returning from a dialog;
* exiting an input mode.

The exact action depends on the currently active GUI context.

---

# Electrical Configuration

The input buttons are connected between the corresponding GPIO and GND.

The firmware uses internal pull-up resistors.

The resulting logic is:

```text
Button released
    |
    +---- GPIO = HIGH

Button pressed
    |
    +---- GPIO = LOW
```

All KeyKeeper2 physical buttons therefore use **active-low** logic. 

---

# Encoder Wiring

The intended external connection is:

```
ESP32-S3-LCD-1.47B          EC11


3V3  ----------------------> VCC
GND  ----------------------> GND
GPIO4 ---------------------> Phase A
GPIO5 ---------------------> Phase B
GPIO6 ---------------------> Push Button
```

The dedicated BACK button is connected separately:

```
ESP32-S3-LCD-1.47B          BACK Button


GPIO7 ---------------------> Button
GND  ----------------------> Button
```

The input signals use the GPIO internal pull-ups.

------

# Encoder Direction

The encoder provides quadrature transitions on GPIO4 and GPIO5.

The input driver interprets the phase relationship between the two signals to determine the direction of rotation.

The resulting logical events are:

```
ENCODER_CW
ENCODER_CCW
```

The input component is responsible for converting physical encoder transitions into these logical events.

The GUI must not process GPIO4 and GPIO5 directly.

------

# Encoder Event Processing

The logical processing chain is:

```
EC11
 |
 +---- Phase A ---- GPIO4
 |
 +---- Phase B ---- GPIO5
 |
 v
Input Driver
 |
 v
Encoder Decoder
 |
 v
Logical Encoder Event
 |
 +---- ENCODER_CW
 |
 +---- ENCODER_CCW
 |
 v
EventBus
 |
 v
Application / GUI
```

The encoder decoder belongs to the input component.

------

# Button Event Processing

Button processing follows the same hardware-abstraction principle.

```
Physical Button
      |
      v
GPIO Input
      |
      v
Input Driver
      |
      v
Debounce / State Processing
      |
      v
Logical Button Event
      |
      v
EventBus
      |
      v
Application / GUI
```

The GUI must receive logical events rather than raw GPIO states.

------

# Logical Input Events

The input subsystem should expose logical events rather than hardware-specific signals.

The basic event set is:

| Event         | Source | Meaning                    |
| ------------- | ------ | -------------------------- |
| `ENCODER_CW`  | EC11   | Rotation clockwise         |
| `ENCODER_CCW` | EC11   | Rotation counter-clockwise |
| `OK_PRESS`    | GPIO6  | Encoder button pressed     |
| `BACK_PRESS`  | GPIO7  | BACK button pressed        |

Additional events may be introduced when required by the application, but they must remain independent of the physical GPIO implementation.

------

# Event Flow

The normal input event flow is:

```
GPIO4 / GPIO5
      |
      v
Encoder Driver
      |
      v
ENCODER_CW / ENCODER_CCW
      |
      v
EventBus
      |
      v
GUI / Application
```

For the buttons:

```
GPIO6
  |
  v
OK Button
  |
  v
OK_PRESS
  |
  v
EventBus
  |
  v
GUI / Application
GPIO7
  |
  v
BACK Button
  |
  v
BACK_PRESS
  |
  v
EventBus
  |
  v
GUI / Application
```

------

# Input Abstraction

The input component separates physical hardware from application logic.

The architecture is:

```
+----------------------+
| Application / GUI    |
+----------+-----------+
           |
           v
+----------------------+
| Logical Input Events |
+----------+-----------+
           |
           v
+----------------------+
| Input Component      |
+----------+-----------+
           |
           v
+----------------------+
| GPIO / Encoder       |
+----------------------+
```

This allows the application to remain independent of the exact GPIO implementation.

------

# GUI Integration

The GUI consumes logical input events.

The GUI must not:

- read GPIO4 directly;
- read GPIO5 directly;
- read GPIO6 directly;
- read GPIO7 directly;
- implement encoder quadrature decoding;
- implement button debounce;
- depend on active-low GPIO logic.

Instead, the GUI receives events from the input subsystem.

------

# Navigation Model

The EC11 encoder is the primary navigation control.

Typical navigation mapping:

| Input                      | Typical Action                       |
| -------------------------- | ------------------------------------ |
| Clockwise rotation         | Move selection down / increase value |
| Counter-clockwise rotation | Move selection up / decrease value   |
| OK press                   | Select / confirm                     |
| BACK press                 | Back / cancel                        |

The actual action is determined by the active screen or application state.

------

# Value Adjustment

The encoder may also be used to modify numeric or selectable values.

For example:

```
Clockwise
    |
    v
Increase value


Counter-clockwise
    |
    v
Decrease value
```

The input component generates the rotation event only.

Value limits, increments and application-specific behaviour belong to the corresponding application or GUI component.

------

# Button Debouncing

Mechanical buttons and rotary encoders may generate multiple electrical transitions during a single physical action.

The input subsystem therefore owns input-state stabilization and debouncing.

Debouncing must be implemented below the GUI layer.

The exact debounce implementation is an implementation detail of the input component and must not be duplicated by individual screens.

------

# Input Timing

The input subsystem is responsible for converting physical transitions into stable logical events.

Application code must not depend on GPIO polling timing.

The input component may use:

- GPIO interrupts;
- periodic polling;
- hardware timers;
- software state machines;

provided that the resulting logical event behaviour remains consistent.

The selected implementation must not block the main application task.

------

# Long Press and Repeated Input

If long-press or key-repeat behaviour is required, it must be implemented by the input subsystem or a common input-event layer.

Individual GUI screens must not implement independent GPIO timers for the same physical buttons.

The logical interface may later provide events such as:

```
OK_PRESS
OK_LONG_PRESS
BACK_PRESS
BACK_LONG_PRESS
ENCODER_CW
ENCODER_CCW
```

Only events actually required by the application should be implemented.

------

# Input Component Responsibilities

The `input` component is responsible for:

- GPIO initialization;
- encoder initialization;
- button initialization;
- pull-up configuration;
- encoder quadrature decoding;
- button state processing;
- debounce;
- logical event generation;
- publishing input events.

The component must hide all hardware-specific details from the application layer.

------

# Input Component Interface

The application-facing interface should provide operations at the logical level.

Typical operations include:

```
init()
start()
stop()
```

Event delivery is performed through the project's common event mechanism.

The exact class and function names are defined by the implementation of `components/input/`.

------

# GPIO Ownership

The input component owns the following GPIOs:

| GPIO  | Owner | Function            |
| ----- | ----- | ------------------- |
| GPIO4 | Input | Encoder A           |
| GPIO5 | Input | Encoder B           |
| GPIO6 | Input | Encoder Button / OK |
| GPIO7 | Input | BACK                |

No other component may configure these GPIOs.

GPIO numbers must be obtained from the BSP rather than duplicated inside the input implementation.

------

# Hardware Restrictions

The following GPIOs are reserved for the KeyKeeper2 input subsystem:

```
GPIO4
GPIO5
GPIO6
GPIO7
```

They must not be reassigned to other peripherals without changing the hardware specification.

The following rules apply:

- inputs are active-low;
- buttons use internal pull-ups;
- GPIO4 and GPIO5 are dedicated to the EC11 encoder;
- GPIO6 is dedicated to the encoder push button;
- GPIO7 is dedicated to BACK;
- GPIO definitions belong to the BSP;
- input processing belongs to the input component;
- GUI code must use logical events.

------

# Input and Display Relationship

The input subsystem and display subsystem are separate components.

Their relationship is:

```
+------------------+          +------------------+
| Input Component  |          | Display Component|
|                  |          |                  |
| EC11             |          | ST7789           |
| OK               |          | LovyanGFX         |
| BACK             |          | LVGL 9            |
+--------+---------+          +--------+---------+
         |                             |
         v                             v
      EventBus                     GUI / LVGL
```

Input events control navigation of the GUI, but the input component does not depend on the physical LCD implementation.

------

# Input and Application Relationship

The application layer consumes logical input events.

For example:

```
ENCODER_CW
    |
    v
Current screen
    |
    +---- move selection
    |
    +---- increase value
    |
    +---- navigate menu
```

Similarly:

```
OK_PRESS
    |
    v
Current screen
    |
    +---- confirm
    |
    +---- open selected item
```

And:

```
BACK_PRESS
    |
    v
Current screen
    |
    +---- return
    |
    +---- cancel
    |
    +---- close dialog
```

The input component does not determine the meaning of an event in a particular screen.

------

# Future Extensions

The input architecture should allow additional input devices to be added without changing the application event model.

Possible future devices include:

- additional buttons;
- additional encoders;
- keypad;
- external control panel;
- other GPIO-based input devices.

New hardware should be integrated through the input component and exposed as logical events.

------

# Design Rules

The following rules apply to the KeyKeeper2 input subsystem:

1. GPIO4 must remain assigned to encoder Phase A.
2. GPIO5 must remain assigned to encoder Phase B.
3. GPIO6 must remain assigned to the encoder push button / OK.
4. GPIO7 must remain assigned to BACK.
5. All four inputs use active-low logic.
6. Buttons use internal pull-up resistors.
7. GPIO numbers must be defined in the BSP.
8. GPIO access must remain inside the input component.
9. Encoder quadrature decoding must remain inside the input component.
10. Button debounce must remain inside the input component.
11. GUI screens must consume logical input events.
12. GUI screens must not access the input GPIOs directly.
13. Input processing must not block the main application task.
14. Changes to the physical input hardware must be reflected in `01_board.md`, `03_input.md` and `06_interfaces.md`.

------

# Related Documents

| Document           | Description                     |
| ------------------ | ------------------------------- |
| `01_board.md`      | Board and hardware platform     |
| `02_display.md`    | LCD and display subsystem       |
| `04_power.md`      | Power subsystem                 |
| `05_storage.md`    | Flash, PSRAM and microSD        |
| `06_interfaces.md` | GPIO and hardware interfaces    |
| `07_boot.md`       | Boot process and strapping pins |

------

# Summary

The KeyKeeper2 input subsystem uses an external **EC11 rotary encoder**, its integrated push button and a dedicated **BACK** button.

The definitive GPIO allocation is:

| Function                 | GPIO  |
| ------------------------ | ----- |
| Encoder Phase A          | GPIO4 |
| Encoder Phase B          | GPIO5 |
| Encoder Push Button / OK | GPIO6 |
| BACK Button              | GPIO7 |

The input signals are **active-low** and the buttons use internal pull-up resistors. 

The input component owns the complete hardware interface for these controls, including:

- GPIO configuration;
- encoder quadrature decoding;
- button state processing;
- debounce;
- logical input event generation.

The application and GUI layers receive logical events such as:

```
ENCODER_CW
ENCODER_CCW
OK_PRESS
BACK_PRESS
```

and must not access GPIO4–GPIO7 directly.

The EC11 encoder is the primary navigation control of the KeyKeeper2 user interface. The encoder push button provides **OK / Select / Confirm**, while the dedicated BACK button provides **Back / Cancel** functionality.

The input subsystem is independent of the display hardware and communicates with the application through the common event architecture.

The GPIO assignment described in this document is the KeyKeeper2 hardware baseline and must remain synchronized with `01_board.md`, `06_interfaces.md` and the BSP pin definitions.
