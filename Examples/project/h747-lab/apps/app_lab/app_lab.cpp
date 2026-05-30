#include "app_lab.h"
#include "elf_load_region.h"

#include "charm_app_api.h"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"
#include "charm_app_store.hpp"
#include "charm_app_store_install.hpp"
#include "console.h"
#include "console_service.hpp"
#include "display_raster.h"
#include "input.h"
#include "port.h"
#include "qspi_nor.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

import out.core;
import out.format;
import posix.program_image;
import posix.program_image_elf;
import util.core;
import util.error;

#include "hello_app.elf.inc"
#include "appstore.bin.inc"
#include "player_min.elf.inc"

namespace h747::apps::app_lab {
namespace {

using namespace std::literals::string_view_literals;
namespace app_abi = charm::app_abi;

struct Elf32HeaderView {
    std::uint8_t ident[16]{};
    std::uint16_t type{0};
    std::uint16_t machine{0};
    std::uint32_t version{0};
    std::uint32_t entry{0};
    std::uint32_t phoff{0};
    std::uint32_t shoff{0};
    std::uint32_t flags{0};
    std::uint16_t ehsize{0};
    std::uint16_t phentsize{0};
    std::uint16_t phnum{0};
    std::uint16_t shentsize{0};
    std::uint16_t shnum{0};
    std::uint16_t shstrndx{0};
};

struct Elf32ProgramHeaderView {
    std::uint32_t type{0};
    std::uint32_t offset{0};
    std::uint32_t vaddr{0};
    std::uint32_t paddr{0};
    std::uint32_t filesz{0};
    std::uint32_t memsz{0};
    std::uint32_t flags{0};
    std::uint32_t align{0};
};

template <charm::cap::ByteSink Sink>
class OutSinkAdapter {
public:
    explicit OutSinkAdapter(Sink& sink) : sink_(&sink) {}

    out::result<std::size_t> write(const out::bytes bytes) noexcept {
        if (sink_ == nullptr) {
            return out::ok<std::size_t>(0U);
        }
        const auto transfer = sink_->write(bytes);
        return out::ok(static_cast<std::size_t>(transfer.bytes));
    }

