#include "vault/vault_backup.hpp"

#include "vault/vault.hpp"

#include "security/lock_manager.hpp"
#include "storage/storage.hpp"
#include "storage/storage_paths.hpp"
#include "storage/vaultfile.hpp"

#include "esp_log.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

namespace vault::backup {

namespace {

constexpr char TAG[] = "vault.backup";
constexpr char PREFIX[] = "vault_";
constexpr char SUFFIX[] = ".bak";

bool is_unlocked()
{
    return security::lock::state() == security::lock::State::Unlocked;
}

bool build_path(const char* filename, char* out, size_t out_size)
{
    const int n = std::snprintf(out, out_size, "%s/%s", storage::paths::VAULT_BACKUP_DIR, filename);
    return n > 0 && static_cast<size_t>(n) < out_size;
}

/**
 * Scans existing backup filenames for the highest "vault_NNN.bak"
 * index, so a fresh one can be numbered NNN+1 -- see vault_backup.hpp
 * for why this isn't a real timestamp.
 */
uint32_t next_backup_index()
{
    uint32_t highest = 0;

    DIR* dir = opendir(storage::paths::VAULT_BACKUP_DIR);
    if (dir == nullptr) {
        return 1; // directory doesn't exist yet -- first backup
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        unsigned index = 0;
        if (std::sscanf(entry->d_name, "vault_%u.bak", &index) == 1) {
            if (index > highest) {
                highest = index;
            }
        }
    }
    closedir(dir);

    return highest + 1;
}

} // namespace

bool create_backup(char* out_filename, size_t out_filename_size)
{
    if (!is_unlocked()) {
        ESP_LOGE(TAG, "create_backup: vault is locked");
        return false;
    }
    if (!storage::is_initialized() || !storage::status().sdcard_present) {
        ESP_LOGE(TAG, "create_backup: no SD card present");
        return false;
    }

    const size_t size = storage::vaultfile::size();

    std::vector<uint8_t> buffer(size);
    size_t actual = size;
    if (size > 0 && !storage::vaultfile::read_all(buffer.data(), actual)) {
        ESP_LOGE(TAG, "create_backup: failed to read vault.db");
        return false;
    }

    char filename[32];
    std::snprintf(filename, sizeof(filename), "%s%03u%s", PREFIX,
                   static_cast<unsigned>(next_backup_index()), SUFFIX);

    char full_path[96];
    if (!build_path(filename, full_path, sizeof(full_path))) {
        ESP_LOGE(TAG, "create_backup: path too long");
        return false;
    }

    FILE* f = std::fopen(full_path, "wb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "create_backup: fopen failed for '%s'", full_path);
        return false;
    }

    const size_t written = (size > 0) ? std::fwrite(buffer.data(), 1, size, f) : 0;
    std::fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "create_backup: short write (%u of %u bytes)",
                   static_cast<unsigned>(written), static_cast<unsigned>(size));
        std::remove(full_path);
        return false;
    }

    if (out_filename != nullptr && out_filename_size > 0) {
        std::strncpy(out_filename, filename, out_filename_size - 1);
        out_filename[out_filename_size - 1] = '\0';
    }

    ESP_LOGI(TAG, "Backup created: %s (%u bytes)", filename, static_cast<unsigned>(size));
    return true;
}

size_t list_backups(BackupInfo* out, size_t max_count)
{
    if (out == nullptr || max_count == 0) {
        return 0;
    }
    if (!storage::is_initialized() || !storage::status().sdcard_present) {
        return 0;
    }

    DIR* dir = opendir(storage::paths::VAULT_BACKUP_DIR);
    if (dir == nullptr) {
        return 0;
    }

    size_t count = 0;
    struct dirent* entry = nullptr;
    while (count < max_count && (entry = readdir(dir)) != nullptr) {
        unsigned index = 0;
        if (std::sscanf(entry->d_name, "vault_%u.bak", &index) != 1) {
            continue; // not one of ours -- skip
        }

        char full_path[96];
        if (!build_path(entry->d_name, full_path, sizeof(full_path))) {
            continue;
        }

        struct stat st{};
        if (::stat(full_path, &st) != 0) {
            continue;
        }

        std::strncpy(out[count].filename, entry->d_name, sizeof(out[count].filename) - 1);
        out[count].filename[sizeof(out[count].filename) - 1] = '\0';
        out[count].size_bytes = static_cast<size_t>(st.st_size);
        ++count;
    }
    closedir(dir);

    // Most-recently-created first -- filenames sort numerically by
    // index since they're all the same fixed-width "vault_NNN.bak"
    // shape, so a simple reverse of directory-listing order is
    // enough (FAT's readdir() order is typically creation order, so
    // newest-created backup -- highest index -- tends to come last).
    for (size_t i = 0; i < count / 2; ++i) {
        BackupInfo tmp = out[i];
        out[i] = out[count - 1 - i];
        out[count - 1 - i] = tmp;
    }

    return count;
}

bool restore_backup(const char* filename)
{
    if (filename == nullptr || filename[0] == '\0') {
        return false;
    }
    if (!is_unlocked()) {
        ESP_LOGE(TAG, "restore_backup: vault is locked");
        return false;
    }
    if (!storage::is_initialized() || !storage::status().sdcard_present) {
        ESP_LOGE(TAG, "restore_backup: no SD card present");
        return false;
    }

    char full_path[96];
    if (!build_path(filename, full_path, sizeof(full_path))) {
        ESP_LOGE(TAG, "restore_backup: path too long");
        return false;
    }

    struct stat st{};
    if (::stat(full_path, &st) != 0) {
        ESP_LOGE(TAG, "restore_backup: '%s' not found", filename);
        return false;
    }
    const size_t size = static_cast<size_t>(st.st_size);

    std::vector<uint8_t> buffer(size);
    FILE* f = std::fopen(full_path, "rb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "restore_backup: fopen failed for '%s'", full_path);
        return false;
    }
    const size_t read = std::fread(buffer.data(), 1, size, f);
    std::fclose(f);

    if (read != size) {
        ESP_LOGE(TAG, "restore_backup: short read (%u of %u bytes)",
                   static_cast<unsigned>(read), static_cast<unsigned>(size));
        return false;
    }

    if (!storage::vaultfile::write_all(buffer.data(), size)) {
        ESP_LOGE(TAG, "restore_backup: failed to write vault.db");
        return false;
    }

    ESP_LOGI(TAG, "Restored vault.db from %s (%u bytes) -- caller must restart the device now",
               filename, static_cast<unsigned>(size));
    return true;
}

bool delete_backup(const char* filename)
{
    if (filename == nullptr || filename[0] == '\0') {
        return false;
    }

    char full_path[96];
    if (!build_path(filename, full_path, sizeof(full_path))) {
        return false;
    }

    return std::remove(full_path) == 0;
}

} // namespace vault::backup
