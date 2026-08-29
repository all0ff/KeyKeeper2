# Display

Detailed description of the display subsystem used by the **KeyKeeper2** project.

The project uses the integrated display of the **Waveshare ESP32-S3-LCD-1.47B** development board.

---

# Purpose

This document defines the hardware configuration and software architecture of the KeyKeeper2 display subsystem.

It describes:

* LCD hardware;
* ST7789 controller;
* LCD GPIO assignment;
* SPI3 configuration;
* LovyanGFX driver;
* LVGL 9 integration;
* display geometry and offsets;
* DMA usage;
* backlight control;
* display initialization;
* display subsystem responsibilities.

The display configuration described here is the project hardware baseline.

---

# Display Information

| Parameter     | Value                        |
| ------------- | ---------------------------- |
| Board         | Waveshare ESP32-S3-LCD-1.47B |
| Display       | 1.47" TFT LCD                |
| Controller    | ST7789                       |
| Resolution    | 172 × 320                    |
| Color Format  | RGB565 / 16-bit              |
| Interface     | SPI                          |
| SPI Host      | SPI3_HOST                    |
| SPI Mode      | 0                            |
| SPI Interface | 3-wire                       |
| DMA           | Enabled                      |
| Touchscreen   | No                           |

The display is the primary visual interface of KeyKeeper2.

The board does not have a touchscreen. User interaction is provided by the external EC11 rotary encoder and physical buttons.

---

# LCD GPIO Assignment

The onboard LCD is connected to the ESP32-S3 as follows:

| LCD Pin | ESP32-S3 GPIO | Function          |
| ------- | ------------- | ----------------- |
| MOSI    | GPIO45        | SPI data          |
| SCLK    | GPIO40        | SPI clock         |
| LCD_CS  | GPIO42        | Chip select       |
| LCD_DC  | GPIO41        | Data / command    |
| LCD_RST | GPIO39        | Reset             |
| LCD_BL  | GPIO48        | Backlight control |

These GPIOs are reserved for the display subsystem.

**GPIO48 is the LCD backlight control pin.**

GPIO46 is not used for the LCD backlight.

---

# Display Controller

The display uses the **ST7789** controller.

The KeyKeeper2 display driver is based on the verified configuration for the Waveshare ESP32-S3-LCD-1.47B.

The display component is responsible for:

* ST7789 initialization;
* SPI configuration;
* display orientation;
* display offsets;
* display data transfer;
* DMA transfers;
* LVGL integration;
* backlight control.

Application components must not access the ST7789 controller directly.

---

# Display Driver

KeyKeeper2 uses **LovyanGFX** for the low-level LCD driver.

The driver provides the hardware interface between the ESP32-S3 and the ST7789 controller.

The architecture is:

```text
LVGL 9
   |
   v
Display Component
   |
   v
LovyanGFX
   |
   v
ST7789
   |
   v
SPI3_HOST
   |
   v
Waveshare LCD
```

LovyanGFX configuration must remain inside the display component.

The application and GUI layers must not contain hardware-specific LCD configuration.

------

# SPI Configuration

The LCD uses the ESP32-S3 **SPI3_HOST** peripheral.

The verified configuration is:

| Parameter       | Value      |
| --------------- | ---------- |
| SPI Host        | SPI3_HOST  |
| SPI Mode        | 0          |
| Interface       | 3-wire SPI |
| Write Frequency | 27 MHz     |
| Read Frequency  | 16 MHz     |
| DMA             | Enabled    |
| MOSI            | GPIO45     |
| SCLK            | GPIO40     |
| CS              | GPIO42     |
| DC              | GPIO41     |

SPI3 is dedicated to the LCD.

Other application components must not initialize or reconfigure SPI3.

------

# ST7789 Configuration

The ST7789 requires board-specific geometry and orientation parameters.

The verified KeyKeeper2 configuration is:

| Parameter       | Value  |
| --------------- | ------ |
| Controller      | ST7789 |
| Offset Rotation | 1      |
| Offset X        | 34     |
| Offset Y        | 0      |
| Invert          | true   |
| RGB Order       | false  |

These parameters are specific to the Waveshare ESP32-S3-LCD-1.47B.

Generic ST7789 configuration values must not be substituted for the verified board configuration.

---

# Display Resolution

The physical LCD resolution is:

| Parameter | Value      |
| --------- | ---------- |
| Width     | 172 pixels |
| Height    | 320 pixels |

The logical LVGL display resolution is also:

```
172 × 320
```

GUI components must work with the logical display dimensions and must not contain physical ST7789 offset calculations.

----

# Display Orientation

The display orientation is configured by the display driver.

The required board-specific rotation parameter is:

```
offset_rotation = 1
```

The GUI layer must not perform additional hardware-specific rotation or coordinate corrections.

All orientation handling belongs to the display subsystem.

------

# LVGL 9 Integration

KeyKeeper2 uses **LVGL 9** as the graphical user interface framework.

The display component provides the connection between LVGL and the physical LCD.

The architecture is:

```
Application
    |
    v
GUI / Screen Manager
    |
    v
LVGL 9
    |
    v
Display Component
    |
    v
LovyanGFX
    |
    v
ST7789
```

