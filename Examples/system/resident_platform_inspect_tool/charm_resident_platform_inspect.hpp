#pragma once

#include "charm_app_elf_probe.hpp"
#include "charm_app_store.hpp"
#include "charm_dev_loader_packets.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace charm::resident_platform::inspect {

namespace app_abi = charm::app_abi;
namespace dev_loader = charm::dev_loader;
namespace fs = std::filesystem;

inline constexpr std::string_view kManifestSchema{"charm.resident_platform.artifacts.v1"};
inline constexpr std::uint32_t kH747ElfRunRegionSize = 64U * 1024U;
inline constexpr std::uint32_t kModuleXWireMagic = 0x43484D4DU; // "CHMM", little-endian.
inline constexpr std::uint16_t kModuleXWireVersion = 2U;
inline constexpr std::uint16_t kModuleXWireFlagXipText = 1U << 0U;
inline constexpr std::uint16_t kModuleXWireFlagXipRo = 1U << 1U;
inline constexpr std::uint16_t kModuleXWireFlagXipData = 1U << 2U;
inline constexpr std::uint16_t kModuleXWireSymbolGlobal = 1U;
inline constexpr std::uint16_t kModuleXWireSymbolExternal = 2U;
inline constexpr std::uint32_t kModuleXWireRelocNone = 0U;
inline constexpr std::uint32_t kModuleXWireRelocAbsAddr = 1U;
inline constexpr std::uint32_t kModuleXWireRelocRel32 = 2U;

struct ModuleXWireHeader {
    std::uint32_t magic{0};
    std::uint16_t version{0};
    std::uint16_t flags{0};
    std::uint32_t entry_offset{0};
    std::uint32_t text_offset{0};
    std::uint32_t ro_offset{0};
    std::uint32_t data_offset{0};
    std::uint32_t rel_offset{0};
    std::uint32_t sym_offset{0};
    std::uint32_t str_offset{0};
    std::uint32_t str_size{0};
    std::uint32_t dep_offset{0};
    std::uint32_t dep_size{0};
    std::uint32_t text_size{0};
    std::uint32_t ro_size{0};
    std::uint32_t data_size{0};
    std::uint32_t bss_size{0};
    std::uint32_t rel_size{0};
    std::uint32_t sym_size{0};
    std::uint32_t image_size{0};
};

struct ModuleXWireSymbol32 {
    std::uint32_t name_offset{0};
    std::uint32_t value{0};
    std::uint32_t size{0};
    std::uint16_t kind{0};
    std::uint16_t flags{0};
};

struct ModuleXWireReloc32 {
    std::uint32_t offset{0};
    std::uint32_t type{0};
    std::uint32_t sym_index{0};
    std::uint32_t addend{0};
};

static_assert(sizeof(ModuleXWireHeader) == 76U);
static_assert(sizeof(ModuleXWireSymbol32) == 16U);
static_assert(sizeof(ModuleXWireReloc32) == 16U);

enum class Severity : std::uint8_t {
    warning,
    error,
};

struct Diagnostic {
    Severity severity{Severity::error};
    std::string category{};
    std::string message{};
};

struct StoreManifest {
    std::string path{};
    std::uint32_t size{0};
    std::uint32_t crc32{0};
    std::string packetstream_path{};
    std::uint32_t packetstream_size{0};
};

struct ElfProbeManifest {
    bool present{false};
    std::string code{};
    std::uint32_t entry_offset{0};
    std::uint32_t load_span{0};
    std::uint32_t segment_count{0};
    bool runnable{false};
    std::uint32_t run_region_size{0};
    bool run_region_fits{false};
};

struct ArtifactManifest {
    std::string name{};
    std::string format{};
    std::string path{};
    std::uint32_t size{0};
    std::uint32_t crc32{0};
    std::uint32_t store_flags{0};
    std::string packetstream_path{};
    std::uint32_t packetstream_size{0};
    ElfProbeManifest elf_probe{};
};

struct Manifest {
    fs::path manifest_path{};
    fs::path base_dir{};
    std::string schema{};
    StoreManifest store{};
    std::vector<ArtifactManifest> artifacts{};
};

struct PacketstreamInspect {
    std::string label{};
    bool ok{false};
    std::uint32_t payload_size{0};
    std::uint32_t crc32{0};
    std::uint32_t file_size{0};
};

struct StoreEntryInspect {
    std::string name{};
    std::uint32_t offset{0};
    std::uint32_t size{0};
    std::uint32_t flags{0};
    app_abi::AppImageFormat format{app_abi::AppImageFormat::elf};
};

struct StoreInspect {
    bool ok{false};
    app_abi::AppStoreHeader header{};
    std::vector<StoreEntryInspect> entries{};
};

struct ElfInspect {
    std::string name{};
    app_abi::AppElfProbeResult probe{};
    std::uint32_t run_region_size{kH747ElfRunRegionSize};
    bool run_region_fits{false};
};

struct ModuleXInspect {
    std::string name{};
    bool ok{false};
    std::string code{"ok"};
    std::uint32_t entry_offset{0};
    std::uint32_t image_span{0};
    std::uint32_t text_size{0};
    std::uint32_t rel_count{0};
    std::uint32_t sym_count{0};
    bool entry_symbol{false};
    bool relocated{false};
    bool unsupported_bss{false};
    bool unsupported_xip{false};
};

struct ArtifactInspect {
    ArtifactManifest manifest{};
    bool file_ok{false};
    app_abi::AppImageFormat format{app_abi::AppImageFormat::elf};
    std::optional<ElfInspect> elf{};
    std::optional<ModuleXInspect> modulex{};
};

struct InspectOptions {
    bool strict{false};
};

struct InspectSummary {
    Manifest manifest{};
    bool manifest_ok{false};
    bool packetstream_ok{true};
    StoreInspect store{};
    std::vector<PacketstreamInspect> packetstreams{};
    std::vector<ArtifactInspect> artifacts{};
    std::vector<Diagnostic> diagnostics{};

