#include "storage/sdcard.hpp"

#include "storage/storage_paths.hpp"

#include "bsp/pins.hpp"

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <cerrno>
#include <sys/stat.h>

namespace storage::sd {

namespace {

constexpr char TAG[] = "storage.sd";

sdmmc_card_t* card = nullptr;
bool mounted = false;

void ensure_dir(const char* path)
{
    struct stat st{};
    if (::stat(path, &st) == 0) {
        return;
    }
    if (::mkdir(path, 0755) != 0) {
        ESP_LOGW(TAG, "mkdir('%s') failed (errno %d)", path, errno);
    }
}

void ensure_vault_dirs()
{
    ensure_dir("/sdcard/vault");
    ensure_dir(paths::VAULT_BACKUP_DIR);
    ensure_dir(paths::VAULT_IMPORT_DIR);
    ensure_dir(paths::VAULT_EXPORT_DIR);
}

bool do_mount()
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.clk = bsp::pins::SD_CLK;
    slot_config.cmd = bsp::pins::SD_CMD;
    slot_config.d0 = bsp::pins::SD_D0;
    slot_config.d1 = bsp::pins::SD_D1;
    slot_config.d2 = bsp::pins::SD_D2;
    slot_config.d3 = bsp::pins::SD_D3;
    // Internal pull-ups even if the board also has external ones --
    // harmless if redundant, and cheap insurance against a marginal
    // signal if it doesn't.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_config{};
    // Deliberately false: this is removable media that may already
    // contain the user's own files. Silently reformatting it on a
    // mount failure would be destructive -- unlike the internal
    // LittleFS partition, which is ours alone.
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;

    const esp_err_t err = esp_vfs_fat_sdmmc_mount(
        paths::SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            ESP_LOGW(TAG, "SD card mount failed (bad filesystem or card absent)");
        } else {
            ESP_LOGI(TAG, "No SD card detected (%s)", esp_err_to_name(err));
        }
        card = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "SD card mounted at %s", paths::SDCARD_MOUNT_POINT);
    ensure_vault_dirs();
    return true;
}

} // namespace

bool init()
{
    mounted = do_mount();
    return mounted;
}

bool is_mounted()
{
    return mounted;
}

void unmount()
{
    if (!mounted) {
        return;
    }

    esp_vfs_fat_sdcard_unmount(paths::SDCARD_MOUNT_POINT, card);
    card = nullptr;
    mounted = false;
    ESP_LOGI(TAG, "SD card unmounted");
}

bool remount()
{
    unmount();
    mounted = do_mount();
    return mounted;
}

bool get_usage(uint64_t& total_bytes, uint64_t& used_bytes)
{
    if (!mounted) {
        return false;
    }

    uint64_t free_bytes = 0;
    if (esp_vfs_fat_info(paths::SDCARD_MOUNT_POINT, &total_bytes, &free_bytes) != ESP_OK) {
        return false;
    }

    used_bytes = total_bytes - free_bytes;
    return true;
}

} // namespace storage::sd