LVGL is responsible for:

- GUI objects;
- screen layouts;
- rendering;
- invalidation;
- drawing;
- GUI events.

The display component is responsible for transferring rendered data to the LCD.

------

# Display Buffer

LVGL requires drawing buffers for rendering display content.

KeyKeeper2 may use PSRAM for large display buffers where appropriate.

DMA-capable memory requirements must be respected by the actual LCD transfer buffers.

The display subsystem is responsible for selecting and managing suitable buffers for the configured LCD transfer mechanism.

------

# DMA

LCD transfers use DMA to reduce CPU overhead.

The display component is responsible for:

- preparing transfer buffers;
- starting DMA transfers;
- handling transfer completion;
- notifying LVGL when a flush operation is complete.

Application components must not control LCD DMA directly.

------

# LVGL Flush

When LVGL has rendered a changed area, it requests the display subsystem to transfer that area to the LCD.

The transfer sequence is:

```
LVGL rendering
      |
      v
Display flush request
      |
      v
Display Component
      |
      v
LovyanGFX
      |
      v
ST7789
      |
      v
SPI3 + DMA
      |
      v
LCD
      |
      v
Flush complete
      |
      v
LVGL continues
```

Only the required display area should be transferred where supported by the implementation.

------

# Display Initialization

Display initialization is performed during system startup.

The logical initialization sequence is:

```
Board initialization
        |
        v
Display component initialization
        |
        v
SPI3 configuration
        |
        v
LovyanGFX initialization
        |
        v
ST7789 initialization
        |
        v
Display geometry configuration
        |
        v
Display orientation configuration
        |
        v
Backlight initialization
        |
        v
LVGL initialization
        |
        v
LVGL display registration
```

The display must be initialized before GUI screens are created.

------

# Backlight

The LCD backlight is controlled independently from the ST7789 controller.

The backlight control pin is:

| Function      | GPIO   |
| ------------- | ------ |
| LCD Backlight | GPIO48 |

GPIO48 is owned by the display component.

Application components must not manipulate GPIO48 directly.

------

# Brightness Control

The display subsystem provides brightness control.

Typical operations include:

- set brightness;
- increase brightness;
- decrease brightness;
- dim display;
- restore brightness;
- disable backlight;
- enable backlight.

The actual hardware control remains inside the display component.

------

# Display Sleep

The display subsystem may disable the backlight when the device becomes inactive.

The intended sequence is:

```
Display active
      |
      v
Inactivity timeout
      |
      v
Reduce brightness
      |
      v
Disable backlight
```

When user activity is detected:

```
User input
    |
    v
Enable backlight
    |
    v
Restore brightness
    |
    v
Display active
```

The exact power-management policy is defined by the system and power-management components.

------

# Display Service

The display component provides a hardware abstraction to the rest of the firmware.

The display service is responsible for:

- display initialization;
- LVGL integration;
- display flushing;
- brightness control;
- display sleep;
- display wake-up;
- access to the LVGL display instance.

The rest of the firmware must not depend directly on LovyanGFX or ST7789.

------

# Display API

The application-facing display interface should remain hardware-independent.

Typical operations include:

```
init()
setBrightness()
getDisplay()
sleep()
wake()
```

The exact implementation belongs to the `display` component.

Low-level LCD configuration must remain private to that component.

------

# GUI Integration

GUI screens communicate with the display through LVGL.

The GUI layer must not contain:

- LCD GPIO numbers;
- SPI host configuration;
- SPI frequencies;
- ST7789 commands;
- LovyanGFX configuration;
- DMA configuration;
- backlight GPIO control.

The GUI operates only with LVGL objects and the display service.

------

# GUI Design Considerations

The LCD has a relatively small resolution of 172 × 320 pixels.

GUI screens should therefore:

- use the available screen area efficiently;
- use readable fonts;
- keep navigation controls clearly visible;
- avoid unnecessary animations;
- minimize full-screen redraws;
- avoid excessively small interface elements;
- provide clear visual feedback for encoder navigation.

Because there is no touchscreen, GUI controls must be designed for navigation using the EC11 encoder and physical buttons.

------

# Color Format

The display uses a 16-bit RGB color representation.

The configured format is:

```
RGB565
```

The relevant display parameters are:

| Parameter   | Value  |
| ----------- | ------ |
| Color Depth | 16-bit |
| RGB Order   | false  |
| Invert      | true   |

The GUI layer should use LVGL color types.

Manual byte-order manipulation should not be performed by application components.

------

# Display Errors

Display initialization errors must be handled by the display component.

Possible errors include:

- SPI initialization failure;
- DMA buffer allocation failure;
- LovyanGFX initialization failure;
- ST7789 initialization failure;
- invalid display configuration;
- LVGL display registration failure.

Errors must be reported through the project logging system.

The application must not silently ignore a display initialization failure.

------

# Hardware Restrictions

The following resources are reserved for the display subsystem:

