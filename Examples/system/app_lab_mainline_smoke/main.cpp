#include "charm_app_api.h"
#include "charm_app_runtime.hpp"
#include "charm_app_store.hpp"
#include "charm_app_store_install.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

extern "C" int hello_app_main(const CharmAppApi* api, int argc, char** argv);
extern "C" int player_min_main(const CharmAppApi* api, int argc, char** argv);

namespace {

namespace app_abi = charm::app_abi;
using namespace std::literals::string_view_literals;

constexpr std::size_t kQspiStoreBytes = 4096U;
constexpr std::size_t kQspiCacheBytes = 1024U;
constexpr std::uint32_t kQspiStoreBaseOffset = 0U;

struct HostState {
    std::array<char, 2048> console{};
    std::size_t console_used{0};
    std::uint32_t present_count{0};
    std::uint32_t input_poll_count{0};
    std::uint32_t last_present_bytes{0};
    int requested_exit{0};
};

struct RuntimeState {
    std::string_view last_request{"-"sv};
    std::string_view last_app{"-"sv};
    std::string_view last_source{"none"sv};
    bool last_exited{false};
    int last_exit_code{0};
    app_abi::AppRunCode last_code{app_abi::AppRunCode::ok};
    int last_backend_error{0};
    std::string_view last_stage{"idle"sv};
    bool store_install_attempted{false};
    bool store_install_backend_ready{false};
    app_abi::AppStoreInstallCode store_install_code{app_abi::AppStoreInstallCode::invalid_argument};
    std::uint32_t store_install_target{0U};
    std::uint32_t store_install_written{0U};
    std::uint32_t store_install_erased{0U};
};

struct MemoryNor {
    std::array<std::byte, kQspiStoreBytes> bytes{};
    std::uint32_t erase_block_size{64U};
    std::uint32_t write_align{4U};
    bool fail_erase{false};
    bool fail_write{false};
    bool fail_read{false};

    MemoryNor() {
        bytes.fill(std::byte{0xff});
    }
};

struct QspiState {
    bool ready{true};
    std::uint32_t read_count{0U};
    std::uint32_t read_fail_count{0U};
    std::uint32_t write_count{0U};
    std::uint32_t write_fail_count{0U};
    std::uint32_t erase_count{0U};
    std::uint32_t erase_fail_count{0U};
};

struct MediaContext {
    MemoryNor* nor{nullptr};
    QspiState* qspi{nullptr};
};

struct StoreReaderContext {
    MemoryNor* nor{nullptr};
    QspiState* qspi{nullptr};
    std::uint32_t base_offset{0U};
};

struct SourceEntry {
    app_abi::AppImage image{};
    CharmAppMainFn entry{nullptr};
};

struct FunctionSourceContext {
    std::span<const SourceEntry> entries{};
    const app_abi::AppImage* staged_image{nullptr};
    CharmAppMainFn staged_entry{nullptr};
    bool staged_only{false};
};

struct RunOutcome {
    std::optional<int> code{};
    HostState host{};
};

HostState* g_host = nullptr;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

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
    return 4321U;
}

