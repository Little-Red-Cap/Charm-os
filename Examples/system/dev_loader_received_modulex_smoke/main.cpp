#include "charm_app_api.h"
#include "charm_app_received_image.hpp"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"
#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_received_image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

import module_core;
import module_link;
import module_loader;
import module_view;

#include "charm_app_modulex_loader.hpp"

namespace {

namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::array<std::byte, 2048> bytes{};
    bool fail_read{false};
};

struct HostRuntime {
    std::array<char, 512> console{};
    std::size_t console_used{0};
    std::uint32_t now_count{0};
    int exit_requested{-1};
};

HostRuntime* g_host = nullptr;

int console_write(const char* text, const std::size_t len) {
    if (g_host == nullptr || text == nullptr) {
        return -1;
    }
    const std::size_t remaining = g_host->console.size() - g_host->console_used;
    const std::size_t copy = len < remaining ? len : remaining;
    if (copy != 0U) {
        std::memcpy(g_host->console.data() + g_host->console_used, text, copy);
        g_host->console_used += copy;
    }
    return static_cast<int>(len);
}

std::uint32_t now_ms() {
    if (g_host != nullptr) {
        ++g_host->now_count;
    }
    return 1357U;
}

void app_exit(const int code) {
    if (g_host != nullptr) {
        g_host->exit_requested = code;
    }
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
        .afe = CharmAppAfeApi{
            .configure = unsupported_afe_configure,
            .read = unsupported_afe_read,
        },
        .app = CharmAppControlApi{.exit = app_exit},
    };
}

extern "C" int fake_charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    if (api == nullptr || api->console.write == nullptr || api->time.now_ms == nullptr ||
        api->app.exit == nullptr || argv == nullptr) {
        return 90;
    }
    if (argc != 3 || argv[0] == nullptr || argv[1] == nullptr || argv[2] == nullptr) {
        return 91;
    }
    if (std::string_view{argv[0]} != "received_modulex" ||
        std::string_view{argv[1]} != "alpha" ||
        std::string_view{argv[2]} != "beta") {
        return 92;
    }
    if (api->time.now_ms() != 1357U) {
        return 93;
    }
    static constexpr char text[] = "received-modulex-app\n";
    if (api->console.write(text, sizeof(text) - 1U) != static_cast<int>(sizeof(text) - 1U)) {
        return 94;
    }
    api->app.exit(44);
    return 11;
}

bool resolve_entry(std::string_view name, modulex::Addr& out_addr) noexcept {
    if (name == "charm_app_main") {
        out_addr = modulex::to_addr(reinterpret_cast<const void*>(&fake_charm_app_main));
        return true;
    }
    return false;
}

bool fail_resolve_entry(std::string_view, modulex::Addr&) noexcept {
    return false;
}

bool resolve_dependency(std::string_view name, std::string_view version) noexcept {
    return name == "host" && version == "v1";
}

bool fail_resolve_dependency(std::string_view, std::string_view) noexcept {
    return false;
}

struct ModuleXFixture {
    modulex::ImageHeader hdr{};
    std::array<std::byte, 4> text{};
    std::array<modulex::Symbol, 1> syms{};
    std::array<char, 24> strtab{};
    std::array<modulex::Dependency, 1> deps{};
};

void init_fixture(ModuleXFixture& image,
                  const bool include_symbol = true,
                  const bool include_dependency = false) {
    image = ModuleXFixture{};
    image.hdr.magic = modulex::k_magic;
    image.hdr.version = modulex::k_version;
    image.hdr.entry_offset = 0;
    image.hdr.text_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, text));
    image.hdr.text_size = static_cast<std::uint32_t>(image.text.size());
    image.hdr.sym_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, syms));
    image.hdr.sym_size = include_symbol ? static_cast<std::uint32_t>(sizeof(image.syms)) : 0U;
    image.hdr.str_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, strtab));
    image.hdr.str_size = static_cast<std::uint32_t>(image.strtab.size());
    image.hdr.dep_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, deps));
    image.hdr.dep_size = include_dependency ? static_cast<std::uint32_t>(sizeof(image.deps)) : 0U;
    image.hdr.image_size = static_cast<std::uint32_t>(sizeof(ModuleXFixture));

    if (include_symbol) {
        image.syms[0].name_offset = 0;
        image.syms[0].value = 0;
        image.syms[0].size = 0;
        image.syms[0].kind = modulex::SymbolKind::external;
    }

    constexpr char strings[] = "charm_app_main\0host\0v1\0";
    std::memcpy(image.strtab.data(), strings, sizeof(strings));
    if (include_dependency) {
        image.deps[0].name_offset = 15;
        image.deps[0].version_offset = 20;
    }
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
    if (storage == nullptr || storage->fail_read || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), storage->bytes.data() + offset, bytes.size());
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

