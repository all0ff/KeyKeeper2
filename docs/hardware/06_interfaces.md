# Interfaces

Detailed description of the hardware interfaces and GPIO allocation used by the
**KeyKeeper2** project.

This document defines the interface ownership of the
**Waveshare ESP32-S3-LCD-1.47B** board and the external KeyKeeper2 peripherals.

The GPIO allocation defined here is the project hardware baseline.

---

# Purpose

This document describes:

* GPIO allocation;
* onboard peripheral interfaces;
* LCD interface;
* microSD interface;
* user input interface;
* USB interface;
* RGB status LED;
* expansion GPIO;
* interface ownership;
* hardware restrictions;
* BSP interface requirements.

The purpose of this document is to provide one authoritative reference for
GPIO and hardware-interface allocation.

Individual subsystem behaviour is described in the corresponding subsystem
documents.

---

# Interface Architecture

The KeyKeeper2 interface architecture is divided into:

```text
Onboard Hardware
        |
        +---- LCD
        |
        +---- microSD
        |
        +---- RGB LED
        |
        +---- USB
        |
        +---- Flash
        |
        +---- PSRAM
        |
        v
     ESP32-S3
        |
        +---- Project GPIO
        |
        v
External Peripherals
        |
        +---- EC11 Encoder
        +---- OK Button
        +---- BACK Button
```

KeyKeeper2 does not modify the original Waveshare PCB.

Project-specific hardware is connected through the expansion header. 

------

# GPIO Allocation Policy

GPIO resources are divided into three categories:

1. **Reserved GPIO**
2. **Project GPIO**
3. **Available GPIO**

Reserved GPIO belongs to onboard hardware and must not be reassigned.

Project GPIO is assigned to KeyKeeper2 external peripherals.

Available GPIO may be used for future expansion after the allocation is updated in
 this document.

------

# Complete GPIO Allocation

| GPIO      | Function                 | Owner         | Status            |
| --------- | ------------------------ | ------------- | ----------------- |
| GPIO0     | BOOT                     | Board         | Reserved          |
| GPIO1     | —                        | —             | Not assigned      |
| GPIO2     | —                        | —             | Not assigned      |
| GPIO3     | —                        | —             | Not assigned      |
| GPIO4     | Encoder Phase A          | Input         | Project           |
| GPIO5     | Encoder Phase B          | Input         | Project           |
| GPIO6     | Encoder Push Button / OK | Input         | Project           |
| GPIO7     | BACK Button              | Input         | Project           |
| GPIO8     | —                        | —             | Available         |
| GPIO9     | —                        | —             | Available         |
| GPIO10    | —                        | —             | Available         |
| GPIO11    | —                        | —             | Available         |
| GPIO12    | —                        | —             | Available         |
| GPIO13    | —                        | —             | Available         |
| GPIO14    | microSD                  | Storage       | Reserved          |
| GPIO15    | microSD                  | Storage       | Reserved          |
| GPIO16    | microSD                  | Storage       | Reserved          |
| GPIO17    | microSD                  | Storage       | Reserved          |
| GPIO18    | microSD                  | Storage       | Reserved          |
| GPIO19    | —                        | —             | Not assigned      |
| GPIO20    | —                        | —             | Not assigned      |
| GPIO21    | microSD                  | Storage       | Reserved          |
| GPIO22–25 | —                        | —             | Not assigned      |
| GPIO26–37 | PSRAM                    | Board         | Reserved          |
| GPIO38    | RGB LED                  | System        | Reserved          |
| GPIO39    | LCD Reset                | Display       | Reserved          |
| GPIO40    | LCD SCLK                 | Display       | Reserved          |
| GPIO41    | LCD DC                   | Display       | Reserved          |
| GPIO42    | LCD CS                   | Display       | Reserved          |
| GPIO43    | UART TX                  | Board / Debug | Reserved          |
| GPIO44    | UART RX                  | Board / Debug | Reserved          |
| GPIO45    | LCD MOSI                 | Display       | Reserved          |
| GPIO46    | —                        | —             | Not LCD backlight |
| GPIO47    | —                        | —             | Not assigned      |
| GPIO48    | LCD Backlight            | Display       | Reserved          |