    out::result<std::size_t> flush() noexcept {
        if (sink_ != nullptr) {
            (void)sink_->flush();
        }
        return out::ok<std::size_t>(0U);
    }

private:
    Sink* sink_{nullptr};
};

h747::console::ConsoleStream& console_stream() noexcept {
    static h747::console::ConsoleStream stream{};
    return stream;
}

OutSinkAdapter<h747::console::ConsoleStream>& out_sink() noexcept {
    static OutSinkAdapter adapter{console_stream()};
    return adapter;
}

template <out::fixed_string Fmt, class... Args>
void emit(Args&&... args) noexcept {
    out::discard(out::vprint<Fmt>(out_sink(), std::forward<Args>(args)...));
}

constexpr app_abi::AppImage kApps[] = {
    app_abi::AppImage{
        .name = "hello_app"sv,
        .format = app_abi::AppImageFormat::elf,
        .image_base = hello_app_elf,
        .image_size = hello_app_elf_len,
    },
    app_abi::AppImage{
        .name = "player_min"sv,
        .format = app_abi::AppImageFormat::elf,
        .image_base = player_min_elf,
        .image_size = player_min_elf_len,
    },
};

constexpr util::usize kQspiImageCacheSize = 128U * 1024U;
constexpr std::uint32_t kQspiStoreBaseOffset = 0U;
constexpr std::size_t kLastRequestStorageSize = 96U;
constexpr std::size_t kLastImageStorageSize = 64U;

std::array<util::u8, kQspiImageCacheSize>& qspi_image_cache() noexcept {
    alignas(32) static std::array<util::u8, kQspiImageCacheSize> cache{};
    return cache;
}

app_abi::AppStoreReader qspi_store_reader() noexcept;

struct BlobReaderContext {
    const std::byte* data{nullptr};
    std::uint32_t size{0U};
};

struct RuntimeState {
    h747::console::ConsoleLineSource line_source{};
    bool ready{false};
    bool prompt_needed{true};
    bool display_ready{false};
    bool input_ready{false};
    bool file_backed_ready{false};
    bool qspi_ready{false};
    std::uint32_t qspi_jedec{0};
    std::uint32_t qspi_capacity{0};
    std::array<char, kLastRequestStorageSize> last_request_storage{};
    std::array<char, kLastImageStorageSize> last_app_storage{};
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

RuntimeState& state() noexcept {
    static RuntimeState runtime{};
    return runtime;
}

template <std::size_t N>
std::string_view copy_runtime_text(std::array<char, N>& storage, std::string_view text) noexcept {
    static_assert(N >= 2U, "runtime text storage must include space for terminator");
    storage.fill('\0');
    if (text.empty()) {
        text = "-"sv;
    }
    const auto copied = (text.size() < (N - 1U)) ? text.size() : (N - 1U);
    std::memcpy(storage.data(), text.data(), copied);
    return {storage.data(), copied};
}

void set_last_request(RuntimeState& runtime, std::string_view request) noexcept {
    runtime.last_request = copy_runtime_text(runtime.last_request_storage, request);
}

void set_last_app(RuntimeState& runtime, std::string_view app) noexcept {
    runtime.last_app = copy_runtime_text(runtime.last_app_storage, app);
}

void begin_last_result(RuntimeState& runtime,
                       std::string_view source,
                       std::string_view request,
                       std::string_view image) noexcept {
    set_last_request(runtime, request);
    set_last_app(runtime, image);
    runtime.last_source = source.empty() ? "none"sv : source;
    runtime.last_stage = "lookup"sv;
    runtime.last_code = app_abi::AppRunCode::ok;
    runtime.last_backend_error = 0;
    runtime.last_exited = false;
    runtime.last_exit_code = 0;
}

std::string_view format_request(char* buffer,
                                const std::size_t buffer_size,
                                const char* prefix,
                                std::string_view suffix) noexcept {
    if (buffer == nullptr || buffer_size == 0U || prefix == nullptr) {
        return {};
    }
    const int written = std::snprintf(
        buffer,
        buffer_size,
        "%s%.*s",
        prefix,
        static_cast<int>(suffix.size()),
        suffix.data());
    if (written <= 0) {
        buffer[0] = '\0';
        return {};
    }
    const auto length = static_cast<std::size_t>(written);
    const auto clipped = (length < buffer_size) ? length : (buffer_size - 1U);
    return {buffer, clipped};
}

constexpr std::string_view trim_left(std::string_view sv) noexcept {
    while (!sv.empty() && sv.front() == ' ') {
        sv.remove_prefix(1);
    }
    return sv;
}

constexpr std::pair<std::string_view, std::string_view> split_token(std::string_view sv) noexcept {
    sv = trim_left(sv);
    const auto pos = sv.find(' ');
    if (pos == std::string_view::npos) {
        return {sv, {}};
    }
    return {sv.substr(0, pos), trim_left(sv.substr(pos + 1))};
}

const app_abi::AppImage* app_by_name(std::string_view name) noexcept {
    for (const auto& app : kApps) {
        if (app.name == name) {
            return &app;
        }
    }
    return nullptr;
}

BlobReaderContext& embedded_store_context() noexcept {
    static BlobReaderContext ctx{
        .data = reinterpret_cast<const std::byte*>(appstore_bin),
        .size = static_cast<std::uint32_t>(appstore_bin_len),
    };
    return ctx;
}

std::span<const std::byte> embedded_store_bytes() noexcept {
    auto& ctx = embedded_store_context();
    return {ctx.data, ctx.size};
}

app_abi::AppStoreReader embedded_store_reader() noexcept {
    return app_abi::AppStoreReader{
        .ctx = &embedded_store_context(),
        .read = [](void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            auto* blob = static_cast<BlobReaderContext*>(ctx);
            if (blob == nullptr || blob->data == nullptr || offset > blob->size || bytes.size() > (blob->size - offset)) {
                return false;
            }
            std::memcpy(bytes.data(), blob->data + offset, bytes.size());
            return true;
        },
    };
}

app_abi::AppStoreWritableMedia qspi_store_media() noexcept {
    return app_abi::AppStoreWritableMedia{
        .ctx = nullptr,
        .capacity = h747_qspi_nor_capacity(),
        .erase_block_size = h747_qspi_nor_erase_block_size(),
        .write_align = h747_qspi_nor_write_align(),
        .erase = [](void*, std::uint32_t offset, std::uint32_t size) noexcept -> bool {
            return h747_qspi_nor_erase(offset, size) != 0U;
        },
        .write = [](void*, std::uint32_t offset, std::span<const std::byte> bytes) noexcept -> bool {
            return h747_qspi_nor_write(
                       offset,
                       reinterpret_cast<const util::u8*>(bytes.data()),
                       static_cast<std::uint32_t>(bytes.size())) != 0U;
        },
        .read = [](void*, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            return h747_qspi_nor_read(
                       offset,
                       reinterpret_cast<util::u8*>(bytes.data()),
                       static_cast<std::uint32_t>(bytes.size())) != 0U;
        },
    };
}

struct StoreSnapshot {
    bool readable{false};
    bool valid{false};
    std::uint32_t entry_count{0U};
    std::uint32_t magic{0U};
    std::uint32_t header_size{0U};
    std::uint32_t entry_size{0U};
    std::uint16_t version{0U};
};

StoreSnapshot snapshot_store(app_abi::AppStoreReader reader) noexcept {
    app_abi::AppStoreHeader header{};
    const auto code = app_abi::app_store_read_header(reader, header);
    if (code != app_abi::AppStoreReadCode::ok) {
        return {};
    }
    return StoreSnapshot{
        .readable = true,
        .valid = app_abi::app_store_header_valid(header),
        .entry_count = header.entry_count,
        .magic = header.magic,
        .header_size = header.header_size,
        .entry_size = header.entry_size,
        .version = header.version,
    };
}

bool parse_hex_u32(std::string_view text, std::uint32_t& out) noexcept {
    if (text.empty()) {
        return false;
    }
    if (text.size() > 2U && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2U);
    }
    if (text.empty()) {
        return false;
    }
    std::uint32_t value = 0;
    for (const char ch : text) {
        std::uint32_t nibble = 0;
        if (ch >= '0' && ch <= '9') {
            nibble = static_cast<std::uint32_t>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            nibble = static_cast<std::uint32_t>(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            nibble = static_cast<std::uint32_t>(ch - 'A' + 10);
        } else {
            return false;
        }
        if (value > ((0xFFFFFFFFU - nibble) >> 4U)) {
            return false;
        }
        value = (value << 4U) | nibble;
    }
    out = value;
    return true;
}

bool inspect_elf32(const app_abi::AppImage& app,
                   std::uint32_t& entry,
                   std::uint32_t& min_vaddr,
                   std::uint16_t& phnum) noexcept {
    if (app.image_base == nullptr || app.image_size < sizeof(Elf32HeaderView)) {
        return false;
    }
    const auto* bytes = static_cast<const util::u8*>(app.image_base);
    const auto* hdr = reinterpret_cast<const Elf32HeaderView*>(bytes);
    if (hdr->ident[0] != 0x7f || hdr->ident[1] != 'E' || hdr->ident[2] != 'L' || hdr->ident[3] != 'F') {
        return false;
    }
    if (hdr->ident[4] != 1 || hdr->ident[5] != 1) {
        return false;
    }
    const auto phoff = static_cast<util::usize>(hdr->phoff);
    const auto phentsize = static_cast<util::usize>(hdr->phentsize);
    if (phoff >= app.image_size || phentsize < sizeof(Elf32ProgramHeaderView)) {
        return false;
    }
    if ((phoff + (phentsize * hdr->phnum)) > app.image_size) {
        return false;
    }
    bool found_load = false;
    std::uint32_t local_min_vaddr = 0;
    for (std::uint16_t i = 0; i < hdr->phnum; ++i) {
        const auto* ph = reinterpret_cast<const Elf32ProgramHeaderView*>(bytes + phoff + (phentsize * i));
        if (ph->type != 1) {
            continue;
        }
        if (!found_load || ph->vaddr < local_min_vaddr) {
            local_min_vaddr = ph->vaddr;
            found_load = true;
        }
    }
    if (!found_load) {
        return false;
    }
    entry = hdr->entry;
    min_vaddr = local_min_vaddr;
    phnum = hdr->phnum;
    return true;
}

int api_console_write(const char* text, const std::size_t len) {
    if (text == nullptr) {
        return -1;
    }
    for (std::size_t i = 0; i < len; ++i) {
        h747::console::write_char(text[i]);
    }
    return static_cast<int>(len);
}

std::uint32_t api_now_ms() {
    return h747::port::tick_ms();
}

int api_display_describe(CharmAppDisplayMode* out_mode) {
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

int api_display_present(const void* pixels, const std::uint32_t bytes) {
    if (pixels == nullptr || bytes == 0U) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    if (display_raster_present(pixels, bytes) == 0U) {
        return CHARM_APP_STATUS_IO_ERROR;
    }
    return CHARM_APP_STATUS_OK;
}

int api_input_poll(CharmAppInputState* out_state) {
    if (out_state == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    input_poll();
    const auto snapshot = input_snapshot();
    *out_state = CharmAppInputState{
        .encoder1_delta = snapshot.encoder1.detent_delta,
        .encoder2_delta = snapshot.encoder2.detent_delta,
        .encoder1_pressed = snapshot.encoder1.button_pressed,
        .encoder2_pressed = snapshot.encoder2.button_pressed,
        .pointer_detected = snapshot.touch.detected,
        .pointer_down = snapshot.touch.down,
        .pointer_x = snapshot.touch.x,
        .pointer_y = snapshot.touch.y,
        .pointer_max_x = snapshot.touch.max_x,
        .pointer_max_y = snapshot.touch.max_y,
    };
    return CHARM_APP_STATUS_OK;
}

int api_storage_open(const char*, int, int) {
    return -1;
}

int api_storage_read(int, void*, std::size_t) {
    return -1;
}

int api_storage_write(int, const void*, std::size_t) {
    return -1;
}

int api_storage_close(int) {
    return -1;
}

int api_afe_configure(std::uint32_t, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int api_afe_read(void*, std::size_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

void api_app_exit(int code) {
    state().last_exit_code = code;
}

CharmAppApi make_api() noexcept {
    return CharmAppApi{
        .magic = CHARM_APP_API_MAGIC,
        .version = CHARM_APP_API_VERSION,
        .size = sizeof(CharmAppApi),
        .flags = 0,
        .console = CharmAppConsoleApi{.write = api_console_write},
        .time = CharmAppTimeApi{.now_ms = api_now_ms},
        .display = CharmAppDisplayApi{.describe = api_display_describe, .present = api_display_present},
        .input = CharmAppInputApi{.poll = api_input_poll},
        .storage = CharmAppStorageApi{
            .open = api_storage_open,
            .read = api_storage_read,
            .write = api_storage_write,
            .close = api_storage_close,
        },
        .afe = CharmAppAfeApi{.configure = api_afe_configure, .read = api_afe_read},
        .app = CharmAppControlApi{.exit = api_app_exit},
    };
}

void set_failure(RuntimeState& runtime,
                 std::string_view stage,
                 app_abi::AppRunCode code,
                 int backend_error = 0) noexcept {
    runtime.last_stage = stage;
    runtime.last_code = code;
    runtime.last_backend_error = backend_error;
    runtime.last_exited = false;
    emit<"app: failed stage={} code={} backend={}\n">(
        stage,
        app_abi::code_name(code),
        backend_error);
}

app_abi::AppRunCode map_load_error(util::Errc error) noexcept {
    switch (error) {
        case util::Errc::ok:
            return app_abi::AppRunCode::ok;
        case util::Errc::not_supported:
            return app_abi::AppRunCode::not_supported;
        case util::Errc::noent:
            return app_abi::AppRunCode::image_not_found;
        default:
            return app_abi::AppRunCode::load_failed;
    }
}

app_abi::AppLoadResult load_app_image(void*, const app_abi::AppImage& app, const app_abi::AppLoadBuffer& buffer) noexcept {
    posix::ElfLoadConfig cfg{};
    cfg.image_base = app.image_base;
    cfg.image_size = app.image_size;
    cfg.load_base = buffer.base;
    cfg.load_size = buffer.size;
    cfg.load_align = buffer.align;
    auto image = posix::load_elf_image(cfg);
    if (!image) {
        return app_abi::AppLoadResult{
            .code = map_load_error(image.error()),
            .backend_error = static_cast<int>(image.error()),
        };
    }
    if (image.value().entry == nullptr) {
        return app_abi::AppLoadResult{.code = app_abi::AppRunCode::abi_missing};
    }
    return app_abi::AppLoadResult{
        .code = app_abi::AppRunCode::ok,
        .image = app_abi::LoadedAppImage::from_entry(app.name, app.format, image.value().entry),
    };
}

app_abi::AppImageSource builtin_image_source() noexcept {
    return app_abi::AppImageSource{
        .ctx = nullptr,
        .find = [](void*, std::string_view name) noexcept -> const app_abi::AppImage* {
            return app_by_name(name);
        },
        .load = load_app_image,
    };
}

void prepare_app_load_buffer(void*) noexcept {
    prepare_elf_load_region();
}

app_abi::AppLoadBuffer app_load_buffer() noexcept {
    return app_abi::AppLoadBuffer{
        .base = elf_load_region_base(),
        .size = elf_load_region_capacity(),
        .align = 16,
        .prepare = prepare_app_load_buffer,
        .prepare_ctx = nullptr,
    };
}

void record_result(RuntimeState& runtime, const app_abi::AppRunResult& result) noexcept {
    set_last_app(runtime, result.name);
    runtime.last_stage = app_abi::stage_name(result.stage);
    runtime.last_code = result.code;
    runtime.last_backend_error = result.backend_error;
    runtime.last_exited = result.exited;
    runtime.last_exit_code = result.exited ? result.exit_code : 0;
}

app_abi::AppLoadResult load_cached_app_image(void*, const app_abi::AppImage& app, const app_abi::AppLoadBuffer& buffer) noexcept {
    return load_app_image(nullptr, app, buffer);
}

std::optional<int> run_staged(RuntimeState& runtime,
                              const app_abi::AppImage& image,
                              std::string_view arg_text) noexcept {
    app_abi::StagedAppImageSource staged{
        .image = image,
        .load_ctx = nullptr,
        .load = load_cached_app_image,
    };
    auto source = app_abi::make_staged_app_image_source(staged);
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> app_runtime{};
    const auto result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_load_buffer(),
        .api = &api,
        .name = staged.image.name,
        .arg_text = arg_text,
    });
    record_result(runtime, result);
    if (result.code != app_abi::AppRunCode::ok || !result.exited) {
        emit<"app: failed name={} stage={} code={} backend={}\n">(
            staged.image.name,
            app_abi::stage_name(result.stage),
            app_abi::code_name(result.code),
            result.backend_error);
        return std::nullopt;
    }
    emit<"app: exit name={} code={}\n">(staged.image.name, result.exit_code);
    return result.exit_code;
}

std::optional<int> run_builtin(RuntimeState& runtime,
                               std::string_view name,
                               std::string_view arg_text) noexcept {
    begin_last_result(runtime, "embedded"sv, name, name);

    const auto* app = app_by_name(name);
    if (app == nullptr) {
        set_failure(runtime, "lookup"sv, app_abi::AppRunCode::image_not_found);
        return std::nullopt;
    }

    std::uint32_t entry = 0;
    std::uint32_t min_vaddr = 0;
    std::uint16_t phnum = 0;
    if (inspect_elf32(*app, entry, min_vaddr, phnum)) {
        emit<"app: load name={} size={} entry=0x{:08x} base=0x{:08x} ph={} load=0x{:08x}/{}\n">(
            app->name,
            static_cast<unsigned>(app->image_size),
            entry,
            min_vaddr,
            static_cast<unsigned>(phnum),
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(elf_load_region_base())),
            static_cast<unsigned>(elf_load_region_capacity()));
    } else {
        emit<"app: load name={} size={} elf=unparsed\n">(
            app->name,
            static_cast<unsigned>(app->image_size));
    }

    CharmAppApi api = make_api();
    auto source = builtin_image_source();
    app_abi::AppRuntime<> app_runtime{};
    const auto result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_load_buffer(),
        .api = &api,
        .name = name,
        .arg_text = arg_text,
    });
    record_result(runtime, result);
    if (result.code != app_abi::AppRunCode::ok || !result.exited) {
        emit<"app: failed name={} stage={} code={} backend={}\n">(
            name,
            app_abi::stage_name(result.stage),
            app_abi::code_name(result.code),
            result.backend_error);
        return std::nullopt;
    }
    emit<"app: exit name={} code={}\n">(name, result.exit_code);
    return result.exit_code;
}

std::span<std::byte> qspi_cache_bytes() noexcept {
    auto& cache = qspi_image_cache();
    return std::as_writable_bytes(std::span<util::u8>{cache.data(), cache.size()});
}

std::optional<int> run_qspi_offset(RuntimeState& runtime,
                                   std::string_view spec,
                                   std::string_view arg_text) noexcept {
    char request_buffer[32]{};
    const auto request = format_request(request_buffer, sizeof(request_buffer), "qspi:@", spec);
    if (!request.empty()) {
        begin_last_result(runtime, "qspi-raw"sv, request, "qspi_app"sv);
    } else {
        begin_last_result(runtime, "qspi-raw"sv, "qspi:@"sv, "qspi_app"sv);
    }
    const auto sep = spec.find(':');
    if (sep == std::string_view::npos) {
        set_failure(runtime, "qspi-parse"sv, app_abi::AppRunCode::invalid_argument);
        emit<"app: qspi offset syntax is qspi:@<hex_offset>:<hex_size>\n">();
        return std::nullopt;
    }
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    if (!parse_hex_u32(spec.substr(0, sep), offset) || !parse_hex_u32(spec.substr(sep + 1U), size)) {
        set_failure(runtime, "qspi-parse"sv, app_abi::AppRunCode::invalid_argument);
        return std::nullopt;
    }
    const auto staged = app_abi::app_store_stage_raw_image(
        qspi_store_reader(),
        "qspi_app"sv,
        offset,
        size,
        qspi_cache_bytes());
    if (staged.code != app_abi::AppStoreReadCode::ok) {
        set_failure(runtime, "qspi-read"sv, app_abi::AppRunCode::load_failed);
        emit<"app: qspi raw stage failed offset=0x{:08x} size={} code={}\n">(
            offset,
            static_cast<unsigned>(size),
            app_abi::app_store_read_code_name(staged.code));
        return std::nullopt;
    }
    emit<"app: qspi read offset=0x{:08x} size={}\n">(offset, static_cast<unsigned>(size));
    return run_staged(runtime, staged.image, arg_text);
}

app_abi::AppStoreReader qspi_store_reader() noexcept {
    return app_abi::AppStoreReader{
        .ctx = nullptr,
        .read = [](void*, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            return h747_qspi_nor_read(
                       offset,
                       reinterpret_cast<util::u8*>(bytes.data()),
                       static_cast<std::uint32_t>(bytes.size())) != 0U;
        },
    };
}

bool install_qspi_store(RuntimeState& runtime) noexcept {
    runtime.store_install_attempted = true;
    runtime.store_install_backend_ready = runtime.qspi_ready;
    runtime.store_install_target = kQspiStoreBaseOffset;
    runtime.store_install_written = 0U;
    runtime.store_install_erased = 0U;
    runtime.store_install_code = app_abi::AppStoreInstallCode::invalid_argument;

    if (!runtime.qspi_ready) {
        runtime.store_install_code = app_abi::AppStoreInstallCode::invalid_argument;
        emit<"store: install backend=not_ready code={}\n">(
            app_abi::app_store_install_code_name(runtime.store_install_code));
        return false;
    }

    const auto image = embedded_store_bytes();
    const auto result = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = qspi_store_media(),
        .target_offset = kQspiStoreBaseOffset,
        .image = image,
    });
    runtime.store_install_code = result.code;
    runtime.store_install_target = result.target_offset;
    runtime.store_install_written = result.bytes_written;
    runtime.store_install_erased = result.bytes_erased;
    emit<"store: install code={} target=0x{:08x} written={} erased={}\n">(
        app_abi::app_store_install_code_name(result.code),
        result.target_offset,
        result.bytes_written,
        result.bytes_erased);
    return result.code == app_abi::AppStoreInstallCode::ok;
}