int display_describe(CharmAppDisplayMode* out_mode) {
    if (out_mode == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    *out_mode = CharmAppDisplayMode{
        .width = 720U,
        .height = 1280U,
        .stride_bytes = 720U * 4U,
        .format = CHARM_APP_PIXEL_FORMAT_ARGB8888,
    };
    return CHARM_APP_STATUS_OK;
}

int display_present(const void* pixels, const std::uint32_t bytes) {
    if (g_host == nullptr || pixels == nullptr || bytes == 0U) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    ++g_host->present_count;
    g_host->last_present_bytes = bytes;
    return CHARM_APP_STATUS_OK;
}

int input_poll(CharmAppInputState* out_state) {
    if (g_host == nullptr || out_state == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    ++g_host->input_poll_count;
    *out_state = CharmAppInputState{
        .encoder1_delta = 1,
        .encoder2_delta = -1,
        .encoder1_pressed = 0,
        .encoder2_pressed = 1,
        .pointer_detected = 1,
        .pointer_down = 0,
        .pointer_x = 10,
        .pointer_y = 20,
        .pointer_max_x = 720,
        .pointer_max_y = 1280,
    };
    return CHARM_APP_STATUS_OK;
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
        g_host->requested_exit = code;
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

void begin_last_result(RuntimeState& runtime,
                       std::string_view source,
                       std::string_view request,
                       std::string_view image) noexcept {
    runtime.last_request = request.empty() ? "-"sv : request;
    runtime.last_app = image.empty() ? "-"sv : image;
    runtime.last_source = source.empty() ? "none"sv : source;
    runtime.last_stage = "lookup"sv;
    runtime.last_code = app_abi::AppRunCode::ok;
    runtime.last_backend_error = 0;
    runtime.last_exited = false;
    runtime.last_exit_code = 0;
}

void set_failure(RuntimeState& runtime,
                 std::string_view stage,
                 app_abi::AppRunCode code,
                 int backend_error = 0) noexcept {
    runtime.last_stage = stage;
    runtime.last_code = code;
    runtime.last_backend_error = backend_error;
    runtime.last_exited = false;
}

void record_result(RuntimeState& runtime, const app_abi::AppRunResult& result) noexcept {
    runtime.last_app = result.name;
    runtime.last_stage = app_abi::stage_name(result.stage);
    runtime.last_code = result.code;
    runtime.last_backend_error = result.backend_error;
    runtime.last_exited = result.exited;
    runtime.last_exit_code = result.exited ? result.exit_code : 0;
}

bool nor_erase(void* ctx, std::uint32_t offset, std::uint32_t size) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || nor->fail_erase || size == 0U ||
        offset > nor->bytes.size() || size > (nor->bytes.size() - offset) ||
        (offset % nor->erase_block_size) != 0U || (size % nor->erase_block_size) != 0U) {
        return false;
    }
    for (std::uint32_t i = 0; i < size; ++i) {
        nor->bytes[offset + i] = std::byte{0xff};
    }
    return true;
}

bool nor_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || nor->fail_write || bytes.empty() ||
        offset > nor->bytes.size() || bytes.size() > (nor->bytes.size() - offset) ||
        (offset % nor->write_align) != 0U) {
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
    if (nor == nullptr || nor->fail_read ||
        offset > nor->bytes.size() || bytes.size() > (nor->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), nor->bytes.data() + offset, bytes.size());
    return true;
}

bool media_erase(void* ctx, std::uint32_t offset, std::uint32_t size) noexcept {
    auto* media = static_cast<MediaContext*>(ctx);
    if (media == nullptr || media->nor == nullptr || media->qspi == nullptr) {
        return false;
    }
    const bool ok = nor_erase(media->nor, offset, size);
    if (ok) {
        ++media->qspi->erase_count;
    } else {
        ++media->qspi->erase_fail_count;
    }
    return ok;
}

bool media_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* media = static_cast<MediaContext*>(ctx);
    if (media == nullptr || media->nor == nullptr || media->qspi == nullptr) {
        return false;
    }
    const bool ok = nor_write(media->nor, offset, bytes);
    if (ok) {
        ++media->qspi->write_count;
    } else {
        ++media->qspi->write_fail_count;
    }
    return ok;
}

bool media_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* media = static_cast<MediaContext*>(ctx);
    if (media == nullptr || media->nor == nullptr || media->qspi == nullptr) {
        return false;
    }
    const bool ok = nor_read(media->nor, offset, bytes);
    if (ok) {
        ++media->qspi->read_count;
    } else {
        ++media->qspi->read_fail_count;
    }
    return ok;
}

app_abi::AppStoreWritableMedia make_media(MemoryNor& nor, QspiState& qspi) {
    static MediaContext ctx{};
    ctx = MediaContext{
        .nor = &nor,
        .qspi = &qspi,
    };
    return app_abi::AppStoreWritableMedia{
        .ctx = &ctx,
        .capacity = static_cast<std::uint32_t>(nor.bytes.size()),
        .erase_block_size = nor.erase_block_size,
        .write_align = nor.write_align,
        .erase = media_erase,
        .write = media_write,
        .read = media_read,
    };
}

app_abi::AppStoreReader make_reader(StoreReaderContext& ctx, QspiState& qspi) {
    ctx.qspi = &qspi;
    return app_abi::AppStoreReader{
        .ctx = &ctx,
        .read = [](void* raw, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            auto* local = static_cast<StoreReaderContext*>(raw);
            if (local == nullptr || local->nor == nullptr || local->qspi == nullptr) {
                return false;
            }
            const bool ok = nor_read(local->nor, local->base_offset + offset, bytes);
            if (ok) {
                ++local->qspi->read_count;
            } else {
                ++local->qspi->read_fail_count;
            }
            return ok;
        },
    };
}

