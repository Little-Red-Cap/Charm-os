#include "charm_resident_platform_inspect.hpp"

#include "charm_app_store.hpp"
#include "charm_dev_loader_packets.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace app_abi = charm::app_abi;
namespace dev_loader = charm::dev_loader;
namespace inspect = charm::resident_platform::inspect;
namespace fs = std::filesystem;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

bool read_file(const fs::path& path, std::vector<std::byte>& out) {
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

bool write_file(const fs::path& path, std::span<const std::byte> bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(file);
}

bool read_text(const fs::path& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    return static_cast<bool>(file);
}

bool write_text(const fs::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

bool copy_tree(const fs::path& source, const fs::path& destination) {
    std::error_code ec{};
    fs::remove_all(destination, ec);
    fs::create_directories(destination, ec);
    if (ec) {
        return false;
    }
    for (const auto& entry : fs::recursive_directory_iterator(source)) {
        const auto relative = fs::relative(entry.path(), source, ec);
        if (ec) {
            return false;
        }
        const auto target = destination / relative;
        if (entry.is_directory()) {
            fs::create_directories(target, ec);
            if (ec) {
                return false;
            }
        } else if (entry.is_regular_file()) {
            fs::create_directories(target.parent_path(), ec);
            fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                return false;
            }
        }
    }
    return true;
}

bool mutate_manifest(const fs::path& manifest_path,
                     std::string_view from,
                     std::string_view to) {
    std::string text{};
    if (!read_text(manifest_path, text)) {
        return false;
    }
    const auto pos = text.find(from);
    if (pos == std::string::npos) {
        return false;
    }
    text.replace(pos, from.size(), to);
    return write_text(manifest_path, text);
}

bool mutate_first_manifest_u32(const fs::path& manifest_path,
                               std::string_view key,
                               std::string_view value) {
    std::string text{};
    if (!read_text(manifest_path, text)) {
        return false;
    }
    const auto key_pos = text.find(key);
    if (key_pos == std::string::npos) {
        return false;
    }
    const auto colon_pos = text.find(':', key_pos + key.size());
    if (colon_pos == std::string::npos) {
        return false;
    }
    auto first = colon_pos + 1U;
    while (first < text.size() && (text[first] == ' ' || text[first] == '\t')) {
        ++first;
    }
    auto last = first;
    while (last < text.size() && text[last] >= '0' && text[last] <= '9') {
        ++last;
    }
    if (first == last) {
        return false;
    }
    text.replace(first, last - first, value);
    return write_text(manifest_path, text);
}

bool mutate_u32_at(const fs::path& path, std::uint32_t offset, std::uint32_t value) {
    std::vector<std::byte> bytes{};
    if (!read_file(path, bytes) || offset > bytes.size() ||
        sizeof(value) > (bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
    return write_file(path, bytes);
}

bool remove_file(const fs::path& path) {
    std::error_code ec{};
    fs::remove(path, ec);
    return !ec;
}

bool corrupt_first_byte(const fs::path& path) {
    std::vector<std::byte> bytes{};
    if (!read_file(path, bytes) || bytes.empty()) {
        return false;
    }
    bytes[0] = std::byte{0};
    return write_file(path, bytes);
}

bool corrupt_store_flags(const fs::path& path, std::uint32_t flags) {
    std::vector<std::byte> bytes{};
    if (!read_file(path, bytes) ||
        bytes.size() < sizeof(app_abi::AppStoreHeader) + sizeof(app_abi::AppStoreEntry)) {
        return false;
    }
    app_abi::AppStoreHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (!app_abi::app_store_header_valid(header) || header.entry_count == 0U) {
        return false;
    }
    const auto offset =
        app_abi::app_store_entry_offset(header, 0U) + offsetof(app_abi::AppStoreEntry, flags);
    if (offset > bytes.size() || sizeof(flags) > (bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data() + offset, &flags, sizeof(flags));
    return write_file(path, bytes);
}

using Mutator = bool (*)(const fs::path& root);

bool with_fixture(std::string_view name,
                  Mutator mutator,
                  std::string_view expected_category) {
    const fs::path source = fs::path{CHARM_ARTIFACT_OUT_DIR};
    const fs::path root =
        fs::temp_directory_path() / ("charm_resident_platform_inspect_" + std::string{name});
    if (!copy_tree(source, root)) {
        std::printf("[ERR] failed to copy fixture %s\n", name.data());
        return false;
    }
    if (!mutator(root)) {
        std::printf("[ERR] failed to mutate fixture %s\n", name.data());
        return false;
    }
    const auto summary = inspect::inspect_manifest(root / "artifact_manifest.json");
    const bool ok = expect(!summary.ok(), "negative inspect fixture fails") &&
                    expect(summary.has_error(expected_category),
                           "negative inspect fixture reports expected category");
    std::error_code ec{};
    fs::remove_all(root, ec);
    return ok;
}

bool bad_manifest_schema(const fs::path& root) {
    return mutate_manifest(root / "artifact_manifest.json",
                           "charm.resident_platform.artifacts.v1",
                           "bad.schema");
}

bool missing_artifact(const fs::path& root) {
    return remove_file(root / "hello_app.elf");
}

bool crc_mismatch(const fs::path& root) {
    return corrupt_first_byte(root / "hello_app.elf");
}

bool store_flags_mismatch(const fs::path& root) {
    return corrupt_store_flags(root / "appstore.bin", app_abi::kAppStoreFormatModuleX);
}

bool bad_packetstream_magic(const fs::path& root) {
    return mutate_u32_at(root / "appstore.bin.packetstream", 0U, 0U);
}

bool bad_elf_magic(const fs::path& root) {
    std::vector<std::byte> bytes{};
    if (!read_file(root / "hello_app.elf", bytes) || bytes.size() < 4U) {
        return false;
    }
    bytes[0] = std::byte{0};
    bytes[1] = std::byte{0};
    bytes[2] = std::byte{0};
    bytes[3] = std::byte{0};
    return write_file(root / "hello_app.elf", bytes);
}

bool elf_probe_metadata_mismatch(const fs::path& root) {
    return mutate_first_manifest_u32(root / "artifact_manifest.json", "\"load_span\"", "271");
}

bool bad_modulex_layout(const fs::path& root) {
    constexpr std::uint32_t kBadTextSize = 0U;
    std::vector<std::byte> bytes{};
    if (!read_file(root / "modulex_hello_app.modulex", bytes) ||
        bytes.size() < sizeof(inspect::ModuleXWireHeader)) {
        return false;
    }
    std::memcpy(bytes.data() + offsetof(inspect::ModuleXWireHeader, text_size),
                &kBadTextSize,
                sizeof(kBadTextSize));
    return write_file(root / "modulex_hello_app.modulex", bytes);
}

} // namespace

int main() {
    bool ok = true;
    const fs::path manifest = fs::path{CHARM_ARTIFACT_OUT_DIR} / "artifact_manifest.json";
    const auto positive = inspect::inspect_manifest(manifest);
    ok = expect(positive.ok(), "generated resident platform artifacts inspect cleanly") && ok;
    ok = expect(positive.store.entries.size() == 3U, "generated Store has three entries") && ok;
    ok = expect(positive.packetstreams.size() == 4U, "generated artifacts have four packetstreams") && ok;
    ok = expect(std::any_of(positive.artifacts.begin(),
                            positive.artifacts.end(),
                            [](const inspect::ArtifactInspect& artifact) {
                                return artifact.manifest.name == "hello_app" &&
                                       artifact.elf.has_value() &&
                                       artifact.elf->probe.code == app_abi::AppElfProbeCode::ok &&
                                       artifact.elf->run_region_size == 64U * 1024U &&
                                       artifact.elf->run_region_fits &&
                                       artifact.manifest.elf_probe.present;
                            }),
                "generated ELF artifact carries matching run-region probe metadata") && ok;

    ok = with_fixture("bad_schema", bad_manifest_schema, "manifest_invalid_schema") && ok;
    ok = with_fixture("missing_artifact", missing_artifact, "artifact_missing") && ok;
    ok = with_fixture("crc_mismatch", crc_mismatch, "artifact_crc_mismatch") && ok;
    ok = with_fixture("store_flags", store_flags_mismatch, "store_entry_flags_mismatch") && ok;
    ok = with_fixture("bad_packetstream", bad_packetstream_magic, "packetstream_bad_magic") && ok;
    ok = with_fixture("bad_elf", bad_elf_magic, "bad_elf_magic") && ok;
    ok = with_fixture("elf_probe_metadata", elf_probe_metadata_mismatch, "elf_probe_metadata_mismatch") && ok;
    ok = with_fixture("bad_modulex", bad_modulex_layout, "modulex_bad_layout") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[resident-platform-inspect-smoke] ok");
    return 0;
}