std::optional<int> run_qspi_named(RuntimeState& runtime,
                                  std::string_view name,
                                  std::string_view arg_text) noexcept {
    char request_buffer[48]{};
    const auto request = format_request(request_buffer, sizeof(request_buffer), "qspi:", name);
    if (!request.empty()) {
        begin_last_result(runtime, "qspi-named"sv, request, name);
    } else {
        begin_last_result(runtime, "qspi-named"sv, "qspi:"sv, name);
    }
    const auto staged = app_abi::app_store_stage_named_image(qspi_store_reader(), name, qspi_cache_bytes());
    if (staged.code != app_abi::AppStoreReadCode::ok) {
        const auto run_code = (staged.code == app_abi::AppStoreReadCode::image_not_found ||
                               staged.code == app_abi::AppStoreReadCode::header_invalid ||
                               staged.code == app_abi::AppStoreReadCode::header_unreadable ||
                               staged.code == app_abi::AppStoreReadCode::entry_read_failed)
            ? app_abi::AppRunCode::image_not_found
            : app_abi::AppRunCode::load_failed;
        set_failure(runtime, "qspi-stage"sv, run_code);
        emit<"app: qspi stage failed name={} code={}\n">(
            name,
            app_abi::app_store_read_code_name(staged.code));
        return std::nullopt;
    }
    emit<"app: qspi read name={} offset=0x{:08x} size={}\n">(
        name,
        staged.lookup.entry.offset,
        static_cast<unsigned>(staged.lookup.entry.size));
    return run_staged(runtime, staged.image, arg_text);
}