| Resource  | Function          |
| --------- | ----------------- |
| GPIO39    | LCD reset         |
| GPIO40    | LCD clock         |
| GPIO41    | LCD DC            |
| GPIO42    | LCD CS            |
| GPIO45    | LCD MOSI          |
| GPIO48    | LCD backlight     |
| SPI3_HOST | LCD SPI interface |

These resources must not be reassigned to other peripherals.

The following rules also apply:

- GPIO46 must not be treated as LCD backlight;
- SPI2 must not be used for the LCD;
- the LCD must use SPI3_HOST;
- generic ST7789 offsets must not replace the verified board values;
- touchscreen functionality must not be assumed;
- application components must not access LCD hardware directly.

------

# Display Component

The corresponding firmware component is:

```
components/display/
```

The component owns:

- LovyanGFX configuration;
- ST7789 configuration;
- SPI3 configuration;
- LCD GPIO configuration;
- DMA configuration;
- LVGL display integration;
- backlight control.

The BSP provides the board-level GPIO definitions.

The GUI uses LVGL and the display service.

------

# Display Baseline

The following configuration is the definitive KeyKeeper2 display baseline:

```
Board:
    Waveshare ESP32-S3-LCD-1.47B


Display:
    1.47" TFT LCD
    172 × 320
    ST7789


Driver:
    LovyanGFX


GUI:
    LVGL 9


SPI:
    SPI3_HOST
    3-wire
    DMA
    Mode 0
    Write = 27 MHz
    Read  = 16 MHz


GPIO:
    MOSI = GPIO45
    SCLK = GPIO40
    CS   = GPIO42
    DC   = GPIO41
    RST  = GPIO39
    BL   = GPIO48


ST7789:
    offset_rotation = 1
    offset_x        = 34
    offset_y        = 0
    invert          = true
    rgb_order       = false


Touch:
    Not present
    Not used
```

------

# Design Rules

The following rules apply to the KeyKeeper2 display subsystem:

1. The LCD must use the verified Waveshare ESP32-S3-LCD-1.47B configuration.
2. The LCD must use the ST7789 controller configuration defined in this document.
3. The LCD must use SPI3_HOST.
4. LCD GPIO definitions must be centralized in the BSP.
5. SPI3 must remain dedicated to the LCD.
6. ST7789 offsets must not be changed without hardware verification.
7. The GUI must not access the LCD driver directly.
8. The GUI must not access GPIO48 directly.
9. Backlight control must remain inside the display subsystem.
10. LVGL must remain independent of the physical LCD GPIO configuration.
11. Touchscreen support must not be added because the board has no touchscreen.
12. Any hardware change must be reflected in `01_board.md`, `02_display.md` and `06_interfaces.md`.

------

# Related Documents

| Document           | Description                  |
| ------------------ | ---------------------------- |
| `01_board.md`      | Board and hardware platform  |
| `03_input.md`      | EC11 encoder and buttons     |
| `04_power.md`      | Power subsystem              |
| `05_storage.md`    | Flash, PSRAM and microSD     |
| `06_interfaces.md` | GPIO and hardware interfaces |

------

# Summary

The KeyKeeper2 display subsystem is based on the integrated **1.47" ST7789 TFT LCD** of the **Waveshare ESP32-S3-LCD-1.47B** board.

The definitive display configuration is:

| Parameter       | Value         |
| --------------- | ------------- |
| Display         | 1.47" TFT LCD |
| Controller      | ST7789        |
| Resolution      | 172 × 320     |
| Driver          | LovyanGFX     |
| GUI Framework   | LVGL 9        |
| SPI Host        | SPI3_HOST     |
| SPI Mode        | 0             |
| SPI Interface   | 3-wire        |
| DMA             | Enabled       |
| Write Frequency | 27 MHz        |
| Read Frequency  | 16 MHz        |
| Offset Rotation | 1             |
| Offset X        | 34            |
| Offset Y        | 0             |
| Invert          | true          |
| RGB Order       | false         |

The LCD GPIO allocation is:

| LCD Function | GPIO   |
| ------------ | ------ |
| MOSI         | GPIO45 |
| SCLK         | GPIO40 |
| CS           | GPIO42 |
| DC           | GPIO41 |
| RST          | GPIO39 |
| BL           | GPIO48 |

The display subsystem owns the complete LCD hardware interface, including:

- ST7789 initialization;
- SPI3 configuration;
- DMA transfers;
- LVGL display integration;
- display flushing;
- display orientation;
- display offsets;
- backlight control;
- display sleep and wake-up.

The GUI layer communicates with the display through **LVGL 9** and must not access the ST7789 controller, LovyanGFX, SPI3 or GPIO48 directly.

The LCD backlight is controlled through **GPIO48**.

**GPIO46 is not used for the LCD backlight.**

The board does not have a touchscreen. KeyKeeper2 therefore uses the external EC11 rotary encoder and physical buttons for user interaction.

The verified Waveshare LCD configuration described in this document is the hardware baseline for `components/display/`.

Any change to the LCD controller, GPIO assignment, SPI host, display geometry, driver or backlight configuration must be reflected in:

- `01_board.md`;
- `02_display.md`;
- `06_interfaces.md`;
- the corresponding BSP and display component configuration.
