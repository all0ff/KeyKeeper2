# Storage

Detailed description of the storage subsystem used by the **KeyKeeper2** project.

KeyKeeper2 uses several different memory and storage resources of the
Waveshare ESP32-S3-LCD-1.47B platform.

The storage architecture separates:

* internal Flash;
* NVS;
* LittleFS;
* external PSRAM;
* microSD;
* user vault data;
* configuration data;
* backup and import/export data.

---

# Purpose

This document defines the storage architecture and responsibilities of the
KeyKeeper2 storage subsystem.

It describes:

* internal Flash usage;
* NVS usage;
* LittleFS;
* external PSRAM;
* microSD;
* storage hierarchy;
* vault storage;
* backup and restore;
* import and export;
* storage services;
* filesystem ownership;
* storage initialization;
* storage error handling.

The storage subsystem provides a hardware-independent interface to the
application.

---

# Storage Architecture

KeyKeeper2 uses different storage resources for different types of data.

The basic architecture is:

```text
                    KeyKeeper2 Storage
                           |
          +----------------+----------------+
          |                |                |
          v                v                v
         NVS            LittleFS         microSD
          |                |                |
          v                v                v
      Settings        System Files      User Data
                           |
                           v
                       vault.db
```

The architecture defines:

- **NVS** for persistent settings;
- **LittleFS** for the internal filesystem;
- **microSD** as optional removable storage;
- **PSRAM** as runtime memory rather than persistent storage.

The project architecture explicitly defines LittleFS as the default filesystem,
 with optional microSD support for `vault.db`, backups and import/export data.
 Settings are always stored in NVS. 

------

# Storage Resources

| Resource       | Capacity / Type           | Purpose                              |
| -------------- | ------------------------- | ------------------------------------ |
| Internal Flash | 16 MB                     | Firmware and system storage          |
| NVS            | Flash-based               | Persistent settings                  |
| LittleFS       | Internal Flash filesystem | System and application files         |
| PSRAM          | 8 MB                      | Runtime memory and large buffers     |
| microSD        | Removable                 | Vault, backup and import/export data |

The ESP32-S3R8 platform used by KeyKeeper2 provides 16 MB Flash and 8 MB external
 PSRAM. 

------

# Internal Flash

The board provides onboard SPI Flash.

The documented capacity is:

| Parameter | Value                    |
| --------- | ------------------------ |
| Type      | SPI Flash                |
| Capacity  | 16 MB                    |
| Usage     | Firmware and system data |

The internal Flash contains firmware and system-related data.

Typical contents include:

```
Bootloader
Partition Table
Application
OTA Images
NVS
Configuration
LittleFS
```

Internal Flash is not intended to be used as the primary location for large
 user databases. 

------

# NVS

NVS is used for persistent system settings.

Settings are always stored in NVS.

Typical settings include:

- system configuration;
- user interface settings;
- language selection;
- display settings;
- application preferences.

The GUI must not access NVS directly.

All settings access must pass through the settings service.

The architecture explicitly defines `SettingsService` as the interface responsible
 for persistent settings. 

The storage relationship is:

```
GUI
 |
 v
SettingsService
 |
 v
NVS
 |
 v
Internal Flash
```

------

# LittleFS

LittleFS is the default internal filesystem used by KeyKeeper2.

It provides persistent file storage inside the internal Flash.

LittleFS is intended for:

- application files;
- system files;
- filesystem metadata;
- resources;
- internal storage required by the firmware.

The filesystem is not accessed directly by GUI components.

Access is performed through the appropriate service layer.

------

# LittleFS Mount Point

The logical storage hierarchy defined by the project includes:

```
/vault
```

The storage structure is designed around the following logical locations:

```
/vault/
    vault.db
    backup/
    import/
    export/
```

The architecture document defines this structure as the basis of the storage
 subsystem. 

------

# microSD

The Waveshare ESP32-S3-LCD-1.47B board includes an integrated microSD card
 socket.

KeyKeeper2 supports the microSD card as removable storage.

The project architecture uses microSD for:

- `vault.db`;
- backup archives;
- import files;
- export files.

The board documentation also identifies the microSD interface as a hardware
 resource reserved by the board. 

------

# microSD Interface

The KeyKeeper2 implementation uses the board's microSD interface in **4-bit
 mode**.

The microSD GPIO resources are reserved by the board and must not be assigned
 to other peripherals.

The currently documented reserved microSD GPIO resources include:

```
GPIO14–GPIO18
GPIO21
```

The exact signal-to-GPIO mapping belongs to the board interface documentation
 and must remain centralized in the BSP. 