struct ModuleXLoadCtx {
    app_abi::AppModuleXLoadConfig config{};
};

app_abi::AppLoadResult load_modulex(void* ctx,
                                    const app_abi::AppImage& image,
                                    const app_abi::AppLoadBuffer&) noexcept {
    const auto* load_ctx = static_cast<const ModuleXLoadCtx*>(ctx);
    const auto loaded = app_abi::app_modulex_load_image(
        image,
        load_ctx != nullptr ? load_ctx->config : app_abi::AppModuleXLoadConfig{});
    return loaded.load;
}

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

bool contains(const HostRuntime& host, std::string_view needle) {
    return std::string_view{host.console.data(), host.console_used}.find(needle) != std::string_view::npos;
}

loader::ByteTransportResult receive_payload(std::span<const std::byte> payload,
                                             MemoryStorage& storage,
                                             std::array<std::byte, 1024>& transport_buffer) {
    std::array<std::byte, 2048> stream{};
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 31,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   stream);
    if (built.code != loader::PacketStreamBuildCode::ok) {
        return {.code = loader::ByteTransportCode::invalid_argument};
    }

    loader::PacketRuntime packet_runtime{make_storage(storage)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{.buffer = transport_buffer, .max_payload_size = 128},
    };

    loader::ByteTransportResult result{};
    std::uint32_t offset = 0;
    while (offset < built.bytes_written) {
        const auto remaining = built.bytes_written - offset;
        const auto count = remaining < 17U ? remaining : 17U;
        result = transport.ingest(std::span<const std::byte>{stream.data() + offset, count});
        if (result.code != loader::ByteTransportCode::ok) {
            return result;
        }
        offset += count;
    }
    return transport.status();
}

bool stage_received_modulex(std::span<const std::byte> payload,
                            std::array<std::byte, 512>& received_cache,
                            std::array<std::byte, 512>& app_cache,
                            app_abi::AppImage& out_image,
                            loader::ByteTransportResult& out_status) {
    MemoryStorage storage{};
    std::array<std::byte, 1024> transport_buffer{};
    out_status = receive_payload(payload, storage, transport_buffer);
    if (!expect(out_status.code == loader::ByteTransportCode::ok &&
                    out_status.packet.receive.stage == loader::Stage::launch_ready,
                "packetstream reaches launch_ready")) {
        return false;
    }

    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = out_status.packet.receive,
        .manifest = out_status.packet.manifest,
        .storage = make_storage(storage),
        .output = received_cache,
    });
    if (!expect(read.code == loader::ReceivedImageReadCode::ok &&
                    read.bytes_read == payload.size(),
                "launch_ready ModuleX reads back from storage")) {
        return false;
    }
    if (!expect(std::memcmp(read.image.data(), payload.data(), payload.size()) == 0,
                "received ModuleX bytes match payload")) {
        return false;
    }

    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_modulex",
        .format = app_abi::AppImageFormat::modulex,
        .image = read.image,
        .verified = read.code == loader::ReceivedImageReadCode::ok,
        .cache = app_cache,
    });
    if (!expect(staged.code == app_abi::AppReceivedImageStageCode::ok &&
                    staged.bytes_copied == payload.size() &&
                    staged.image.format == app_abi::AppImageFormat::modulex,
                "received ModuleX stages as AppImage(format=modulex)")) {
        return false;
    }

    out_image = staged.image;
    return true;
}

bool expect_run_received_modulex() {
    ModuleXFixture fixture{};
    init_fixture(fixture, true, true);

    std::array<std::byte, 512> received_cache{};
    std::array<std::byte, 512> app_cache{};
    app_abi::AppImage image{};
    loader::ByteTransportResult status{};
    if (!stage_received_modulex(std::span<const std::byte>{
                                    reinterpret_cast<const std::byte*>(&fixture),
                                    sizeof(fixture),
                                },
                                received_cache,
                                app_cache,
                                image,
                                status)) {
        return false;
    }

    ModuleXLoadCtx load_ctx{
        .config = app_abi::AppModuleXLoadConfig{
            .resolve_external = resolve_entry,
            .resolve_dependency = resolve_dependency,
        },
    };
    app_abi::StagedAppImageSource staged_source_ctx{
        .image = image,
        .load_ctx = &load_ctx,
        .load = load_modulex,
    };
    auto source = app_abi::make_staged_app_image_source(staged_source_ctx);

    HostRuntime host{};
    g_host = &host;
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> runtime{};
    const auto result = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "received_modulex",
        .arg_text = "alpha beta",
    });

    return expect(result.stage == app_abi::AppRunStage::exit &&
                      result.code == app_abi::AppRunCode::ok &&
                      result.exited &&
                      result.exit_code == 11,
                  "received ModuleX reaches AppRuntime exit stage") &&
           expect(contains(host, "received-modulex-app"),
                  "received ModuleX receives console capability") &&
           expect(host.now_count == 1U, "received ModuleX receives time capability") &&
           expect(host.exit_requested == 44, "received ModuleX receives app.exit capability") &&
           expect(status.packet.receive.received_bytes == sizeof(fixture),
                  "received byte count matches ModuleX payload");
}

