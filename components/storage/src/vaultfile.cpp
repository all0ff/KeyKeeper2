#include "storage/vaultfile.hpp"

#include "storage/storage_paths.hpp"

#include "esp_log.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace storage::vaultfile {

namespace {

constexpr char TAG[] = "storage.vaultfile";

// Same directory as vault.db, so the rename() in write_all() is
// guaranteed to be on the same filesystem (a cross-filesystem rename
// would silently fall back to copy+delete on some VFS drivers, losing
// the crash-safety this is here for).
constexpr char TEMP_PATH[] = "/data/vault/vault.db.tmp";

} // namespace

bool exists()
{
    struct stat st{};
    return ::stat(paths::VAULT_DB, &st) == 0;
}

size_t size()
{
    struct stat st{};
    if (::stat(paths::VAULT_DB, &st) != 0) {
        return 0;
    }
    return static_cast<size_t>(st.st_size);
}

bool read_all(uint8_t* out, size_t& inout_size)
{
    struct stat st{};
    if (::stat(paths::VAULT_DB, &st) != 0) {
        ESP_LOGW(TAG, "read_all: %s does not exist", paths::VAULT_DB);
        inout_size = 0;
        return false;
    }

    const size_t actual_size = static_cast<size_t>(st.st_size);
    const size_t capacity = inout_size;
    inout_size = actual_size;

    if (actual_size > capacity) {
        ESP_LOGE(TAG, "read_all: buffer too small (need %u, have %u)",
                 static_cast<unsigned>(actual_size), static_cast<unsigned>(capacity));
        return false;
    }

    FILE* f = ::fopen(paths::VAULT_DB, "rb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "read_all: fopen failed");
        return false;
    }

    const size_t read_bytes = ::fread(out, 1, actual_size, f);
    ::fclose(f);

    if (read_bytes != actual_size) {
        ESP_LOGE(TAG, "read_all: short read (%u of %u bytes)",
                 static_cast<unsigned>(read_bytes), static_cast<unsigned>(actual_size));
        return false;
    }

    return true;
}

bool write_all(const uint8_t* data, size_t size)
{
    FILE* f = ::fopen(TEMP_PATH, "wb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "write_all: fopen(temp) failed");
        return false;
    }

    const size_t written = ::fwrite(data, 1, size, f);

    // Push to the LittleFS driver before closing -- fclose() alone
    // isn't guaranteed to be as thorough about flushing on every VFS
    // driver, and this file is worth the extra care.
    ::fflush(f);
    ::fsync(fileno(f));
    ::fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "write_all: short write (%u of %u bytes), aborting",
                 static_cast<unsigned>(written), static_cast<unsigned>(size));
        ::remove(TEMP_PATH);
        return false;
    }

    if (::rename(TEMP_PATH, paths::VAULT_DB) != 0) {
        ESP_LOGE(TAG, "write_all: rename(temp -> vault.db) failed");
        ::remove(TEMP_PATH);
        return false;
    }

    return true;
}

bool remove()
{
    return ::remove(paths::VAULT_DB) == 0;
}

} // namespace storage::vaultfile
