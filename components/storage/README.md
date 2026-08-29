# components/storage

## Action required before `idf.py build`

1. **Custom partition table.** A `partitions.csv` was added at the
   **project root** (next to the top-level `CMakeLists.txt`), sized
   for 16 MB flash: `nvs` (32K) + `phy_init` (4K) + `factory` app (3M)
   + `storage` LittleFS data (4M) — see the file for the exact layout
   and reasoning. This needs two lines added to `sdkconfig.defaults`
   (see the separate patch I'm sending) so ESP-IDF actually uses it
   instead of the tiny built-in default table:
   ```
   CONFIG_PARTITION_TABLE_CUSTOM=y
   CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
   ```
   **This changes the flash layout.** If you'd already flashed
   anything with the old (default) partition table, do a full erase
   before the next flash: `idf.py erase-flash`, then `idf.py flash`.
2. **LittleFS** is pulled automatically via `idf_component.yml`
   (`joltwallet/littlefs`), same mechanism as `lvgl` in
   `components/display` — needs network on first build.

## Design decisions made here (not fully specified in the plan)

- The plan listed 4 paths (`/vault/vault.db`, `.../backup/`,
  `.../import/`, `.../export/`) without saying which physical backend
  each belongs to. Decision: `vault.db` on internal LittleFS (must
  always work, no card required); backup/import/export on microSD
  (removable, meant to be read on a PC). See the comment block at the
  top of `storage_paths.hpp`.
- microSD is mounted with `format_if_mount_failed = false`. Unlike the
  internal LittleFS partition (which is exclusively ours), the SD card
  may already hold the user's own files -- auto-formatting it on a
  mount failure would be destructive. A failed/absent card just means
  `storage::status().sdcard_present == false`.
- No card-detect GPIO exists on this board, so card insertion/removal
  can't be detected automatically. `storage::refresh_sdcard()` is
  meant to be wired to a UI action ("refresh SD card") later.

## Please verify on first build

- **`sdkconfig.defaults` currently has two lines that are silently
  ignored** (confirmed by the "unknown kconfig symbol" NOTE in your
  earlier build log): `CONFIG_LITTLEFS=y` and
  `CONFIG_SDMMC_HOST_SUPPORTS_8BIT=y`. Both are removed in the patch I
  'm sending -- the first does nothing (there's no such bare toggle;
  including the littlefs component in the build is what matters), and
  the second is actively wrong for this board (we use 4-bit SDMMC, not
  8-bit -- only 6 SD pins are wired: CLK/CMD/D0-D3).
- **REQUIRES for the SDMMC driver** (`esp_driver_sdmmc`, `sdmmc`,
  `fatfs`, `vfs`) — same story as `esp_driver_gpio`/`esp_driver_pcnt`
  earlier: component names for storage/SD peripherals have moved
  around across ESP-IDF versions. If the build complains about a
  missing component, check the exact name reported and swap it in
  here.
- **`littlefs` as the PRIV_REQUIRES target name** for the vendored
  `joltwallet/littlefs` package -- I'm fairly confident but haven't
  been able to confirm against your exact installed version. If CMake
  can't find it, look in `managed_components/` after the first
  `idf.py build` attempt for the actual folder/component name.
- **`esp_vfs_fat_info()`** (used in `sdcard.cpp` for usage
  reporting) -- exists in current ESP-IDF, but if it's missing on your
  version, delete/stub `storage::sd::get_usage()`; nothing else in
  this component depends on it.
- **microSD D1/D2 pins**: still not resolved from the earlier finding
  (your `pins.hpp` has D1=17/D2=18, USBArmyKnife's reference has them
  swapped). `sdcard.cpp` just uses `bsp::pins::SD_D1`/`SD_D2` as
  defined, so fixing this is a one-line change in `pins.hpp` if it
  turns out to be wrong -- but please check against the actual
  Waveshare schematic (or just try it: a wrong D1/D2 mapping usually
  shows up immediately as "no SD card detected" or garbled
  reads/writes, not a compile error) before trusting SD card
  read/write on real hardware.
