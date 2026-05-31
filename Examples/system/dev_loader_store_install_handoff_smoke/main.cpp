#include "charm_app_api.h"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"
#include "charm_app_store.hpp"
#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_store_handoff.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace {

namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::array<std::byte, 1024> bytes{};
    bool fail_read{false};
};

struct MemoryNor {
    std::array<std::byte, 2048> bytes{};
    bool fail_erase{false};
    bool fail_write{false};
    bool corrupt_read{false};

    MemoryNor() {
        bytes.fill(std::byte{0xff});
    }
};

struct ReaderCtx {
    MemoryNor* nor{};
    std::uint32_t base_offset{};
    bool fail_entry_read{false};
};

struct HostRuntime {
    std::array<char, 256> console{};
    std::size_t console_used{0};
    int argc_seen{0};
    int exit_requested{-1};
};

struct StagedLoadCtx {
    CharmAppMainFn entry{nullptr};
    app_abi::AppImageFormat expected_format{app_abi::AppImageFormat::function};
    app_abi::AppRunCode load_code{app_abi::AppRunCode::ok};
    int backend_error{0};
};

HostRuntime* g_host = nullptr;

int console_write(const char* text, const std::size_t len) {
    if (g_host == nullptr || text == nullptr) {
        return -1;
    }
    const auto remaining = g_host->console.size() - g_host->console_used;
    const auto copy = len < remaining ? len : remaining;
    if (copy != 0U) {
        std::memcpy(g_host->console.data() + g_host->console_used, text, copy);
        g_host->console_used += copy;
    }
    return static_cast<int>(len);
}

std::uint32_t now_ms() {
    return 1357U;
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
        .display = CharmAppDisplayApi{
            .describe = unsupported_display_describe,
            .present = unsupported_display_present,
        },
        .input = CharmAppInputApi{.poll = unsupported_input_poll},
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
    constexpr char kMessage[] = "store handoff app\n";
    if (api->console.write == nullptr ||
        api->console.write(kMessage, sizeof(kMessage) - 1U) < 0) {
        return 91;
    }
    if (api->time.now_ms == nullptr || api->time.now_ms() != 1357U) {
        return 92;
    }
    const std::string_view app_name{argv[0]};
    if (argc != 3 || (app_name != "hello_app" && app_name != "modulex_hello_app") ||
        std::string_view{argv[1]} != "alpha" || std::string_view{argv[2]} != "beta") {
        return 93;
    }
    if (api->app.exit != nullptr) {
        api->app.exit(44);
    }
    return 12;
}

bool storage_write_bytes(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(storage->bytes.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool storage_read_bytes(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
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
        .base_address = 0x24040000U,
        .capacity_bytes = static_cast<std::uint32_t>(storage.bytes.size()),
        .write = storage_write_bytes,
        .read = storage_read_bytes,
    };
}

bool nor_erase(void* ctx, std::uint32_t offset, std::uint32_t size) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || nor->fail_erase || offset > nor->bytes.size() ||
        size > (nor->bytes.size() - offset) || (offset % 64U) != 0U || (size % 64U) != 0U) {
        return false;
    }
    for (std::uint32_t i = 0; i < size; ++i) {
        nor->bytes[offset + i] = std::byte{0xff};
    }
    return true;
}

bool nor_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || nor->fail_write || offset > nor->bytes.size() ||
        bytes.size() > (nor->bytes.size() - offset) || (offset % 4U) != 0U) {
        return false;
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto current = static_cast<unsigned>(nor->bytes[offset + i]);
        const auto next = static_cast<unsigned>(bytes[i]);
        if ((~current & next) != 0U) {
            return false;
        }
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        nor->bytes[offset + i] &= bytes[i];
    }
    return true;
}

bool nor_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || offset > nor->bytes.size() ||
        bytes.size() > (nor->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), nor->bytes.data() + offset, bytes.size());
    if (nor->corrupt_read && !bytes.empty()) {
        bytes.back() ^= std::byte{0x01};
    }
    return true;
}

