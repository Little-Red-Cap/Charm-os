#pragma once

#include "charm_app_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::app_abi {

inline constexpr std::uint32_t kAppStoreMagic = 0x50415043U; // "CPAP", little-endian.
inline constexpr std::uint16_t kAppStoreVersion = 1U;
inline constexpr std::uint32_t kAppStoreMaxEntries = 128U;
inline constexpr std::uint32_t kAppStoreFormatMask = 0x0000000FU;
inline constexpr std::uint32_t kAppStoreFormatElf = 0x00000000U;
inline constexpr std::uint32_t kAppStoreFormatModuleX = 0x00000001U;

struct AppStoreHeader {
    std::uint32_t magic{0};
    std::uint16_t version{0};
    std::uint16_t header_size{0};
    std::uint32_t entry_count{0};
    std::uint32_t entry_size{0};
};

struct AppStoreEntry {
    char name[32]{};
    std::uint32_t offset{0};
    std::uint32_t size{0};
    std::uint32_t flags{0};
};

enum class AppStoreReadCode : std::uint8_t {
    ok,
    invalid_argument,
    header_unreadable,
    header_invalid,
    entry_read_failed,
    image_not_found,
    image_too_large,
    image_read_failed,
};

enum class AppStoreBuildCode : std::uint8_t {
    ok,
    invalid_argument,
    empty_name,
    name_too_long,
    duplicate_name,
    too_many_entries,
    output_too_small,
};