The project GPIO assignments for GPIO4–7 and the reserved onboard GPIO groups are
 part of the current KeyKeeper2 hardware specification. 

------

# LCD Interface

The onboard LCD uses the following GPIO allocation:

| LCD Pin | ESP32-S3 GPIO | Function       |
| ------- | ------------- | -------------- |
| MOSI    | GPIO45        | SPI data       |
| SCLK    | GPIO40        | SPI clock      |
| LCD_CS  | GPIO42        | Chip select    |
| LCD_DC  | GPIO41        | Data / command |
| LCD_RST | GPIO39        | Reset          |
| LCD_BL  | GPIO48        | Backlight      |

These GPIOs are permanently reserved for the LCD.

**GPIO46 is not used for LCD backlight.**

The Waveshare schematic identifies the LCD signals as LCD reset, data, clock,
 chip select, DC and backlight. 

The verified KeyKeeper2 display configuration uses:

```
SPI3_HOST
3-wire SPI
DMA
27 MHz write
16 MHz read
```

Display-specific configuration is defined in `02_display.md`.

------

# LCD GPIO Ownership

The following resources belong exclusively to the display subsystem:

```
GPIO39
GPIO40
GPIO41
GPIO42
GPIO45
GPIO48
SPI3_HOST
```

No other firmware component may configure these GPIOs or the LCD SPI host.

The display component owns the low-level LCD interface.

------

# microSD Interface

The board provides an integrated microSD card interface.

The KeyKeeper2 configuration uses the microSD interface in **4-bit mode**.

The relevant reserved GPIO resources are:

```
GPIO14
GPIO15
GPIO16
GPIO17
GPIO18
GPIO21
```

These GPIOs are owned by the storage subsystem.

The board schematic identifies the microSD interface signals as:

```
SDIO_D0
SDIO_D1
SDIO_D2
SDIO_D3
SDIO_SCK
SDIO_CMD
```



The exact driver configuration belongs to `components/storage/`.

------

# microSD GPIO Ownership

The following GPIOs must not be assigned to other project peripherals:

```
GPIO14
GPIO15
GPIO16
GPIO17
GPIO18
GPIO21
```

The storage subsystem owns the microSD interface.

The GUI and application layers must not access these GPIOs directly.

------

# EC11 Encoder Interface

The external EC11 rotary encoder uses:

| Encoder Signal | GPIO  | Configuration   |
| -------------- | ----- | --------------- |
| Phase A        | GPIO4 | Input + Pull-up |
| Phase B        | GPIO5 | Input + Pull-up |
| Push Button    | GPIO6 | Input + Pull-up |

The encoder is connected through the expansion header.

The encoder signals are interpreted by the input subsystem.

------

# BACK Button Interface

The dedicated BACK button uses:

| Function | GPIO  | Configuration   |
| -------- | ----- | --------------- |
| BACK     | GPIO7 | Input + Pull-up |

The button is connected between GPIO7 and GND.

The firmware treats the button as active-low.

```
Released = HIGH
Pressed  = LOW
```

The input subsystem owns GPIO7.

------

# Input GPIO Ownership

The following GPIOs belong exclusively to the input subsystem:

```
GPIO4
GPIO5
GPIO6
GPIO7
```

The input component is responsible for:

- GPIO configuration;
- pull-up configuration;
- encoder decoding;
- button processing;
- debounce;
- logical event generation.

The GUI must not access these GPIOs directly.

Detailed input behaviour is described in `03_input.md`.

------

# USB Interface

The ESP32-S3 provides native USB functionality.

The board uses a USB Type-C connector.

USB is used by KeyKeeper2 for:

- power;
- firmware flashing;
- USB communication;
- USB functionality defined by the application.

The ESP32-S3 provides native USB OTG functionality. 

USB pins and internal USB resources are reserved by the hardware platform and
 must not be reassigned as general-purpose application GPIO.

USB-specific firmware behaviour is defined by the USB component.

------

# UART Interface

The board exposes UART signals associated with:

```
GPIO43
GPIO44
```

The current board documentation reserves these signals for UART.

| GPIO   | Function |
| ------ | -------- |
| GPIO43 | UART TX  |
| GPIO44 | UART RX  |