bool expect_failure_paths() {
    bool ok = true;
    ModuleXFixture fixture{};
    init_fixture(fixture);

    std::array<std::byte, 512> received_cache{};
    std::array<std::byte, 512> app_cache{};
    app_abi::AppImage image{};
    loader::ByteTransportResult status{};
    ok = stage_received_modulex(std::span<const std::byte>{
                                    reinterpret_cast<const std::byte*>(&fixture),
                                    sizeof(fixture),
                                },
                                received_cache,
                                app_cache,
                                image,
                                status) && ok;

    ModuleXLoadCtx fail_external_ctx{
        .config = app_abi::AppModuleXLoadConfig{
            .resolve_external = fail_resolve_entry,
        },
    };
    app_abi::StagedAppImageSource fail_external_source_ctx{
        .image = image,
        .load_ctx = &fail_external_ctx,
        .load = load_modulex,
    };
    auto fail_external_source = app_abi::make_staged_app_image_source(fail_external_source_ctx);
    HostRuntime host{};
    g_host = &host;
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> runtime{};
    auto result = runtime.run(app_abi::AppRunConfig{
        .source = &fail_external_source,
        .api = &api,
        .name = "received_modulex",
    });
    ok = expect(result.stage == app_abi::AppRunStage::load &&
                    result.code == app_abi::AppRunCode::load_failed &&
                    result.backend_error == static_cast<int>(app_abi::AppModuleXLoadCode::external_failed),
                "external resolver failure stays at AppRuntime load stage") && ok;

    init_fixture(fixture, true, true);
    std::array<std::byte, 512> received_cache_dep{};
    std::array<std::byte, 512> app_cache_dep{};
    app_abi::AppImage dep_image{};
    ok = stage_received_modulex(std::span<const std::byte>{
                                    reinterpret_cast<const std::byte*>(&fixture),
                                    sizeof(fixture),
                                },
                                received_cache_dep,
                                app_cache_dep,
                                dep_image,
                                status) && ok;
    ModuleXLoadCtx fail_dep_ctx{
        .config = app_abi::AppModuleXLoadConfig{
            .resolve_external = resolve_entry,
            .resolve_dependency = fail_resolve_dependency,
        },
    };
    app_abi::StagedAppImageSource fail_dep_source_ctx{
        .image = dep_image,
        .load_ctx = &fail_dep_ctx,
        .load = load_modulex,
    };
    auto fail_dep_source = app_abi::make_staged_app_image_source(fail_dep_source_ctx);
    result = runtime.run(app_abi::AppRunConfig{
        .source = &fail_dep_source,
        .api = &api,
        .name = "received_modulex",
    });
    ok = expect(result.stage == app_abi::AppRunStage::load &&
                    result.code == app_abi::AppRunCode::load_failed &&
                    result.backend_error == static_cast<int>(app_abi::AppModuleXLoadCode::dependency_failed),
                "dependency resolver failure stays at AppRuntime load stage") && ok;

    auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_modulex",
        .format = app_abi::AppImageFormat::modulex,
        .image = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(&fixture),
            sizeof(fixture),
        },
        .verified = false,
        .cache = app_cache,
    });
    ok = expect(staged.code == app_abi::AppReceivedImageStageCode::not_verified,
                "unverified ModuleX image is rejected before loader") && ok;

    std::array<std::byte, 16> tiny_cache{};
    staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_modulex",
        .format = app_abi::AppImageFormat::modulex,
        .image = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(&fixture),
            sizeof(fixture),
        },
        .verified = true,
        .cache = tiny_cache,
    });
    ok = expect(staged.code == app_abi::AppReceivedImageStageCode::buffer_too_small,
                "small ModuleX stage cache is rejected") && ok;

    MemoryStorage storage{};
    std::array<std::byte, 64> output{};
    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = loader::Result{.stage = loader::Stage::verified, .code = loader::Code::ok},
        .manifest = loader::ImageManifest{
            .load_address = 0x24060000U,
            .entry_address = 0x24060001U,
            .size_bytes = static_cast<std::uint32_t>(sizeof(fixture)),
        },
        .storage = make_storage(storage),
        .output = output,
    });
    ok = expect(read.code == loader::ReceivedImageReadCode::not_launch_ready,
                "non launch-ready ModuleX payload cannot be read for staging") && ok;

    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok = expect_run_received_modulex() && ok;
    ok = expect_failure_paths() && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-received-modulex-smoke] ok");
    return 0;
}
