#include "resident_launcher.h"

#include "charm_app_api.h"
#include "charm_app_elf_probe.hpp"
#include "charm_app_fat_catalog.hpp"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"
#include "console.h"
#include "display_raster.h"
#include "input.h"
#include "port.h"
#include "resident_launcher_domain.hpp"
#include "storage.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace h747::apps::resident_launcher {
namespace {

using namespace std::literals::string_view_literals;
namespace app_abi = charm::app_abi;

constexpr std::uintptr_t kSdramStageBase = 0xD0040000U;
constexpr std::uint32_t kStageCacheBytes = 256U * 1024U;
constexpr std::uintptr_t kAppRunBase = 0x24070000U;
constexpr std::uint32_t kAppRunBytes = 64U * 1024U;
constexpr std::uint32_t kAppRunAlign = 16U;
constexpr std::uint32_t kScreenWidth = 720U;
constexpr std::uint32_t kScreenHeight = 1280U;
constexpr std::uint32_t kScreenBytes = kScreenWidth * kScreenHeight * 4U;
constexpr std::uint32_t kListTop = 190U;
constexpr std::uint32_t kRowHeight = 72U;

static_assert(kAppRunBase == 0x24070000U,
              "resident_launcher ELF run base must match app_elf.ld ELF_BASE");

struct Runtime {
    ResidentLauncherState launcher{};
    app_abi::AppElfLoadBackend elf_backend{};
    std::uint32_t app_console_bytes{0};
    std::uint32_t app_present_count{0};
    std::uint32_t app_input_count{0};
};

Runtime& runtime() noexcept {
    static Runtime rt{};
    return rt;
}

std::span<std::byte> stage_cache() noexcept {
    return {
        reinterpret_cast<std::byte*>(kSdramStageBase),
        kStageCacheBytes,
    };
}

std::span<std::byte> app_run_buffer() noexcept {
    return {
        reinterpret_cast<std::byte*>(kAppRunBase),
        kAppRunBytes,
    };
}

bool emmc_read_block(void*, std::uint32_t lba, std::span<std::byte> bytes) noexcept {
    if (bytes.size() != app_abi::kFirmwareCatalogSectorSize) {
        return false;
    }
    return h747_storage_read_blocks(
               lba,
               reinterpret_cast<std::uint8_t*>(bytes.data()),
               static_cast<std::uint32_t>(bytes.size())) != 0U;
}

app_abi::FatBlockReader emmc_reader() noexcept {
    return app_abi::FatBlockReader{
        .ctx = nullptr,
        .read = emmc_read_block,
        .block_count = h747_storage_block_count(),
    };
}

void write_sv(std::string_view text) noexcept {
    for (const char c : text) {
        h747::console::write_char(c);
    }
}

void write_line(std::string_view text) noexcept {
    write_sv(text);
    h747::console::write_char('\n');
}

void write_dec(std::uint32_t value) noexcept {
    h747::console::write_dec(value);
}

void write_catalog_status(const ResidentLauncherState& state) noexcept {
    write_sv("resident_launcher: catalog code=");
    write_sv(app_abi::firmware_catalog_code_name(state.catalog_code));
    write_sv(" count=");
    write_dec(state.catalog.count);
    h747::console::write_char('\n');
}

std::uint32_t argb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return 0xFF000000U |
           (static_cast<std::uint32_t>(r) << 16U) |
           (static_cast<std::uint32_t>(g) << 8U) |
           static_cast<std::uint32_t>(b);
}

void put_pixel(std::span<std::byte> fb, std::uint32_t x, std::uint32_t y, std::uint32_t color) noexcept {
    if (x >= kScreenWidth || y >= kScreenHeight) {
        return;
    }
    const auto offset = ((y * kScreenWidth) + x) * 4U;
    if (offset + 4U > fb.size()) {
        return;
    }
    std::memcpy(fb.data() + offset, &color, sizeof(color));
}

void fill_rect(std::span<std::byte> fb,
               std::uint32_t x,
               std::uint32_t y,
               std::uint32_t w,
               std::uint32_t h,
               std::uint32_t color) noexcept {
    const auto x1 = std::min<std::uint32_t>(kScreenWidth, x + w);
    const auto y1 = std::min<std::uint32_t>(kScreenHeight, y + h);
    for (std::uint32_t yy = y; yy < y1; ++yy) {
        for (std::uint32_t xx = x; xx < x1; ++xx) {
            put_pixel(fb, xx, yy, color);
        }
    }
}

