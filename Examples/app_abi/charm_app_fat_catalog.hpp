#pragma once

#include "charm_app_received_image.hpp"
#include "charm_app_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::app_abi {

inline constexpr std::uint32_t kFirmwareCatalogMaxEntries = 16U;
inline constexpr std::uint32_t kFirmwareCatalogMaxName = 48U;
inline constexpr std::uint32_t kFirmwareCatalogMaxPath = 96U;
inline constexpr std::uint32_t kFirmwareCatalogSectorSize = 512U;
inline constexpr std::string_view kFirmwareCatalogDefaultDirectory = "/CHARM/APPS";

enum class FirmwareCatalogCode : std::uint8_t {
    ok,
    invalid_argument,
    storage_unavailable,
    fat_invalid,
    directory_missing,
    no_firmware,
    too_many_entries,
    file_not_found,
    file_read_failed,
    image_too_large,
    name_too_long,
};

enum class FirmwareImageSource : std::uint8_t {
    emmc_fat,
};

struct FirmwareEntry {
    std::array<char, kFirmwareCatalogMaxName> name{};
    std::array<char, kFirmwareCatalogMaxPath> path{};
    AppImageFormat format{AppImageFormat::elf};
    FirmwareImageSource source{FirmwareImageSource::emmc_fat};
    std::uint32_t size{0};
    std::uint32_t first_cluster{0};
};

struct FirmwareCatalogResult {
    FirmwareCatalogCode code{FirmwareCatalogCode::ok};
    std::uint32_t count{0};
};

struct FirmwareFileStageResult {
    FirmwareCatalogCode code{FirmwareCatalogCode::ok};
    AppImage image{};
    std::uint32_t bytes_read{0};
};

struct FatBlockReader {
    void* ctx{nullptr};
    bool (*read)(void* ctx, std::uint32_t lba, std::span<std::byte> bytes) noexcept{nullptr};
    std::uint32_t block_count{0};
};

struct FirmwareCatalog {
    std::array<FirmwareEntry, kFirmwareCatalogMaxEntries> entries{};
    std::uint32_t count{0};
};

struct FatVolume {
    FatBlockReader reader{};
    std::uint32_t partition_lba{0};
    std::uint32_t bytes_per_sector{0};
    std::uint32_t sectors_per_cluster{0};
    std::uint32_t reserved_sectors{0};
    std::uint32_t fat_count{0};
    std::uint32_t fat_size_sectors{0};
    std::uint32_t root_cluster{0};
    std::uint32_t first_fat_lba{0};
    std::uint32_t first_data_lba{0};
};

struct FatDirectoryEntry {
    std::array<char, kFirmwareCatalogMaxName> name{};
    std::uint32_t first_cluster{0};
    std::uint32_t size{0};
    bool directory{false};
};

