#include "charm_app_api.h"
#include "charm_app_received_image.hpp"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"
#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_received_image.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace {

namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::array<std::byte, 256> bytes{};
    bool fail_read{false};
};

struct HostState {
    std::array<char, 256> console{};
    std::size_t console_used{0};
    int exit_requested{-1};
    int argc_seen{0};
};

HostState* g_host = nullptr;

int console_write(const char* text, const std::size_t len) {
    if (g_host == nullptr || text == nullptr) {
        return -1;
    }
    const auto remaining = g_host->console.size() - g_host->console_used;
    const auto count = len < remaining ? len : remaining;
    if (count != 0U) {
        std::memcpy(g_host->console.data() + g_host->console_used, text, count);
        g_host->console_used += count;
    }
    return static_cast<int>(len);
}

std::uint32_t now_ms() {
    return 2468U;
}

int display_describe(CharmAppDisplayMode*) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int display_present(const void*, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int input_poll(CharmAppInputState*) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int storage_open(const char*, int, int) {
    return -1;
}

int storage_read(int, void*, std::size_t) {
    return -1;
}

int storage_write(int, const void*, std::size_t) {
    return -1;
}

int storage_close(int) {
    return -1;
}

int afe_configure(std::uint32_t, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int afe_read(void*, std::size_t) {
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
        .display = CharmAppDisplayApi{.describe = display_describe, .present = display_present},
        .input = CharmAppInputApi{.poll = input_poll},
        .storage = CharmAppStorageApi{
            .open = storage_open,
            .read = storage_read,
            .write = storage_write,
            .close = storage_close,
        },
        .afe = CharmAppAfeApi{.configure = afe_configure, .read = afe_read},
        .app = CharmAppControlApi{.exit = app_exit},
    };
}

extern "C" int fake_charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    if (g_host == nullptr || api == nullptr || argv == nullptr) {
        return 90;
    }
    g_host->argc_seen = argc;
    constexpr char kMessage[] = "handoff app\n";
    if (api->console.write == nullptr ||
        api->console.write(kMessage, sizeof(kMessage) - 1U) < 0) {
        return 91;
    }
    if (argc < 2 || std::string_view{argv[0]} != "received_app" ||
        std::string_view{argv[1]} != "demo") {
        return 92;
    }
    if (api->app.exit != nullptr) {
        api->app.exit(33);
    }
    return 7;
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

loader::Storage make_storage(MemoryStorage& memory) {
    return loader::Storage{
        .ctx = &memory,
        .base_address = 0x24070000U,
        .capacity_bytes = static_cast<std::uint32_t>(memory.bytes.size()),
        .write = storage_write,
        .read = storage_read,
    };
}

struct StagedLoadCtx {
    CharmAppMainFn entry{fake_charm_app_main};
    app_abi::AppRunCode load_code{app_abi::AppRunCode::ok};
    int backend_error{0};
};

app_abi::AppLoadResult load_image(void* ctx,
                                  const app_abi::AppImage& image,
                                  const app_abi::AppLoadBuffer&) noexcept {
    auto* source = static_cast<StagedLoadCtx*>(ctx);
    if (source == nullptr) {
        return {.code = app_abi::AppRunCode::image_not_found};
    }
    if (source->load_code != app_abi::AppRunCode::ok) {
        return {.code = source->load_code, .backend_error = source->backend_error};
    }
    if (image.image_base == nullptr || image.image_size == 0U) {
        return {.code = app_abi::AppRunCode::load_failed, .backend_error = 1};
    }
    return app_abi::AppLoadResult{
        .code = app_abi::AppRunCode::ok,
        .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, source->entry),
    };
}

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

bool contains(const HostState& host, std::string_view needle) {
    return std::string_view{host.console.data(), host.console_used}.find(needle) != std::string_view::npos;
}

loader::ByteTransportResult receive_payload(std::span<const std::byte> payload,
                                             MemoryStorage& storage,
                                             loader::PacketRuntime& packet_runtime,
                                             loader::ByteTransportRuntime& transport) {
    std::array<std::byte, 512> stream{};
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 5,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   stream);
    if (built.code != loader::PacketStreamBuildCode::ok) {
        return {.code = loader::ByteTransportCode::invalid_argument};
    }

    loader::ByteTransportResult result{};
    std::uint32_t offset = 0;
    while (offset < built.bytes_written) {
        const auto remaining = built.bytes_written - offset;
        const auto count = remaining < 7U ? remaining : 7U;
        result = transport.ingest(std::span<const std::byte>{stream.data() + offset, count});
        if (result.code != loader::ByteTransportCode::ok) {
            return result;
        }
        offset += count;
    }
    (void)storage;
    (void)packet_runtime;
    return transport.status();
}