app_abi::AppLoadResult load_function_image(void* ctx,
                                           const app_abi::AppImage& image,
                                           const app_abi::AppLoadBuffer&) noexcept {
    auto* source = static_cast<FunctionSourceContext*>(ctx);
    if (source == nullptr) {
        return {.code = app_abi::AppRunCode::invalid_argument};
    }
    if (source->staged_only &&
        source->staged_image != nullptr &&
        source->staged_image->name == image.name &&
        source->staged_entry != nullptr) {
        return app_abi::AppLoadResult{
            .code = app_abi::AppRunCode::ok,
            .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, source->staged_entry),
        };
    }
    for (const auto& entry : source->entries) {
        if (entry.image.name != image.name) {
            continue;
        }
        return app_abi::AppLoadResult{
            .code = app_abi::AppRunCode::ok,
            .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, entry.entry),
        };
    }
    return {.code = app_abi::AppRunCode::image_not_found};
}

const app_abi::AppImage* find_function_image(void* ctx, std::string_view name) noexcept {
    auto* source = static_cast<FunctionSourceContext*>(ctx);
    if (source == nullptr) {
        return nullptr;
    }
    if (source->staged_only) {
        return (source->staged_image != nullptr && source->staged_image->name == name) ? source->staged_image : nullptr;
    }
    for (const auto& entry : source->entries) {
        if (entry.image.name == name) {
            return &entry.image;
        }
    }
    return nullptr;
}

RunOutcome run_runtime_image(RuntimeState& runtime,
                            const app_abi::AppImageSource& source,
                            std::string_view name,
                            std::string_view arg_text) {
    RunOutcome outcome{};
    HostState& host = outcome.host;
    g_host = &host;
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> app_runtime{};
    const auto result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = name,
        .arg_text = arg_text,
    });
    record_result(runtime, result);
    if (result.code != app_abi::AppRunCode::ok || !result.exited) {
        return outcome;
    }
    outcome.code = result.exit_code;
    return outcome;
}

RunOutcome run_builtin(RuntimeState& runtime,
                       const app_abi::AppImageSource& source,
                       std::string_view name,
                       std::string_view arg_text) {
    begin_last_result(runtime, "embedded"sv, name, name);
    return run_runtime_image(runtime, source, name, arg_text);
}

bool install_qspi_store(RuntimeState& runtime,
                        MemoryNor& nor,
                        QspiState& qspi,
                        std::span<const std::byte> store_image) {
    runtime.store_install_attempted = true;
    runtime.store_install_backend_ready = qspi.ready;
    runtime.store_install_target = kQspiStoreBaseOffset;
    runtime.store_install_written = 0U;
    runtime.store_install_erased = 0U;
    runtime.store_install_code = app_abi::AppStoreInstallCode::invalid_argument;

    if (!qspi.ready) {
        return false;
    }

    const auto result = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(nor, qspi),
        .target_offset = kQspiStoreBaseOffset,
        .image = store_image,
    });
    runtime.store_install_code = result.code;
    runtime.store_install_target = result.target_offset;
    runtime.store_install_written = result.bytes_written;
    runtime.store_install_erased = result.bytes_erased;
    return result.code == app_abi::AppStoreInstallCode::ok;
}