std::optional<int> run_path(RuntimeState& runtime, std::string_view path, std::string_view arg_text) noexcept {
    begin_last_result(runtime, "file-backed"sv, path, path);
    if (path.starts_with("qspi:"sv)) {
        const auto rest = path.substr(5U);
        if (!runtime.qspi_ready) {
            runtime.last_source = "qspi"sv;
            set_failure(runtime, "qspi"sv, app_abi::AppRunCode::not_supported);
            emit<"app: qspi backend not ready path={}\n">(path);
            return std::nullopt;
        }
        if (rest.starts_with("@"sv)) {
            return run_qspi_offset(runtime, rest.substr(1U), arg_text);
        }
        return run_qspi_named(runtime, rest, arg_text);
    }
    if (!runtime.file_backed_ready) {
        set_failure(runtime, "file-backed"sv, app_abi::AppRunCode::not_supported);
        emit<"app: run-path backend not ready path={} err=not_supported\n">(path);
        return std::nullopt;
    }
    set_failure(runtime, "file-backed"sv, app_abi::AppRunCode::not_supported);
    return std::nullopt;
}

void print_prompt() noexcept {
    emit<"\r\napp-lab> ">();
}

void print_banner() noexcept {
    emit<"app_lab: resident App ELF monitor ready\n">();
    emit<"app_lab: entry=charm_app_main(api,argc,argv) image_source=elfmem+qspi-nor+file-backed(stub)\n">();
}

