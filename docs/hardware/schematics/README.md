# Schematics

This directory contains electrical schematics and hardware diagrams related to the **KeyKeeper2** project.

The documentation is divided into three categories:

* vendor documentation;
* editable project schematics;
* exported reference documents.

---

# Directory Structure

```text
schematics/
│
├── README.md
│
├── vendor/
│   └── waveshare_board.pdf
│
├── drawio/
│   ├── keykeeper2_io.drawio
│   ├── encoder_connection.drawio
│   ├── gpio_map.drawio
│   ├── storage_architecture.drawio
│   └── power_overview.drawio
│
└── pdf/
    ├── keykeeper2_io.pdf
    ├── encoder_connection.pdf
    ├── gpio_map.pdf
    ├── storage_architecture.pdf
    └── power_overview.pdf
```

---

# Contents

| File                                 | Description                                                                                         |
| ------------------------------------ | --------------------------------------------------------------------------------------------------- |
| `vendor/waveshare_board.pdf`         | Original schematic provided by Waveshare. Used as the hardware reference for the development board. |
| `drawio/keykeeper2_io.drawio`        | Editable block diagram of the complete KeyKeeper2 hardware platform.                                |
| `drawio/encoder_connection.drawio`   | Editable schematic of the EC11 rotary encoder and BACK button connections.                          |
| `drawio/gpio_map.drawio`             | Editable GPIO allocation diagram.                                                                   |
| `drawio/storage_architecture.drawio` | Editable storage subsystem diagram.                                                                 |
| `drawio/power_overview.drawio`       | Editable power distribution diagram.                                                                |
| `pdf/*.pdf`                          | Exported PDF versions of all project schematics for documentation and printing.                     |

---

# Vendor Documentation

The `vendor` directory contains documentation supplied by the hardware manufacturer.

These files are **read-only** and must never be modified.

Any project-specific information should be documented separately rather than editing the original manufacturer documentation.

---

# Project Schematics

The `drawio` directory contains the editable source files created specifically for the KeyKeeper2 project.

All new hardware diagrams should be created in **Draw.io** (`.drawio`) format.

These files are considered the master copies of the schematics.

---

# PDF Exports

The `pdf` directory contains exported versions of the Draw.io diagrams.

These files are intended for:

* documentation;
* reviews;
* printing;
* distribution.

PDF files should always be regenerated after modifying the corresponding `.drawio` source.

---

# Editing Rules

When updating hardware diagrams:

* modify only the `.drawio` source file;
* regenerate the corresponding PDF;
* keep filenames synchronized between `drawio` and `pdf`;
* do not edit files in the `vendor` directory.

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

Editable Draw.io sources are maintained as the primary design files, while exported PDF documents provide convenient reference material. Original vendor documentation is preserved separately to ensure traceability and simplify future hardware maintenance.