These pins should not be assigned to project peripherals without updating the
 hardware interface specification.

------

# RGB LED Interface

The board contains a programmable RGB status LED.

The LED is assigned to the system status subsystem.

Its GPIO is:

```
GPIO38
```

The LED is not available for general-purpose application use.

Typical logical states are:

| State    | Meaning          |
| -------- | ---------------- |
| Blue     | Boot             |
| Green    | Normal operation |
| Yellow   | Warning          |
| Red      | Error            |
| Flashing | Activity         |

The LED is controlled through the system status service.

Application components must not manipulate GPIO38 directly. 

------

# Flash Interface

The board contains 16 MB onboard SPI Flash.

The Flash interface is connected internally to the ESP32-S3.

The Flash bus is reserved by the hardware platform.

Application components must not attempt to reconfigure the Flash interface.

Flash is used for:

- bootloader;
- partition table;
- application firmware;
- OTA images;
- NVS;
- system configuration;
- filesystem data where configured.



------

# PSRAM Interface

The board provides 8 MB external PSRAM.

The PSRAM interface is internal to the hardware platform.

GPIO26–GPIO37 are reserved for the internal memory interface and must not be
 used by application peripherals.

PSRAM is used for runtime memory such as:

- LVGL buffers;
- image data;
- temporary buffers;
- large runtime structures;
- filesystem cache.



------

# Expansion Header

The board provides an 18-pin expansion header.

The header provides access to:

- 3.3 V;
- GND;
- GPIO;
- UART;
- additional expansion signals.

The expansion header is the preferred connection point for KeyKeeper2 external
 hardware.

The standard project peripherals connected through the header are:

```
EC11 encoder
OK button
BACK button
```

Additional peripherals should use currently available GPIOs where possible.

------

# Available Expansion GPIO

The current KeyKeeper2 allocation leaves the following GPIOs available for
 future expansion:

| GPIO   | Status    |
| ------ | --------- |
| GPIO8  | Available |
| GPIO9  | Available |
| GPIO10 | Available |
| GPIO11 | Available |
| GPIO12 | Available |
| GPIO13 | Available |

These GPIOs are available only after confirming that the intended peripheral
 does not conflict with another board function.

Before assigning a new peripheral, this document must be updated.

The current project documentation explicitly identifies GPIO8–13 as available
 for future hardware extensions. 

------

# GPIOs Not Available for General Expansion

The following GPIO groups are not available for arbitrary project use:

| GPIO      | Reason           |
| --------- | ---------------- |
| GPIO0     | BOOT / strapping |
| GPIO14–18 | microSD          |
| GPIO21    | microSD          |
| GPIO26–37 | PSRAM            |
| GPIO38    | RGB LED          |
| GPIO39–42 | LCD              |
| GPIO43–44 | UART             |
| GPIO45    | LCD MOSI         |
| GPIO48    | LCD backlight    |

These resources are reserved by the hardware platform or KeyKeeper2 project.

------

# GPIO46

GPIO46 requires explicit documentation because it was previously incorrectly
 associated with the LCD backlight.

The correct configuration is:

```
GPIO48 = LCD_BL
GPIO46 = not used for LCD backlight
```

GPIO46 must therefore not appear as the LCD backlight pin in:

- BSP definitions;
- display driver configuration;
- hardware documentation;
- wiring documentation.

------

# Interface Ownership

Every hardware interface has one responsible subsystem.

| Interface     | Owner           |
| ------------- | --------------- |
| LCD           | Display         |
| LCD Backlight | Display         |
| LCD SPI3      | Display         |
| EC11 Encoder  | Input           |
| OK Button     | Input           |
| BACK Button   | Input           |
| microSD       | Storage         |
| NVS           | Settings        |
| LittleFS      | Storage         |
| Flash         | Board / ESP-IDF |
| PSRAM         | Board / ESP-IDF |
| RGB LED       | System          |
| USB           | USB             |
| UART          | Board / Debug   |

The owner is the only subsystem allowed to configure the corresponding hardware
 interface.

------

# Interface Abstraction

Hardware interfaces must be accessed through their responsible components.

The application architecture is:

```
Application
     |
     v
Services
     |
     v
Drivers
     |
     v
BSP / Hardware
```