    [[nodiscard]] std::size_t error_count() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& diagnostic) noexcept {
                return diagnostic.severity == Severity::error;
            }));
    }

    [[nodiscard]] std::size_t warning_count() const noexcept {
        return diagnostics.size() - error_count();
    }

    [[nodiscard]] bool ok(bool strict = false) const noexcept {
        return error_count() == 0U && (!strict || warning_count() == 0U);
    }

    [[nodiscard]] bool has_error(std::string_view category) const noexcept {
        return std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [category](const Diagnostic& diagnostic) noexcept {
                return diagnostic.severity == Severity::error &&
                       diagnostic.category == category;
            });
    }
};

struct FileReader {
    std::span<const std::byte> bytes{};
};

inline void add_diagnostic(InspectSummary& summary,
                           Severity severity,
                           std::string category,
                           std::string message) {
    summary.diagnostics.push_back(Diagnostic{
        .severity = severity,
        .category = std::move(category),
        .message = std::move(message),
    });
}

inline void add_error(InspectSummary& summary,
                      std::string category,
                      std::string message) {
    add_diagnostic(summary, Severity::error, std::move(category), std::move(message));
}

inline void add_warning(InspectSummary& summary,
                        std::string category,
                        std::string message) {
    add_diagnostic(summary, Severity::warning, std::move(category), std::move(message));
}

[[nodiscard]] inline std::string to_string(std::string_view text) {
    return std::string{text.data(), text.size()};
}

[[nodiscard]] inline std::string crc_hex(std::uint32_t crc) {
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "0x%08x", crc);
    return buffer;
}

[[nodiscard]] inline std::uint32_t crc32(std::span<const std::byte> bytes) {
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xedb88320U : (crc >> 1U);
        }
    }
    return crc ^ 0xffffffffU;
}

[[nodiscard]] inline bool read_file(const fs::path& path, std::vector<std::byte>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const auto size = file.tellg();
    if (size < 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!out.empty()) {
        file.read(reinterpret_cast<char*>(out.data()), size);
    }
    return static_cast<bool>(file);
}

[[nodiscard]] inline bool read_text(const fs::path& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    return static_cast<bool>(file);
}

[[nodiscard]] inline bool store_read(void* ctx,
                                     std::uint32_t offset,
                                     std::span<std::byte> bytes) noexcept {
    const auto* reader = static_cast<const FileReader*>(ctx);
    if (reader == nullptr || offset > reader->bytes.size() ||
        bytes.size() > (reader->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), reader->bytes.data() + offset, bytes.size());
    return true;
}

[[nodiscard]] inline app_abi::AppStoreReader make_store_reader(FileReader& reader) noexcept {
    return app_abi::AppStoreReader{
        .ctx = &reader,
        .read = store_read,
    };
}

[[nodiscard]] inline std::string compact_json(std::string_view text) {
    std::string out{};
    out.reserve(text.size());
    bool in_string = false;
    bool escaped = false;
    for (const char ch : text) {
        if (in_string) {
            out.push_back(ch);
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            out.push_back(ch);
            continue;
        }
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            out.push_back(ch);
        }
    }
    return out;
}

[[nodiscard]] inline std::optional<std::size_t> find_key_value(std::string_view object,
                                                               std::string_view key) {
    const std::string token = "\"" + std::string{key} + "\":";
    const auto pos = object.find(token);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    return pos + token.size();
}