app_abi::AppStoreWritableMedia make_media(MemoryNor& nor) {
    return app_abi::AppStoreWritableMedia{
        .ctx = &nor,
        .capacity = static_cast<std::uint32_t>(nor.bytes.size()),
        .erase_block_size = 64,
        .write_align = 4,
        .erase = nor_erase,
        .write = nor_write,
        .read = nor_read,
    };
}

bool reader_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* reader = static_cast<ReaderCtx*>(ctx);
    if (reader == nullptr || reader->nor == nullptr) {
        return false;
    }
    if (reader->fail_entry_read && offset >= sizeof(app_abi::AppStoreHeader) &&
        offset < (sizeof(app_abi::AppStoreHeader) + sizeof(app_abi::AppStoreEntry))) {
        return false;
    }
    return nor_read(reader->nor, reader->base_offset + offset, bytes);
}

app_abi::AppStoreReader make_reader(ReaderCtx& ctx) {
    return app_abi::AppStoreReader{
        .ctx = &ctx,
        .read = reader_read,
    };
}

app_abi::AppLoadResult load_staged_function(void* ctx,
                                            const app_abi::AppImage& image,
                                            const app_abi::AppLoadBuffer&) noexcept {
    auto* load = static_cast<StagedLoadCtx*>(ctx);
    if (load == nullptr) {
        return {.code = app_abi::AppRunCode::image_not_found};
    }
    if (load->load_code != app_abi::AppRunCode::ok) {
        return {.code = load->load_code, .backend_error = load->backend_error};
    }
    if (image.format != load->expected_format) {
        return {.code = app_abi::AppRunCode::not_supported, .backend_error = 44};
    }
    return app_abi::AppLoadResult{
        .code = app_abi::AppRunCode::ok,
        .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, load->entry),
    };
}

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

bool contains(const HostRuntime& runtime, const std::string_view needle) {
    return std::string_view{runtime.console.data(), runtime.console_used}.find(needle) != std::string_view::npos;
}

std::span<const std::byte> make_store_image(std::array<std::byte, 512>& store,
                                            app_abi::AppStoreBuildCode* code = nullptr) {
    const std::array<std::byte, 16> hello_payload{
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
        std::byte{0x1b},
    };
    const std::array<std::byte, 8> player_payload{
        std::byte{0x50},
        std::byte{0x4c},
        std::byte{0x41},
        std::byte{0x59},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
    };
    const std::array<std::byte, 12> modulex_payload{
        std::byte{'C'},
        std::byte{'H'},
        std::byte{'M'},
        std::byte{'M'},
        std::byte{0x02},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0xAA},
        std::byte{0xBB},
        std::byte{0xCC},
        std::byte{0xDD},
    };
    const std::array<app_abi::AppStoreBuildEntry, 3> entries{
        app_abi::AppStoreBuildEntry{.name = "hello_app", .payload = hello_payload},
        app_abi::AppStoreBuildEntry{.name = "player_min", .payload = player_payload},
        app_abi::AppStoreBuildEntry{
            .name = "modulex_hello_app",
            .payload = modulex_payload,
            .flags = app_abi::app_store_format_flags(app_abi::AppImageFormat::modulex),
        },
    };
    const auto built = app_abi::app_store_build_image(entries, store);
    if (code != nullptr) {
        *code = built.code;
    }
    return std::span<const std::byte>{store.data(), built.bytes_written};
}

loader::ByteTransportResult receive_payload(std::span<const std::byte> payload,
                                             MemoryStorage& storage,
                                             std::array<std::byte, 1536>& stream,
                                             std::array<std::byte, 256>& transport_buffer) {
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
        loader::ByteTransportConfig{
            .buffer = transport_buffer,
            .max_payload_size = 64,
        },
    };

    loader::ByteTransportResult result{};
    std::uint32_t cursor = 0;
    while (cursor < built.bytes_written) {
        const auto remaining = built.bytes_written - cursor;
        const auto count = remaining < 17U ? remaining : 17U;
        result = transport.ingest(std::span<const std::byte>{stream.data() + cursor, count});
        if (result.code != loader::ByteTransportCode::ok) {
            return result;
        }
        cursor += count;
    }
    return transport.status();
}

