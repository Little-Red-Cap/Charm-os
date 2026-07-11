#include "charm_app_api.h"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"
#include "charm_app_store.hpp"
#include "charm_dev_loader_packet_stream.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;
namespace fs = std::filesystem;

struct ArtifactSpec {
    std::string_view name{};
    std::string_view format{};
    std::string_view path{};
    std::uint32_t store_flags{};
    std::string_view packetstream_path{};
};

struct MemoryStorage {
    std::vector<std::byte> bytes{};
};

struct FileReader {
    std::span<const std::byte> bytes{};
};

struct HostRuntime {
    std::array<char, 256> console{};
    std::size_t console_used{0};
    int exit_requested{-1};
};

struct LoadCtx {
    CharmAppMainFn entry{nullptr};
};

HostRuntime* g_host = nullptr;

constexpr std::array<ArtifactSpec, 3> kArtifacts{
    ArtifactSpec{
        .name = "hello_app",
        .format = "elf",
        .path = "hello_app.elf",
        .store_flags = app_abi::kAppStoreFormatElf,
        .packetstream_path = "hello_app.elf.packetstream",
    },
    ArtifactSpec{
        .name = "player_min",
        .format = "elf",
        .path = "player_min.elf",
        .store_flags = app_abi::kAppStoreFormatElf,
        .packetstream_path = "player_min.elf.packetstream",
    },
    ArtifactSpec{
        .name = "modulex_hello_app",
        .format = "modulex",
        .path = "modulex_hello_app.modulex",
        .store_flags = app_abi::kAppStoreFormatModuleX,
        .packetstream_path = "modulex_hello_app.modulex.packetstream",
    },
};

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
        std::printf("[ERR] failed to open %s\n", path.string().c_str());
        return false;
    }
    const auto size = file.tellg();
    if (size <= 0) {
        std::printf("[ERR] empty file %s\n", path.string().c_str());
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(out.data()), size);
    if (!file) {
        std::printf("[ERR] failed to read %s\n", path.string().c_str());
        return false;
    }
    return true;
}

bool read_text(const fs::path& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::printf("[ERR] failed to open %s\n", path.string().c_str());
        return false;
    }
    out.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    return true;
}

std::uint32_t crc32(std::span<const std::byte> bytes) {
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xedb88320U : (crc >> 1U);
        }
    }
    return crc ^ 0xffffffffU;
}

std::string crc_hex(std::uint32_t crc) {
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "0x%08x", crc);
    return buffer;
}

std::string compact_json(std::string_view text) {
    std::string out{};
    out.reserve(text.size());
    for (const char ch : text) {
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            out.push_back(ch);
        }
    }
    return out;
}

bool manifest_has_string(const std::string& manifest, std::string_view key, std::string_view value) {
    const auto compact = compact_json(manifest);
    const std::string token = "\"" + std::string{key} + "\":\"" + std::string{value} + "\"";
    return compact.find(token) != std::string::npos;
}

bool manifest_has_number(const std::string& manifest, std::string_view key, std::uint64_t value) {
    const auto compact = compact_json(manifest);
    const std::string token = "\"" + std::string{key} + "\":" + std::to_string(value);
    return compact.find(token) != std::string::npos;
}

bool manifest_has_bool(const std::string& manifest, std::string_view key, bool value) {
    const auto compact = compact_json(manifest);
    const std::string token = "\"" + std::string{key} + "\":" + (value ? "true" : "false");
    return compact.find(token) != std::string::npos;
}

bool storage_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(storage->bytes.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool storage_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), storage->bytes.data() + offset, bytes.size());
    return true;
}

bool store_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* reader = static_cast<FileReader*>(ctx);
    if (reader == nullptr || offset > reader->bytes.size() ||
        bytes.size() > (reader->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), reader->bytes.data() + offset, bytes.size());
    return true;
}

loader::Storage make_storage(MemoryStorage& storage) {
    return loader::Storage{
        .ctx = &storage,
        .base_address = 0x24060000U,
        .capacity_bytes = static_cast<std::uint32_t>(storage.bytes.size()),
        .write = storage_write,
        .read = storage_read,
    };
}

app_abi::AppStoreReader make_reader(FileReader& reader) {
    return app_abi::AppStoreReader{
        .ctx = &reader,
        .read = store_read,
    };
}

int console_write(const char* text, std::size_t len) {
    if (g_host == nullptr || text == nullptr) {
        return -1;
    }
    const auto copy = std::min(len, g_host->console.size() - g_host->console_used);
    if (copy != 0U) {
        std::memcpy(g_host->console.data() + g_host->console_used, text, copy);
        g_host->console_used += copy;
    }
    return static_cast<int>(len);
}

std::uint32_t now_ms() {
    return 4321U;
}