[[nodiscard]] constexpr std::string_view firmware_catalog_code_name(
    FirmwareCatalogCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case FirmwareCatalogCode::ok:
            return "ok"sv;
        case FirmwareCatalogCode::invalid_argument:
            return "invalid_argument"sv;
        case FirmwareCatalogCode::storage_unavailable:
            return "storage_unavailable"sv;
        case FirmwareCatalogCode::fat_invalid:
            return "fat_invalid"sv;
        case FirmwareCatalogCode::directory_missing:
            return "directory_missing"sv;
        case FirmwareCatalogCode::no_firmware:
            return "no_firmware"sv;
        case FirmwareCatalogCode::too_many_entries:
            return "too_many_entries"sv;
        case FirmwareCatalogCode::file_not_found:
            return "file_not_found"sv;
        case FirmwareCatalogCode::file_read_failed:
            return "file_read_failed"sv;
        case FirmwareCatalogCode::image_too_large:
            return "image_too_large"sv;
        case FirmwareCatalogCode::name_too_long:
            return "name_too_long"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::uint16_t fat_le16(const std::byte* p) noexcept {
    return static_cast<std::uint16_t>(std::uint8_t(p[0])) |
           static_cast<std::uint16_t>(std::uint8_t(p[1]) << 8U);
}

[[nodiscard]] constexpr std::uint32_t fat_le32(const std::byte* p) noexcept {
    return static_cast<std::uint32_t>(std::uint8_t(p[0])) |
           (static_cast<std::uint32_t>(std::uint8_t(p[1])) << 8U) |
           (static_cast<std::uint32_t>(std::uint8_t(p[2])) << 16U) |
           (static_cast<std::uint32_t>(std::uint8_t(p[3])) << 24U);
}

[[nodiscard]] constexpr bool fat_is_eoc(std::uint32_t cluster) noexcept {
    return (cluster & 0x0FFFFFF8U) == 0x0FFFFFF8U;
}

[[nodiscard]] constexpr bool fat_cluster_valid(std::uint32_t cluster) noexcept {
    return cluster >= 2U && cluster < 0x0FFFFFF0U;
}

[[nodiscard]] constexpr char fat_ascii_upper(char c) noexcept {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
}

[[nodiscard]] constexpr bool fat_char_equal_ci(char a, char b) noexcept {
    return fat_ascii_upper(a) == fat_ascii_upper(b);
}

[[nodiscard]] constexpr bool fat_path_equal_ci(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!fat_char_equal_ci(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool fat_ends_with_elf(std::string_view name) noexcept {
    return name.size() > 4U &&
           name[name.size() - 4U] == '.' &&
           fat_char_equal_ci(name[name.size() - 3U], 'E') &&
           fat_char_equal_ci(name[name.size() - 2U], 'L') &&
           fat_char_equal_ci(name[name.size() - 1U], 'F');
}

template <std::size_t N>
[[nodiscard]] inline std::string_view fat_array_view(const std::array<char, N>& text) noexcept {
    std::size_t len = 0;
    while (len < text.size() && text[len] != '\0') {
        ++len;
    }
    return {text.data(), len};
}

template <std::size_t N>
[[nodiscard]] inline bool fat_copy_text(std::array<char, N>& out,
                                        std::string_view text) noexcept {
    if (text.size() + 1U > out.size()) {
        return false;
    }
    out.fill('\0');
    if (!text.empty()) {
        std::memcpy(out.data(), text.data(), text.size());
    }
    return true;
}

[[nodiscard]] inline bool fat_read_sector(const FatBlockReader& reader,
                                          std::uint32_t lba,
                                          std::span<std::byte, kFirmwareCatalogSectorSize> out) noexcept {
    if (reader.read == nullptr || lba >= reader.block_count) {
        return false;
    }
    return reader.read(reader.ctx, lba, out);
}

[[nodiscard]] inline bool fat_mount(FatBlockReader reader,
                                    std::uint32_t partition_lba,
                                    FatVolume& out) noexcept {
    if (reader.read == nullptr || reader.block_count == 0U ||
        partition_lba >= reader.block_count) {
        return false;
    }

    std::array<std::byte, kFirmwareCatalogSectorSize> sector{};
    if (!fat_read_sector(reader, partition_lba, sector)) {
        return false;
    }
    if (std::uint8_t(sector[510]) != 0x55U || std::uint8_t(sector[511]) != 0xAAU) {
        return false;
    }

    const std::uint32_t bytes_per_sector = fat_le16(sector.data() + 11);
    const std::uint32_t sectors_per_cluster = std::uint8_t(sector[13]);
    const std::uint32_t reserved = fat_le16(sector.data() + 14);
    const std::uint32_t fats = std::uint8_t(sector[16]);
    const std::uint32_t root_entries = fat_le16(sector.data() + 17);
    const std::uint32_t total16 = fat_le16(sector.data() + 19);
    const std::uint32_t total32 = fat_le32(sector.data() + 32);
    const std::uint32_t fat16 = fat_le16(sector.data() + 22);
    const std::uint32_t fat32 = fat_le32(sector.data() + 36);
    const std::uint32_t root_cluster = fat_le32(sector.data() + 44);
    const std::uint32_t total = total32 != 0U ? total32 : total16;
    const std::uint32_t fat_size = fat32 != 0U ? fat32 : fat16;

    if (bytes_per_sector != kFirmwareCatalogSectorSize || sectors_per_cluster == 0U ||
        reserved == 0U || fats == 0U || fat_size == 0U || total == 0U ||
        root_entries != 0U || root_cluster < 2U) {
        return false;
    }
    if ((sectors_per_cluster & (sectors_per_cluster - 1U)) != 0U) {
        return false;
    }
    if (partition_lba + total > reader.block_count) {
        return false;
    }

    out = FatVolume{
        .reader = reader,
        .partition_lba = partition_lba,
        .bytes_per_sector = bytes_per_sector,
        .sectors_per_cluster = sectors_per_cluster,
        .reserved_sectors = reserved,
        .fat_count = fats,
        .fat_size_sectors = fat_size,
        .root_cluster = root_cluster,
        .first_fat_lba = partition_lba + reserved,
        .first_data_lba = partition_lba + reserved + (fats * fat_size),
    };
    return true;
}

[[nodiscard]] constexpr std::uint32_t fat_cluster_lba(const FatVolume& volume,
                                                       std::uint32_t cluster) noexcept {
    return volume.first_data_lba + ((cluster - 2U) * volume.sectors_per_cluster);
}

[[nodiscard]] inline bool fat_next_cluster(const FatVolume& volume,
                                           std::uint32_t cluster,
                                           std::uint32_t& out_next) noexcept {
    if (!fat_cluster_valid(cluster)) {
        return false;
    }
    const std::uint32_t fat_offset = cluster * 4U;
    const std::uint32_t sector_lba = volume.first_fat_lba + (fat_offset / kFirmwareCatalogSectorSize);
    const std::uint32_t entry_offset = fat_offset % kFirmwareCatalogSectorSize;
    std::array<std::byte, kFirmwareCatalogSectorSize> sector{};
    if (!fat_read_sector(volume.reader, sector_lba, sector)) {
        return false;
    }
    out_next = fat_le32(sector.data() + entry_offset) & 0x0FFFFFFFU;
    return true;
}

[[nodiscard]] inline bool fat_short_name(const std::byte* raw,
                                         std::array<char, kFirmwareCatalogMaxName>& out) noexcept {
    out.fill('\0');
    std::size_t cursor = 0;
    for (std::size_t i = 0; i < 8U; ++i) {
        const char c = static_cast<char>(std::uint8_t(raw[i]));
        if (c == ' ') {
            break;
        }
        if (cursor + 1U >= out.size()) {
            return false;
        }
        out[cursor++] = c;
    }
    bool has_ext = false;
    for (std::size_t i = 8U; i < 11U; ++i) {
        if (static_cast<char>(std::uint8_t(raw[i])) != ' ') {
            has_ext = true;
            break;
        }
    }
    if (has_ext) {
        if (cursor + 1U >= out.size()) {
            return false;
        }
        out[cursor++] = '.';
        for (std::size_t i = 8U; i < 11U; ++i) {
            const char c = static_cast<char>(std::uint8_t(raw[i]));
            if (c == ' ') {
                break;
            }
            if (cursor + 1U >= out.size()) {
                return false;
            }
            out[cursor++] = c;
        }
    }
    return cursor != 0U;
}

inline void fat_lfn_append_chars(const std::byte* entry,
                                 std::array<char, kFirmwareCatalogMaxName>& lfn,
                                 std::uint32_t& len) noexcept {
    constexpr std::uint8_t kOffsets[] = {
        1, 3, 5, 7, 9,
        14, 16, 18, 20, 22, 24,
        28, 30,
    };
    for (const auto offset : kOffsets) {
        const auto ch = fat_le16(entry + offset);
        if (ch == 0x0000U || ch == 0xFFFFU) {
            return;
        }
        if (ch > 0x7FU || len + 1U >= lfn.size()) {
            continue;
        }
        lfn[len++] = static_cast<char>(ch);
    }
}

[[nodiscard]] inline bool fat_parse_dir_entry(const std::byte* entry,
                                              const std::array<char, kFirmwareCatalogMaxName>& lfn,
                                              std::uint32_t lfn_len,
                                              FatDirectoryEntry& out) noexcept {
    const auto first = std::uint8_t(entry[0]);
    const auto attr = std::uint8_t(entry[11]);
    if (first == 0x00U || first == 0xE5U || attr == 0x0FU || (attr & 0x08U) != 0U) {
        return false;
    }

    std::array<char, kFirmwareCatalogMaxName> name{};
    if (lfn_len != 0U) {
        name = lfn;
    } else if (!fat_short_name(entry, name)) {
        return false;
    }

    const std::uint32_t cluster_hi = fat_le16(entry + 20);
    const std::uint32_t cluster_lo = fat_le16(entry + 26);
    out = FatDirectoryEntry{
        .name = name,
        .first_cluster = (cluster_hi << 16U) | cluster_lo,
        .size = fat_le32(entry + 28),
        .directory = (attr & 0x10U) != 0U,
    };
    return true;
}

template <class Visitor>
[[nodiscard]] inline bool fat_visit_directory(const FatVolume& volume,
                                              std::uint32_t start_cluster,
                                              Visitor&& visitor) noexcept {
    if (!fat_cluster_valid(start_cluster)) {
        return false;
    }

    std::uint32_t cluster = start_cluster;
    std::uint32_t guard = 0;
    std::array<std::byte, kFirmwareCatalogSectorSize> sector{};
    while (fat_cluster_valid(cluster) && !fat_is_eoc(cluster)) {
        if (++guard > 65536U) {
            return false;
        }
        const auto cluster_lba = fat_cluster_lba(volume, cluster);
        for (std::uint32_t s = 0; s < volume.sectors_per_cluster; ++s) {
            if (!fat_read_sector(volume.reader, cluster_lba + s, sector)) {
                return false;
            }
            std::array<char, kFirmwareCatalogMaxName> lfn{};
            std::uint32_t lfn_len = 0;
            for (std::uint32_t off = 0; off < kFirmwareCatalogSectorSize; off += 32U) {
                const auto* entry = sector.data() + off;
                const auto first = std::uint8_t(entry[0]);
                if (first == 0x00U) {
                    return true;
                }
                const auto attr = std::uint8_t(entry[11]);
                if (first == 0xE5U) {
                    lfn.fill('\0');
                    lfn_len = 0;
                    continue;
                }
                if (attr == 0x0FU) {
                    const auto order = std::uint8_t(entry[0]) & 0x1FU;
                    if ((std::uint8_t(entry[0]) & 0x40U) != 0U) {
                        lfn.fill('\0');
                        lfn_len = 0;
                    }
                    const std::uint32_t segment_len = (order > 0U) ? ((order - 1U) * 13U) : 0U;
                    std::uint32_t cursor = segment_len;
                    fat_lfn_append_chars(entry, lfn, cursor);
                    if (cursor > lfn_len) {
                        lfn_len = cursor;
                    }
                    continue;
                }

                FatDirectoryEntry parsed{};
                if (fat_parse_dir_entry(entry, lfn, lfn_len, parsed)) {
                    if (!visitor(parsed)) {
                        return true;
                    }
                }
                lfn.fill('\0');
                lfn_len = 0;
            }
        }
        std::uint32_t next = 0;
        if (!fat_next_cluster(volume, cluster, next)) {
            return false;
        }
        cluster = next;
    }
    return true;
}

[[nodiscard]] inline FirmwareCatalogCode fat_find_child_directory(const FatVolume& volume,
                                                                  std::uint32_t parent_cluster,
                                                                  std::string_view name,
                                                                  std::uint32_t& out_cluster) noexcept {
    bool found = false;
    const bool ok = fat_visit_directory(volume, parent_cluster, [&](const FatDirectoryEntry& entry) noexcept {
        const auto entry_name = fat_array_view(entry.name);
        if (entry.directory && fat_path_equal_ci(entry_name, name)) {
            out_cluster = entry.first_cluster;
            found = true;
            return false;
        }
        return true;
    });
    if (!ok) {
        return FirmwareCatalogCode::file_read_failed;
    }
    return found ? FirmwareCatalogCode::ok : FirmwareCatalogCode::directory_missing;
}

[[nodiscard]] inline FirmwareCatalogCode fat_resolve_directory(const FatVolume& volume,
                                                               std::string_view path,
                                                               std::uint32_t& out_cluster) noexcept {
    out_cluster = volume.root_cluster;
    while (!path.empty() && path.front() == '/') {
        path.remove_prefix(1);
    }
    while (!path.empty()) {
        const auto slash = path.find('/');
        const auto part = slash == std::string_view::npos ? path : path.substr(0, slash);
        if (!part.empty()) {
            const auto code = fat_find_child_directory(volume, out_cluster, part, out_cluster);
            if (code != FirmwareCatalogCode::ok) {
                return code;
            }
        }
        if (slash == std::string_view::npos) {
            break;
        }
        path.remove_prefix(slash + 1U);
    }
    return FirmwareCatalogCode::ok;
}

[[nodiscard]] inline FirmwareCatalogResult firmware_catalog_scan_fat(
    FatBlockReader reader,
    std::uint32_t partition_lba,
    std::string_view directory,
    FirmwareCatalog& catalog) noexcept {
    catalog = {};
    if (directory.empty()) {
        directory = kFirmwareCatalogDefaultDirectory;
    }
    FatVolume volume{};
    if (!fat_mount(reader, partition_lba, volume)) {
        return {.code = FirmwareCatalogCode::fat_invalid};
    }
    std::uint32_t dir_cluster = 0;
    const auto resolved = fat_resolve_directory(volume, directory, dir_cluster);
    if (resolved != FirmwareCatalogCode::ok) {
        return {.code = resolved};
    }

    const bool ok = fat_visit_directory(volume, dir_cluster, [&](const FatDirectoryEntry& entry) noexcept {
        if (entry.directory || entry.size == 0U || !fat_ends_with_elf(fat_array_view(entry.name))) {
            return true;
        }
        if (catalog.count >= catalog.entries.size()) {
            return false;
        }
        auto& dst = catalog.entries[catalog.count++];
        dst.name = entry.name;
        dst.size = entry.size;
        dst.first_cluster = entry.first_cluster;
        dst.format = AppImageFormat::elf;
        dst.source = FirmwareImageSource::emmc_fat;
        const auto name = fat_array_view(dst.name);
        char path_buf[kFirmwareCatalogMaxPath]{};
        const int written = std::snprintf(path_buf,
                                          sizeof(path_buf),
                                          "%.*s/%.*s",
                                          static_cast<int>(directory.size()),
                                          directory.data(),
                                          static_cast<int>(name.size()),
                                          name.data());
        if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(path_buf)) {
            catalog.count--;
            return false;
        }
        (void)fat_copy_text(dst.path, std::string_view{path_buf, static_cast<std::size_t>(written)});
        return true;
    });
    if (!ok) {
        return {.code = catalog.count >= catalog.entries.size()
                    ? FirmwareCatalogCode::too_many_entries
                    : FirmwareCatalogCode::file_read_failed,
                .count = catalog.count};
    }
    if (catalog.count == 0U) {
        return {.code = FirmwareCatalogCode::no_firmware};
    }
    return {.code = FirmwareCatalogCode::ok, .count = catalog.count};
}

[[nodiscard]] inline FirmwareCatalogCode fat_read_file_clusters(const FatVolume& volume,
                                                                std::uint32_t first_cluster,
                                                                std::uint32_t file_size,
                                                                std::span<std::byte> output,
                                                                std::uint32_t& bytes_read) noexcept {
    bytes_read = 0;
    if (!fat_cluster_valid(first_cluster)) {
        return FirmwareCatalogCode::file_read_failed;
    }
    if (file_size > output.size()) {
        return FirmwareCatalogCode::image_too_large;
    }
    std::uint32_t cluster = first_cluster;
    std::uint32_t guard = 0;
    std::array<std::byte, kFirmwareCatalogSectorSize> sector{};
    while (bytes_read < file_size && fat_cluster_valid(cluster) && !fat_is_eoc(cluster)) {
        if (++guard > 65536U) {
            return FirmwareCatalogCode::file_read_failed;
        }
        const auto cluster_lba = fat_cluster_lba(volume, cluster);
        for (std::uint32_t s = 0; s < volume.sectors_per_cluster && bytes_read < file_size; ++s) {
            if (!fat_read_sector(volume.reader, cluster_lba + s, sector)) {
                return FirmwareCatalogCode::file_read_failed;
            }
            const auto remaining = file_size - bytes_read;
            const auto copy = remaining < kFirmwareCatalogSectorSize ? remaining : kFirmwareCatalogSectorSize;
            std::memcpy(output.data() + bytes_read, sector.data(), copy);
            bytes_read += copy;
        }
        if (bytes_read >= file_size) {
            return FirmwareCatalogCode::ok;
        }
        std::uint32_t next = 0;
        if (!fat_next_cluster(volume, cluster, next)) {
            return FirmwareCatalogCode::file_read_failed;
        }
        cluster = next;
    }
    return bytes_read == file_size ? FirmwareCatalogCode::ok : FirmwareCatalogCode::file_read_failed;
}

[[nodiscard]] inline FirmwareFileStageResult firmware_file_stage_fat(
    FatBlockReader reader,
    std::uint32_t partition_lba,
    const FirmwareEntry& entry,
    std::span<std::byte> cache) noexcept {
    const auto name = fat_array_view(entry.name);
    if (name.empty() || entry.format != AppImageFormat::elf) {
        return {.code = FirmwareCatalogCode::invalid_argument};
    }
    if (name.size() + 1U > kAppReceivedImageMaxName) {
        return {.code = FirmwareCatalogCode::name_too_long};
    }
    if (entry.size == 0U) {
        return {.code = FirmwareCatalogCode::file_read_failed};
    }
    if (entry.size > cache.size()) {
        return {.code = FirmwareCatalogCode::image_too_large};
    }

    FatVolume volume{};
    if (!fat_mount(reader, partition_lba, volume)) {
        return {.code = FirmwareCatalogCode::fat_invalid};
    }
    std::uint32_t read = 0;
    const auto code = fat_read_file_clusters(volume, entry.first_cluster, entry.size, cache, read);
    if (code != FirmwareCatalogCode::ok) {
        return {.code = code, .bytes_read = read};
    }

    return FirmwareFileStageResult{
        .code = FirmwareCatalogCode::ok,
        .image = AppImage{
            .name = name,
            .format = AppImageFormat::elf,
            .image_base = cache.data(),
            .image_size = read,
        },
        .bytes_read = read,
    };
}

} // namespace charm::app_abi