bool expect_success_path() {
    bool ok = true;
    std::array<std::byte, 512> store{};
    app_abi::AppStoreBuildCode build_code{};
    const auto store_image = make_store_image(store, &build_code);
    ok = expect(build_code == app_abi::AppStoreBuildCode::ok, "store image builds") && ok;

    MemoryStorage storage{};
    std::array<std::byte, 1536> packet_stream{};
    std::array<std::byte, 256> transport_buffer{};
    const auto status = receive_payload(store_image, storage, packet_stream, transport_buffer);
    ok = expect(status.code == loader::ByteTransportCode::ok &&
                    status.packet.receive.stage == loader::Stage::launch_ready,
                "appstore packetstream reaches launch_ready") && ok;

    std::array<std::byte, 512> received_cache{};
    MemoryNor nor{};
    const std::uint32_t store_offset = 128;
    const auto installed = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = make_media(nor),
        .target_offset = store_offset,
    });
    ok = expect(installed.received.code == loader::ReceivedImageReadCode::ok,
                "received appstore reads from RAM") && ok;
    ok = expect(installed.store_code == app_abi::AppStoreReadCode::ok,
                "received appstore header validates before install") && ok;
    ok = expect(installed.install.code == app_abi::AppStoreInstallCode::ok,
                "received appstore installs into NOR") && ok;
    ok = expect(installed.install.bytes_written == store_image.size(),
                "install writes received appstore bytes") && ok;

    ReaderCtx reader_ctx{.nor = &nor, .base_offset = store_offset};
    const auto reader = make_reader(reader_ctx);
    app_abi::AppStoreHeader header{};
    ok = expect(app_abi::app_store_read_header(reader, header) == app_abi::AppStoreReadCode::ok &&
                    header.entry_count == 3,
                "installed store header reads back") && ok;

    std::array<std::byte, 64> stage_cache{};
    const auto staged = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = reader,
        .name = "hello_app",
        .cache = stage_cache,
        .format = app_abi::AppImageFormat::function,
    });
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok &&
                    staged.image.name == "hello_app" &&
                    staged.image.format == app_abi::AppImageFormat::function,
                "installed store stages named image") && ok;

    StagedLoadCtx load_ctx{
        .entry = fake_charm_app_main,
        .expected_format = app_abi::AppImageFormat::function,
    };
    app_abi::StagedAppImageSource source_ctx{
        .image = staged.image,
        .load_ctx = &load_ctx,
        .load = load_staged_function,
    };
    auto source = app_abi::make_staged_app_image_source(source_ctx);
    HostRuntime host{};
    g_host = &host;
    auto api = make_api();
    app_abi::AppRuntime<> runtime{};
    const auto run = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "hello_app",
        .arg_text = "alpha beta",
    });
    ok = expect(run.stage == app_abi::AppRunStage::exit &&
                    run.code == app_abi::AppRunCode::ok &&
                    run.exited &&
                    run.exit_code == 12,
                "staged store image runs through AppRuntime") && ok;
    ok = expect(host.exit_requested == 44 &&
                    host.argc_seen == 3 &&
                    contains(host, "store handoff app"),
                "AppRuntime exposes capabilities and argv for store image") && ok;

    std::array<std::byte, 64> modulex_stage_cache{};
    const auto staged_modulex = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = reader,
        .name = "modulex_hello_app",
        .cache = modulex_stage_cache,
    });
    ok = expect(staged_modulex.code == app_abi::AppStoreReadCode::ok &&
                    staged_modulex.image.name == "modulex_hello_app" &&
                    staged_modulex.image.format == app_abi::AppImageFormat::modulex,
                "installed mixed store stages ModuleX entry by flags") && ok;
    StagedLoadCtx modulex_load_ctx{
        .entry = fake_charm_app_main,
        .expected_format = app_abi::AppImageFormat::modulex,
    };
    app_abi::StagedAppImageSource modulex_source_ctx{
        .image = staged_modulex.image,
        .load_ctx = &modulex_load_ctx,
        .load = load_staged_function,
    };
    auto modulex_source = app_abi::make_staged_app_image_source(modulex_source_ctx);
    HostRuntime modulex_host{};
    g_host = &modulex_host;
    const auto modulex_run = runtime.run(app_abi::AppRunConfig{
        .source = &modulex_source,
        .api = &api,
        .name = "modulex_hello_app",
        .arg_text = "alpha beta",
    });
    ok = expect(modulex_run.stage == app_abi::AppRunStage::exit &&
                    modulex_run.code == app_abi::AppRunCode::ok &&
                    modulex_run.exited &&
                    modulex_run.exit_code == 12,
                "store ModuleX entry uses same staged source and AppRuntime") && ok;

    return ok;
}