For example:

```
GUI
 |
 v
Display Service
 |
 v
Display Driver
 |
 v
BSP
 |
 v
ST7789 / SPI3
```

and:

```
GUI
 |
 v
Input Events
 |
 v
Input Driver
 |
 v
BSP
 |
 v
GPIO4–GPIO7
```

------

# BSP Requirements

The BSP must provide centralized definitions for all board-specific hardware
 resources.

The BSP must contain:

- LCD GPIO definitions;
- input GPIO definitions;
- microSD GPIO definitions;
- RGB LED definition;
- UART definitions;
- board-specific peripheral configuration.

Application components must not duplicate GPIO numbers.

The BSP is the single source of truth for physical GPIO assignment.

------

# Interface Configuration Rules

The following rules apply to all hardware interfaces:

1. GPIO numbers must be defined in the BSP.
2. A GPIO may have only one owner.
3. Reserved board GPIOs must not be reassigned.
4. Application components must not contain duplicated GPIO numbers.
5. Hardware drivers must not depend on GUI code.
6. GUI components must not access hardware GPIO directly.
7. Changes to GPIO allocation require documentation updates.
8. Changes to onboard interface usage require BSP updates.
9. Shared buses must be explicitly documented before use.
10. Expansion peripherals must use available GPIOs or documented shared buses.

------

# Interface Conflict Prevention

Before adding a new peripheral, its required resources must be checked against
 this document.

The following resources are considered occupied:

```
LCD:
    GPIO39
    GPIO40
    GPIO41
    GPIO42
    GPIO45
    GPIO48
    SPI3_HOST


Input:
    GPIO4
    GPIO5
    GPIO6
    GPIO7


microSD:
    GPIO14
    GPIO15
    GPIO16
    GPIO17
    GPIO18
    GPIO21


RGB LED:
    GPIO38


UART:
    GPIO43
    GPIO44


PSRAM:
    GPIO26–GPIO37


BOOT:
    GPIO0
```

Only explicitly available resources may be assigned without revising the
 allocation.

------

# Interface and Event Architecture

Hardware events are converted into logical events before reaching the
 application.

For example:

```
EC11
 |
 v
GPIO4 / GPIO5
 |
 v
Input Driver
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

For the display:

```
GUI
 |
 v
LVGL
 |
 v
Display Driver
 |
 v
SPI3
 |
 v
LCD
```

For storage:

```
VaultService
 |
 v
StorageService
 |
 +----> LittleFS
 |
 +----> microSD
```

This separation prevents hardware-specific code from propagating into the
 application layer.

------

# Interface Restrictions

The following direct-access restrictions apply:

| Component       | Must Not Directly Access |
| --------------- | ------------------------ |
| GUI             | GPIO, SPI, microSD, NVS  |
| Application     | LCD GPIO, encoder GPIO   |
| Vault           | GPIO, LCD                |
| Storage Service | GUI objects              |
| Display Service | Application logic        |
| Input Service   | GUI objects              |

Hardware access must remain inside the appropriate driver/BSP layer.

------

# Hardware Extension Policy

New external hardware should preferably be connected through the expansion
 header.

Potential extensions include:

- additional buttons;
- sensors;
- I²C peripherals;
- SPI peripherals;
- LEDs;
- buzzers;
- RTC;
- NFC;
- secure element;
- external storage.

The project should reuse existing buses where appropriate rather than consuming
 additional GPIO resources. 

Before introducing an extension, the following must be checked:

1. GPIO availability;
2. boot/strapping implications;
3. bus ownership;
4. interrupt requirements;
5. power requirements;
6. conflicts with onboard peripherals;
7. BSP changes;
8. documentation changes.

------

# Interface Validation

The hardware interface configuration should be validated during firmware
 development.

At minimum, validation should confirm:

```
LCD:
    GPIO39 / 40 / 41 / 42 / 45 / 48
    SPI3_HOST
    ST7789


Input:
    GPIO4 / 5 / 6 / 7


microSD:
    GPIO14–18 / 21


RGB LED:
    GPIO38


UART:
    GPIO43 / 44