void print_help() noexcept {
    emit<"Commands:\n">();
    emit<"  help                     - Show help\n">();
    emit<"  app list                 - List builtin App ELF images\n">();
    emit<"  app store status         - Show QSPI App store status\n">();
    emit<"  app store list           - List QSPI App store entries\n">();
    emit<"  app store install        - Install embedded App store into QSPI\n">();
    emit<"  app status               - Show runtime status\n">();
    emit<"  app run <name> [args]    - Run builtin App ELF\n">();
    emit<"  app run-path <path> ...  - Run file-backed App ELF path\n">();
    emit<"    path qspi:<name>       - Run App ELF from QSPI App store\n">();
    emit<"    path qspi:@<off>:<sz>  - Run raw QSPI App ELF slice\n">();
    emit<"  app smoke                - Run first-generation App ABI smoke\n">();
}

bool read_qspi_store_header(app_abi::AppStoreHeader& header) noexcept {
    return app_abi::app_store_read_header(qspi_store_reader(), header) == app_abi::AppStoreReadCode::ok;
}

void print_store_status(RuntimeState& runtime) noexcept {
    const auto qspi = h747_qspi_nor_state();
    emit<"store: qspi ready={} power={} jedec_ok={} jedec=0x{:08x} capacity={}\n">(
        qspi.ready,
        qspi.power_ok,
        qspi.jedec_ok,
        qspi.jedec_id,
        qspi.capacity_bytes);
    emit<"store: qspi reads={}/{} writes={}/{} erases={}/{} last=0x{:08x}:{} hal={} err={} regs dcr=0x{:08x} sr=0x{:08x} cr=0x{:08x}\n">(
        qspi.read_count,
        qspi.read_fail_count,
        qspi.write_count,
        qspi.write_fail_count,
        qspi.erase_count,
        qspi.erase_fail_count,
        qspi.last_offset,
        qspi.last_bytes,
        qspi.last_hal_status,
        qspi.last_error,
        qspi.dcr,
        qspi.sr,
        qspi.cr);
    emit<"store: embedded bytes={} install attempted={} ready={} code={} target=0x{:08x} written={} erased={} last_write=0x{:08x}:{} last_erase=0x{:08x}:{}\n">(
        static_cast<unsigned>(embedded_store_bytes().size()),
        runtime.store_install_attempted,
        runtime.store_install_backend_ready,
        app_abi::app_store_install_code_name(runtime.store_install_code),
        runtime.store_install_target,
        runtime.store_install_written,
        runtime.store_install_erased,
        qspi.last_write_offset,
        qspi.last_write_bytes,
        qspi.last_erase_offset,
        qspi.last_erase_bytes);
    app_abi::AppStoreHeader header{};
    if (!runtime.qspi_ready || !read_qspi_store_header(header)) {
        emit<"store: header readable=0 valid=0\n">();
        return;
    }
    emit<"store: header readable=1 valid={} magic=0x{:08x} version={} entries={} header_size={} entry_size={}\n">(
        app_abi::app_store_header_valid(header),
        header.magic,
        static_cast<unsigned>(header.version),
        header.entry_count,
        header.header_size,
        header.entry_size);
}

