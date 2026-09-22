#include "vault/vault_csv.hpp"

#include "vault/vault.hpp"

#include "security/lock_manager.hpp"
#include "storage/storage.hpp"
#include "storage/storage_paths.hpp"

#include "esp_log.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace vault::csv {

namespace {

constexpr char TAG[] = "vault.csv";
constexpr char EXPORT_FILENAME[] = "vault_export.csv";

bool is_unlocked()
{
    return security::lock::state() == security::lock::State::Unlocked;
}

bool build_export_path(char* out, size_t out_size)
{
    const int n = std::snprintf(out, out_size, "%s/%s", storage::paths::VAULT_EXPORT_DIR, EXPORT_FILENAME);
    return n > 0 && static_cast<size_t>(n) < out_size;
}

bool build_import_path(const char* filename, char* out, size_t out_size)
{
    const int n = std::snprintf(out, out_size, "%s/%s", storage::paths::VAULT_IMPORT_DIR, filename);
    return n > 0 && static_cast<size_t>(n) < out_size;
}

// -----------------------------------------------------------------
// Writing
// -----------------------------------------------------------------

void write_csv_field(FILE* f, const std::string& value, bool last)
{
    const bool needs_quotes = value.find_first_of(",\"\r\n") != std::string::npos;

    if (needs_quotes) {
        std::fputc('"', f);
        for (char c : value) {
            if (c == '"') {
                std::fputc('"', f); // RFC 4180: an embedded quote is doubled
            }
            std::fputc(c, f);
        }
        std::fputc('"', f);
    } else {
        std::fputs(value.c_str(), f);
    }

    std::fputc(last ? '\n' : ',', f);
}

void write_csv_row(FILE* f, const vault::VaultEntry& e)
{
    write_csv_field(f, e.login, false);
    write_csv_field(f, e.password, false);
    write_csv_field(f, e.url, false);
    write_csv_field(f, e.notes, false);
    write_csv_field(f, e.totp_secret, false);
    write_csv_field(f, e.category, false);
    write_csv_field(f, e.favorite ? "1" : "0", true);
}

// -----------------------------------------------------------------
// Reading
// -----------------------------------------------------------------

/**
 * @brief Read one logical CSV row from f (RFC 4180-style: a quoted
 *        field may contain literal commas and newlines).
 *
 * @return false only when there was nothing left to read at all (true
 *         EOF with no partial row) -- a final row with no trailing
 *         newline still returns true.
 */
bool read_csv_row(FILE* f, std::vector<std::string>& fields)
{
    fields.clear();
    std::string field;
    bool in_quotes = false;
    bool any_char_read = false;

    int c;
    while ((c = std::fgetc(f)) != EOF) {
        any_char_read = true;

        if (in_quotes) {
            if (c == '"') {
                const int next = std::fgetc(f);
                if (next == '"') {
                    field += '"'; // escaped quote
                } else {
                    in_quotes = false;
                    if (next != EOF) {
                        std::ungetc(next, f);
                    }
                }
            } else {
                field += static_cast<char>(c);
            }
            continue;
        }

        if (c == '"' && field.empty()) {
            in_quotes = true;
            continue;
        }
        if (c == ',') {
            fields.push_back(field);
            field.clear();
            continue;
        }
        if (c == '\r') {
            continue; // swallow -- \n (below) ends the row either way
        }
        if (c == '\n') {
            fields.push_back(field);
            return true;
        }

        field += static_cast<char>(c);
    }

    if (any_char_read) {
        fields.push_back(field); // last row, no trailing newline
        return true;
    }
    return false;
}

enum class Column : uint8_t
{
    Login,
    Password,
    Url,
    Notes,
    TotpSecret,
    Category,
    Favorite,
    Unknown,
};

std::string trim_lower(const std::string& raw)
{
    std::string h;
    h.reserve(raw.size());
    for (char c : raw) {
        h += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    while (!h.empty() && std::isspace(static_cast<unsigned char>(h.front()))) {
        h.erase(h.begin());
    }
    while (!h.empty() && std::isspace(static_cast<unsigned char>(h.back()))) {
        h.pop_back();
    }
    return h;
}

/**
 * @brief Map a CSV header cell to one of VaultEntry's fields, leniently
 *        -- see vault_csv.hpp's file comment for why (interop with
 *        other apps' own CSV exports, not just this project's own
 *        header row).
 */
Column column_from_header(const std::string& raw)
{
    const std::string h = trim_lower(raw);

    // "name" included -- this project's own AccountEditScreen already
    // treats Name/Username as the same field (VaultEntry::login), so
    // an export from something that calls it "Name" maps the same way.
    if (h == "login" || h == "username" || h == "user" || h == "name") return Column::Login;
    if (h == "password" || h == "pass") return Column::Password;
    if (h == "url" || h == "website" || h == "site" || h == "uri") return Column::Url;
    if (h == "notes" || h == "note" || h == "comment" || h == "comments") return Column::Notes;
    if (h == "totp_secret" || h == "totp" || h == "otp" || h == "otp_secret" || h == "otpauth") return Column::TotpSecret;
    if (h == "category" || h == "folder" || h == "group") return Column::Category;
    if (h == "favorite" || h == "favourite" || h == "fav" || h == "starred") return Column::Favorite;
    return Column::Unknown;
}

bool parse_bool(const std::string& raw)
{
    const std::string v = trim_lower(raw);
    return v == "1" || v == "true" || v == "yes" || v == "y";
}

} // namespace

bool export_csv(char* out_filename, size_t out_filename_size)
{
    if (!is_unlocked()) {
        ESP_LOGE(TAG, "export_csv: vault is locked");
        return false;
    }
    if (!storage::is_initialized() || !storage::status().sdcard_present) {
        ESP_LOGE(TAG, "export_csv: no SD card present");
        return false;
    }

    char full_path[96];
    if (!build_export_path(full_path, sizeof(full_path))) {
        ESP_LOGE(TAG, "export_csv: path too long");
        return false;
    }

    FILE* f = std::fopen(full_path, "wb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "export_csv: fopen failed for '%s'", full_path);
        return false;
    }

    std::fputs("login,password,url,notes,totp_secret,category,favorite\n", f);

    // Heap-allocated, NOT a stack array -- vault::VaultEntry holds
    // several std::string/std::vector members (recovery_codes,
    // seed_phrase), and PAGE (16) of those on the "ui" task's own
    // stack overflowed it outright on real hardware (confirmed via a
    // "stack overflow in task ui" panic + reboot triggered by this
    // exact export). Same class of bug already caught and fixed
    // elsewhere in this project (web_vault_routes.cpp's own
    // handle_search(), see that file's comment) -- this one predates
    // that fix and wasn't caught at the time.
    constexpr size_t PAGE = 16;
    std::vector<vault::VaultEntry> buf(PAGE);
    size_t offset = 0;
    size_t total_written = 0;

    while (true) {
        const size_t n = vault::list_entries(buf.data(), PAGE, offset);
        if (n == 0) {
            break;
        }
        for (size_t i = 0; i < n; ++i) {
            write_csv_row(f, buf[i]);
        }
        total_written += n;
        offset += n;
        if (n < PAGE) {
            break; // last page
        }
    }

    const bool write_ok = std::ferror(f) == 0;
    std::fclose(f);

    if (!write_ok) {
        ESP_LOGE(TAG, "export_csv: write error");
        std::remove(full_path);
        return false;
    }

    if (out_filename != nullptr && out_filename_size > 0) {
        std::strncpy(out_filename, EXPORT_FILENAME, out_filename_size - 1);
        out_filename[out_filename_size - 1] = '\0';
    }

    ESP_LOGI(TAG, "Exported %u entries to %s", static_cast<unsigned>(total_written), EXPORT_FILENAME);
    return true;
}

ImportResult import_csv(const char* filename)
{
    ImportResult result;

    if (filename == nullptr || filename[0] == '\0') {
        return result;
    }
    if (!is_unlocked()) {
        ESP_LOGE(TAG, "import_csv: vault is locked");
        return result;
    }
    if (!storage::is_initialized() || !storage::status().sdcard_present) {
        ESP_LOGE(TAG, "import_csv: no SD card present");
        return result;
    }

    char full_path[96];
    if (!build_import_path(filename, full_path, sizeof(full_path))) {
        ESP_LOGE(TAG, "import_csv: path too long");
        return result;
    }

    FILE* f = std::fopen(full_path, "rb");
    if (f == nullptr) {
        ESP_LOGE(TAG, "import_csv: fopen failed for '%s'", full_path);
        return result;
    }

    std::vector<std::string> header;
    if (!read_csv_row(f, header)) {
        ESP_LOGW(TAG, "import_csv: empty file");
        std::fclose(f);
        return result;
    }

    std::vector<Column> columns;
    columns.reserve(header.size());
    for (const std::string& cell : header) {
        columns.push_back(column_from_header(cell));
    }

    std::vector<std::string> row;
    while (read_csv_row(f, row)) {
        if (row.size() == 1 && row[0].empty()) {
            continue; // a lone trailing blank line -- not a real row
        }

        vault::VaultEntry entry;
        for (size_t i = 0; i < row.size() && i < columns.size(); ++i) {
            switch (columns[i]) {
                case Column::Login:      entry.login = row[i]; break;
                case Column::Password:   entry.password = row[i]; break;
                case Column::Url:        entry.url = row[i]; break;
                case Column::Notes:      entry.notes = row[i]; break;
                case Column::TotpSecret: entry.totp_secret = row[i]; break;
                case Column::Category:   entry.category = row[i]; break;
                case Column::Favorite:   entry.favorite = parse_bool(row[i]); break;
                case Column::Unknown:    break;
            }
        }

        // Matches AccountEditScreen's own "Name must not be empty"
        // rule (GUI.md 12) -- vault::validate() itself only checks
        // length limits, not emptiness (see vault_model.cpp).
        if (entry.login.empty() || !vault::validate(entry)) {
            ++result.skipped;
            continue;
        }

        if (vault::create_entry(entry) != vault::INVALID_ID) {
            ++result.created;
        } else {
            ++result.skipped;
        }
    }

    std::fclose(f);
    ESP_LOGI(TAG, "Imported from %s: %u created, %u skipped", filename,
               static_cast<unsigned>(result.created), static_cast<unsigned>(result.skipped));
    return result;
}

size_t list_import_files(ImportFileInfo* out, size_t max_count)
{
    if (out == nullptr || max_count == 0) {
        return 0;
    }
    if (!storage::is_initialized() || !storage::status().sdcard_present) {
        return 0;
    }

    DIR* dir = opendir(storage::paths::VAULT_IMPORT_DIR);
    if (dir == nullptr) {
        return 0;
    }

    size_t count = 0;
    struct dirent* entry = nullptr;
    while (count < max_count && (entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue; // skip "." / ".." and any dotfiles
        }

        char full_path[96];
        const int n = std::snprintf(full_path, sizeof(full_path), "%s/%s", storage::paths::VAULT_IMPORT_DIR,
                                      entry->d_name);
        if (n <= 0 || static_cast<size_t>(n) >= sizeof(full_path)) {
            continue;
        }

        struct stat st{};
        if (::stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue; // skip subdirectories etc.
        }

        std::strncpy(out[count].filename, entry->d_name, sizeof(out[count].filename) - 1);
        out[count].filename[sizeof(out[count].filename) - 1] = '\0';
        out[count].size_bytes = static_cast<size_t>(st.st_size);
        ++count;
    }
    closedir(dir);

    return count;
}

} // namespace vault::csv