```

The validation configuration must correspond to the board hardware and the
 KeyKeeper2 BSP.

------

# Interface Documentation Synchronization

The following documents must remain synchronized:

| Document           | Responsibility                |
| ------------------ | ----------------------------- |
| `01_board.md`      | Hardware platform             |
| `02_display.md`    | LCD interface                 |
| `03_input.md`      | User input                    |
| `05_storage.md`    | Storage interfaces            |
| `06_interfaces.md` | Complete interface allocation |
| `07_boot.md`       | Boot and strapping resources  |

`06_interfaces.md` is the consolidated GPIO allocation reference.

Subsystem documents describe behaviour; this document defines the consolidated
 hardware ownership.

------

# Design Rules

The following rules define the KeyKeeper2 hardware interface baseline:

1. GPIO4–GPIO7 are reserved for external user input.
2. GPIO39–42, GPIO45 and GPIO48 are reserved for the LCD.
3. GPIO48 is the LCD backlight control.
4. GPIO46 is not the LCD backlight.
5. SPI3_HOST is reserved for the LCD.
6. GPIO14–18 and GPIO21 are reserved for microSD.
7. GPIO38 is reserved for the RGB status LED.
8. GPIO43–44 are reserved for UART.
9. GPIO26–37 are reserved for PSRAM.
10. GPIO0 must not be used as a normal expansion GPIO.
11. GPIO8–13 are the currently available expansion GPIOs.
12. GPIO ownership must be unique.
13. GPIO definitions must be centralized in the BSP.
14. Hardware drivers must hide GPIO implementation details from services.
15. Services must hide hardware details from the application.
16. GUI code must use logical events and services.
17. Any hardware change must update this document and the corresponding
     subsystem documentation.

------

# Related Documents

| Document                 | Description                     |
| ------------------------ | ------------------------------- |
| `01_board.md`            | Board and hardware platform     |
| `02_display.md`          | LCD and display subsystem       |
| `03_input.md`            | EC11 encoder and buttons        |
| `04_power.md`            | Power subsystem                 |
| `05_storage.md`          | Flash, PSRAM and microSD        |
| `07_boot.md`             | Boot process and strapping pins |
| `08_revision_history.md` | Project revision history        |

------

# Summary

The KeyKeeper2 interface architecture uses the unmodified
 **Waveshare ESP32-S3-LCD-1.47B** board and assigns each hardware resource to a
 single responsible subsystem.

The primary project GPIO allocation is:

| GPIO   | Function                 | Owner         |
| ------ | ------------------------ | ------------- |
| GPIO4  | Encoder Phase A          | Input         |
| GPIO5  | Encoder Phase B          | Input         |
| GPIO6  | Encoder Push Button / OK | Input         |
| GPIO7  | BACK Button              | Input         |
| GPIO38 | RGB LED                  | System        |
| GPIO39 | LCD Reset                | Display       |
| GPIO40 | LCD SCLK                 | Display       |
| GPIO41 | LCD DC                   | Display       |
| GPIO42 | LCD CS                   | Display       |
| GPIO43 | UART TX                  | Board / Debug |
| GPIO44 | UART RX                  | Board / Debug |
| GPIO45 | LCD MOSI                 | Display       |
| GPIO48 | LCD Backlight            | Display       |

The microSD interface reserves:

```
GPIO14–GPIO18
GPIO21
```

The PSRAM interface reserves:

```
GPIO26–GPIO37
```

The currently available GPIOs for future expansion are:

```
GPIO8
GPIO9
GPIO10
GPIO11
GPIO12
GPIO13
```

The LCD configuration is based on the verified hardware mapping:

```
MOSI = GPIO45
SCLK = GPIO40
CS   = GPIO42
DC   = GPIO41
RST  = GPIO39
BL   = GPIO48
```

The board schematic confirms the LCD signal group and the microSD SDIO signal
 group. 

The KeyKeeper2 architecture requires hardware access to remain layered:

```
Application
     |
     v
Services
     |
     v
Drivers
     |
     v
BSP
     |
     v
Hardware
```

No application component may directly manipulate a GPIO belonging to another
 subsystem.

`06_interfaces.md` is the consolidated interface-allocation document and must
 remain synchronized with the BSP and all hardware-specific subsystem
 documentation.