void list_qspi_store(RuntimeState& runtime) noexcept {
    app_abi::AppStoreHeader header{};
    if (!runtime.qspi_ready || !read_qspi_store_header(header)) {
        emit<"store: qspi header unreadable\n">();
        return;
    }
    if (!app_abi::app_store_header_valid(header)) {
        emit<"store: invalid header magic=0x{:08x} version={} entries={} header_size={} entry_size={}\n">(
            header.magic,
            static_cast<unsigned>(header.version),
            header.entry_count,
            header.header_size,
            header.entry_size);
        return;
    }
    emit<"store: entries={}\n">(header.entry_count);
    for (std::uint32_t i = 0; i < header.entry_count; ++i) {
        app_abi::AppStoreEntry entry{};
        const auto entry_code = app_abi::app_store_read_entry(qspi_store_reader(), header, i, entry);
        if (entry_code != app_abi::AppStoreReadCode::ok) {
            emit<"  [{}] read_failed offset=0x{:08x} code={}\n">(
                i,
                app_abi::app_store_entry_offset(header, i),
                app_abi::app_store_read_code_name(entry_code));
            return;
        }
        emit<"  [{}] name={} offset=0x{:08x} size={} flags=0x{:08x} runnable={}\n">(
            i,
            app_abi::app_store_entry_name(entry),
            entry.offset,
            entry.size,
            entry.flags,
            app_abi::app_store_entry_runnable(entry));
    }
}