------

# microSD Usage

The removable card is intended for data that benefits from being portable or
 externally accessible.

Typical contents are:

```
microSD
 |
 +-- vault.db
 |
 +-- backup/
 |
 +-- import/
 |
 +-- export/
```

The card can therefore be used for:

- transferring vault data;
- creating backups;
- restoring backups;
- importing records;
- exporting records;
- moving data between compatible KeyKeeper2 installations.

------

# Vault

The password database is represented by the logical vault storage.

The architecture defines:

```
/vault/vault.db
```

as the main vault database path.

The vault is handled by a dedicated service and is not accessed directly by
 the GUI.

The architecture defines `VaultService` with operations including:

```
GetAccounts()
Save()
Load()
Import()
Export()
Backup()
Restore()
```



------

# Vault Access

The access hierarchy is:

```
GUI
 |
 v
VaultService
 |
 v
StorageService
 |
 +----------+----------+
 |                     |
 v                     v
LittleFS              microSD
```

The GUI must never open `vault.db` directly.

The application must never depend on a particular physical storage medium.

The storage service determines where the requested data is located.

------

# Storage Service

The `StorageService` provides the common storage abstraction.

Its responsibilities include:

- filesystem initialization;
- filesystem mounting;
- filesystem unmounting;
- file access;
- directory management;
- storage availability;
- storage error reporting;
- removable-media detection;
- storage synchronization where required.

The storage service hides filesystem and hardware details from the application
 layer.

------

# Storage Driver

The low-level storage layer is responsible for hardware-specific operations.

The architecture defines a `StorageDriver` in the driver layer. 

The driver layer is responsible for:

- filesystem hardware access;
- microSD interface access;
- filesystem mounting;
- low-level storage errors.

The service layer must not contain board-specific GPIO configuration.

------

# Storage Component

The corresponding firmware component is:

```
components/storage/
```

The component provides the storage abstraction used by the rest of the
 firmware.

The project architecture places `storage/` alongside the other main firmware
 components. 

The storage component should contain:

```
components/storage/
    CMakeLists.txt
    include/
    src/
```

The exact source files are defined by the implementation structure of the
 firmware.

------

# PSRAM

The board provides 8 MB external PSRAM.

PSRAM is runtime memory and is not persistent storage.

The documented PSRAM configuration is:

| Parameter | Value          |
| --------- | -------------- |
| Capacity  | 8 MB           |
| Interface | Octal PSRAM    |
| Purpose   | Runtime memory |

PSRAM is intended primarily for:

- LVGL frame buffers;
- image decoding;
- temporary buffers;
- large runtime data structures;
- filesystem cache.



------

# PSRAM and Storage

PSRAM must not be treated as permanent storage.

Data stored in PSRAM is lost when the device loses power or resets.

Its purpose in the storage subsystem is limited to runtime assistance such as:

```
microSD / LittleFS
       |
       v
Storage buffers
       |
       v
PSRAM
```

PSRAM may be used to reduce pressure on internal SRAM when processing large
 storage buffers.

------

# Storage Hierarchy

The KeyKeeper2 storage hierarchy is:

```
                 Application
                      |
          +-----------+-----------+
          |                       |
          v                       v
   SettingsService          VaultService
          |                       |
          v                       v
         NVS                StorageService
                                  |
                         +--------+--------+
                         |                 |
                         v                 v
                      LittleFS          microSD
```

This separation prevents application code from depending on the physical
 storage implementation.

------

# Storage Priority

The project architecture defines:

- NVS as the persistent settings store;
- LittleFS as the default internal filesystem;
- microSD as optional removable storage for vault and data exchange.

Therefore:

```
Settings
   |
   +--> NVS


System Files
   |
   +--> LittleFS


Vault / Backup / Import / Export
   |
   +--> LittleFS
   |
   +--> microSD
```

The exact selection of the active vault storage medium is handled by the
 storage/vault services.

------

# Backup

The storage subsystem supports vault backups.

The logical backup location is:

```
/vault/backup/
```

Backups may also be stored on removable microSD media.

The backup operation belongs to `VaultService`, not to the GUI.

The logical flow is:

```
VaultService
     |
     v
Read vault
     |
     v
Create backup
     |
     v
StorageService
     |
     +----> LittleFS
     |
     +----> microSD
```

------

# Restore

Restore operations are also handled through `VaultService`.

The storage layer provides the file access required by the restore operation.

The logical flow is:

```
Backup File
     |
     v
StorageService
     |
     v
VaultService
     |
     v
Validate / Restore
     |
     v
vault.db
```