RunOutcome run_qspi_named(RuntimeState& runtime,
                          std::string_view name,
                          std::string_view arg_text,
                          app_abi::AppStoreReader reader,
                          std::span<std::byte> cache,
                          FunctionSourceContext& staged_source_ctx) {
    static std::array<char, 64> request_storage{};
    std::snprintf(request_storage.data(), request_storage.size(), "qspi:%.*s",
                  static_cast<int>(name.size()), name.data());
    begin_last_result(runtime, "qspi-named"sv, request_storage.data(), name);

    const auto staged = app_abi::app_store_stage_named_image(reader, name, cache);
    if (staged.code != app_abi::AppStoreReadCode::ok) {
        const auto run_code = (staged.code == app_abi::AppStoreReadCode::image_not_found ||
                               staged.code == app_abi::AppStoreReadCode::header_invalid ||
                               staged.code == app_abi::AppStoreReadCode::header_unreadable ||
                               staged.code == app_abi::AppStoreReadCode::entry_read_failed)
            ? app_abi::AppRunCode::image_not_found
            : app_abi::AppRunCode::load_failed;
        set_failure(runtime, "qspi-stage"sv, run_code);
        return {};
    }

    staged_source_ctx.staged_image = &staged.image;
    staged_source_ctx.staged_entry = nullptr;
    for (const auto& entry : staged_source_ctx.entries) {
        if (entry.image.name == staged.image.name) {
            staged_source_ctx.staged_entry = entry.entry;
            break;
        }
    }
    app_abi::AppImageSource staged_source{
        .ctx = &staged_source_ctx,
        .find = find_function_image,
        .load = load_function_image,
    };
    return run_runtime_image(runtime, staged_source, staged.image.name, arg_text);
}

RunOutcome run_qspi_raw(RuntimeState& runtime,
                        const app_abi::AppStoreEntry& entry,
                        CharmAppMainFn expected_entry,
                        std::string_view arg_text,
                        app_abi::AppStoreReader reader,
                        std::span<std::byte> cache,
                        FunctionSourceContext& staged_source_ctx) {
    static std::array<char, 64> request_storage{};
    std::snprintf(request_storage.data(),
                  request_storage.size(),
                  "qspi:@%08x:%08x",
                  static_cast<unsigned>(entry.offset),
                  static_cast<unsigned>(entry.size));
    begin_last_result(runtime, "qspi-raw"sv, request_storage.data(), "qspi_app"sv);

    const auto staged = app_abi::app_store_stage_raw_image(reader, "qspi_app"sv, entry.offset, entry.size, cache);
    if (staged.code != app_abi::AppStoreReadCode::ok) {
        set_failure(runtime, "qspi-read"sv, app_abi::AppRunCode::load_failed);
        return {};
    }

    staged_source_ctx.staged_image = &staged.image;
    staged_source_ctx.staged_entry = expected_entry;
    app_abi::AppImageSource staged_source{
        .ctx = &staged_source_ctx,
        .find = find_function_image,
        .load = load_function_image,
    };
    return run_runtime_image(runtime, staged_source, staged.image.name, arg_text);
}

std::optional<int> run_path(RuntimeState& runtime, std::string_view path) {
    begin_last_result(runtime, "file-backed"sv, path, path);
    set_failure(runtime, "file-backed"sv, app_abi::AppRunCode::not_supported);
    return std::nullopt;
}

std::string_view format_status(const RuntimeState& runtime) {
    static std::array<char, 512> buffer{};
    const int written = std::snprintf(
        buffer.data(),
        buffer.size(),
        "status: source embedded entries=2 readable=true valid=true qspi_ready=true qspi_readable=true qspi_valid=true\n"
        "status: store install attempted=%s ready=%s code=%.*s target=0x%08x written=%u erased=%u\n"
        "status: last request=%.*s image=%.*s source=%.*s stage=%.*s code=%.*s exited=%s exit=%d backend=%d\n",
        runtime.store_install_attempted ? "true" : "false",
        runtime.store_install_backend_ready ? "true" : "false",
        static_cast<int>(app_abi::app_store_install_code_name(runtime.store_install_code).size()),
        app_abi::app_store_install_code_name(runtime.store_install_code).data(),
        runtime.store_install_target,
        runtime.store_install_written,
        runtime.store_install_erased,
        static_cast<int>(runtime.last_request.size()),
        runtime.last_request.data(),
        static_cast<int>(runtime.last_app.size()),
        runtime.last_app.data(),
        static_cast<int>(runtime.last_source.size()),
        runtime.last_source.data(),
        static_cast<int>(runtime.last_stage.size()),
        runtime.last_stage.data(),
        static_cast<int>(app_abi::code_name(runtime.last_code).size()),
        app_abi::code_name(runtime.last_code).data(),
        runtime.last_exited ? "true" : "false",
        runtime.last_exit_code,
        runtime.last_backend_error);
    const auto clipped = written > 0 ? static_cast<std::size_t>(written) : 0U;
    return {buffer.data(), clipped};
}

} // namespace