void print_status(RuntimeState& runtime) noexcept {
    const auto raster = display_raster_state();
    const auto input_state_now = input_state();
    const auto qspi = h747_qspi_nor_state();
    const auto embedded_store = snapshot_store(embedded_store_reader());
    const auto qspi_store = runtime.qspi_ready ? snapshot_store(qspi_store_reader()) : StoreSnapshot{};

    emit<"status: monitor=ready display_ready={} input_ready={} file_backed={}\n">(
        runtime.display_ready,
        runtime.input_ready,
        runtime.file_backed_ready);

    emit<"status: source embedded entries={} readable={} valid={} qspi_ready={} qspi_readable={} qspi_valid={}\n">(
        embedded_store.entry_count,
        embedded_store.readable,
        embedded_store.valid,
        runtime.qspi_ready,
        qspi_store.readable,
        qspi_store.valid);
    emit<"status: qspi jedec=0x{:08x} capacity={} reads={}/{} writes={}/{} erases={}/{}\n">(
        runtime.qspi_jedec,
        static_cast<unsigned>(runtime.qspi_capacity),
        static_cast<unsigned>(qspi.read_count),
        static_cast<unsigned>(qspi.read_fail_count),
        static_cast<unsigned>(qspi.write_count),
        static_cast<unsigned>(qspi.write_fail_count),
        static_cast<unsigned>(qspi.erase_count),
        static_cast<unsigned>(qspi.erase_fail_count));
    emit<"status: store install attempted={} ready={} code={} target=0x{:08x} written={} erased={}\n">(
        runtime.store_install_attempted,
        runtime.store_install_backend_ready,
        app_abi::app_store_install_code_name(runtime.store_install_code),
        runtime.store_install_target,
        runtime.store_install_written,
        runtime.store_install_erased);
    emit<"status: last request={} image={} source={} stage={} code={} exited={} exit={} backend={}\n">(
        runtime.last_request,
        runtime.last_app,
        runtime.last_source,
        runtime.last_stage,
        app_abi::code_name(runtime.last_code),
        runtime.last_exited,
        runtime.last_exit_code,
        runtime.last_backend_error);
    emit<"status: elf_load=[0x{:08x},0x{:08x}) size={}\n">(
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(elf_load_region_base())),
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(elf_load_region_base()) + elf_load_region_capacity()),
        static_cast<unsigned>(elf_load_region_capacity()));
    emit<"status: qspi-store magic=0x{:08x} version={} entries={} header_size={} entry_size={}\n">(
        qspi_store.magic,
        static_cast<unsigned>(qspi_store.version),
        qspi_store.entry_count,
        qspi_store.header_size,
        qspi_store.entry_size);
    emit<"status: display init={} present_count={} last_present_ok={}\n">(
        raster.init_ok,
        static_cast<unsigned>(raster.present_count),
        raster.present_ok);
    emit<"status: input initialized={} touch_ready={} enc1_delta={} enc2_delta={}\n">(
        input_state_now.initialized,
        input_state_now.touch.ready,
        static_cast<int>(input_state_now.encoder1.detent_delta),
        static_cast<int>(input_state_now.encoder2.detent_delta));
}

void list_apps() noexcept {
    emit<"builtin App ELF images:\n">();
    for (const auto& app : kApps) {
        std::uint32_t entry = 0;
        std::uint32_t min_vaddr = 0;
        std::uint16_t phnum = 0;
        if (inspect_elf32(app, entry, min_vaddr, phnum)) {
            emit<"  {} ({} bytes, entry=0x{:08x}, base=0x{:08x}, ph={})\n">(
                app.name,
                static_cast<unsigned>(app.image_size),
                entry,
                min_vaddr,
                static_cast<unsigned>(phnum));
            continue;
        }
        emit<"  {} ({} bytes)\n">(app.name, static_cast<unsigned>(app.image_size));
    }
}

bool smoke_step(RuntimeState& runtime,
                std::string_view name,
                std::string_view args = {},
                int expected_code = 0) noexcept {
    emit<"[app-smoke] run {} {}\n">(name, args);
    const auto code = run_builtin(runtime, name, args);
    const bool ok = code.has_value() && code.value() == expected_code;
    if (!ok) {
        emit<"[app-smoke] failed name={} expected={} got={}\n">(
            name,
            expected_code,
            code.has_value() ? code.value() : -999);
    }
    return ok;
}

bool smoke_qspi_named(RuntimeState& runtime,
                      std::string_view name,
                      std::string_view args = {},
                      int expected_code = 0) noexcept {
    emit<"[app-smoke] qspi named {} {}\n">(name, args);
    const auto code = run_qspi_named(runtime, name, args);
    const bool ok = code.has_value() && code.value() == expected_code;
    if (!ok) {
        emit<"[app-smoke] failed qspi-named name={} expected={} got={}\n">(
            name,
            expected_code,
            code.has_value() ? code.value() : -999);
    }
    return ok;
}

bool smoke_qspi_raw(RuntimeState& runtime,
                    const app_abi::AppStoreEntry& entry,
                    std::string_view args = {},
                    int expected_code = 0) noexcept {
    char spec_buffer[64]{};
    const int spec_len = std::snprintf(
        spec_buffer,
        sizeof(spec_buffer),
        "%08x:%08x",
        static_cast<unsigned>(entry.offset),
        static_cast<unsigned>(entry.size));
    if (spec_len <= 0 || static_cast<std::size_t>(spec_len) >= sizeof(spec_buffer)) {
        emit<"[app-smoke] failed qspi-raw spec-build\n">();
        return false;
    }
    const std::string_view spec{spec_buffer, static_cast<std::size_t>(spec_len)};
    emit<"[app-smoke] qspi raw {} {}\n">(spec, args);
    const auto code = run_qspi_offset(runtime, spec, args);
    const bool ok = code.has_value() && code.value() == expected_code;
    if (!ok) {
        emit<"[app-smoke] failed qspi-raw spec={} expected={} got={}\n">(
            spec,
            expected_code,
            code.has_value() ? code.value() : -999);
    }
    return ok;
}

