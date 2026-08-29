# Hardware Revision History

Revision history of the hardware platform used by the **KeyKeeper2** project.

---

# Purpose

This document records the hardware revisions supported by the KeyKeeper2 firmware.

Its purpose is to:

* track hardware changes;
* document compatibility between firmware and hardware;
* simplify future maintenance;
* provide a reference for developers.

Only released or officially planned hardware revisions should be recorded.

---

# Current Hardware Platform

The current implementation of KeyKeeper2 is based on the following development board.

| Parameter | Value                        |
| --------- | ---------------------------- |
| Board     | Waveshare ESP32-S3-LCD-1.47B |
| MCU       | ESP32-S3R8                   |
| Flash     | 16 MB                        |
| PSRAM     | 8 MB                         |
| Display   | 1.47" TFT LCD                |
| Storage   | microSD                      |
| Input     | EC11 Encoder + BACK Button   |

---

# Revision History

## Revision A1

### Status

Current development revision.

### Hardware

| Component         | Description                  |
| ----------------- | ---------------------------- |
| Development Board | Waveshare ESP32-S3-LCD-1.47B |
| Display           | Integrated 1.47" TFT LCD     |
| User Input        | External EC11 rotary encoder |
| Additional Button | BACK button                  |
| Storage           | microSD card                 |
| Power             | USB Type-C                   |

### GPIO Assignment

| GPIO  | Function        |
| ----- | --------------- |
| GPIO4 | Encoder Phase A |
| GPIO5 | Encoder Phase B |
| GPIO6 | Encoder Button  |
| GPIO7 | BACK Button     |

### Notes

* First supported hardware revision.
* Baseline reference for all firmware development.
* No touchscreen support.
* No custom PCB modifications.

---

# Firmware Compatibility

| Firmware Version | Hardware Revision | Status    |
| ---------------- | ----------------- | --------- |
| 1.x              | A1                | Supported |

Future firmware releases should maintain compatibility with Revision A1 whenever technically possible.

---

# Planned Hardware Changes

No hardware changes are currently planned.

Potential future revisions may introduce:

* additional sensors;
* RTC module;
* secure element;
* NFC reader;
* optional expansion modules.

Such changes must preserve backward compatibility unless a major hardware revision is introduced.

---

# Revision Policy

Hardware revisions are identified by a revision code.

Recommended format:

| Revision | Meaning                 |
| -------- | ----------------------- |
| A1       | Initial release         |
| A2       | Minor hardware update   |
| B1       | Major hardware revision |
| B2       | Improved major revision |

Minor revisions should not require firmware modifications unless explicitly documented.

---

# Documentation Rules

When a new hardware revision is released, this document should be updated with:

* revision identifier;
* release date;
* hardware modifications;
* affected GPIO assignments;
* firmware compatibility;
* migration notes.

Historical entries must not be removed.

---

# Compatibility Requirements

New hardware revisions should preserve:

* GPIO assignments where possible;
* storage architecture;
* display interface;
* power subsystem;
* USB functionality;
* encoder-based user interface.

Changes that affect firmware behavior must be documented in both the hardware documentation and the corresponding Architecture Decision Records (ADR).

---

# Related Documents

| Document           | Description                   |
| ------------------ | ----------------------------- |
| `01_board.md`      | Board description             |
| `06_interfaces.md` | GPIO allocation               |
| `07_boot.md`       | Boot process                  |
| `../adr/`          | Architecture Decision Records |

---

# Summary

Revision **A1** is the initial and currently supported hardware platform for the KeyKeeper2 project.

This document provides a controlled history of hardware revisions and serves as the authoritative reference for compatibility between the firmware and the underlying hardware platform. Future revisions should prioritize backward compatibility and clearly document any hardware changes that affect firmware operation.
