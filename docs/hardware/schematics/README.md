# Schematics

This directory contains electrical schematics and hardware diagrams related to the **KeyKeeper2** project.

The documentation is divided into two categories:

* vendor documentation (manufacturer-supplied, read-only);
* editable project schematics.

---

# Directory Structure

```text
schematics/
│
├── README.md
│
├── vendor/
│   ├── ESP32-S3-LCD-1.47_schematic_diagram.pdf
│   ├── ESP32-S3-LCD-1.47_schematic_diagram.html
│   ├── ESP32-S3-LCD-1.47_schematic_diagram/   (assets for the .html viewer)
│   └── ESP32-S3-LCD-1.47-introduction-01.png
│
└── drawio/
    ├── README.md
    ├── board_overview.drawio
    ├── keykeeper2_io.drawio
    ├── gpio_map.drawio
    ├── encoder_connection.drawio
    ├── storage_architecture.drawio
    └── power_overview.drawio
```

Note: there is no separate `pdf/` export directory -- the `.drawio` files under `drawio/` are the only
copies of the project's own diagrams. Open them directly with diagrams.net / Draw.io when a rendered view
is needed; nothing here is pre-exported to PDF.

---

# Contents

| File                                              | Description                                                                               |
| -------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| `vendor/ESP32-S3-LCD-1.47_schematic_diagram.pdf`  | Original schematic for the Waveshare ESP32-S3-LCD-1.47B board, as supplied by the manufacturer. |
| `vendor/ESP32-S3-LCD-1.47_schematic_diagram.html` | Same schematic, as an interactive HTML viewer (see the adjacent directory for its assets). |
| `vendor/ESP32-S3-LCD-1.47-introduction-01.png`    | Board overview image from the manufacturer.                                                |
| `drawio/board_overview.drawio`                    | Editable top-level overview of the KeyKeeper2 hardware platform.                           |
| `drawio/keykeeper2_io.drawio`                     | Editable block diagram of I/O connections.                                                 |
| `drawio/encoder_connection.drawio`                | Editable schematic of the EC11 rotary encoder and BACK button connections.                 |
| `drawio/gpio_map.drawio`                          | Editable GPIO allocation diagram.                                                          |
| `drawio/storage_architecture.drawio`              | Editable storage subsystem diagram.                                                        |
| `drawio/power_overview.drawio`                    | Editable power distribution diagram.                                                       |

---

# Vendor Documentation

The `vendor` directory contains documentation supplied by the hardware manufacturer (Waveshare / the
ESP32-S3-LCD-1.47B board's own schematic).

These files are **read-only** and must never be modified.

Any project-specific information should be documented separately rather than editing the original
manufacturer documentation. A separate, unrelated reference copy of Espressif's own `espp/ws-s3-lcd-1-47`
component source (studied during display bring-up, not part of the build) lives at
`/vendor/espp_ws-s3-lcd-1-47/` at the repository root -- not here, and not actually "vendor documentation"
in the sense this directory means.

---

# Project Schematics

The `drawio` directory contains the editable source files created specifically for the KeyKeeper2 project
-- see `drawio/README.md` for the current file list, kept there since it's the one most likely to need
updating when a diagram is added or renamed.

All new hardware diagrams should be created in **Draw.io** (`.drawio`) format. These files are the master
copies; there is no separate exported-PDF step in this project's current workflow.

---

# Editing Rules

When updating hardware diagrams:

* modify only the `.drawio` source file;
* keep `drawio/README.md`'s file list in sync if you add, remove, or rename a diagram;
* do not edit files in the `vendor` directory.

Draw.io's own autosave/lock files (named like `.$<filename>.drawio.bkp`) are excluded via `.gitignore` --
if your editor leaves one behind, it won't get committed, but also won't need to be manually cleaned up
before committing.

---

# Naming Convention

Project schematics should use lowercase filenames with underscores.

Examples:

* `gpio_map.drawio`
* `power_overview.drawio`
* `encoder_connection.drawio`
* `storage_architecture.drawio`

---

# Related Documentation

| Document              | Description       |
| --------------------- | ----------------- |
| `../01_board.md`      | Board description |
| `../06_interfaces.md` | GPIO allocation   |
| `../05_storage.md`    | Storage subsystem |
| `../04_power.md`      | Power subsystem   |

---

# Summary

The `schematics` directory stores all electrical diagrams related to the KeyKeeper2 hardware platform.
Editable Draw.io sources under `drawio/` are the primary, and currently only, project-authored design
files. Original vendor documentation is preserved separately under `vendor/` to ensure traceability and
simplify future hardware maintenance.