struct AppStoreReader {
    void* ctx{nullptr};
    bool (*read)(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept{nullptr};
};

struct AppStoreBuildEntry {
    std::string_view name{};
    std::span<const std::byte> payload{};
    std::uint32_t flags{0};
};

struct AppStoreBuildResult {
    AppStoreBuildCode code{AppStoreBuildCode::ok};
    std::uint32_t bytes_written{0};
    std::uint32_t entry_count{0};
};

struct AppStoreLookupResult {
    AppStoreReadCode code{AppStoreReadCode::ok};
    AppStoreHeader header{};
    AppStoreEntry entry{};
    std::uint32_t entry_index{0};
};

struct AppStoreStageResult {
    AppStoreReadCode code{AppStoreReadCode::ok};
    AppImage image{};
    AppStoreLookupResult lookup{};
};

[[nodiscard]] constexpr std::uint32_t app_store_align_up(std::uint32_t value,
                                                          std::uint32_t alignment) noexcept {
    if (alignment <= 1U) {
        return value;
    }
    const std::uint32_t rem = value % alignment;
    return rem == 0U ? value : value + (alignment - rem);
}

[[nodiscard]] constexpr bool app_store_header_valid(const AppStoreHeader& header) noexcept {
    return header.magic == kAppStoreMagic &&
           header.version == kAppStoreVersion &&
           header.header_size >= sizeof(AppStoreHeader) &&
           header.entry_size >= sizeof(AppStoreEntry) &&
           header.entry_count <= kAppStoreMaxEntries;
}

[[nodiscard]] constexpr std::uint32_t app_store_entry_offset(const AppStoreHeader& header,
                                                             std::uint32_t index) noexcept {
    return header.header_size + (index * header.entry_size);
}

[[nodiscard]] constexpr std::string_view app_store_entry_name(const AppStoreEntry& entry) noexcept {
    std::size_t len = 0;
    while (len < sizeof(entry.name) && entry.name[len] != '\0') {
        ++len;
    }
    return {entry.name, len};
}

[[nodiscard]] constexpr bool app_store_entry_runnable(const AppStoreEntry& entry) noexcept {
    return entry.name[0] != '\0' && entry.size != 0U;
}

[[nodiscard]] constexpr AppImageFormat app_store_entry_format(const AppStoreEntry& entry) noexcept {
    switch (entry.flags & kAppStoreFormatMask) {
        case kAppStoreFormatModuleX:
            return AppImageFormat::modulex;
        case kAppStoreFormatElf:
        default:
            return AppImageFormat::elf;
    }
}

[[nodiscard]] constexpr std::uint32_t app_store_format_flags(AppImageFormat format) noexcept {
    switch (format) {
        case AppImageFormat::modulex:
            return kAppStoreFormatModuleX;
        case AppImageFormat::function:
        case AppImageFormat::elf:
        default:
            return kAppStoreFormatElf;
    }
}

[[nodiscard]] constexpr bool app_store_name_valid(std::string_view name) noexcept {
    return !name.empty() && name.size() < sizeof(AppStoreEntry::name);
}

[[nodiscard]] constexpr std::string_view app_store_read_code_name(AppStoreReadCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case AppStoreReadCode::ok:
            return "ok"sv;
        case AppStoreReadCode::invalid_argument:
            return "invalid_argument"sv;
        case AppStoreReadCode::header_unreadable:
            return "header_unreadable"sv;
        case AppStoreReadCode::header_invalid:
            return "header_invalid"sv;
        case AppStoreReadCode::entry_read_failed:
            return "entry_read_failed"sv;
        case AppStoreReadCode::image_not_found:
            return "image_not_found"sv;
        case AppStoreReadCode::image_too_large:
            return "image_too_large"sv;
        case AppStoreReadCode::image_read_failed:
            return "image_read_failed"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::string_view app_store_build_code_name(AppStoreBuildCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case AppStoreBuildCode::ok:
            return "ok"sv;
        case AppStoreBuildCode::invalid_argument:
            return "invalid_argument"sv;
        case AppStoreBuildCode::empty_name:
            return "empty_name"sv;
        case AppStoreBuildCode::name_too_long:
            return "name_too_long"sv;
        case AppStoreBuildCode::duplicate_name:
            return "duplicate_name"sv;
        case AppStoreBuildCode::too_many_entries:
            return "too_many_entries"sv;
        case AppStoreBuildCode::output_too_small:
            return "output_too_small"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] inline AppStoreBuildResult app_store_build_image(
    std::span<const AppStoreBuildEntry> entries,
    std::span<std::byte> output,
    std::uint32_t payload_alignment = 16U) noexcept {
    AppStoreBuildResult result{};
    if (entries.empty() || output.empty() || payload_alignment == 0U) {
        result.code = AppStoreBuildCode::invalid_argument;
        return result;
    }
    if (entries.size() > kAppStoreMaxEntries) {
        result.code = AppStoreBuildCode::too_many_entries;
        return result;
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name.empty()) {
            result.code = AppStoreBuildCode::empty_name;
            return result;
        }
        if (!app_store_name_valid(entries[i].name)) {
            result.code = AppStoreBuildCode::name_too_long;
            return result;
        }
        if (entries[i].payload.empty()) {
            result.code = AppStoreBuildCode::invalid_argument;
            return result;
        }
        if (entries[i].payload.size() > UINT32_MAX) {
            result.code = AppStoreBuildCode::output_too_small;
            return result;
        }
        for (std::size_t j = i + 1U; j < entries.size(); ++j) {
            if (entries[i].name == entries[j].name) {
                result.code = AppStoreBuildCode::duplicate_name;
                return result;
            }
        }
    }

    const std::uint32_t entry_count = static_cast<std::uint32_t>(entries.size());
    const std::uint32_t table_bytes =
        static_cast<std::uint32_t>(sizeof(AppStoreHeader) + (entries.size() * sizeof(AppStoreEntry)));
    std::uint32_t cursor = app_store_align_up(table_bytes, payload_alignment);
    for (const auto& entry : entries) {
        cursor = app_store_align_up(cursor, payload_alignment);
        const auto payload_size = static_cast<std::uint32_t>(entry.payload.size());
        if (payload_size > (UINT32_MAX - cursor)) {
            result.code = AppStoreBuildCode::output_too_small;
            return result;
        }
        cursor += payload_size;
    }

    if (cursor > output.size()) {
        result.code = AppStoreBuildCode::output_too_small;
        return result;
    }

    std::memset(output.data(), 0, cursor);
    AppStoreHeader header{
        .magic = kAppStoreMagic,
        .version = kAppStoreVersion,
        .header_size = sizeof(AppStoreHeader),
        .entry_count = entry_count,
        .entry_size = sizeof(AppStoreEntry),
    };
    std::memcpy(output.data(), &header, sizeof(header));

    cursor = app_store_align_up(table_bytes, payload_alignment);
    for (std::uint32_t i = 0; i < entry_count; ++i) {
        const auto& source = entries[i];
        cursor = app_store_align_up(cursor, payload_alignment);
        AppStoreEntry entry{};
        std::memcpy(entry.name, source.name.data(), source.name.size());
        entry.offset = cursor;
        entry.size = static_cast<std::uint32_t>(source.payload.size());
        entry.flags = source.flags;
        std::memcpy(output.data() + app_store_entry_offset(header, i), &entry, sizeof(entry));
        std::memcpy(output.data() + cursor, source.payload.data(), source.payload.size());
        cursor += entry.size;
    }

    result.code = AppStoreBuildCode::ok;
    result.bytes_written = cursor;
    result.entry_count = entry_count;
    return result;
}

[[nodiscard]] inline AppStoreReadCode app_store_read_header(const AppStoreReader& reader,
                                                            AppStoreHeader& out_header) noexcept {
    out_header = {};
    if (reader.read == nullptr) {
        return AppStoreReadCode::invalid_argument;
    }
    auto bytes = std::as_writable_bytes(std::span<AppStoreHeader>{&out_header, 1});
    if (!reader.read(reader.ctx, 0U, bytes)) {
        return AppStoreReadCode::header_unreadable;
    }
    if (!app_store_header_valid(out_header)) {
        return AppStoreReadCode::header_invalid;
    }
    return AppStoreReadCode::ok;
}

[[nodiscard]] inline AppStoreReadCode app_store_read_entry(const AppStoreReader& reader,
                                                           const AppStoreHeader& header,
                                                           std::uint32_t index,
                                                           AppStoreEntry& out_entry) noexcept {
    out_entry = {};
    if (reader.read == nullptr || !app_store_header_valid(header) || index >= header.entry_count) {
        return AppStoreReadCode::invalid_argument;
    }
    auto bytes = std::as_writable_bytes(std::span<AppStoreEntry>{&out_entry, 1});
    if (!reader.read(reader.ctx, app_store_entry_offset(header, index), bytes)) {
        return AppStoreReadCode::entry_read_failed;
    }
    return AppStoreReadCode::ok;
}

[[nodiscard]] inline AppStoreLookupResult app_store_find_entry(const AppStoreReader& reader,
                                                               std::string_view name) noexcept {
    AppStoreLookupResult result{};
    if (name.empty() || reader.read == nullptr) {
        result.code = AppStoreReadCode::invalid_argument;
        return result;
    }

    result.code = app_store_read_header(reader, result.header);
    if (result.code != AppStoreReadCode::ok) {
        return result;
    }

    for (std::uint32_t i = 0; i < result.header.entry_count; ++i) {
        AppStoreEntry entry{};
        const auto entry_code = app_store_read_entry(reader, result.header, i, entry);
        if (entry_code != AppStoreReadCode::ok) {
            result.code = entry_code;
            result.entry_index = i;
            return result;
        }
        if (app_store_entry_name(entry) != name) {
            continue;
        }
        if (!app_store_entry_runnable(entry)) {
            result.code = AppStoreReadCode::image_not_found;
            result.entry_index = i;
            result.entry = entry;
            return result;
        }
        result.code = AppStoreReadCode::ok;
        result.entry_index = i;
        result.entry = entry;
        return result;
    }

    result.code = AppStoreReadCode::image_not_found;
    return result;
}

[[nodiscard]] inline AppStoreReadCode app_store_read_image(const AppStoreReader& reader,
                                                           std::uint32_t offset,
                                                           std::uint32_t size,
                                                           std::span<std::byte> destination) noexcept {
    if (reader.read == nullptr || size == 0U) {
        return AppStoreReadCode::invalid_argument;
    }
    if (size > destination.size()) {
        return AppStoreReadCode::image_too_large;
    }
    if (!reader.read(reader.ctx, offset, destination.first(size))) {
        return AppStoreReadCode::image_read_failed;
    }
    return AppStoreReadCode::ok;
}

[[nodiscard]] inline AppStoreStageResult app_store_stage_named_image(
    const AppStoreReader& reader,
    std::string_view name,
    std::span<std::byte> cache,
    AppImageFormat format = AppImageFormat::elf) noexcept {
    AppStoreStageResult result{};
    result.lookup = app_store_find_entry(reader, name);
    result.code = result.lookup.code;
    if (result.code != AppStoreReadCode::ok) {
        return result;
    }

    result.code = app_store_read_image(reader, result.lookup.entry.offset, result.lookup.entry.size, cache);
    if (result.code != AppStoreReadCode::ok) {
        return result;
    }
    const auto image_format = format == AppImageFormat::elf
        ? app_store_entry_format(result.lookup.entry)
        : format;
    result.image = AppImage{
        .name = name,
        .format = image_format,
        .image_base = cache.data(),
        .image_size = result.lookup.entry.size,
    };
    return result;
}

[[nodiscard]] inline AppStoreStageResult app_store_stage_raw_image(
    const AppStoreReader& reader,
    std::string_view name,
    std::uint32_t offset,
    std::uint32_t size,
    std::span<std::byte> cache,
    AppImageFormat format = AppImageFormat::elf) noexcept {
    AppStoreStageResult result{};
    result.code = app_store_read_image(reader, offset, size, cache);
    if (result.code != AppStoreReadCode::ok) {
        return result;
    }
    result.image = AppImage{
        .name = name,
        .format = format,
        .image_base = cache.data(),
        .image_size = size,
    };
    return result;
}

} // namespace charm::app_abi