bool expect_error_paths() {
    bool ok = true;
    std::array<std::byte, 512> store{};
    const auto store_image = make_store_image(store);

    MemoryStorage storage{};
    std::array<std::byte, 1536> packet_stream{};
    std::array<std::byte, 256> transport_buffer{};
    const auto status = receive_payload(store_image, storage, packet_stream, transport_buffer);
    ok = expect(status.packet.receive.stage == loader::Stage::launch_ready,
                "error path fixture reaches launch_ready") && ok;

    std::array<std::byte, 512> received_cache{};
    MemoryNor nor{};
    const auto not_ready = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = loader::Result{.stage = loader::Stage::verified, .code = loader::Code::ok},
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = make_media(nor),
    });
    ok = expect(not_ready.received.code == loader::ReceivedImageReadCode::not_launch_ready,
                "store install rejects non-launch-ready receive state") && ok;

    std::array<std::byte, 8> tiny_cache{};
    const auto too_small = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = tiny_cache,
        },
        .media = make_media(nor),
    });
    ok = expect(too_small.received.code == loader::ReceivedImageReadCode::output_too_small,
                "store install reports received cache too small") && ok;

    std::array<std::byte, 4> invalid_payload{
        std::byte{'n'},
        std::byte{'o'},
        std::byte{'p'},
        std::byte{'e'},
    };
    MemoryStorage invalid_storage{};
    const auto invalid_status = receive_payload(invalid_payload, invalid_storage, packet_stream, transport_buffer);
    const auto invalid_store = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = invalid_status.packet.receive,
            .manifest = invalid_status.packet.manifest,
            .storage = make_storage(invalid_storage),
            .output = received_cache,
        },
        .media = make_media(nor),
    });
    ok = expect(invalid_store.received.code == loader::ReceivedImageReadCode::ok &&
                    invalid_store.store_code == app_abi::AppStoreReadCode::header_unreadable,
                "store install rejects invalid store payload before erase") && ok;

    MemoryNor small_nor{};
    auto small_media = make_media(small_nor);
    small_media.capacity = 64;
    const auto image_too_large = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = small_media,
    });
    ok = expect(image_too_large.install.code == app_abi::AppStoreInstallCode::image_too_large,
                "store install reports media too small") && ok;

    MemoryNor erase_fail{};
    erase_fail.fail_erase = true;
    const auto erase_failed = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = make_media(erase_fail),
    });
    ok = expect(erase_failed.install.code == app_abi::AppStoreInstallCode::erase_failed,
                "store install propagates erase failure") && ok;

    MemoryNor write_fail{};
    write_fail.fail_write = true;
    const auto write_failed = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = make_media(write_fail),
    });
    ok = expect(write_failed.install.code == app_abi::AppStoreInstallCode::write_failed,
                "store install propagates write failure") && ok;

    MemoryNor verify_fail{};
    verify_fail.corrupt_read = true;
    const auto verify_failed = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = make_media(verify_fail),
    });
    ok = expect(verify_failed.install.code == app_abi::AppStoreInstallCode::verify_failed,
                "store install propagates readback verify failure") && ok;

    MemoryNor installed_nor{};
    const auto installed = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = make_media(installed_nor),
        .target_offset = 0,
    });
    ok = expect(installed.install.code == app_abi::AppStoreInstallCode::ok,
                "error path fixture installs") && ok;

    ReaderCtx reader_ctx{.nor = &installed_nor};
    const auto reader = make_reader(reader_ctx);
    std::array<std::byte, 64> cache{};
    const auto missing = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = reader,
        .name = "missing",
        .cache = cache,
    });
    ok = expect(missing.code == app_abi::AppStoreReadCode::image_not_found,
                "store stage reports missing name") && ok;

    app_abi::AppStoreHeader bad_header{};
    std::memcpy(installed_nor.bytes.data(), &bad_header, sizeof(bad_header));
    const auto bad_header_stage = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = reader,
        .name = "hello_app",
        .cache = cache,
    });
    ok = expect(bad_header_stage.code == app_abi::AppStoreReadCode::header_invalid,
                "store stage reports bad header") && ok;

    MemoryNor entry_fail_nor{};
    const auto entry_fixture = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
        .received = loader::ReceivedImageReadConfig{
            .status = status.packet.receive,
            .manifest = status.packet.manifest,
            .storage = make_storage(storage),
            .output = received_cache,
        },
        .media = make_media(entry_fail_nor),
    });
    ok = expect(entry_fixture.install.code == app_abi::AppStoreInstallCode::ok,
                "entry read fixture installs") && ok;
    ReaderCtx entry_fail_ctx{.nor = &entry_fail_nor, .fail_entry_read = true};
    const auto entry_failed = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = make_reader(entry_fail_ctx),
        .name = "hello_app",
        .cache = cache,
    });
    ok = expect(entry_failed.code == app_abi::AppStoreReadCode::entry_read_failed,
                "store stage reports entry read failure") && ok;

    std::array<std::byte, 4> tiny_stage{};
    ReaderCtx too_large_ctx{.nor = &entry_fail_nor};
    const auto stage_too_large = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = make_reader(too_large_ctx),
        .name = "hello_app",
        .cache = tiny_stage,
    });
    ok = expect(stage_too_large.code == app_abi::AppStoreReadCode::image_too_large,
                "store stage reports image too large for cache") && ok;

    ReaderCtx run_ctx{.nor = &entry_fail_nor};
    const auto staged = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = make_reader(run_ctx),
        .name = "hello_app",
        .cache = cache,
        .format = app_abi::AppImageFormat::function,
    });
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok,
                "run error fixture stages") && ok;
    StagedLoadCtx load_ctx{
        .entry = fake_charm_app_main,
        .expected_format = app_abi::AppImageFormat::function,
        .load_code = app_abi::AppRunCode::load_failed,
        .backend_error = 77,
    };
    app_abi::StagedAppImageSource source_ctx{
        .image = staged.image,
        .load_ctx = &load_ctx,
        .load = load_staged_function,
    };
    auto source = app_abi::make_staged_app_image_source(source_ctx);
    HostRuntime host{};
    g_host = &host;
    auto api = make_api();
    app_abi::AppRuntime<> runtime{};
    const auto run_failed = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "hello_app",
    });
    ok = expect(run_failed.stage == app_abi::AppRunStage::load &&
                    run_failed.code == app_abi::AppRunCode::load_failed &&
                    run_failed.backend_error == 77,
                "store-backed AppRuntime preserves loader failure") && ok;

    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok = expect_success_path() && ok;
    ok = expect_error_paths() && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-store-install-handoff-smoke] ok");
    return 0;
}
