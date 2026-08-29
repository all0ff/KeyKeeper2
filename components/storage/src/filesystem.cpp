#include "storage/filesystem.hpp"

#include "storage/storage_paths.hpp"

#include "esp_littlefs.h"
#include "esp_log.h"

#include <cerrno>
#include <sys/stat.h>

namespace storage::fs {

namespace {

constexpr char TAG[] = "storage.fs";

// Must match the partition label in partitions.csv.
constexpr char PARTITION_LABEL[] = "storage";

bool initialized = false;

void ensure_dir(const char* path)
{
    struct stat st{};
    if (::stat(path, &st) == 0) {
        return; // already exists
    }

    if (::mkdir(path, 0755) != 0) {
        ESP_LOGW(TAG, "mkdir('%s') failed (errno %d)", path, errno);
    }
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    esp_vfs_littlefs_conf_t conf{};
    conf.base_path = paths::INTERNAL_MOUNT_POINT;
    conf.partition_label = PARTITION_LABEL;
    conf.format_if_mount_failed = true;
    conf.dont_mount = false;

    const esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_littlefs_register failed: %s", esp_err_to_name(err));
        return false;
    }

    size_t total = 0;
    size_t used = 0;
    if (esp_littlefs_info(PARTITION_LABEL, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted at %s: %u/%u bytes used",
                 paths::INTERNAL_MOUNT_POINT, static_cast<unsigned>(used), static_cast<unsigned>(total));
    } else {
        ESP_LOGW(TAG, "LittleFS mounted, but esp_littlefs_info failed (non-fatal)");
    }

    // So callers can fopen(paths::VAULT_DB, ...) directly without
    // having to create parent directories themselves.
    ensure_dir("/data/vault");

    initialized = true;
    return true;
}

bool is_initialized()
{
    return initialized;
}

bool get_usage(size_t& total_bytes, size_t& used_bytes)
{
    if (!initialized) {
        return false;
    }
    return esp_littlefs_info(PARTITION_LABEL, &total_bytes, &used_bytes) == ESP_OK;
}

} // namespace storage::fs
