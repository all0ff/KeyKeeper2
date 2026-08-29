# Hardware Documentation

Hardware documentation for the **KeyKeeper2** project based on the **Waveshare ESP32-S3-LCD-1.47B** development board.

---

# Purpose

This directory contains the hardware reference for the KeyKeeper2 platform.

The documentation describes the physical hardware used by the project, including:

* development board;
* display subsystem;
* power supply;
* storage devices;
* GPIO allocation;
* hardware interfaces;
* boot process;
* hardware revisions.

Software architecture, BSP implementation and application design are documented separately in **docs/developer**.

---

# Hardware Platform

KeyKeeper2 is built on the following hardware platform.

| Component         | Description                       |
| ----------------- | --------------------------------- |
| MCU               | ESP32-S3                          |
| Development Board | Waveshare ESP32-S3-LCD-1.47B      |
| Display           | 1.47" TFT LCD                     |
| Storage           | Internal Flash + PSRAM + microSD  |
| User Input        | EC11 Rotary Encoder + BACK Button |
| USB               | Native USB Type-C                 |
| Wireless          | Wi-Fi / Bluetooth LE              |

---

# Directory Structure

```text
docs/
└── hardware/
    ├── README.md
    ├── 00_overview.md
    ├── 01_board.md
    ├── 02_display.md
    ├── 03_input.md
    ├── 04_power.md
    ├── 05_storage.md
    ├── 06_interfaces.md
    ├── 07_boot.md
    └── 08_revision_history.md
```

---

# Documents

| Document                   | Description                                      |
| -------------------------- | ------------------------------------------------ |
| **00_overview.md**         | Hardware platform overview and block diagram     |
| **01_board.md**            | Waveshare board description and component layout |
| **02_display.md**          | LCD hardware and display interface               |
| **03_input.md**            | Rotary encoder and button interface              |
| **04_power.md**            | Power supply and power management                |
| **05_storage.md**          | Flash, PSRAM and microSD storage subsystem       |
| **06_interfaces.md**       | GPIO allocation and hardware interfaces          |
| **07_boot.md**             | ESP32-S3 boot process and strapping pins         |
| **08_revision_history.md** | Hardware revision history                        |

---

# Design Principles

The hardware documentation follows these principles:

* describe the actual hardware used by KeyKeeper2;
* avoid duplication with software documentation;
* document all hardware limitations;
* provide complete GPIO allocation;
* describe all external interfaces;
* keep documentation synchronized with hardware revisions.

---

# Related Documentation

| Document          | Description                        |
| ----------------- | ---------------------------------- |
| `ARCHITECTURE.md` | Overall project architecture       |
| `SOFTWARE.md`     | Software architecture              |
| `BUILD.md`        | Build system                       |
| `docs/developer/` | Firmware development documentation |
| `docs/adr/`       | Architecture Decision Records      |

---

# Summary

The **docs/hardware** directory serves as the official hardware reference for the KeyKeeper2 project.

It provides firmware developers with all hardware-specific information required to implement, maintain and extend the platform while keeping software and hardware documentation clearly separated.