std::array<std::uint8_t, 7> glyph(char c) noexcept {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - ('a' - 'A'));
    }
    switch (c) {
        case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
        case 'C': return {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F};
        case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
        case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
        case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
        case 'G': return {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F};
        case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
        case 'J': return {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
        case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
        case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
        case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
        case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
        case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
        case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
        case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
        case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
        case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'V': return {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04};
        case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
        case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
        case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
        case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
        case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
        case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
        case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
        case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
        case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
        case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
        case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
        case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
        case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
        case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
        case '/': return {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
        case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
        case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
        case '_': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
        case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
        case '=': return {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00};
        case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        default: return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    }
}

void draw_char(std::span<std::byte> fb,
               std::uint32_t x,
               std::uint32_t y,
               char c,
               std::uint32_t color,
               std::uint32_t scale = 2U) noexcept {
    const auto rows = glyph(c);
    for (std::uint32_t row = 0; row < rows.size(); ++row) {
        for (std::uint32_t col = 0; col < 5U; ++col) {
            if ((rows[row] & (1U << (4U - col))) != 0U) {
                fill_rect(fb, x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

void draw_text(std::span<std::byte> fb,
               std::uint32_t x,
               std::uint32_t y,
               std::string_view text,
               std::uint32_t color,
               std::uint32_t scale = 2U) noexcept {
    std::uint32_t cursor = x;
    for (const char c : text) {
        if (cursor + (6U * scale) >= kScreenWidth) {
            break;
        }
        if (c != ' ') {
            draw_char(fb, cursor, y, c, color, scale);
        }
        cursor += 6U * scale;
    }
}

void render() noexcept {
    auto* fb_ptr = static_cast<std::byte*>(display_raster_framebuffer());
    const auto fb_bytes = display_raster_framebuffer_bytes();
    if (fb_ptr == nullptr || fb_bytes < kScreenBytes) {
        return;
    }
    std::span<std::byte> fb{fb_ptr, fb_bytes};
    auto& rt = runtime();
    const auto view = resident_launcher_view(rt.launcher);

    fill_rect(fb, 0, 0, kScreenWidth, kScreenHeight, argb(18, 24, 28));
    fill_rect(fb, 0, 0, kScreenWidth, 132, argb(12, 60, 70));
    draw_text(fb, 36, 34, "CHARM RESIDENT LAUNCHER", argb(245, 248, 240), 3);

    const auto storage = h747_storage_state();
    if (view.catalog_code == app_abi::FirmwareCatalogCode::ok) {
        draw_text(fb, 38, 146, "/CHARM/APPS  encoder=select  press=run", argb(175, 218, 190), 2);
    } else {
        draw_text(fb, 38, 146, "catalog error", argb(255, 180, 120), 2);
        draw_text(fb, 220, 146, app_abi::firmware_catalog_code_name(view.catalog_code), argb(255, 180, 120), 2);
    }

    for (std::uint32_t i = 0; i < view.row_count; ++i) {
        const auto& row = view.rows[i];
        const auto y = kListTop + (i * kRowHeight);
        fill_rect(fb, 32, y, kScreenWidth - 64U, kRowHeight - 8U,
                  row.selected ? argb(230, 180, 80) : argb(36, 48, 54));
        draw_text(fb,
                  54,
                  y + 18U,
                  row.name,
                  row.selected ? argb(20, 24, 24) : argb(230, 236, 226),
                  2);
    }

    if (view.has_run_record) {
        fill_rect(fb, 32, 1000, kScreenWidth - 64U, 130, argb(32, 36, 42));
        draw_text(fb, 54, 1028, "APP RECORD", argb(245, 248, 240), 2);
        draw_text(fb, 240, 1028, app_abi::stage_name(view.run.result.stage), argb(180, 210, 255), 2);
        draw_text(fb, 54, 1078, resident_launcher_record_name(view.run), argb(180, 210, 255), 2);
        draw_text(fb, 300, 1078, app_abi::code_name(view.run.result.code), argb(180, 210, 255), 2);
    }

    draw_text(fb, 38, 1196, "emmc bus", argb(128, 158, 164), 2);
    char bus[24]{};
    const int n = std::snprintf(bus, sizeof(bus), "%lu clk 0x%08lx",
                                static_cast<unsigned long>(storage.selected_bus_width),
                                static_cast<unsigned long>(storage.clkcr));
    if (n > 0) {
        draw_text(fb, 170, 1196, std::string_view{bus, static_cast<std::size_t>(n)}, argb(128, 158, 164), 2);
    }

    (void)display_raster_present(fb.data(), kScreenBytes);
    resident_launcher_mark_clean(rt.launcher);
}

void refresh_catalog() noexcept {
    auto& rt = runtime();
    const auto storage = h747_storage_state();
    if (storage.ready == 0U || h747_storage_block_count() == 0U) {
        resident_launcher_apply_catalog_error(rt.launcher, app_abi::FirmwareCatalogCode::storage_unavailable);
        write_catalog_status(rt.launcher);
        return;
    }

    app_abi::FirmwareCatalog catalog{};
    const auto result = app_abi::firmware_catalog_scan_fat(
        emmc_reader(),
        0U,
        app_abi::kFirmwareCatalogDefaultDirectory,
        catalog);
    resident_launcher_apply_catalog(rt.launcher, result.code, catalog);
    write_catalog_status(rt.launcher);
}

std::uintptr_t cache_align_down(std::uintptr_t address) noexcept {
    return address & ~static_cast<std::uintptr_t>(31U);
}

std::uintptr_t cache_align_up(std::uintptr_t address) noexcept {
    return (address + 31U) & ~static_cast<std::uintptr_t>(31U);
}

void prepare_loaded_app_buffer(void* ctx) noexcept {
    auto* backend = static_cast<app_abi::AppElfLoadBackend*>(ctx);
    if (backend == nullptr || backend->last.plan.probe.code != app_abi::AppElfProbeCode::ok ||
        backend->last.plan.load_base == 0U || backend->last.plan.probe.load_span == 0U) {
        return;
    }
    const auto start = backend->last.plan.load_base;
    const auto end = start + backend->last.plan.probe.load_span;
    const auto aligned_start = cache_align_down(start);
    const auto aligned_end = cache_align_up(end);
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
        SCB_CleanDCache_by_Addr(reinterpret_cast<std::uint32_t*>(aligned_start),
                                static_cast<std::int32_t>(aligned_end - aligned_start));
    }
#endif
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_IC_Msk) != 0U) {
        SCB_InvalidateICache();
    }
#endif
    __DSB();
    __ISB();
}

app_abi::AppLoadResult load_elf(void* ctx,
                                const app_abi::AppImage& image,
                                const app_abi::AppLoadBuffer& buffer) noexcept {
    auto* backend = static_cast<app_abi::AppElfLoadBackend*>(ctx);
    if (backend == nullptr) {
        return {.code = app_abi::AppRunCode::invalid_argument};
    }
    return app_abi::app_elf_load_image(backend, image, buffer);
}

int app_console_write(const char* text, std::size_t len) {
    if (text == nullptr) {
        return -1;
    }
    runtime().app_console_bytes += static_cast<std::uint32_t>(len);
    for (std::size_t i = 0; i < len; ++i) {
        h747::console::write_char(text[i]);
    }
    return static_cast<int>(len);
}

std::uint32_t app_now_ms() {
    return h747::port::tick_ms();
}

int app_display_describe(CharmAppDisplayMode* out_mode) {
    if (out_mode == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    *out_mode = CharmAppDisplayMode{
        .width = kScreenWidth,
        .height = kScreenHeight,
        .stride_bytes = kScreenWidth * 4U,
        .format = CHARM_APP_PIXEL_FORMAT_ARGB8888,
    };
    return CHARM_APP_STATUS_OK;
}

int app_display_present(const void* pixels, std::uint32_t bytes) {
    if (pixels == nullptr || bytes == 0U) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    ++runtime().app_present_count;
    return display_raster_present(pixels, bytes) != 0U ? CHARM_APP_STATUS_OK : CHARM_APP_STATUS_IO_ERROR;
}

int app_input_poll(CharmAppInputState* out_state) {
    if (out_state == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    ++runtime().app_input_count;
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

int app_storage_open(const char*, int, int) {
    return -1;
}

int app_storage_read(int, void*, std::size_t) {
    return -1;
}

int app_storage_write(int, const void*, std::size_t) {
    return -1;
}

int app_storage_close(int) {
    return -1;
}

int app_afe_configure(std::uint32_t, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int app_afe_read(void*, std::size_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

void app_exit(int) {
}

CharmAppApi make_api() noexcept {
    CharmAppApi api{};
    api.magic = CHARM_APP_API_MAGIC;
    api.version = CHARM_APP_API_VERSION;
    api.size = sizeof(CharmAppApi);
    api.console = CharmAppConsoleApi{.write = app_console_write};
    api.time = CharmAppTimeApi{.now_ms = app_now_ms};
    api.display = CharmAppDisplayApi{.describe = app_display_describe, .present = app_display_present};
    api.input = CharmAppInputApi{.poll = app_input_poll};
    api.storage = CharmAppStorageApi{
        .open = app_storage_open,
        .read = app_storage_read,
        .write = app_storage_write,
        .close = app_storage_close,
    };
    api.afe = CharmAppAfeApi{.configure = app_afe_configure, .read = app_afe_read};
    api.app = CharmAppControlApi{.exit = app_exit};
    return api;
}

void run_selected(const ResidentLauncherRunRequest& request) noexcept {
    auto& rt = runtime();
    if (!request.requested) {
        return;
    }
    const auto& entry = request.entry;
    write_sv("resident_launcher: run ");
    write_sv(app_abi::fat_array_view(entry.name));
    h747::console::write_char('\n');

    const auto staged = app_abi::firmware_file_stage_fat(emmc_reader(), 0U, entry, stage_cache());
    if (staged.code != app_abi::FirmwareCatalogCode::ok) {
        resident_launcher_record_stage_failure(rt.launcher, entry, staged.code);
        write_sv("resident_launcher: stage failed code=");
        write_line(app_abi::firmware_catalog_code_name(staged.code));
        return;
    }

    rt.app_console_bytes = 0;
    rt.app_present_count = 0;
    rt.app_input_count = 0;
    app_abi::StagedAppImageSource staged_source{
        .image = staged.image,
        .load_ctx = &rt.elf_backend,
        .load = load_elf,
    };
    auto source = app_abi::make_staged_app_image_source(staged_source);
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> app_runtime{};
    const auto load = app_run_buffer();
    const auto run_result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{
            .base = load.data(),
            .size = load.size(),
            .align = kAppRunAlign,
            .prepare = prepare_loaded_app_buffer,
            .prepare_ctx = &rt.elf_backend,
        },
        .api = &api,
        .name = staged.image.name,
        .arg_text = {},
    });

    ResidentLauncherRunRecord record{};
    resident_launcher_copy_entry_name(record, entry);
    record.format = staged.image.format;
    record.source = entry.source;
    record.stage_code = staged.code;
    record.result = run_result;
    record.load_base = rt.elf_backend.last.plan.load_base;
    record.entry_address = rt.elf_backend.last.plan.entry_address;
    record.load_span = rt.elf_backend.last.plan.probe.load_span;
    record.segment_count = rt.elf_backend.last.plan.probe.segment_count;
    record.console_bytes = rt.app_console_bytes;
    record.present_count = rt.app_present_count;
    record.input_count = rt.app_input_count;
    resident_launcher_record_run(rt.launcher, record);

    write_sv("resident_launcher: app record source=emmc_fat format=elf name=");
    write_sv(staged.image.name);
    write_sv(" stage=");
    write_sv(app_abi::stage_name(run_result.stage));
    write_sv(" code=");
    write_sv(app_abi::code_name(run_result.code));
    write_sv(" exit=");
    h747::console::write_dec(static_cast<std::uint32_t>(run_result.exit_code));
    write_sv(" load=0x");
    h747::console::write_hex32(static_cast<std::uint32_t>(rt.elf_backend.last.plan.load_base));
    write_sv(" entry=0x");
    h747::console::write_hex32(static_cast<std::uint32_t>(rt.elf_backend.last.plan.entry_address));
    write_sv(" span=");
    h747::console::write_dec(rt.elf_backend.last.plan.probe.load_span);
    write_sv(" segments=");
    h747::console::write_dec(rt.elf_backend.last.plan.probe.segment_count);
    h747::console::write_char('\n');
}

} // namespace

void init() {
    write_line("resident_launcher: init");
    refresh_catalog();
    render();
}

void loop_once() noexcept {
    auto& rt = runtime();
    input_poll();
    const auto snapshot = input_snapshot();
    const auto request = resident_launcher_reduce(
        rt.launcher,
        ResidentLauncherInput{
            .select_delta = snapshot.encoder1.detent_delta + snapshot.encoder2.detent_delta,
            .confirm = snapshot.encoder1.button_pressed != 0U || snapshot.encoder2.button_pressed != 0U,
        });
    if (request.requested) {
        run_selected(request);
    }
    if (rt.launcher.dirty) {
        render();
    }
}

} // namespace h747::apps::resident_launcher