bool expect_app_run_from_payload() {
    const std::array<std::byte, 15> payload{
        std::byte{0x7f},
        std::byte{'E'},
        std::byte{'L'},
        std::byte{'F'},
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
        std::byte{0x13},
        std::byte{0x14},
        std::byte{0x15},
        std::byte{0x16},
        std::byte{0x17},
        std::byte{0x18},
        std::byte{0x19},
        std::byte{0x1a},
    };

    MemoryStorage storage{};
    std::array<std::byte, 128> transport_buffer{};
    loader::PacketRuntime packet_runtime{make_storage(storage)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{.buffer = transport_buffer, .max_payload_size = 64},
    };

    const auto received = receive_payload(payload, storage, packet_runtime, transport);
    if (!expect(received.code == loader::ByteTransportCode::ok &&
                    received.packet.receive.stage == loader::Stage::launch_ready,
                "packetstream reaches launch_ready")) {
        return false;
    }

    std::array<std::byte, 64> received_cache{};
    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = received.packet.receive,
        .manifest = received.packet.manifest,
        .storage = make_storage(storage),
        .output = received_cache,
    });
    if (!expect(read.code == loader::ReceivedImageReadCode::ok &&
                    read.bytes_read == payload.size(),
                "launch_ready image reads from storage")) {
        return false;
    }
    if (!expect(std::memcmp(read.image.data(), payload.data(), payload.size()) == 0,
                "received image bytes match payload")) {
        return false;
    }

    std::array<std::byte, 64> app_cache{};
    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_app",
        .format = app_abi::AppImageFormat::function,
        .image = read.image,
        .verified = read.code == loader::ReceivedImageReadCode::ok,
        .cache = app_cache,
    });
    if (!expect(staged.code == app_abi::AppReceivedImageStageCode::ok &&
                    staged.bytes_copied == payload.size(),
                "received image stages as AppImage")) {
        return false;
    }

    StagedLoadCtx source_ctx{};
    app_abi::StagedAppImageSource staged_source_ctx{
        .image = staged.image,
        .load_ctx = &source_ctx,
        .load = load_image,
    };
    auto source = app_abi::make_staged_app_image_source(staged_source_ctx);
    HostState host{};
    g_host = &host;
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> runtime{};
    const auto result = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "received_app",
        .arg_text = "demo",
    });

    return expect(result.exited && result.exit_code == 7, "AppRuntime recovers exit code") &&
           expect(host.exit_requested == 33, "app.exit capability is visible") &&
           expect(host.argc_seen == 2, "AppRuntime passes argv") &&
           expect(contains(host, "handoff app"), "AppRuntime passes console capability");
}

} // namespace

int main() {
    bool ok = true;
    ok = expect_app_run_from_payload() && ok;

    MemoryStorage storage{};
    std::array<std::byte, 32> output{};
    auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = loader::Result{.stage = loader::Stage::verified, .code = loader::Code::ok},
        .manifest = loader::ImageManifest{
            .load_address = 0x24070000U,
            .entry_address = 0x24070001U,
            .size_bytes = 4,
        },
        .storage = make_storage(storage),
        .output = output,
    });
    ok = expect(read.code == loader::ReceivedImageReadCode::not_launch_ready,
                "non launch-ready image is rejected") && ok;

    read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = loader::Result{.stage = loader::Stage::launch_ready, .code = loader::Code::ok},
        .manifest = loader::ImageManifest{
            .load_address = 0x24070000U,
            .entry_address = 0x24070001U,
            .size_bytes = 40,
        },
        .storage = make_storage(storage),
        .output = output,
    });
    ok = expect(read.code == loader::ReceivedImageReadCode::output_too_small,
                "small read output is rejected") && ok;

    storage.fail_read = true;
    read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = loader::Result{.stage = loader::Stage::launch_ready, .code = loader::Code::ok},
        .manifest = loader::ImageManifest{
            .load_address = 0x24070000U,
            .entry_address = 0x24070001U,
            .size_bytes = 4,
        },
        .storage = make_storage(storage),
        .output = output,
    });
    ok = expect(read.code == loader::ReceivedImageReadCode::read_failed,
                "storage read failure is surfaced") && ok;

    std::array<std::byte, 4> image{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    std::array<std::byte, 2> tiny_cache{};
    auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_app",
        .format = app_abi::AppImageFormat::function,
        .image = image,
        .verified = true,
        .cache = tiny_cache,
    });
    ok = expect(staged.code == app_abi::AppReceivedImageStageCode::buffer_too_small,
                "small app stage cache is rejected") && ok;

    std::array<std::byte, 8> cache{};
    staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_app",
        .format = app_abi::AppImageFormat::function,
        .image = {},
        .verified = true,
        .cache = cache,
    });
    ok = expect(staged.code == app_abi::AppReceivedImageStageCode::empty_image,
                "empty image is rejected") && ok;

    staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_app",
        .format = app_abi::AppImageFormat::elf,
        .image = image,
        .verified = false,
        .cache = cache,
    });
    ok = expect(staged.code == app_abi::AppReceivedImageStageCode::not_verified,
                "unverified app image is rejected") && ok;

    staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_app_name_that_is_far_too_long_for_the_staging_boundary",
        .format = app_abi::AppImageFormat::elf,
        .image = image,
        .verified = true,
        .cache = cache,
    });
    ok = expect(staged.code == app_abi::AppReceivedImageStageCode::name_too_long,
                "long app image name is rejected") && ok;

    StagedLoadCtx failing_load_ctx{
        .load_code = app_abi::AppRunCode::load_failed,
        .backend_error = 123,
    };
    app_abi::StagedAppImageSource failing_source_ctx{
        .image = app_abi::AppImage{
            .name = "received_app",
            .format = app_abi::AppImageFormat::function,
            .image_base = image.data(),
            .image_size = image.size(),
        },
        .load_ctx = &failing_load_ctx,
        .load = load_image,
    };
    auto failing_source = app_abi::make_staged_app_image_source(failing_source_ctx);
    HostState host{};
    g_host = &host;
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> runtime{};
    const auto failed_run = runtime.run(app_abi::AppRunConfig{
        .source = &failing_source,
        .api = &api,
        .name = "received_app",
    });
    ok = expect(failed_run.stage == app_abi::AppRunStage::load &&
                    failed_run.code == app_abi::AppRunCode::load_failed &&
                    failed_run.backend_error == 123,
                "loader failure is reported through AppRuntime") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-app-handoff-smoke] ok");
    return 0;
}