The GUI must not manipulate the database file directly.

------

# Import

Import files are stored in:

```
/vault/import/
```

The import process is:

```
Import File
     |
     v
StorageService
     |
     v
VaultService
     |
     v
Validate
     |
     v
Import Records
```

Import processing is an application-level operation.

Storage is responsible only for providing reliable file access.

------

# Export

Export files are stored in:

```
/vault/export/
```

The export process is:

```
VaultService
     |
     v
Read Vault
     |
     v
Generate Export
     |
     v
StorageService
     |
     v
Export File
```

The export destination may be internal storage or removable microSD storage,
 depending on the selected operation.

------

# Storage Initialization

Storage initialization is performed during system startup.

The logical sequence is:

```
Power On
    |
    v
Board Init
    |
    v
Display Init
    |
    v
Settings Init
    |
    v
Storage Init
    |
    +----> NVS
    |
    +----> LittleFS
    |
    +----> microSD detection / initialization
    |
    v
Storage Ready
```

The project architecture places storage initialization after board and display
 initialization. 

------

# microSD Availability

microSD is removable and therefore must be treated as an optional resource.

The firmware must be able to operate without an inserted card.

The absence of microSD must not prevent:

- system startup;
- GUI operation;
- access to NVS settings;
- operation of the internal filesystem.

When the card is available, storage services may expose its additional storage
 operations.

------

# Storage Errors

The storage subsystem must detect and report errors such as:

- NVS initialization failure;
- filesystem mount failure;
- filesystem corruption;
- file open failure;
- file read failure;
- file write failure;
- insufficient storage space;
- microSD not present;
- microSD initialization failure;
- microSD removal;
- invalid backup file;
- invalid import file.

Errors must be reported through the project logging and service architecture.

The application must not silently ignore persistent-storage failures.

------

# Data Integrity

Storage operations involving persistent user data must be performed in a way
 that avoids leaving the vault in an inconsistent state.

The storage layer is responsible for reliable file operations.

The vault layer is responsible for database-level consistency.

The GUI is not responsible for either.

The separation is:

```
GUI
 |
 v
VaultService
 |
 v
StorageService
 |
 v
Filesystem / microSD
```

------

# Storage Security

The storage subsystem is responsible for storing and retrieving encrypted or
 otherwise protected vault data as required by the vault architecture.

The storage layer itself must not implement application cryptography.

Cryptographic operations belong to the security/vault layer.

The storage subsystem must therefore treat vault data as opaque data.

------

# Storage and Encryption

The logical separation is:

```
VaultService
      |
      v
Security / Cryptographic Layer
      |
      v
Encrypted Vault Data
      |
      v
StorageService
      |
      v
LittleFS / microSD
```

Storage does not interpret the contents of `vault.db`.

This keeps the storage component independent of the cryptographic implementation.

------

# File Ownership

The following ownership rules apply:

| Data            | Owner                  | Storage            |
| --------------- | ---------------------- | ------------------ |
| Settings        | SettingsService        | NVS                |
| System files    | Application / Services | LittleFS           |
| Vault           | VaultService           | LittleFS / microSD |
| Backups         | VaultService           | LittleFS / microSD |
| Import files    | VaultService           | LittleFS / microSD |
| Export files    | VaultService           | LittleFS / microSD |
| Runtime buffers | Application / Services | PSRAM              |

------

# GUI Restrictions

The GUI must not:

- access NVS directly;
- mount LittleFS directly;
- access microSD directly;
- open `vault.db` directly;
- implement backup operations;
- implement restore operations;
- implement import/export storage operations.

All storage operations must be performed through services.

The architecture explicitly requires the GUI to use services rather than access
 NVS or LittleFS directly. 

------

# Storage and EventBus

Components should communicate through the common event architecture where
 asynchronous notification is required.

Examples include:

```
EVENT_STORAGE_READY
EVENT_STORAGE_ERROR
EVENT_SD_INSERTED
EVENT_SD_REMOVED
EVENT_VAULT_LOADED
EVENT_VAULT_SAVED
```

Only events required by the actual implementation should be introduced.

The EventBus is the common mechanism used by the architecture for inter-component
 communication. 

------

# Storage Hardware Restrictions

The following hardware resources are reserved:

| Resource       | Function                  |
| -------------- | ------------------------- |
| Internal Flash | Firmware / system storage |
| PSRAM Bus      | External PSRAM            |
| microSD GPIO   | microSD interface         |

The microSD GPIO resources documented for the board include:

```
GPIO14–GPIO18
GPIO21
```