int unsupported_display_describe(CharmAppDisplayMode*) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int unsupported_display_present(const void*, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int unsupported_input_poll(CharmAppInputState*) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int unsupported_storage_open(const char*, int, int) {
    return -1;
}

int unsupported_storage_read(int, void*, std::size_t) {
    return -1;
}

int unsupported_storage_write(int, const void*, std::size_t) {
    return -1;
}

int unsupported_storage_close(int) {
    return -1;
}

int unsupported_afe_configure(std::uint32_t, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int unsupported_afe_read(void*, std::size_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

void app_exit(int code) {
    if (g_host != nullptr) {
        g_host->exit_requested = code;
    }
}

CharmAppApi make_api() {
    return CharmAppApi{
        .magic = CHARM_APP_API_MAGIC,
        .version = CHARM_APP_API_VERSION,
        .size = sizeof(CharmAppApi),
        .flags = 0,
        .console = CharmAppConsoleApi{.write = console_write},
        .time = CharmAppTimeApi{.now_ms = now_ms},
        .display = CharmAppDisplayApi{
            .describe = unsupported_display_describe,
            .present = unsupported_display_present,
        },
        .input = CharmAppInputApi{.poll = unsupported_input_poll},
        .storage = CharmAppStorageApi{
            .open = unsupported_storage_open,
            .read = unsupported_storage_read,
            .write = unsupported_storage_write,
            .close = unsupported_storage_close,
        },
        .afe = CharmAppAfeApi{.configure = unsupported_afe_configure, .read = unsupported_afe_read},
        .app = CharmAppControlApi{.exit = app_exit},
    };
}

extern "C" int fake_artifact_app_main(const CharmAppApi* api, int argc, char** argv) {
    if (api == nullptr || argv == nullptr || argc < 1) {
        return 90;
    }
    if (api->console.write == nullptr || api->time.now_ms == nullptr || api->time.now_ms() != 4321U) {
        return 91;
    }
    constexpr char kMessage[] = "resident artifact app\n";
    if (api->console.write(kMessage, sizeof(kMessage) - 1U) < 0) {
        return 92;
    }
    if (api->app.exit != nullptr) {
        api->app.exit(66);
    }
    return std::string_view{argv[0]} == "modulex_hello_app" ? 18 : 17;
}

app_abi::AppLoadResult load_fake(void* ctx,
                                 const app_abi::AppImage& image,
                                 const app_abi::AppLoadBuffer&) noexcept {
    auto* load = static_cast<LoadCtx*>(ctx);
    if (load == nullptr || load->entry == nullptr) {
        return {.code = app_abi::AppRunCode::abi_missing};
    }
    return app_abi::AppLoadResult{
        .code = app_abi::AppRunCode::ok,
        .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, load->entry),
    };
}

bool replay_packetstream(const fs::path& path, std::uint32_t expected_payload_size) {
    std::vector<std::byte> stream{};
    if (!read_file(path, stream)) {
        return false;
    }
    MemoryStorage storage{};
    storage.bytes.resize(expected_payload_size);
    loader::PacketRuntime runtime{make_storage(storage)};
    const auto replay = loader::packet_stream_replay(stream, runtime);
    return expect(replay.code == loader::PacketStreamReplayCode::ok, "packetstream replays") &&
           expect(replay.packet.receive.stage == loader::Stage::launch_ready, "packetstream reaches launch_ready") &&
           expect(replay.packet.manifest.size_bytes == expected_payload_size,
                  "packetstream payload size matches manifest");
}

bool verify_manifest_entry(const std::string& manifest,
                           const ArtifactSpec& spec,
                           std::span<const std::byte> payload,
                           std::uint32_t packetstream_size) {
    bool ok = true;
    ok = expect(manifest_has_string(manifest, "name", spec.name), "manifest records artifact name") && ok;
    ok = expect(manifest_has_string(manifest, "format", spec.format), "manifest records artifact format") && ok;
    ok = expect(manifest_has_string(manifest, "path", spec.path), "manifest records artifact path") && ok;
    ok = expect(manifest_has_string(manifest, "packetstream_path", spec.packetstream_path),
                "manifest records packetstream path") && ok;
    ok = expect(manifest_has_number(manifest, "size", payload.size()), "manifest records artifact size") && ok;
    ok = expect(manifest_has_string(manifest, "crc32", crc_hex(crc32(payload))),
                "manifest records artifact crc32") && ok;
    ok = expect(manifest_has_number(manifest, "store_flags", spec.store_flags),
                "manifest records store flags") && ok;
    ok = expect(manifest_has_number(manifest, "packetstream_size", packetstream_size),
                "manifest records packetstream size") && ok;
    if (spec.format == "elf") {
        ok = expect(manifest.find("\"elf_probe\"") != std::string::npos,
                    "manifest records ELF probe metadata") && ok;
        ok = expect(manifest_has_string(manifest, "code", "ok"),
                    "manifest records ELF probe ok code") && ok;
        ok = expect(manifest_has_number(manifest, "run_region_size", 64U * 1024U),
                    "manifest records ELF run region size") && ok;
        ok = expect(manifest_has_bool(manifest, "run_region_fits", true),
                    "manifest records ELF run region fit") && ok;
    }
    return ok;
}

bool verify_store_and_runtime(const fs::path& out_dir, const std::string& manifest) {
    bool ok = true;
    std::vector<std::byte> store{};
    ok = expect(read_file(out_dir / "appstore.bin", store), "appstore.bin reads") && ok;
    if (!ok) {
        return false;
    }

    std::vector<std::byte> store_stream{};
    ok = expect(read_file(out_dir / "appstore.bin.packetstream", store_stream),
                "appstore packetstream reads") && ok;
    ok = expect(manifest_has_string(manifest, "path", "appstore.bin"), "manifest records store path") && ok;
    ok = expect(manifest_has_string(manifest, "packetstream_path", "appstore.bin.packetstream"),
                "manifest records store packetstream path") && ok;
    ok = expect(manifest_has_number(manifest, "size", store.size()), "manifest records store size") && ok;
    ok = expect(manifest_has_string(manifest, "crc32", crc_hex(crc32(store))),
                "manifest records store crc32") && ok;
    ok = expect(manifest_has_number(manifest, "packetstream_size", store_stream.size()),
                "manifest records store packetstream size") && ok;
    ok = expect(replay_packetstream(out_dir / "appstore.bin.packetstream", static_cast<std::uint32_t>(store.size())),
                "store packetstream reaches launch_ready") && ok;

    FileReader reader_ctx{.bytes = store};
    const auto reader = make_reader(reader_ctx);
    app_abi::AppStoreHeader header{};
    ok = expect(app_abi::app_store_read_header(reader, header) == app_abi::AppStoreReadCode::ok,
                "store header validates") && ok;
    ok = expect(header.entry_count == kArtifacts.size(), "store entry count matches manifest artifacts") && ok;

    for (const auto& spec : kArtifacts) {
        const auto found = app_abi::app_store_find_entry(reader, spec.name);
        ok = expect(found.code == app_abi::AppStoreReadCode::ok, "store entry lookup succeeds") && ok;
        ok = expect((found.entry.flags & app_abi::kAppStoreFormatMask) == spec.store_flags,
                    "store entry flags match format") && ok;

        std::vector<std::byte> payload{};
        ok = expect(read_file(out_dir / spec.path, payload), "artifact payload reads") && ok;
        ok = expect(found.entry.size == payload.size(), "store entry size matches artifact") && ok;

        std::vector<std::byte> cache(payload.size());
        const auto staged = app_abi::app_store_stage_named_image(reader, spec.name, cache);
        ok = expect(staged.code == app_abi::AppStoreReadCode::ok, "store entry stages") && ok;
        ok = expect(staged.image.format == (spec.store_flags == app_abi::kAppStoreFormatModuleX
                                                ? app_abi::AppImageFormat::modulex
                                                : app_abi::AppImageFormat::elf),
                    "staged AppImage format follows Store flags") && ok;
        ok = expect(std::memcmp(cache.data(), payload.data(), payload.size()) == 0,
                    "staged payload matches artifact bytes") && ok;

        LoadCtx load_ctx{.entry = fake_artifact_app_main};
        app_abi::StagedAppImageSource source_ctx{
            .image = staged.image,
            .load_ctx = &load_ctx,
            .load = load_fake,
        };
        auto source = app_abi::make_staged_app_image_source(source_ctx);
        HostRuntime host{};
        g_host = &host;
        auto api = make_api();
        app_abi::AppRuntime<> runtime{};
        const auto run = runtime.run(app_abi::AppRunConfig{
            .source = &source,
            .api = &api,
            .name = spec.name,
            .arg_text = "alpha beta",
        });
        ok = expect(run.stage == app_abi::AppRunStage::exit &&
                        run.code == app_abi::AppRunCode::ok &&
                        run.exited &&
                        (run.exit_code == 17 || run.exit_code == 18),
                    "staged store artifact reaches AppRuntime exit") && ok;
        ok = expect(host.exit_requested == 66, "fake AppRuntime capability exit observed") && ok;
    }

    return ok;
}

} // namespace

int main() {
    const fs::path out_dir = fs::path{CHARM_ARTIFACT_OUT_DIR};
    const fs::path manifest_path = out_dir / "artifact_manifest.json";
    std::string manifest{};
    bool ok = true;

    ok = expect(read_text(manifest_path, manifest), "artifact manifest reads") && ok;
    ok = expect(manifest_has_string(manifest, "schema", "charm.resident_platform.artifacts.v1"),
                "manifest schema matches") && ok;

    for (const auto& spec : kArtifacts) {
        std::vector<std::byte> payload{};
        std::vector<std::byte> stream{};
        ok = expect(read_file(out_dir / spec.path, payload), "artifact file reads") && ok;
        ok = expect(read_file(out_dir / spec.packetstream_path, stream), "artifact packetstream reads") && ok;
        if (!payload.empty() && !stream.empty()) {
            ok = verify_manifest_entry(manifest, spec, payload, static_cast<std::uint32_t>(stream.size())) && ok;
            ok = replay_packetstream(out_dir / spec.packetstream_path, static_cast<std::uint32_t>(payload.size())) && ok;
        }
    }

    ok = verify_store_and_runtime(out_dir, manifest) && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[resident-platform-artifact-smoke] ok");
    return 0;
}
