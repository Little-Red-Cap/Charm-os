#include "dev_loader.h"

#include "charm_app_elf_probe.hpp"
#include "charm_app_received_image.hpp"
#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_commands.hpp"
#include "charm_dev_loader_hex.hpp"
#include "charm_dev_loader.hpp"
#include "charm_dev_loader_received_image.hpp"
#include "console.h"
#include "console_service.hpp"
#include "port.h"
#include "usb_dev_loader_service.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <utility>

import out.core;
import out.format;

namespace h747::apps::dev_loader {
namespace {

using namespace std::literals::string_view_literals;
namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;

constexpr std::uint32_t kDevRamBase = 0x24040000U;
constexpr std::uint32_t kDevRamCapacity = 256U * 1024U;
constexpr std::uint32_t kStageProbeScratchSize = 128U * 1024U;
constexpr std::uint32_t kElfProbeLoadBufferSize = 64U * 1024U;
constexpr std::uint32_t kPacketBufferCapacity = 512U;
constexpr std::uint32_t kPacketHexDecodeCapacity = 48U;
constexpr std::uint32_t kPacketMaxPayloadSize = 256U;
constexpr std::string_view kDefaultReceivedAppName = "received_app"sv;

struct AppRunLoadRegion {
    std::string_view name{};
    std::uintptr_t base{0};
    std::uint32_t size{0};
    std::uint32_t align{0};
    std::uintptr_t linked_elf_base{0};
};

constexpr AppRunLoadRegion kAppRunLoadRegion{
    .name = "ram_d1_app_elf"sv,
    .base = 0x24070000U,
    .size = 64U * 1024U,
    .align = 16U,
    .linked_elf_base = 0x24070000U,
};

static_assert(kAppRunLoadRegion.base == kAppRunLoadRegion.linked_elf_base,
              "dev_loader run region must match Examples/app_abi/elf_samples/app_elf.ld ELF_BASE");

loader::Storage ram_storage() noexcept;

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

struct Runtime {
    h747::console::ConsoleLineSource line_source{};
    loader::CommandRuntime commands{ram_storage()};
    loader::PacketRuntime packets{ram_storage()};
    std::array<std::byte, kPacketBufferCapacity> packet_buffer{};
    loader::ByteTransportRuntime packet_transport{
        packets,
        loader::ByteTransportConfig{
            .buffer = packet_buffer,
            .max_payload_size = kPacketMaxPayloadSize,
        },
    };
    bool prompt_needed{true};
    bool raw_active{false};
    std::uint32_t raw_bytes{0};
    loader::ByteTransportResult raw_last{};
    bool usb_active{false};
    std::uint32_t usb_bytes{0};
    loader::ByteTransportResult usb_last{};
    loader::ReceivedImageReadCode app_read_code{loader::ReceivedImageReadCode::not_launch_ready};
    app_abi::AppReceivedImageStageCode app_stage_code{app_abi::AppReceivedImageStageCode::not_verified};
    app_abi::AppElfProbeResult app_probe{};
    app_abi::AppElfLoadBackend app_elf_backend{};
    app_abi::AppRunCode app_plan_code{app_abi::AppRunCode::load_failed};
    int app_plan_backend_error{0};
    std::uintptr_t app_plan_load_base{0};
    std::uintptr_t app_plan_entry{0};
    app_abi::AppRunStage app_prepare_stage{app_abi::AppRunStage::idle};
    app_abi::AppRunCode app_prepare_code{app_abi::AppRunCode::load_failed};
    int app_prepare_backend_error{0};
    std::uintptr_t app_prepare_entry{0};
    int app_prepare_argc{0};
    bool app_prepare_ready{false};
    app_abi::AppRunStage app_run_stage{app_abi::AppRunStage::idle};
    app_abi::AppRunCode app_run_code{app_abi::AppRunCode::load_failed};
    int app_run_backend_error{0};
    int app_run_exit_code{0};
    bool app_run_exited{false};
    bool app_exit_requested{false};
    int app_exit_code{0};
    std::uint32_t app_console_bytes{0};
    std::uint32_t app_display_present_count{0};
    std::uint32_t app_display_last_bytes{0};
    std::uint32_t app_display_sample0{0};
    std::uint32_t app_input_poll_count{0};
    std::uint32_t app_read_bytes{0};
    std::uint32_t app_stage_bytes{0};
    std::array<char, app_abi::kAppReceivedImageMaxName> app_name_storage{};
    std::string_view app_name{kDefaultReceivedAppName};
    std::string_view app_last_command{"none"sv};
};

Runtime& runtime() noexcept {
    static Runtime rt{};
    return rt;
}

std::array<std::byte, kDevRamCapacity>& dev_ram() noexcept {
    alignas(32) static std::array<std::byte, kDevRamCapacity> ram{};
    return ram;
}

std::array<std::byte, kStageProbeScratchSize>& stage_probe_scratch() noexcept {
    alignas(32) static std::array<std::byte, kStageProbeScratchSize> cache{};
    return cache;
}

std::array<std::byte, kElfProbeLoadBufferSize>& elf_probe_load_buffer() noexcept {
    alignas(32) static std::array<std::byte, kElfProbeLoadBufferSize> buffer{};
    return buffer;
}

std::span<std::byte> app_run_load_buffer() noexcept {
    return {
        reinterpret_cast<std::byte*>(kAppRunLoadRegion.base),
        kAppRunLoadRegion.size,
    };
}

std::span<const std::byte> received_payload_view(const loader::ImageManifest& manifest) noexcept {
    auto& ram = dev_ram();
    if (manifest.load_address < kDevRamBase ||
        manifest.size_bytes > (ram.size() - (manifest.load_address - kDevRamBase))) {
        return {};
    }
    const auto offset = manifest.load_address - kDevRamBase;
    return {ram.data() + offset, manifest.size_bytes};
}

bool storage_write(void*, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto& ram = dev_ram();
    if (offset > ram.size() || bytes.size() > (ram.size() - offset)) {
        return false;
    }
    std::memcpy(ram.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool storage_read(void*, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto& ram = dev_ram();
    if (offset > ram.size() || bytes.size() > (ram.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), ram.data() + offset, bytes.size());
    return true;
}

loader::Storage ram_storage() noexcept {
    return loader::Storage{
        .ctx = nullptr,
        .base_address = kDevRamBase,
        .capacity_bytes = kDevRamCapacity,
        .write = storage_write,
        .read = storage_read,
    };
}

struct RuntimeElfSource {
    app_abi::AppImage image{};
    app_abi::AppElfLoadBackend* backend{nullptr};
};

const app_abi::AppImage* find_runtime_elf(void* ctx, std::string_view name) noexcept {
    auto* source = static_cast<RuntimeElfSource*>(ctx);
    if (source == nullptr || source->image.name != name) {
        return nullptr;
    }
    return &source->image;
}

app_abi::AppLoadResult load_runtime_elf(void* ctx,
                                        const app_abi::AppImage& image,
                                        const app_abi::AppLoadBuffer& buffer) noexcept {
    auto* source = static_cast<RuntimeElfSource*>(ctx);
    if (source == nullptr || source->backend == nullptr || &source->image != &image) {
        return {.code = app_abi::AppRunCode::image_not_found};
    }
    return app_abi::app_elf_load_image(source->backend, image, buffer);
}

CharmAppApi make_prepare_api() noexcept {
    CharmAppApi api{};
    api.magic = CHARM_APP_API_MAGIC;
    api.version = CHARM_APP_API_VERSION;
    api.size = sizeof(CharmAppApi);
    return api;
}

constexpr std::uintptr_t cache_align_down(const std::uintptr_t address) noexcept {
    return address & ~static_cast<std::uintptr_t>(31U);
}

constexpr std::uintptr_t cache_align_up(const std::uintptr_t address) noexcept {
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

int app_api_console_write(const char* text, const std::size_t len) {
    if (text == nullptr) {
        return -1;
    }
    runtime().app_console_bytes += static_cast<std::uint32_t>(len);
    for (std::size_t i = 0; i < len; ++i) {
        h747::console::write_char(text[i]);
    }
    return static_cast<int>(len);
}

std::uint32_t app_api_now_ms() {
    return h747::port::tick_ms();
}

int app_api_display_describe(CharmAppDisplayMode* out_mode) {
    if (out_mode == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    *out_mode = CharmAppDisplayMode{
        .width = 16U,
        .height = 16U,
        .stride_bytes = 16U * 4U,
        .format = CHARM_APP_PIXEL_FORMAT_ARGB8888,
    };
    return CHARM_APP_STATUS_OK;
}

int app_api_display_present(const void* pixels, const std::uint32_t bytes) {
    if (pixels == nullptr || bytes == 0U) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    auto& rt = runtime();
    ++rt.app_display_present_count;
    rt.app_display_last_bytes = bytes;
    if (bytes >= sizeof(std::uint32_t)) {
        std::uint32_t sample = 0;
        std::memcpy(&sample, pixels, sizeof(sample));
        rt.app_display_sample0 = sample;
    }
    return CHARM_APP_STATUS_OK;
}

int app_api_input_poll(CharmAppInputState* out_state) {
    if (out_state == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    ++runtime().app_input_poll_count;
    *out_state = CharmAppInputState{
        .encoder1_delta = 0,
        .encoder2_delta = 0,
        .encoder1_pressed = 0,
        .encoder2_pressed = 0,
        .pointer_detected = 0,
        .pointer_down = 0,
        .pointer_x = 0,
        .pointer_y = 0,
        .pointer_max_x = 16,
        .pointer_max_y = 16,
    };
    return CHARM_APP_STATUS_OK;
}

int app_api_storage_open(const char*, int, int) {
    return -1;
}

int app_api_storage_read(int, void*, std::size_t) {
    return -1;
}

int app_api_storage_write(int, const void*, std::size_t) {
    return -1;
}

int app_api_storage_close(int) {
    return -1;
}

int app_api_afe_configure(std::uint32_t, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int app_api_afe_read(void*, std::size_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

void app_api_exit(int code) {
    auto& rt = runtime();
    rt.app_exit_requested = true;
    rt.app_exit_code = code;
}

CharmAppApi make_run_api() noexcept {
    CharmAppApi api = make_prepare_api();
    api.flags = 0;
    api.console = CharmAppConsoleApi{.write = app_api_console_write};
    api.time = CharmAppTimeApi{.now_ms = app_api_now_ms};
    api.display = CharmAppDisplayApi{.describe = app_api_display_describe, .present = app_api_display_present};
    api.input = CharmAppInputApi{.poll = app_api_input_poll};
    api.storage = CharmAppStorageApi{
        .open = app_api_storage_open,
        .read = app_api_storage_read,
        .write = app_api_storage_write,
        .close = app_api_storage_close,
    };
    api.afe = CharmAppAfeApi{.configure = app_api_afe_configure, .read = app_api_afe_read};
    api.app = CharmAppControlApi{.exit = app_api_exit};
    return api;
}

void print_result(loader::Result result) noexcept {
    emit<"dev: stage={} code={} received={} crc=0x{:08x}/0x{:08x}\n">(
        loader::stage_name(result.stage),
        loader::code_name(result.code),
        result.received_bytes,
        result.actual_crc32,
        result.expected_crc32);
}

void print_status(const loader::CommandResult& command) noexcept {
    emit<"dev: ram base=0x{:08x} capacity={} cursor={}\n">(
        kDevRamBase,
        kDevRamCapacity,
        command.cursor);
    print_result(command.session);
    if (command.active) {
        const auto& manifest = command.manifest;
        emit<"dev: manifest load=0x{:08x} entry=0x{:08x} size={} flags=0x{:08x}\n">(
            manifest.load_address,
            manifest.entry_address,
            manifest.size_bytes,
            manifest.flags);
    }
}

void print_packet_result(const loader::ByteTransportResult& result) noexcept {
    const auto rx = h747::console::rx_stats();
    emit<"dev: packet transport={} buffered={} consumed={} dispatched={}\n">(
        loader::byte_transport_code_name(result.code),
        result.buffered_bytes,
        result.bytes_consumed,
        result.packets_dispatched);
    emit<"dev: packet last={} kind={} next_seq={} cursor={} active={}\n">(
        loader::packet_code_name(result.packet.packet_code),
        loader::packet_kind_name(result.packet.kind),
        result.packet.next_sequence,
        result.packet.cursor,
        result.packet.active ? 1U : 0U);
    emit<"dev: console rx bytes={} dma={} fallback={} overrun={} dma_started={} dma_fail={} pos={}/{} size={}\n">(
        rx.bytes,
        rx.dma_bytes,
        rx.fallback_bytes,
        rx.overrun_clears,
        rx.dma_started,
        rx.dma_start_failed,
        rx.dma_read_pos,
        rx.dma_write_pos,
        rx.dma_buffer_size);
    print_result(result.packet.receive);
    if (result.packet.active) {
        const auto& manifest = result.packet.manifest;
        emit<"dev: packet manifest load=0x{:08x} entry=0x{:08x} size={} flags=0x{:08x}\n">(
            manifest.load_address,
            manifest.entry_address,
            manifest.size_bytes,
            manifest.flags);
    }
}

void print_raw_status(const Runtime& rt) noexcept {
    emit<"dev: raw active={} bytes={}\n">(rt.raw_active ? 1U : 0U, rt.raw_bytes);
    print_packet_result(rt.raw_last);
}

void print_usb_status(const Runtime& rt) noexcept {
    const auto usb = h747::usb_dev_loader::status();
    emit<"dev: usb active={} bytes={} init={} started={} cdc_ready={} pcd={} usbd={} class={} iface={} start={}\n">(
        rt.usb_active ? 1U : 0U,
        rt.usb_bytes,
        usb.init_called,
        usb.started,
        usb.cdc_ready,
        usb.pcd_init_status,
        usb.usbd_init_status,
        usb.register_class_status,
        usb.register_interface_status,
        usb.usbd_start_status);
    emit<"dev: usb rx packets={} bytes={} read={} dropped={} overflow={} ctrl={} last_ctrl={}/{}\n">(
        usb.rx_packets,
        usb.rx_bytes,
        usb.bytes_read,
        usb.rx_dropped_bytes,
        usb.rx_overflow_count,
        usb.control_requests,
        usb.last_control_cmd,
        usb.last_control_length);
    emit<"dev: usb bus setup={} reset={} suspend={} resume={} connect={} disconnect={} out_ep1={} in_ep1={}\n">(
        usb.setup_count,
        usb.reset_count,
        usb.suspend_count,
        usb.resume_count,
        usb.connect_count,
        usb.disconnect_count,
        usb.out_ep1_hits,
        usb.in_ep1_hits);
    print_packet_result(rt.usb_last);
}

void print_app_status(const Runtime& rt) noexcept {
    const auto run_state = (rt.app_last_command == "run"sv) ? "enabled"sv : "disabled"sv;
    emit<"dev: app command={} name={} run={}\n">(rt.app_last_command, rt.app_name, run_state);
    emit<"dev: app run-region name={} base=0x{:08x} size={} align={} linked_elf_base=0x{:08x}\n">(
        kAppRunLoadRegion.name,
        static_cast<std::uint32_t>(kAppRunLoadRegion.base),
        kAppRunLoadRegion.size,
        kAppRunLoadRegion.align,
        static_cast<std::uint32_t>(kAppRunLoadRegion.linked_elf_base));
    emit<"dev: app read={} bytes={}\n">(
        loader::received_image_read_code_name(rt.app_read_code),
        rt.app_read_bytes);
    emit<"dev: app stage={} bytes={}\n">(
        app_abi::app_received_image_stage_code_name(rt.app_stage_code),
        rt.app_stage_bytes);
    emit<"dev: app probe={} entry_off=0x{:08x} span={} segments={} runnable={}\n">(
        app_abi::app_elf_probe_code_name(rt.app_probe.code),
        rt.app_probe.entry_offset,
        rt.app_probe.load_span,
        rt.app_probe.segment_count,
        rt.app_probe.runnable ? 1U : 0U);
    emit<"dev: app plan={} backend={} load=0x{:08x} entry=0x{:08x} span={} segments={} runnable={} run=disabled\n">(
        app_abi::code_name(rt.app_plan_code),
        rt.app_plan_backend_error,
        static_cast<std::uint32_t>(rt.app_plan_load_base),
        static_cast<std::uint32_t>(rt.app_plan_entry),
        rt.app_probe.load_span,
        rt.app_probe.segment_count,
        rt.app_probe.runnable ? 1U : 0U);
    emit<"dev: app prepare={} code={} backend={} argc={} ready={} entry=0x{:08x} run=disabled\n">(
        app_abi::stage_name(rt.app_prepare_stage),
        app_abi::code_name(rt.app_prepare_code),
        rt.app_prepare_backend_error,
        rt.app_prepare_argc,
        rt.app_prepare_ready ? 1U : 0U,
        static_cast<std::uint32_t>(rt.app_prepare_entry));
    emit<"dev: app run stage={} code={} backend={} load=0x{:08x} entry=0x{:08x} span={} segments={} exited={} exit={} app_exit={} app_exit_code={}\n">(
        app_abi::stage_name(rt.app_run_stage),
        app_abi::code_name(rt.app_run_code),
        rt.app_run_backend_error,
        static_cast<std::uint32_t>(rt.app_plan_load_base),
        static_cast<std::uint32_t>(rt.app_plan_entry),
        rt.app_probe.load_span,
        rt.app_probe.segment_count,
        rt.app_run_exited ? 1U : 0U,
        rt.app_run_exit_code,
        rt.app_exit_requested ? 1U : 0U,
        rt.app_exit_code);
    emit<"dev: app caps console_bytes={} present_count={} present_bytes={} sample0=0x{:08x} input_polls={}\n">(
        rt.app_console_bytes,
        rt.app_display_present_count,
        rt.app_display_last_bytes,
        rt.app_display_sample0,
        rt.app_input_poll_count);
}

void print_help() noexcept {
    emit<"Commands:\n">();
    emit<"  help                       - Show help\n">();
    emit<"  dev status                 - Show dev-loader session\n">();
    emit<"  dev begin <size> [crc_hex] - Start RAM receive session\n">();
    emit<"  dev fill <hex_byte> <cnt>  - Append repeated test bytes\n">();
    emit<"  dev verify                 - Verify received image\n">();
    emit<"  dev launch dry-run         - Mark launch-ready without jumping\n">();
    emit<"  dev abort                  - Abort current session\n">();
    emit<"  dev packet status          - Show packet byte transport\n">();
    emit<"  dev packet ingest <hex>    - Feed packet bytes as hex pairs\n">();
    emit<"  dev packet reset           - Reset packet byte buffer\n">();
    emit<"  dev packet reset-session   - Reset packet byte buffer and receive session\n">();
    emit<"  dev raw begin              - Enter raw packetstream receive mode\n">();
    emit<"  dev raw status             - Show raw packetstream receive state\n">();
    emit<"  dev raw abort              - Leave raw receive mode\n">();
    emit<"  dev usb begin              - Enter exclusive USB CDC packetstream receive mode\n">();
    emit<"  dev usb status             - Show USB packetstream receive state\n">();
    emit<"  dev usb abort              - Stop USB receive mode and disconnect USB\n">();
    emit<"  dev app stage <name>       - Stage launch_ready payload as App ELF\n">();
    emit<"  dev app probe <name>       - Stage and ELF-probe payload without running\n">();
    emit<"  dev app prepare <name> [args...] - Prepare AppRuntime argv/ABI without running\n">();
    emit<"  dev app run <name> [args...] - Explicitly call charm_app_main from loaded ELF\n">();
    emit<"  dev app status             - Show received App stage/probe diagnostics\n">();
}

void prompt() noexcept {
    emit<"\r\ndev-loader> ">();
}

constexpr std::string_view trim_left(std::string_view sv) noexcept {
    while (!sv.empty() && sv.front() == ' ') {
        sv.remove_prefix(1);
    }
    return sv;
}

constexpr std::pair<std::string_view, std::string_view> split_first_word(std::string_view sv) noexcept {
    sv = trim_left(sv);
    const auto pos = sv.find(' ');
    if (pos == std::string_view::npos) {
        return {sv, {}};
    }
    return {sv.substr(0, pos), trim_left(sv.substr(pos + 1U))};
}

std::uint32_t count_app_argv(std::string_view arg_text) noexcept {
    std::uint32_t argc = 1U; // argv[0] is the App image name.
    auto remaining = trim_left(arg_text);
    while (!remaining.empty()) {
        auto [token, rest] = split_first_word(remaining);
        if (token.empty()) {
            break;
        }
        ++argc;
        remaining = rest;
    }
    return argc;
}

void handle_packet_command(std::string_view line) noexcept {
    auto& rt = runtime();
    if (line == "dev packet status") {
        print_packet_result(rt.packet_transport.status());
        return;
    }
    if (line == "dev packet reset") {
        rt.packet_transport.reset();
        print_packet_result(rt.packet_transport.status());
        return;
    }
    if (line == "dev packet reset-session") {
        rt.packet_transport.reset_session();
        print_packet_result(rt.packet_transport.status());
        return;
    }
    if (line.starts_with("dev packet ingest ")) {
        std::array<std::byte, kPacketHexDecodeCapacity> decoded{};
        const auto hex = loader::hex_decode_bytes(line.substr(18), decoded);
        if (hex.code != loader::HexDecodeCode::ok) {
            emit<"dev: packet hex error={} digits={} bytes={}\n">(
                loader::hex_decode_code_name(hex.code),
                hex.digits_seen,
                hex.bytes_written);
            return;
        }
        const auto result = rt.packet_transport.ingest(
            std::span<const std::byte>{decoded.data(), hex.bytes_written});
        print_packet_result(result);
        return;
    }
    emit<"dev: packet usage error\n">();
    emit<"dev: use 'dev packet status', 'dev packet ingest <hex>', 'dev packet reset', or 'dev packet reset-session'\n">();
}

void handle_raw_command(std::string_view line) noexcept {
    auto& rt = runtime();
    if (line == "dev raw begin") {
        rt.packet_transport.reset_session();
        rt.raw_active = true;
        rt.raw_bytes = 0;
        rt.raw_last = rt.packet_transport.status();
        emit<"dev: raw ready max_payload={}\n">(kPacketMaxPayloadSize);
        return;
    }
    if (line == "dev raw status") {
        rt.raw_last = rt.packet_transport.status();
        print_raw_status(rt);
        return;
    }
    if (line == "dev raw abort") {
        rt.raw_active = false;
        rt.packet_transport.reset_session();
        rt.raw_last = rt.packet_transport.status();
        print_raw_status(rt);
        return;
    }
    emit<"dev: raw usage error\n">();
    emit<"dev: use 'dev raw begin', 'dev raw status', or 'dev raw abort'\n">();
}

void handle_usb_command(std::string_view line) noexcept {
    auto& rt = runtime();
    if (line == "dev usb begin") {
        rt.packet_transport.reset_session();
        h747::usb_dev_loader::init();
        rt.usb_active = true;
        rt.usb_bytes = 0;
        rt.usb_last = rt.packet_transport.status();
        const auto usb = h747::usb_dev_loader::status();
        emit<"dev: usb ready started={} cdc_ready={} pcd={} usbd={} class={} iface={} start={}\n">(
            usb.started,
            usb.cdc_ready,
            usb.pcd_init_status,
            usb.usbd_init_status,
            usb.register_class_status,
            usb.register_interface_status,
            usb.usbd_start_status);
        return;
    }
    if (line == "dev usb status") {
        rt.usb_last = rt.packet_transport.status();
        print_usb_status(rt);
        return;
    }
    if (line == "dev usb abort") {
        rt.usb_active = false;
        h747::usb_dev_loader::stop();
        rt.packet_transport.reset_session();
        rt.usb_last = rt.packet_transport.status();
        print_usb_status(rt);
        return;
    }
    emit<"dev: usb usage error\n">();
    emit<"dev: use 'dev usb begin', 'dev usb status', or 'dev usb abort'\n">();
}

bool set_app_name(Runtime& rt, std::string_view name) noexcept {
    name = name.empty() ? kDefaultReceivedAppName : name;
    rt.app_name_storage.fill('\0');
    if (name.size() + 1U > rt.app_name_storage.size()) {
        constexpr auto too_long = "name_too_long"sv;
        std::memcpy(rt.app_name_storage.data(), too_long.data(), too_long.size());
        rt.app_name = {rt.app_name_storage.data(), too_long.size()};
        return false;
    }
    std::memcpy(rt.app_name_storage.data(), name.data(), name.size());
    rt.app_name = {rt.app_name_storage.data(), name.size()};
    return true;
}

bool reset_app_diagnostics(Runtime& rt, std::string_view command, std::string_view name) noexcept {
    rt.app_last_command = command;
    const bool name_ok = set_app_name(rt, name);
    rt.app_read_code = loader::ReceivedImageReadCode::not_launch_ready;
    rt.app_stage_code = app_abi::AppReceivedImageStageCode::not_verified;
    rt.app_probe = app_abi::AppElfProbeResult{.code = app_abi::AppElfProbeCode::invalid_argument};
    rt.app_elf_backend = {};
    rt.app_plan_code = app_abi::AppRunCode::load_failed;
    rt.app_plan_backend_error = 0;
    rt.app_plan_load_base = 0;
    rt.app_plan_entry = 0;
    rt.app_prepare_stage = app_abi::AppRunStage::idle;
    rt.app_prepare_code = app_abi::AppRunCode::load_failed;
    rt.app_prepare_backend_error = 0;
    rt.app_prepare_entry = 0;
    rt.app_prepare_argc = 0;
    rt.app_prepare_ready = false;
    rt.app_run_stage = app_abi::AppRunStage::idle;
    rt.app_run_code = app_abi::AppRunCode::load_failed;
    rt.app_run_backend_error = 0;
    rt.app_run_exit_code = 0;
    rt.app_run_exited = false;
    rt.app_exit_requested = false;
    rt.app_exit_code = 0;
    rt.app_console_bytes = 0;
    rt.app_display_present_count = 0;
    rt.app_display_last_bytes = 0;
    rt.app_display_sample0 = 0;
    rt.app_input_poll_count = 0;
    rt.app_read_bytes = 0;
    rt.app_stage_bytes = 0;
    if (!name_ok) {
        rt.app_read_code = loader::ReceivedImageReadCode::invalid_argument;
        rt.app_stage_code = app_abi::AppReceivedImageStageCode::name_too_long;
    }
    return name_ok;
}

app_abi::AppImage stage_received_app(Runtime& rt, std::string_view command, std::string_view name) noexcept {
    if (!reset_app_diagnostics(rt, command, name)) {
        return {};
    }

    const auto status = rt.packet_transport.status();
    const auto payload = received_payload_view(status.packet.manifest);
    auto& scratch = stage_probe_scratch();
    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = status.packet.receive,
        .manifest = status.packet.manifest,
        .storage = ram_storage(),
        .output = scratch,
    });
    rt.app_read_code = read.code;
    rt.app_read_bytes = read.bytes_read;
    if (read.code != loader::ReceivedImageReadCode::ok || payload.empty()) {
        if (read.code == loader::ReceivedImageReadCode::ok) {
            rt.app_read_code = loader::ReceivedImageReadCode::invalid_argument;
        }
        return {};
    }

    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = rt.app_name,
        .format = app_abi::AppImageFormat::elf,
        .image = payload,
        .verified = true,
        .cache = scratch,
    });
    rt.app_stage_code = staged.code;
    rt.app_stage_bytes = staged.bytes_copied;
    if (staged.code != app_abi::AppReceivedImageStageCode::ok) {
        return {};
    }
    return staged.image;
}

void prepare_received_app(Runtime& rt, std::string_view name, std::string_view args) noexcept {
    const auto image = stage_received_app(rt, "prepare"sv, name);
    if (rt.app_stage_code != app_abi::AppReceivedImageStageCode::ok) {
        return;
    }

    auto& load = elf_probe_load_buffer();
    RuntimeElfSource source_ctx{
        .image = image,
        .backend = &rt.app_elf_backend,
    };
    app_abi::AppImageSource source{
        .ctx = &source_ctx,
        .find = find_runtime_elf,
        .load = load_runtime_elf,
    };
    CharmAppApi api = make_prepare_api();
    app_abi::AppRuntime<> app_runtime{};
    const auto prepared = app_runtime.prepare(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{
            .base = load.data(),
            .size = load.size(),
            .align = 16,
        },
        .api = &api,
        .name = rt.app_name,
        .arg_text = args,
    });

    rt.app_probe = rt.app_elf_backend.last.plan.probe;
    rt.app_plan_code = rt.app_elf_backend.last.code;
    rt.app_plan_backend_error = rt.app_elf_backend.last.backend_error;
    rt.app_plan_load_base = rt.app_elf_backend.last.plan.load_base;
    rt.app_plan_entry = rt.app_elf_backend.last.plan.entry_address;
    rt.app_prepare_stage = prepared.result.stage;
    rt.app_prepare_code = prepared.result.code;
    rt.app_prepare_backend_error = prepared.result.backend_error;
    rt.app_prepare_entry = reinterpret_cast<std::uintptr_t>(prepared.image.entry);
    rt.app_prepare_argc = prepared.argc;
    rt.app_prepare_ready = prepared.ready;
}

void run_received_app(Runtime& rt, std::string_view name, std::string_view args) noexcept {
    const auto image = stage_received_app(rt, "run"sv, name);
    if (rt.app_stage_code != app_abi::AppReceivedImageStageCode::ok) {
        return;
    }

    const auto load = app_run_load_buffer();
    RuntimeElfSource source_ctx{
        .image = image,
        .backend = &rt.app_elf_backend,
    };
    app_abi::AppImageSource source{
        .ctx = &source_ctx,
        .find = find_runtime_elf,
        .load = load_runtime_elf,
    };
    CharmAppApi api = make_run_api();
    app_abi::AppRuntime<> app_runtime{};
    const auto result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{
            .base = load.data(),
            .size = load.size(),
            .align = kAppRunLoadRegion.align,
            .prepare = prepare_loaded_app_buffer,
            .prepare_ctx = &rt.app_elf_backend,
        },
        .api = &api,
        .name = rt.app_name,
        .arg_text = args,
    });

    rt.app_probe = rt.app_elf_backend.last.plan.probe;
    rt.app_plan_code = rt.app_elf_backend.last.code;
    rt.app_plan_backend_error = rt.app_elf_backend.last.backend_error;
    rt.app_plan_load_base = rt.app_elf_backend.last.plan.load_base;
    rt.app_plan_entry = rt.app_elf_backend.last.plan.entry_address;
    if (result.stage == app_abi::AppRunStage::exit || result.stage == app_abi::AppRunStage::start) {
        rt.app_prepare_stage = app_abi::AppRunStage::start;
        rt.app_prepare_code = app_abi::AppRunCode::ok;
        rt.app_prepare_backend_error = 0;
        rt.app_prepare_entry = rt.app_plan_entry;
        rt.app_prepare_argc = static_cast<int>(count_app_argv(args));
        rt.app_prepare_ready = true;
    }
    rt.app_run_stage = result.stage;
    rt.app_run_code = result.code;
    rt.app_run_backend_error = result.backend_error;
    rt.app_run_exit_code = result.exit_code;
    rt.app_run_exited = result.exited;
}

void handle_app_command(std::string_view line) noexcept {
    auto& rt = runtime();
    if (line == "dev app status") {
        print_app_status(rt);
        return;
    }
    if (line.starts_with("dev app stage")) {
        auto name = trim_left(line.substr(13));
        if (name.empty()) {
            name = kDefaultReceivedAppName;
        }
        (void)stage_received_app(rt, "stage"sv, name);
        print_app_status(rt);
        return;
    }
    if (line.starts_with("dev app probe")) {
        auto name = trim_left(line.substr(13));
        if (name.empty()) {
            name = kDefaultReceivedAppName;
        }
        const auto image = stage_received_app(rt, "probe"sv, name);
        if (rt.app_stage_code == app_abi::AppReceivedImageStageCode::ok) {
            auto& load = elf_probe_load_buffer();
            const auto loaded = app_abi::app_elf_load_image(&rt.app_elf_backend,
                                                            image,
                                                            app_abi::AppLoadBuffer{
                                                                .base = load.data(),
                                                                .size = load.size(),
                                                                .align = 16,
                                                            });
            rt.app_probe = rt.app_elf_backend.last.plan.probe;
            rt.app_plan_code = loaded.code;
            rt.app_plan_backend_error = loaded.backend_error;
            rt.app_plan_load_base = rt.app_elf_backend.last.plan.load_base;
            rt.app_plan_entry = rt.app_elf_backend.last.plan.entry_address;
        }
        print_app_status(rt);
        return;
    }
    if (line.starts_with("dev app prepare")) {
        auto [name, args] = split_first_word(line.substr(15));
        if (name.empty()) {
            name = kDefaultReceivedAppName;
        }
        prepare_received_app(rt, name, args);
        print_app_status(rt);
        return;
    }
    if (line.starts_with("dev app run")) {
        auto [name, args] = split_first_word(line.substr(11));
        if (name.empty()) {
            name = kDefaultReceivedAppName;
        }
        run_received_app(rt, name, args);
        print_app_status(rt);
        return;
    }
    emit<"dev: app usage error\n">();
    emit<"dev: use 'dev app stage <name>', 'dev app probe <name>', 'dev app prepare <name> [args...]', 'dev app run <name> [args...]', or 'dev app status'\n">();
}

void handle_command(std::string_view line) noexcept {
    if (line.starts_with("dev app")) {
        handle_app_command(line);
        return;
    }
    if (line.starts_with("dev usb")) {
        handle_usb_command(line);
        return;
    }
    if (line.starts_with("dev raw")) {
        handle_raw_command(line);
        return;
    }
    if (line.starts_with("dev packet")) {
        handle_packet_command(line);
        return;
    }

    const auto result = runtime().commands.handle(line);
    switch (result.kind) {
        case loader::CommandKind::none:
            return;
        case loader::CommandKind::help:
            print_help();
            return;
        case loader::CommandKind::status:
            print_status(result);
            return;
        case loader::CommandKind::begin:
        case loader::CommandKind::fill:
        case loader::CommandKind::verify:
        case loader::CommandKind::launch_dry_run:
        case loader::CommandKind::abort:
            print_result(result.session);
            return;
        case loader::CommandKind::usage_error:
            emit<"dev: usage error command={}\n">(loader::command_kind_name(result.kind));
            print_result(result.session);
            return;
        case loader::CommandKind::unknown:
            emit<"unknown command\n">();
            return;
    }
}

void pump_raw_uart(Runtime& rt) noexcept {
    std::array<std::uint8_t, 64> raw{};
    const auto count = h747::console::poll_bytes(raw);
    if (count == 0U) {
        return;
    }

    rt.raw_bytes += count;
    rt.raw_last = rt.packet_transport.ingest(
        std::as_bytes(std::span<const std::uint8_t>{raw.data(), count}));
    if (rt.raw_last.code != loader::ByteTransportCode::ok ||
        rt.raw_last.packet.kind == loader::PacketKind::abort ||
        rt.raw_last.packet.receive.stage == loader::Stage::launch_ready) {
        rt.raw_active = false;
        emit<"\n">();
        print_raw_status(rt);
        rt.prompt_needed = true;
    }
}

void pump_usb(Runtime& rt) noexcept {
    h747::usb_dev_loader::poll_irq();
    std::array<std::uint8_t, 256> raw{};
    const auto count = h747::usb_dev_loader::read(raw);
    if (count == 0U) {
        return;
    }

    rt.usb_bytes += static_cast<std::uint32_t>(count);
    rt.usb_last = rt.packet_transport.ingest(
        std::as_bytes(std::span<const std::uint8_t>{raw.data(), count}));
    if (rt.usb_last.code != loader::ByteTransportCode::ok ||
        rt.usb_last.packet.kind == loader::PacketKind::abort ||
        rt.usb_last.packet.receive.stage == loader::Stage::launch_ready) {
        rt.usb_active = false;
        h747::usb_dev_loader::stop();
        emit<"\n">();
        print_usb_status(rt);
        rt.prompt_needed = true;
    }
}

} // namespace

void init() {
    emit<"dev_loader: resident RAM dev-loader skeleton ready\n">();
    emit<"dev_loader: transport=console-test/raw-uart/usb-cdc launch=app-run-explicit\n">();
    print_help();
    print_status(runtime().commands.status());
}

void loop_once() noexcept {
    auto& rt = runtime();
    if (rt.raw_active) {
        pump_raw_uart(rt);
        return;
    }
    if (rt.usb_active) {
        pump_usb(rt);
    }
    if (rt.prompt_needed) {
        prompt();
        rt.prompt_needed = false;
    }
    if (const auto line = rt.line_source.poll_line()) {
        handle_command(*line);
        rt.prompt_needed = true;
    }
}

} // namespace h747::apps::dev_loader