[[nodiscard]] inline std::optional<std::string> json_string_field(std::string_view object,
                                                                  std::string_view key) {
    const auto pos = find_key_value(object, key);
    if (!pos || *pos >= object.size() || object[*pos] != '"') {
        return std::nullopt;
    }
    std::string out{};
    bool escaped = false;
    for (std::size_t i = *pos + 1U; i < object.size(); ++i) {
        const char ch = object[i];
        if (escaped) {
            out.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            return out;
        }
        out.push_back(ch);
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<std::uint32_t> json_u32_field(std::string_view object,
                                                                 std::string_view key) {
    const auto pos = find_key_value(object, key);
    if (!pos || *pos >= object.size()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    const auto* first = object.data() + *pos;
    const auto* last = object.data() + object.size();
    const auto [ptr, ec] = std::from_chars(first, last, value, 10);
    if (ec != std::errc{} || ptr == first || value > UINT32_MAX) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] inline std::optional<bool> json_bool_field(std::string_view object,
                                                         std::string_view key) {
    const auto pos = find_key_value(object, key);
    if (!pos || *pos >= object.size()) {
        return std::nullopt;
    }
    if (object.substr(*pos, 4U) == "true") {
        return true;
    }
    if (object.substr(*pos, 5U) == "false") {
        return false;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<std::uint32_t> parse_crc(std::string_view text) {
    if (text.size() < 3U || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    const auto* first = text.data() + 2U;
    const auto* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value, 16);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] inline std::optional<std::string_view> extract_balanced(std::string_view text,
                                                                      std::size_t open_pos,
                                                                      char open_ch,
                                                                      char close_ch) {
    if (open_pos >= text.size() || text[open_pos] != open_ch) {
        return std::nullopt;
    }
    std::uint32_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t i = open_pos; i < text.size(); ++i) {
        const char ch = text[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == open_ch) {
            ++depth;
        } else if (ch == close_ch) {
            --depth;
            if (depth == 0U) {
                return text.substr(open_pos, i - open_pos + 1U);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<std::string_view> json_object_field(std::string_view object,
                                                                       std::string_view key) {
    const auto pos = find_key_value(object, key);
    if (!pos) {
        return std::nullopt;
    }
    return extract_balanced(object, *pos, '{', '}');
}

[[nodiscard]] inline std::optional<std::string_view> json_array_field(std::string_view object,
                                                                      std::string_view key) {
    const auto pos = find_key_value(object, key);
    if (!pos) {
        return std::nullopt;
    }
    return extract_balanced(object, *pos, '[', ']');
}

[[nodiscard]] inline std::vector<std::string_view> split_json_objects(std::string_view array) {
    std::vector<std::string_view> objects{};
    if (array.size() < 2U || array.front() != '[' || array.back() != ']') {
        return objects;
    }
    std::size_t pos = 1U;
    while (pos + 1U < array.size()) {
        if (array[pos] == '{') {
            auto object = extract_balanced(array, pos, '{', '}');
            if (!object) {
                return {};
            }
            objects.push_back(*object);
            pos += object->size();
            continue;
        }
        ++pos;
    }
    return objects;
}

[[nodiscard]] inline bool parse_store_manifest(std::string_view object, StoreManifest& out) {
    const auto path = json_string_field(object, "path");
    const auto size = json_u32_field(object, "size");
    const auto crc_text = json_string_field(object, "crc32");
    const auto packetstream_path = json_string_field(object, "packetstream_path");
    const auto packetstream_size = json_u32_field(object, "packetstream_size");
    if (!path || !size || !crc_text || !packetstream_path || !packetstream_size) {
        return false;
    }
    const auto crc = parse_crc(*crc_text);
    if (!crc) {
        return false;
    }
    out = StoreManifest{
        .path = *path,
        .size = *size,
        .crc32 = *crc,
        .packetstream_path = *packetstream_path,
        .packetstream_size = *packetstream_size,
    };
    return true;
}

[[nodiscard]] inline bool parse_elf_probe_manifest(std::string_view object,
                                                   ElfProbeManifest& out) {
    const auto code = json_string_field(object, "code");
    const auto entry_offset = json_u32_field(object, "entry_offset");
    const auto load_span = json_u32_field(object, "load_span");
    const auto segment_count = json_u32_field(object, "segment_count");
    const auto runnable = json_bool_field(object, "runnable");
    const auto run_region_size = json_u32_field(object, "run_region_size");
    const auto run_region_fits = json_bool_field(object, "run_region_fits");
    if (!code || !entry_offset || !load_span || !segment_count || !runnable ||
        !run_region_size || !run_region_fits) {
        return false;
    }
    out = ElfProbeManifest{
        .present = true,
        .code = *code,
        .entry_offset = *entry_offset,
        .load_span = *load_span,
        .segment_count = *segment_count,
        .runnable = *runnable,
        .run_region_size = *run_region_size,
        .run_region_fits = *run_region_fits,
    };
    return true;
}

[[nodiscard]] inline bool parse_artifact_manifest(std::string_view object, ArtifactManifest& out) {
    const auto name = json_string_field(object, "name");
    const auto format = json_string_field(object, "format");
    const auto path = json_string_field(object, "path");
    const auto size = json_u32_field(object, "size");
    const auto crc_text = json_string_field(object, "crc32");
    const auto store_flags = json_u32_field(object, "store_flags");
    const auto packetstream_path = json_string_field(object, "packetstream_path");
    const auto packetstream_size = json_u32_field(object, "packetstream_size");
    if (!name || !format || !path || !size || !crc_text || !store_flags ||
        !packetstream_path || !packetstream_size) {
        return false;
    }
    const auto crc = parse_crc(*crc_text);
    if (!crc) {
        return false;
    }
    out = ArtifactManifest{
        .name = *name,
        .format = *format,
        .path = *path,
        .size = *size,
        .crc32 = *crc,
        .store_flags = *store_flags,
        .packetstream_path = *packetstream_path,
        .packetstream_size = *packetstream_size,
    };
    if (const auto elf_probe = json_object_field(object, "elf_probe")) {
        if (!parse_elf_probe_manifest(*elf_probe, out.elf_probe)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool parse_manifest(const fs::path& manifest_path,
                                         std::string_view text,
                                         Manifest& out,
                                         InspectSummary& summary) {
    const auto compact = compact_json(text);
    Manifest parsed{};
    parsed.manifest_path = manifest_path;
    parsed.base_dir = manifest_path.parent_path();

    const auto schema = json_string_field(compact, "schema");
    if (!schema) {
        add_error(summary, "manifest_invalid_schema", "manifest schema field is missing");
        return false;
    }
    parsed.schema = *schema;
    if (parsed.schema != kManifestSchema) {
        add_error(summary,
                  "manifest_invalid_schema",
                  "manifest schema mismatch: " + parsed.schema);
    }

    const auto store_object = json_object_field(compact, "store");
    if (!store_object || !parse_store_manifest(*store_object, parsed.store)) {
        add_error(summary, "manifest_invalid_schema", "manifest store object is incomplete");
    }

    const auto artifact_array = json_array_field(compact, "artifacts");
    if (!artifact_array) {
        add_error(summary, "manifest_invalid_schema", "manifest artifacts array is missing");
    } else {
        const auto objects = split_json_objects(*artifact_array);
        if (objects.empty()) {
            add_error(summary, "manifest_invalid_schema", "manifest artifacts array is empty");
        }
        for (const auto object : objects) {
            ArtifactManifest artifact{};
            if (!parse_artifact_manifest(object, artifact)) {
                add_error(summary, "manifest_invalid_schema", "manifest artifact object is incomplete");
                continue;
            }
            if (std::any_of(parsed.artifacts.begin(),
                            parsed.artifacts.end(),
                            [&](const ArtifactManifest& existing) {
                                return existing.name == artifact.name;
                            })) {
                add_error(summary,
                          "manifest_duplicate_artifact",
                          "duplicate artifact name: " + artifact.name);
            }
            parsed.artifacts.push_back(std::move(artifact));
        }
    }

    out = std::move(parsed);
    return summary.error_count() == 0U || !out.schema.empty();
}

[[nodiscard]] inline std::string format_name(app_abi::AppImageFormat format) {
    switch (format) {
        case app_abi::AppImageFormat::modulex:
            return "modulex";
        case app_abi::AppImageFormat::function:
            return "function";
        case app_abi::AppImageFormat::elf:
        default:
            return "elf";
    }
}

[[nodiscard]] inline std::optional<app_abi::AppImageFormat> parse_format(std::string_view format) {
    if (format == "elf") {
        return app_abi::AppImageFormat::elf;
    }
    if (format == "modulex") {
        return app_abi::AppImageFormat::modulex;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::uint32_t expected_store_flags(app_abi::AppImageFormat format) {
    return app_abi::app_store_format_flags(format) & app_abi::kAppStoreFormatMask;
}

inline void inspect_manifest_artifact_shape(InspectSummary& summary,
                                            const ArtifactManifest& artifact) {
    const auto format = parse_format(artifact.format);
    if (!format) {
        add_error(summary,
                  "manifest_format_mismatch",
                  "unsupported artifact format: " + artifact.name + "=" + artifact.format);
        return;
    }
    const auto expected = expected_store_flags(*format);
    if ((artifact.store_flags & app_abi::kAppStoreFormatMask) != expected) {
        add_error(summary,
                  "manifest_store_flags_mismatch",
                  "artifact " + artifact.name + " format=" + artifact.format +
                      " store_flags=" + std::to_string(artifact.store_flags));
    }
    if ((artifact.store_flags & ~app_abi::kAppStoreFormatMask) != 0U) {
        add_warning(summary,
                    "manifest_reserved_store_flags",
                    "artifact " + artifact.name + " uses reserved Store flags");
    }
    if (*format == app_abi::AppImageFormat::elf) {
        if (artifact.elf_probe.present && artifact.elf_probe.run_region_size != kH747ElfRunRegionSize) {
            add_error(summary,
                      "manifest_elf_probe_mismatch",
                      "artifact " + artifact.name + " uses unexpected ELF run region size " +
                          std::to_string(artifact.elf_probe.run_region_size));
        }
    } else if (artifact.elf_probe.present) {
        add_error(summary,
                  "manifest_elf_probe_mismatch",
                  "non-ELF artifact " + artifact.name + " must not carry elf_probe metadata");
    }
}

[[nodiscard]] inline bool read_checked_file(InspectSummary& summary,
                                            const fs::path& path,
                                            std::uint32_t expected_size,
                                            std::uint32_t expected_crc,
                                            std::string_view label,
                                            std::string_view missing_category,
                                            std::string_view size_category,
                                            std::string_view crc_category,
                                            std::vector<std::byte>& out) {
    if (!read_file(path, out)) {
        add_error(summary,
                  to_string(missing_category),
                  "missing artifact " + std::string{label} + ": " + path.string());
        return false;
    }
    bool ok = true;
    if (out.size() != expected_size) {
        add_error(summary,
                  to_string(size_category),
                  "size mismatch " + std::string{label} + ": expected " +
                      std::to_string(expected_size) + " got " + std::to_string(out.size()));
        ok = false;
    }
    const auto actual_crc = crc32(out);
    if (actual_crc != expected_crc) {
        add_error(summary,
                  to_string(crc_category),
                  "crc mismatch " + std::string{label} + ": expected " +
                      crc_hex(expected_crc) + " got " + crc_hex(actual_crc));
        ok = false;
    }
    return ok;
}

inline void inspect_packetstream(InspectSummary& summary,
                                 const fs::path& path,
                                 std::string label,
                                 std::uint32_t expected_payload_size,
                                 std::uint32_t expected_crc,
                                 std::uint32_t expected_packetstream_size) {
    std::vector<std::byte> bytes{};
    PacketstreamInspect info{
        .label = label,
        .ok = false,
        .file_size = 0,
    };
    if (!read_file(path, bytes)) {
        summary.packetstream_ok = false;
        add_error(summary, "packetstream_missing", "missing packetstream " + label + ": " + path.string());
        summary.packetstreams.push_back(std::move(info));
        return;
    }
    info.file_size = static_cast<std::uint32_t>(bytes.size());
    bool ok = true;
    if (bytes.size() != expected_packetstream_size) {
        ok = false;
        add_error(summary,
                  "packetstream_size_mismatch",
                  "packetstream size mismatch " + label + ": expected " +
                      std::to_string(expected_packetstream_size) + " got " +
                      std::to_string(bytes.size()));
    }
    if (bytes.size() < sizeof(dev_loader::PacketHeader)) {
        ok = false;
        add_error(summary, "packetstream_truncated_header", "packetstream header is truncated: " + label);
    } else {
        dev_loader::PacketHeader header{};
        std::memcpy(&header, bytes.data(), sizeof(header));
        info.payload_size = header.size;
        info.crc32 = header.crc32;
        if (header.magic != dev_loader::kPacketMagic) {
            ok = false;
            add_error(summary, "packetstream_bad_magic", "packetstream bad magic: " + label);
        }
        if (header.version != dev_loader::kPacketVersion) {
            ok = false;
            add_error(summary, "packetstream_bad_version", "packetstream bad version: " + label);
        }
        if (header.header_size != dev_loader::kPacketHeaderSize) {
            ok = false;
            add_error(summary, "packetstream_bad_header", "packetstream bad header size: " + label);
        }
        if (header.kind != static_cast<std::uint16_t>(dev_loader::PacketKind::begin)) {
            ok = false;
            add_error(summary, "packetstream_bad_begin", "packetstream does not start with begin: " + label);
        }
        if (header.size != expected_payload_size) {
            ok = false;
            add_error(summary,
                      "packetstream_payload_size_mismatch",
                      "packetstream payload size mismatch " + label + ": expected " +
                          std::to_string(expected_payload_size) + " got " +
                          std::to_string(header.size));
        }
        if (header.crc32 != expected_crc) {
            ok = false;
            add_error(summary,
                      "packetstream_crc_mismatch",
                      "packetstream payload crc mismatch " + label + ": expected " +
                          crc_hex(expected_crc) + " got " + crc_hex(header.crc32));
        }
    }
    info.ok = ok;
    summary.packetstream_ok = summary.packetstream_ok && ok;
    summary.packetstreams.push_back(std::move(info));
}

[[nodiscard]] inline bool section_fits(std::uint32_t offset,
                                       std::uint32_t size,
                                       std::uint32_t total) noexcept {
    return offset <= total && size <= (total - offset);
}

[[nodiscard]] inline std::uint32_t modulex_layout_text(const ModuleXWireHeader& header) noexcept {
    return header.text_offset != 0U ? header.text_offset : static_cast<std::uint32_t>(sizeof(ModuleXWireHeader));
}

[[nodiscard]] inline std::uint32_t modulex_layout_ro(const ModuleXWireHeader& header) noexcept {
    const auto text = modulex_layout_text(header);
    return header.ro_offset != 0U ? header.ro_offset : text + header.text_size;
}

[[nodiscard]] inline std::uint32_t modulex_layout_data(const ModuleXWireHeader& header) noexcept {
    const auto ro = modulex_layout_ro(header);
    return header.data_offset != 0U ? header.data_offset : ro + header.ro_size;
}

[[nodiscard]] inline std::uint32_t modulex_layout_rel(const ModuleXWireHeader& header) noexcept {
    const auto data = modulex_layout_data(header);
    return header.rel_offset != 0U ? header.rel_offset : data + header.data_size;
}

[[nodiscard]] inline std::uint32_t modulex_layout_sym(const ModuleXWireHeader& header) noexcept {
    const auto rel = modulex_layout_rel(header);
    return header.sym_offset != 0U ? header.sym_offset : rel + header.rel_size;
}

[[nodiscard]] inline std::uint32_t modulex_layout_str(const ModuleXWireHeader& header) noexcept {
    const auto sym = modulex_layout_sym(header);
    return header.str_offset != 0U ? header.str_offset : sym + header.sym_size;
}

[[nodiscard]] inline std::uint32_t modulex_layout_dep(const ModuleXWireHeader& header) noexcept {
    const auto str = modulex_layout_str(header);
    return header.dep_offset != 0U ? header.dep_offset : str + header.str_size;
}

[[nodiscard]] inline std::optional<std::string> modulex_string(std::span<const std::byte> bytes,
                                                               const ModuleXWireHeader& header,
                                                               std::uint32_t name_offset) {
    if (name_offset >= header.str_size) {
        return std::nullopt;
    }
    const auto str_offset = modulex_layout_str(header);
    const auto begin = str_offset + name_offset;
    const auto end = str_offset + header.str_size;
    if (begin >= bytes.size() || end > bytes.size()) {
        return std::nullopt;
    }
    std::string name{};
    for (std::uint32_t cursor = begin; cursor < end; ++cursor) {
        const auto ch = static_cast<char>(bytes[cursor]);
        if (ch == '\0') {
            return name;
        }
        name.push_back(ch);
    }
    return std::nullopt;
}

inline void inspect_modulex(InspectSummary& summary,
                            const ArtifactManifest& artifact,
                            std::span<const std::byte> bytes,
                            ArtifactInspect& artifact_info) {
    ModuleXInspect info{.name = artifact.name};
    auto fail = [&](std::string category, std::string code, std::string message) {
        info.ok = false;
        info.code = std::move(code);
        add_error(summary, std::move(category), std::move(message));
    };

    if (bytes.size() < sizeof(ModuleXWireHeader)) {
        fail("modulex_bad_layout", "bad_size", "ModuleX header is truncated: " + artifact.name);
        artifact_info.modulex = info;
        return;
    }

    ModuleXWireHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    info.entry_offset = header.entry_offset;
    info.image_span = header.image_size != 0U ? header.image_size : static_cast<std::uint32_t>(bytes.size());
    info.text_size = header.text_size;
    info.rel_count = header.rel_size / sizeof(ModuleXWireReloc32);
    info.sym_count = header.sym_size / sizeof(ModuleXWireSymbol32);
    info.relocated = header.rel_size != 0U;
    info.unsupported_bss = header.bss_size != 0U;
    info.unsupported_xip =
        (header.flags & (kModuleXWireFlagXipText | kModuleXWireFlagXipRo | kModuleXWireFlagXipData)) != 0U;

    bool ok = true;
    if (header.magic != kModuleXWireMagic) {
        fail("modulex_bad_magic", "bad_magic", "ModuleX bad magic: " + artifact.name);
        ok = false;
    }
    if (header.version != kModuleXWireVersion) {
        fail("modulex_bad_version", "bad_version", "ModuleX bad version: " + artifact.name);
        ok = false;
    }
    if (header.text_size == 0U || header.entry_offset >= header.text_size) {
        fail("modulex_bad_layout", "bad_entry", "ModuleX entry is outside text: " + artifact.name);
        ok = false;
    }
    if (header.image_size != 0U &&
        (header.image_size < sizeof(ModuleXWireHeader) || header.image_size > bytes.size())) {
        fail("modulex_bad_layout", "bad_size", "ModuleX image_size is invalid: " + artifact.name);
        ok = false;
    }
    const auto total = info.image_span;
    if ((header.rel_size % sizeof(ModuleXWireReloc32)) != 0U ||
        (header.sym_size % sizeof(ModuleXWireSymbol32)) != 0U) {
        fail("modulex_bad_layout", "bad_layout", "ModuleX table size is not aligned: " + artifact.name);
        ok = false;
    }
    if (!section_fits(modulex_layout_text(header), header.text_size, total) ||
        !section_fits(modulex_layout_ro(header), header.ro_size, total) ||
        !section_fits(modulex_layout_data(header), header.data_size, total) ||
        !section_fits(modulex_layout_rel(header), header.rel_size, total) ||
        !section_fits(modulex_layout_sym(header), header.sym_size, total) ||
        !section_fits(modulex_layout_str(header), header.str_size, total) ||
        !section_fits(modulex_layout_dep(header), header.dep_size, total)) {
        fail("modulex_bad_layout", "bad_layout", "ModuleX section layout is invalid: " + artifact.name);
        ok = false;
    }
    if (info.unsupported_bss) {
        fail("modulex_unsupported_bss", "unsupported_bss", "ModuleX App v1 rejects BSS: " + artifact.name);
        ok = false;
    }
    if (info.unsupported_xip) {
        fail("modulex_unsupported_xip", "unsupported_xip", "ModuleX App v1 rejects XIP flags: " + artifact.name);
        ok = false;
    }

    if (ok) {
        const auto sym_offset = modulex_layout_sym(header);
        const auto sym_count = header.sym_size / sizeof(ModuleXWireSymbol32);
        for (std::uint32_t i = 0; i < sym_count; ++i) {
            ModuleXWireSymbol32 symbol{};
            std::memcpy(&symbol,
                        bytes.data() + sym_offset + (i * sizeof(ModuleXWireSymbol32)),
                        sizeof(symbol));
            const auto symbol_name = modulex_string(bytes, header, symbol.name_offset);
            if (!symbol_name) {
                fail("modulex_bad_layout", "bad_sym", "ModuleX symbol name is invalid: " + artifact.name);
                ok = false;
                break;
            }
            if (*symbol_name != "charm_app_main") {
                continue;
            }
            info.entry_symbol = true;
            if (symbol.kind != kModuleXWireSymbolGlobal && symbol.kind != kModuleXWireSymbolExternal) {
                fail("modulex_bad_layout", "bad_sym", "ModuleX charm_app_main has unsupported kind: " + artifact.name);
                ok = false;
            }
            if (symbol.kind == kModuleXWireSymbolGlobal && symbol.value >= header.text_size) {
                fail("modulex_bad_layout", "bad_sym", "ModuleX charm_app_main is outside text: " + artifact.name);
                ok = false;
            }
        }
        if (!info.entry_symbol) {
            fail("modulex_missing_entry", "missing_entry", "ModuleX charm_app_main symbol is missing: " + artifact.name);
            ok = false;
        }

        const auto rel_offset = modulex_layout_rel(header);
        const auto rel_count = header.rel_size / sizeof(ModuleXWireReloc32);
        for (std::uint32_t i = 0; ok && i < rel_count; ++i) {
            ModuleXWireReloc32 reloc{};
            std::memcpy(&reloc,
                        bytes.data() + rel_offset + (i * sizeof(ModuleXWireReloc32)),
                        sizeof(reloc));
            if (reloc.type != kModuleXWireRelocNone &&
                reloc.type != kModuleXWireRelocAbsAddr &&
                reloc.type != kModuleXWireRelocRel32) {
                fail("modulex_bad_layout", "bad_reloc", "ModuleX relocation type is unsupported: " + artifact.name);
                ok = false;
            }
            if (reloc.offset + sizeof(std::uint32_t) > total) {
                fail("modulex_bad_layout", "bad_reloc", "ModuleX relocation target is outside image: " + artifact.name);
                ok = false;
            }
            if (reloc.sym_index != 0xFFFFFFFFU && reloc.sym_index >= sym_count) {
                fail("modulex_bad_layout", "bad_sym", "ModuleX relocation symbol index is invalid: " + artifact.name);
                ok = false;
            }
        }
    }

    if (!info.relocated) {
        add_warning(summary, "modulex_no_relocation", "ModuleX artifact has no relocations: " + artifact.name);
    }
    if (ok) {
        info.ok = true;
        info.code = "ok";
    }
    artifact_info.modulex = info;
}

inline void inspect_elf(InspectSummary& summary,
                        const ArtifactManifest& artifact,
                        std::span<const std::byte> bytes,
                        ArtifactInspect& artifact_info) {
    alignas(32) std::array<std::byte, 128U * 1024U> load_buffer{};
    const app_abi::AppImage image{
        .name = artifact.name,
        .format = app_abi::AppImageFormat::elf,
        .image_base = bytes.data(),
        .image_size = bytes.size(),
    };
    const app_abi::AppLoadBuffer buffer{
        .base = load_buffer.data(),
        .size = load_buffer.size(),
        .align = 4U,
    };
    const auto probe = app_abi::app_elf_probe_load(image, buffer);
    ElfInspect info{
        .name = artifact.name,
        .probe = probe,
        .run_region_size = kH747ElfRunRegionSize,
        .run_region_fits = probe.load_span != 0U && probe.load_span <= kH747ElfRunRegionSize,
    };
    if (info.probe.code != app_abi::AppElfProbeCode::ok) {
        const auto category = info.probe.code == app_abi::AppElfProbeCode::bad_magic
            ? "bad_elf_magic"
            : "elf_probe_failed";
        add_error(summary,
                  category,
                  "ELF probe failed for " + artifact.name + ": " +
                      to_string(app_abi::app_elf_probe_code_name(info.probe.code)));
    }
    if (!artifact.elf_probe.present) {
        add_warning(summary,
                    "elf_probe_metadata_missing",
                    "ELF artifact has no manifest probe metadata: " + artifact.name);
    } else {
        const auto actual_code = to_string(app_abi::app_elf_probe_code_name(info.probe.code));
        if (artifact.elf_probe.code != actual_code ||
            artifact.elf_probe.entry_offset != info.probe.entry_offset ||
            artifact.elf_probe.load_span != info.probe.load_span ||
            artifact.elf_probe.segment_count != info.probe.segment_count ||
            artifact.elf_probe.runnable != info.probe.runnable ||
            artifact.elf_probe.run_region_fits != info.run_region_fits) {
            add_error(summary,
                      "elf_probe_metadata_mismatch",
                      "ELF manifest probe metadata does not match actual probe: " + artifact.name);
        }
    }
    artifact_info.elf = info;
}

inline void inspect_artifact(InspectSummary& summary,
                             const ArtifactManifest& artifact,
                             const fs::path& base_dir,
                             std::vector<std::byte>& bytes_out) {
    ArtifactInspect artifact_info{
        .manifest = artifact,
    };
    const auto format = parse_format(artifact.format);
    if (format) {
        artifact_info.format = *format;
    }

    const auto path = base_dir / artifact.path;
    artifact_info.file_ok = read_checked_file(summary,
                                              path,
                                              artifact.size,
                                              artifact.crc32,
                                              artifact.name,
                                              "artifact_missing",
                                              "artifact_size_mismatch",
                                              "artifact_crc_mismatch",
                                              bytes_out);
    if (!bytes_out.empty() && format) {
        switch (*format) {
            case app_abi::AppImageFormat::elf:
                inspect_elf(summary, artifact, bytes_out, artifact_info);
                break;
            case app_abi::AppImageFormat::modulex:
                inspect_modulex(summary, artifact, bytes_out, artifact_info);
                break;
            case app_abi::AppImageFormat::function:
                break;
        }
    }
    summary.artifacts.push_back(std::move(artifact_info));
}

inline void inspect_store(InspectSummary& summary,
                          std::span<const std::byte> store_bytes,
                          const std::vector<std::vector<std::byte>>& artifact_payloads) {
    FileReader reader_ctx{.bytes = store_bytes};
    const auto reader = make_store_reader(reader_ctx);
    app_abi::AppStoreHeader header{};
    const auto header_code = app_abi::app_store_read_header(reader, header);
    summary.store.header = header;
    if (header_code != app_abi::AppStoreReadCode::ok) {
        add_error(summary,
                  "store_header_invalid",
                  "Store header invalid: " + to_string(app_abi::app_store_read_code_name(header_code)));
        summary.store.ok = false;
        return;
    }

    bool ok = true;
    if (header.entry_count != summary.manifest.artifacts.size()) {
        add_error(summary,
                  "store_entry_count_mismatch",
                  "Store entry count mismatch: expected " +
                      std::to_string(summary.manifest.artifacts.size()) + " got " +
                      std::to_string(header.entry_count));
        ok = false;
    }

    for (std::uint32_t i = 0; i < header.entry_count; ++i) {
        app_abi::AppStoreEntry entry{};
        const auto code = app_abi::app_store_read_entry(reader, header, i, entry);
        if (code != app_abi::AppStoreReadCode::ok) {
            add_error(summary,
                      "store_entry_read_failed",
                      "Store entry read failed at index " + std::to_string(i));
            ok = false;
            continue;
        }
        const auto name = to_string(app_abi::app_store_entry_name(entry));
        summary.store.entries.push_back(StoreEntryInspect{
            .name = name,
            .offset = entry.offset,
            .size = entry.size,
            .flags = entry.flags,
            .format = app_abi::app_store_entry_format(entry),
        });
        if ((entry.flags & ~app_abi::kAppStoreFormatMask) != 0U) {
            add_warning(summary,
                        "store_reserved_flags",
                        "Store entry uses reserved flags: " + name);
        }
        const auto manifest_it = std::find_if(summary.manifest.artifacts.begin(),
                                              summary.manifest.artifacts.end(),
                                              [&](const ArtifactManifest& artifact) {
                                                  return artifact.name == name;
                                              });
        if (manifest_it == summary.manifest.artifacts.end()) {
            add_error(summary, "store_unexpected_entry", "Store entry is not present in manifest: " + name);
            ok = false;
        }
    }

    for (std::size_t i = 0; i < summary.manifest.artifacts.size(); ++i) {
        const auto& artifact = summary.manifest.artifacts[i];
        const auto found = app_abi::app_store_find_entry(reader, artifact.name);
        if (found.code != app_abi::AppStoreReadCode::ok) {
            add_error(summary,
                      "store_entry_missing",
                      "Store entry lookup failed for " + artifact.name + ": " +
                          to_string(app_abi::app_store_read_code_name(found.code)));
            ok = false;
            continue;
        }
        if (found.entry.flags != artifact.store_flags) {
            add_error(summary,
                      "store_entry_flags_mismatch",
                      "Store flags mismatch for " + artifact.name + ": expected " +
                          std::to_string(artifact.store_flags) + " got " +
                          std::to_string(found.entry.flags));
            ok = false;
        }
        if (found.entry.size != artifact.size) {
            add_error(summary,
                      "store_entry_size_mismatch",
                      "Store size mismatch for " + artifact.name + ": expected " +
                          std::to_string(artifact.size) + " got " +
                          std::to_string(found.entry.size));
            ok = false;
        }
        if (found.entry.offset > store_bytes.size() ||
            found.entry.size > (store_bytes.size() - found.entry.offset)) {
            add_error(summary, "store_entry_range_invalid", "Store entry range is invalid: " + artifact.name);
            ok = false;
            continue;
        }
        if (i < artifact_payloads.size() && !artifact_payloads[i].empty() &&
            artifact_payloads[i].size() == found.entry.size &&
            std::memcmp(store_bytes.data() + found.entry.offset,
                        artifact_payloads[i].data(),
                        found.entry.size) != 0) {
            add_error(summary,
                      "store_payload_mismatch",
                      "Store payload does not match artifact bytes: " + artifact.name);
            ok = false;
        }
    }

    summary.store.ok = ok;
}

[[nodiscard]] inline InspectSummary inspect_manifest(const fs::path& manifest_path,
                                                     InspectOptions options = {}) {
    InspectSummary summary{};
    (void)options;

    std::string manifest_text{};
    if (!read_text(manifest_path, manifest_text)) {
        add_error(summary, "manifest_missing", "failed to read manifest: " + manifest_path.string());
        return summary;
    }

    (void)parse_manifest(manifest_path, manifest_text, summary.manifest, summary);
    summary.manifest_ok = summary.manifest.schema == kManifestSchema && !summary.manifest.artifacts.empty();

    for (const auto& artifact : summary.manifest.artifacts) {
        inspect_manifest_artifact_shape(summary, artifact);
    }

    std::vector<std::byte> store_bytes{};
    (void)read_checked_file(summary,
                            summary.manifest.base_dir / summary.manifest.store.path,
                            summary.manifest.store.size,
                            summary.manifest.store.crc32,
                            "store",
                            "store_missing",
                            "store_size_mismatch",
                            "store_crc_mismatch",
                            store_bytes);
    inspect_packetstream(summary,
                         summary.manifest.base_dir / summary.manifest.store.packetstream_path,
                         "store",
                         summary.manifest.store.size,
                         summary.manifest.store.crc32,
                         summary.manifest.store.packetstream_size);

    std::vector<std::vector<std::byte>> artifact_payloads{};
    artifact_payloads.reserve(summary.manifest.artifacts.size());
    for (const auto& artifact : summary.manifest.artifacts) {
        std::vector<std::byte> payload{};
        inspect_artifact(summary, artifact, summary.manifest.base_dir, payload);
        inspect_packetstream(summary,
                             summary.manifest.base_dir / artifact.packetstream_path,
                             artifact.name,
                             artifact.size,
                             artifact.crc32,
                             artifact.packetstream_size);
        artifact_payloads.push_back(std::move(payload));
    }

    if (!store_bytes.empty()) {
        inspect_store(summary, store_bytes, artifact_payloads);
    }

    summary.manifest_ok = summary.manifest_ok && summary.error_count() == 0U;
    return summary;
}

[[nodiscard]] inline std::string json_escape(std::string_view text) {
    std::string out{};
    out.reserve(text.size() + 8U);
    for (const char ch : text) {
        switch (ch) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

inline void print_human(const InspectSummary& summary, std::FILE* out, bool strict = false) {
    const auto pass = summary.ok(strict);
    std::fprintf(out,
                 "manifest=%s schema=%s artifacts=%zu\n",
                 summary.manifest.schema == kManifestSchema ? "ok" : "fail",
                 summary.manifest.schema.c_str(),
                 summary.manifest.artifacts.size());

    std::fprintf(out,
                 "packetstream=%s checked=%zu\n",
                 summary.packetstream_ok ? "ok" : "fail",
                 summary.packetstreams.size());

    for (const auto& entry : summary.store.entries) {
        std::fprintf(out,
                     "store entry name=%s offset=%u size=%u flags=%u format=%s\n",
                     entry.name.c_str(),
                     entry.offset,
                     entry.size,
                     entry.flags,
                     format_name(entry.format).c_str());
    }
    std::fprintf(out,
                 "store=%s entries=%zu\n",
                 summary.store.ok ? "ok" : "fail",
                 summary.store.entries.size());

    for (const auto& artifact : summary.artifacts) {
        if (artifact.elf) {
            const auto& elf = *artifact.elf;
            std::fprintf(out,
                         "elf %s code=%s entry_offset=%u load_span=%u segment_count=%u runnable=%u run_region=%u run_region_fits=%u\n",
                         elf.name.c_str(),
                         to_string(app_abi::app_elf_probe_code_name(elf.probe.code)).c_str(),
                         elf.probe.entry_offset,
                         elf.probe.load_span,
                         elf.probe.segment_count,
                         elf.probe.runnable ? 1U : 0U,
                         elf.run_region_size,
                         elf.run_region_fits ? 1U : 0U);
        }
        if (artifact.modulex) {
            const auto& modulex = *artifact.modulex;
            std::fprintf(out,
                         "modulex %s code=%s entry_offset=%u span=%u text=%u symbols=%u relocs=%u entry_symbol=%u relocated=%u\n",
                         modulex.name.c_str(),
                         modulex.code.c_str(),
                         modulex.entry_offset,
                         modulex.image_span,
                         modulex.text_size,
                         modulex.sym_count,
                         modulex.rel_count,
                         modulex.entry_symbol ? 1U : 0U,
                         modulex.relocated ? 1U : 0U);
        }
    }

    for (const auto& diagnostic : summary.diagnostics) {
        std::fprintf(out,
                     "%s %s: %s\n",
                     diagnostic.severity == Severity::error ? "error" : "warning",
                     diagnostic.category.c_str(),
                     diagnostic.message.c_str());
    }

    std::fprintf(out,
                 "resident-platform-inspect: %s\n",
                 pass ? "ok" : "fail");
}

inline void print_json(const InspectSummary& summary, std::FILE* out, bool strict = false) {
    std::fprintf(out,
                 "{\n"
                 "  \"ok\": %s,\n"
                 "  \"errors\": %zu,\n"
                 "  \"warnings\": %zu,\n"
                 "  \"manifest\": {\"status\": \"%s\", \"schema\": \"%s\", \"artifacts\": %zu},\n"
                 "  \"packetstream\": {\"status\": \"%s\", \"checked\": %zu},\n"
                 "  \"store\": {\"status\": \"%s\", \"entries\": %zu},\n"
                 "  \"artifacts\": [\n",
                 summary.ok(strict) ? "true" : "false",
                 summary.error_count(),
                 summary.warning_count(),
                 summary.manifest.schema == kManifestSchema ? "ok" : "fail",
                 json_escape(summary.manifest.schema).c_str(),
                 summary.manifest.artifacts.size(),
                 summary.packetstream_ok ? "ok" : "fail",
                 summary.packetstreams.size(),
                 summary.store.ok ? "ok" : "fail",
                 summary.store.entries.size());

    for (std::size_t i = 0; i < summary.artifacts.size(); ++i) {
        const auto& artifact = summary.artifacts[i];
        std::fprintf(out,
                     "    {\"name\": \"%s\", \"format\": \"%s\"",
                     json_escape(artifact.manifest.name).c_str(),
                     json_escape(artifact.manifest.format).c_str());
        if (artifact.elf) {
            const auto& elf = *artifact.elf;
            std::fprintf(out,
                         ", \"elf\": {\"code\": \"%s\", \"entry_offset\": %u, \"load_span\": %u, \"segment_count\": %u, \"runnable\": %s, \"run_region_size\": %u, \"run_region_fits\": %s}",
                         json_escape(to_string(app_abi::app_elf_probe_code_name(elf.probe.code))).c_str(),
                         elf.probe.entry_offset,
                         elf.probe.load_span,
                         elf.probe.segment_count,
                         elf.probe.runnable ? "true" : "false",
                         elf.run_region_size,
                         elf.run_region_fits ? "true" : "false");
        }
        std::fprintf(out, "}");
        if (i + 1U != summary.artifacts.size()) {
            std::fprintf(out, ",");
        }
        std::fprintf(out, "\n");
    }
    std::fprintf(out, "  ],\n  \"diagnostics\": [\n");
    for (std::size_t i = 0; i < summary.diagnostics.size(); ++i) {
        const auto& diagnostic = summary.diagnostics[i];
        std::fprintf(out,
                     "    {\"severity\": \"%s\", \"category\": \"%s\", \"message\": \"%s\"}",
                     diagnostic.severity == Severity::error ? "error" : "warning",
                     json_escape(diagnostic.category).c_str(),
                     json_escape(diagnostic.message).c_str());
        if (i + 1U != summary.diagnostics.size()) {
            std::fprintf(out, ",");
        }
        std::fprintf(out, "\n");
    }
    std::fprintf(out, "  ]\n}\n");
}

} // namespace charm::resident_platform::inspect