These resources must not be reassigned to unrelated peripherals. 

------

# Storage Limitations

The storage subsystem must take the following limitations into account:

- internal Flash has limited capacity;
- internal Flash must not be used as a large user-database store;
- PSRAM is volatile;
- PSRAM has higher access latency than internal SRAM;
- microSD is removable;
- microSD may be unavailable during startup;
- microSD may be removed or become unavailable;
- filesystem operations may fail and must be handled explicitly.

The board documentation explicitly notes the limited Flash capacity and the
 higher access latency of PSRAM. 

------

# Storage and Power Loss

Persistent data must be written through the storage service.

The application must not assume that a write has completed merely because a
 logical save operation was requested.

Storage operations must report their completion or failure to the calling
 service.

This is particularly important for:

- vault updates;
- settings;
- backups;
- restore operations;
- import operations;
- export operations.

------

# Storage Lifecycle

The normal storage lifecycle is:

```
Storage Init
     |
     v
NVS Ready
     |
     v
LittleFS Mounted
     |
     v
microSD Detected
     |
     v
Storage Ready
     |
     +---- Application Running
     |
     v
Flush / Save
     |
     v
Storage Shutdown
```

microSD availability may change independently of the internal storage.

------

# Storage Shutdown

During controlled shutdown or restart:

```
Save Settings
     |
     v
Flush Storage
     |
     v
Unmount Filesystems
     |
     v
Display Sleep
     |
     v
Restart / Power Off
```

The architecture explicitly defines saving settings and flushing storage as part
 of the shutdown sequence. 

------

# Design Rules

The following rules apply to the KeyKeeper2 storage subsystem:

1. Settings must always be stored in NVS.
2. LittleFS is the default internal filesystem.
3. microSD is an optional removable storage medium.
4. microSD must be implemented using the board's 4-bit interface.
5. `vault.db` must be accessed through `VaultService`.
6. GUI code must never access NVS directly.
7. GUI code must never access LittleFS directly.
8. GUI code must never access microSD directly.
9. Storage hardware details must remain inside the storage/BSP layers.
10. PSRAM must not be treated as persistent storage.
11. Internal Flash must not be used as a large user-database store.
12. The absence of microSD must not prevent normal device operation.
13. Storage errors must be reported explicitly.
14. Backup and restore operations belong to `VaultService`.
15. Import and export operations belong to `VaultService`.
16. Cryptographic operations must remain outside the storage driver.
17. Changes to storage hardware or filesystem architecture must be reflected in
     `01_board.md`, `05_storage.md` and `06_interfaces.md`.

------

# Related Documents

| Document                 | Description                                |
| ------------------------ | ------------------------------------------ |
| `01_board.md`            | Board and hardware platform                |
| `02_display.md`          | LCD and display subsystem                  |
| `03_input.md`            | EC11 encoder and buttons                   |
| `04_power.md`            | Power subsystem                            |
| `06_interfaces.md`       | GPIO allocation and hardware interfaces    |
| `07_boot.md`             | Boot process                               |
| `08_revision_history.md` | Documentation and project revision history |

------

# Summary

The KeyKeeper2 storage architecture separates persistent settings, internal
 filesystem data, removable user data and runtime memory.

The definitive storage model is:

```
NVS
 |
 +-- Settings


LittleFS
 |
 +-- System files
 +-- Internal application data
 +-- /vault/
      |
      +-- vault.db
      +-- backup/
      +-- import/
      +-- export/


microSD
 |
 +-- Optional removable vault storage
 +-- Backups
 +-- Import
 +-- Export


PSRAM
 |
 +-- Runtime buffers
 +-- LVGL buffers
 +-- Temporary data
 +-- Cache
```

The project architecture defines **LittleFS as the default filesystem** and
 **microSD as optional removable storage** for vault and data-exchange
 operations. Settings are always stored in NVS. 

The storage subsystem is divided into hardware drivers and higher-level
 services.

```
Application
     |
     v
VaultService / SettingsService
     |
     v
StorageService
     |
     +----------+----------+
     |                     |
     v                     v
   LittleFS              microSD


SettingsService
     |
     v
    NVS
```

The GUI does not access storage hardware or files directly.

The storage subsystem must remain independent of the physical location of the
 vault data and must provide reliable handling of removable microSD storage,
 filesystem errors and persistent-data operations.

The storage hardware resources are reserved by the board and must remain
 centralized in the BSP and interface documentation.

The storage configuration described in this document is the KeyKeeper2
 baseline and must remain synchronized with `01_board.md`,
 `06_interfaces.md` and the firmware storage components.