int main() {
    bool ok = true;

    const std::array<SourceEntry, 2> entries{
        SourceEntry{
            .image = app_abi::AppImage{.name = "hello_app", .format = app_abi::AppImageFormat::function},
            .entry = hello_app_main,
        },
        SourceEntry{
            .image = app_abi::AppImage{.name = "player_min", .format = app_abi::AppImageFormat::function},
            .entry = player_min_main,
        },
    };
    FunctionSourceContext function_source_ctx{
        .entries = std::span<const SourceEntry>{entries.data(), entries.size()},
        .staged_image = nullptr,
        .staged_entry = nullptr,
        .staged_only = false,
    };
    FunctionSourceContext staged_source_ctx{
        .entries = function_source_ctx.entries,
        .staged_image = nullptr,
        .staged_entry = nullptr,
        .staged_only = true,
    };
    app_abi::AppImageSource function_source{
        .ctx = &function_source_ctx,
        .find = find_function_image,
        .load = load_function_image,
    };

    const std::array<std::byte, 256> hello_payload{
        std::byte{0x7f}, std::byte{'E'}, std::byte{'L'}, std::byte{'F'},
        std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13},
        std::byte{0x14}, std::byte{0x15}, std::byte{0x16}, std::byte{0x17},
        std::byte{0x18}, std::byte{0x19}, std::byte{0x1a}, std::byte{0x1b},
    };
    const std::array<std::byte, 64> player_payload{
        std::byte{0x7f}, std::byte{'E'}, std::byte{'L'}, std::byte{'F'},
        std::byte{0x21}, std::byte{0x22}, std::byte{0x23}, std::byte{0x24},
    };
    std::array<std::byte, 1024> store_buffer{};
    const std::array<app_abi::AppStoreBuildEntry, 2> store_entries{
        app_abi::AppStoreBuildEntry{.name = "hello_app", .payload = hello_payload},
        app_abi::AppStoreBuildEntry{.name = "player_min", .payload = player_payload},
    };
    const auto built = app_abi::app_store_build_image(store_entries, store_buffer);
    ok = expect(built.code == app_abi::AppStoreBuildCode::ok, "store image builds") && ok;

    RuntimeState runtime{};
    const auto embedded_hello = run_builtin(runtime, function_source, "hello_app", "demo");
    ok = expect(embedded_hello.code.has_value() && embedded_hello.code.value() == 0,
                "embedded hello_app exits with 0") && ok;
    ok = expect(runtime.last_source == "embedded"sv &&
                    runtime.last_stage == "exit"sv &&
                    runtime.last_code == app_abi::AppRunCode::ok &&
                    runtime.last_exited,
                "embedded hello diagnostics are stable") && ok;
    ok = expect(contains(std::string_view{embedded_hello.host.console.data(), embedded_hello.host.console_used},
                         "hello_app: argv1=demo"sv),
                "embedded hello_app receives argv") && ok;

    const auto embedded_player = run_builtin(runtime, function_source, "player_min", {});
    ok = expect(embedded_player.code.has_value() && embedded_player.code.value() == 0,
                "embedded player_min exits with 0") && ok;
    ok = expect(runtime.last_source == "embedded"sv &&
                    runtime.last_stage == "exit"sv &&
                    runtime.last_code == app_abi::AppRunCode::ok &&
                    runtime.last_exited,
                "embedded player diagnostics are stable") && ok;
    ok = expect(embedded_player.host.present_count == 1U &&
                    embedded_player.host.input_poll_count == 1U &&
                    embedded_player.host.last_present_bytes == (16U * 16U * 4U),
                "embedded player_min uses display/input capability path") && ok;

    MemoryNor nor{};
    QspiState qspi{};
    ok = expect(install_qspi_store(runtime,
                                   nor,
                                   qspi,
                                   std::span<const std::byte>{store_buffer.data(), built.bytes_written}),
                "qspi install succeeds") && ok;
    ok = expect(runtime.store_install_attempted &&
                    runtime.store_install_backend_ready &&
                    runtime.store_install_code == app_abi::AppStoreInstallCode::ok &&
                    runtime.store_install_target == 0U &&
                    runtime.store_install_written > 0U &&
                    runtime.store_install_erased > 0U,
                "qspi install diagnostics are stable") && ok;

    StoreReaderContext reader_ctx{
        .nor = &nor,
        .base_offset = kQspiStoreBaseOffset,
    };
    auto reader = make_reader(reader_ctx, qspi);
    std::array<std::byte, kQspiCacheBytes> qspi_cache{};

    const auto hello_entry = app_abi::app_store_find_entry(reader, "hello_app"sv);
    const auto player_entry = app_abi::app_store_find_entry(reader, "player_min"sv);
    ok = expect(hello_entry.code == app_abi::AppStoreReadCode::ok,
                "qspi hello entry is readable") && ok;
    ok = expect(player_entry.code == app_abi::AppStoreReadCode::ok,
                "qspi player entry is readable") && ok;

    const auto qspi_named_hello = run_qspi_named(runtime,
                                                 "hello_app"sv,
                                                 "demo"sv,
                                                 reader,
                                                 qspi_cache,
                                                 staged_source_ctx);
    ok = expect(qspi_named_hello.code.has_value() && qspi_named_hello.code.value() == 0,
                "qspi named hello exits with 0") && ok;
    ok = expect(runtime.last_source == "qspi-named"sv &&
                    runtime.last_stage == "exit"sv &&
                    runtime.last_code == app_abi::AppRunCode::ok &&
                    runtime.last_exited,
                "qspi named hello diagnostics are stable") && ok;
    ok = expect(contains(std::string_view{qspi_named_hello.host.console.data(), qspi_named_hello.host.console_used},
                         "hello_app: argv1=demo"sv),
                "qspi named hello_app preserves argv path") && ok;

    const auto qspi_named_player = run_qspi_named(runtime,
                                                  "player_min"sv,
                                                  {},
                                                  reader,
                                                  qspi_cache,
                                                  staged_source_ctx);
    ok = expect(qspi_named_player.code.has_value() && qspi_named_player.code.value() == 0,
                "qspi named player exits with 0") && ok;
    ok = expect(runtime.last_source == "qspi-named"sv &&
                    runtime.last_stage == "exit"sv &&
                    runtime.last_code == app_abi::AppRunCode::ok &&
                    runtime.last_exited,
                "qspi named player diagnostics are stable") && ok;
    ok = expect(qspi_named_player.host.present_count == 1U &&
                    qspi_named_player.host.input_poll_count == 1U,
                "qspi named player_min preserves capability path") && ok;

    const auto qspi_raw_hello = run_qspi_raw(runtime,
                                             hello_entry.entry,
                                             hello_app_main,
                                             "demo"sv,
                                             reader,
                                             qspi_cache,
                                             staged_source_ctx);
    ok = expect(qspi_raw_hello.code.has_value() && qspi_raw_hello.code.value() == 0,
                "qspi raw hello exits with 0") && ok;
    ok = expect(runtime.last_source == "qspi-raw"sv &&
                    runtime.last_stage == "exit"sv &&
                    runtime.last_code == app_abi::AppRunCode::ok &&
                    runtime.last_exited,
                "qspi raw diagnostics are stable") && ok;
    ok = expect(contains(std::string_view{qspi_raw_hello.host.console.data(), qspi_raw_hello.host.console_used},
                         "hello_app: argv1=demo"sv),
                "qspi raw hello_app preserves argv path") && ok;

    auto generic_stub = run_path(runtime, "/not-supported"sv);
    ok = expect(!generic_stub.has_value(), "generic file-backed stub does not execute") && ok;
    ok = expect(runtime.last_request == "/not-supported"sv &&
                    runtime.last_app == "/not-supported"sv &&
                    runtime.last_source == "file-backed"sv &&
                    runtime.last_stage == "file-backed"sv &&
                    runtime.last_code == app_abi::AppRunCode::not_supported &&
                    !runtime.last_exited &&
                    runtime.last_exit_code == 0,
                "generic file-backed diagnostics are stable") && ok;

    const auto status = format_status(runtime);
    ok = expect(contains(status, "status: source embedded entries=2 readable=true valid=true qspi_ready=true qspi_readable=true qspi_valid=true"),
                "status includes source/store readability facts") && ok;
    ok = expect(contains(status, "status: store install attempted=true ready=true code=ok"),
                "status includes install diagnostics") && ok;
    ok = expect(contains(status,
                         "status: last request=/not-supported image=/not-supported source=file-backed stage=file-backed code=not_supported exited=false exit=0 backend=0"),
                "status includes final blocked file-backed diagnostics") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[app-lab-mainline-smoke] ok");
    return 0;
}