bool smoke_generic_stub(RuntimeState& runtime) noexcept {
    emit<"[app-smoke] generic stub /not-supported\n">();
    runtime.last_stage = "idle"sv;
    runtime.last_code = app_abi::AppRunCode::ok;
    runtime.last_backend_error = 0;
    run_path(runtime, "/not-supported"sv, {});
    const bool ok = runtime.last_stage == "file-backed"sv &&
                    runtime.last_code == app_abi::AppRunCode::not_supported;
    if (!ok) {
        emit<"[app-smoke] failed generic-stub stage={} code={}\n">(
            runtime.last_stage,
            app_abi::code_name(runtime.last_code));
    }
    return ok;
}

void run_smoke(RuntimeState& runtime) noexcept {
    bool builtin_ok = true;
    builtin_ok = smoke_step(runtime, "hello_app", "demo") && builtin_ok;
    builtin_ok = smoke_step(runtime, "player_min") && builtin_ok;
    emit<"[app-smoke] builtin={}\n">(builtin_ok ? "ok"sv : "failed"sv);

    const bool install_ok = install_qspi_store(runtime);
    emit<"[app-smoke] install={}\n">(install_ok ? "ok"sv : "failed"sv);

    bool qspi_named_ok = false;
    bool qspi_raw_ok = false;
    if (install_ok) {
        const auto hello = app_abi::app_store_find_entry(qspi_store_reader(), "hello_app"sv);
        const auto player = app_abi::app_store_find_entry(qspi_store_reader(), "player_min"sv);
        qspi_named_ok = hello.code == app_abi::AppStoreReadCode::ok &&
                        player.code == app_abi::AppStoreReadCode::ok &&
                        smoke_qspi_named(runtime, "hello_app", "demo") &&
                        smoke_qspi_named(runtime, "player_min");
        emit<"[app-smoke] qspi_named={}\n">(qspi_named_ok ? "ok"sv : "failed"sv);

        if (hello.code == app_abi::AppStoreReadCode::ok) {
            qspi_raw_ok = smoke_qspi_raw(runtime, hello.entry, "demo");
        }
    }
    emit<"[app-smoke] qspi_raw={}\n">(qspi_raw_ok ? "ok"sv : "failed"sv);

    const bool generic_stub_ok = smoke_generic_stub(runtime);
    emit<"[app-smoke] generic_stub={}\n">(generic_stub_ok ? "ok"sv : "failed"sv);

    const bool ok = builtin_ok && install_ok && qspi_named_ok && qspi_raw_ok && generic_stub_ok;
    emit<"[app-smoke] result={}\n">(ok ? "ok"sv : "failed"sv);
}

void handle_command(RuntimeState& runtime, std::string_view line) noexcept {
    const auto cmd = trim_left(line);
    if (cmd.empty()) {
        return;
    }
    if (cmd == "help"sv) {
        print_help();
        return;
    }
    if (cmd == "app list"sv) {
        list_apps();
        return;
    }
    if (cmd == "app store status"sv) {
        print_store_status(runtime);
        return;
    }
    if (cmd == "app store list"sv) {
        list_qspi_store(runtime);
        return;
    }
    if (cmd == "app store install"sv) {
        (void)install_qspi_store(runtime);
        return;
    }
    if (cmd == "app status"sv) {
        print_status(runtime);
        return;
    }
    if (cmd == "app smoke"sv) {
        run_smoke(runtime);
        return;
    }
    if (cmd.starts_with("app run-path "sv)) {
        auto rest = cmd.substr(13);
        auto [path, args] = split_token(rest);
        if (path.empty()) {
            emit<"usage: app run-path <path> [args...]\n">();
            return;
        }
        run_path(runtime, path, args);
        return;
    }
    if (cmd.starts_with("app run "sv)) {
        auto rest = cmd.substr(8);
        auto [name, args] = split_token(rest);
        if (name.empty()) {
            emit<"usage: app run <name> [args...]\n">();
            return;
        }
        (void)run_builtin(runtime, name, args);
        return;
    }
    emit<"unknown command: {}\n">(cmd);
}

} // namespace

void init() {
    auto& runtime = state();
    if (runtime.ready) {
        return;
    }
    runtime.display_ready = display_raster_init() != 0U;
    input_init();
    runtime.input_ready = true;
    h747_qspi_nor_init();
    const auto qspi = h747_qspi_nor_state();
    runtime.qspi_ready = qspi.ready != 0U;
    runtime.qspi_jedec = qspi.jedec_id;
    runtime.qspi_capacity = qspi.capacity_bytes;
    runtime.store_install_backend_ready = runtime.qspi_ready;
    runtime.store_install_target = kQspiStoreBaseOffset;
    runtime.store_install_code = app_abi::AppStoreInstallCode::invalid_argument;
    runtime.ready = true;
    print_banner();
    print_help();
}

void loop_once() noexcept {
    auto& runtime = state();
    if (runtime.prompt_needed) {
        print_prompt();
        runtime.prompt_needed = false;
    }
    if (const auto line = runtime.line_source.poll_line()) {
        handle_command(runtime, *line);
        runtime.prompt_needed = true;
    }
}

} // namespace h747::apps::app_lab
