#include "dev_loader.h"

#include "charm_app_elf_probe.hpp"
#include "charm_app_received_image.hpp"
#include "charm_app_store.hpp"
#include "charm_app_store_install.hpp"
#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_commands.hpp"
#include "charm_dev_loader_hex.hpp"
#include "charm_dev_loader.hpp"
#include "charm_dev_loader_received_image.hpp"
#include "charm_dev_loader_store_handoff.hpp"
#include "charm_app_staged_runtime.hpp"
#include "console.h"
#include "console_service.hpp"
#include "memory_probe.h"
#include "port.h"
#include "qspi_nor.h"
#include "storage.h"
#include "usb_dev_loader_service.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <utility>

import out.core;
import out.format;
import module_core;
import module_link;
import module_loader;
import module_view;

#include "charm_app_modulex_loader.hpp"

namespace h747::apps::dev_loader {
namespace {

using namespace std::literals::string_view_literals;
namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;

struct Runtime;

constexpr std::uint32_t kDevRamBase = 0x24040000U;
constexpr std::uint32_t kDevRamCapacity = 256U * 1024U;
constexpr std::uint32_t kStageProbeScratchSize = 128U * 1024U;
constexpr std::uint32_t kD1FallbackCapacity = 128U * 1024U;
constexpr std::uint32_t kElfProbeLoadBufferSize = 64U * 1024U;
constexpr std::uint32_t kPacketBufferCapacity = 1024U;
constexpr std::uint32_t kPacketHexDecodeCapacity = 48U;
constexpr std::uint32_t kPacketMaxPayloadSize = 256U;
constexpr std::uint32_t kUsbReadChunkSize = 512U;
constexpr std::uint32_t kUsbDrainLimitBytes = 4096U;
constexpr std::uint32_t kQspiStoreBaseOffset = 0U;
constexpr std::uint32_t kEmmcStoreTargetOffset = 0U;
constexpr std::uint32_t kEmmcStoreSlotBytes = 16U * 1024U * 1024U;
constexpr std::uint32_t kEmmcSupportedBlockSize = 512U;
constexpr std::uintptr_t kSdram2ArenaBase = 0xD0000000U;
constexpr std::string_view kDefaultReceivedAppName = "received_app"sv;

enum class UsbExitReason : std::uint8_t {
    idle,
    active,
    launch_ready,
    packet_error,
    transport_error,
    abort,
};

constexpr std::string_view usb_exit_reason_name(const UsbExitReason reason) noexcept {
    switch (reason) {
        case UsbExitReason::idle:
            return "idle"sv;
        case UsbExitReason::active:
            return "active"sv;
        case UsbExitReason::launch_ready:
            return "launch_ready"sv;
        case UsbExitReason::packet_error:
            return "packet_error"sv;
        case UsbExitReason::transport_error:
            return "transport_error"sv;
        case UsbExitReason::abort:
            return "abort"sv;
    }
    return "unknown"sv;
}

struct AppRunLoadRegion {
    std::string_view name{};
    std::uintptr_t base{0};
    std::uint32_t size{0};
    std::uint32_t align{0};
    std::uintptr_t linked_elf_base{0};
};

struct ImageStageArena {
    std::string_view name{};
    std::uintptr_t base{0};
    std::uint32_t size{0};
    std::uint32_t align{0};
};

struct EmmcStoreLayout {
    bool ready{false};
    std::uint32_t block_size{0};
    std::uint32_t exposed_blocks{0};
    std::uint32_t slot_lba{0};
    std::uint32_t slot_blocks{0};
    std::uint32_t slot_bytes{0};
};

struct EmmcStoreContext {
    EmmcStoreLayout layout{};
};

struct ModuleXRuntimeLoadContext {
    Runtime* rt{};
    app_abi::AppImage materialized{};
};

constexpr ImageStageArena kImageStageArena{
    .name = "sdram2_stage_cache"sv,
    .base = kSdram2ArenaBase + kDevRamCapacity,
    .size = kStageProbeScratchSize,
    .align = 32U,
};

constexpr ImageStageArena kReceiveArena{
    .name = "sdram2_receive_buffer"sv,
    .base = kSdram2ArenaBase,
    .size = kDevRamCapacity,
    .align = 32U,
};

constexpr ImageStageArena kD1ReceiveArena{
    .name = "ram_d1_receive_fallback"sv,
    .base = kDevRamBase,
    .size = kD1FallbackCapacity,
    .align = 32U,
};

constexpr ImageStageArena kD1StageArena{
    .name = "ram_d1_stage_fallback"sv,
    .base = kDevRamBase,
    .size = kD1FallbackCapacity,
    .align = 32U,
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

constexpr bool app_run_region_fits(std::uint32_t needed) noexcept {
    return needed != 0U && needed <= kAppRunLoadRegion.size;
}

constexpr std::uint32_t app_run_region_free(std::uint32_t needed) noexcept {
    return needed < kAppRunLoadRegion.size ? (kAppRunLoadRegion.size - needed) : 0U;
}

loader::Storage ram_storage() noexcept;
app_abi::AppImage stage_qspi_app(Runtime& rt, std::string_view command, std::string_view spec) noexcept;
app_abi::AppImage stage_emmc_app(Runtime& rt, std::string_view command, std::string_view spec) noexcept;
constexpr std::string_view app_image_format_name(app_abi::AppImageFormat format) noexcept;
std::span<const std::byte> received_payload_view(loader::Storage storage,
                                                 const loader::ImageManifest& manifest) noexcept;
std::span<std::byte> stage_scratch() noexcept;
ImageStageArena receive_arena_descriptor(loader::Storage storage) noexcept;
ImageStageArena stage_arena_descriptor() noexcept;

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

void emit_text(const char* text) noexcept {
    if (text == nullptr) {
        return;
    }
    (void)console_stream().write(std::string_view{text});
}

void emit_hex_bytes(const char* label, const std::uint8_t* bytes, const std::size_t len) noexcept {
    if (label != nullptr) {
        emit_text(label);
    }
    static constexpr char kHex[] = "0123456789ABCDEF";
    char buf[4] = {' ', '0', '0', '\0'};
    for (std::size_t i = 0; i < len; ++i) {
        buf[1] = kHex[(bytes[i] >> 4U) & 0x0FU];
        buf[2] = kHex[bytes[i] & 0x0FU];
        emit_text(buf);
    }
    emit_text("\n");
}

struct Runtime {
    h747::console::ConsoleLineSource line_source{};
    loader::Storage receive_storage{ram_storage()};
    loader::CommandRuntime commands{receive_storage};
    loader::PacketRuntime packets{receive_storage};
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
    UsbExitReason usb_exit_reason{UsbExitReason::idle};
    bool qspi_ready{false};
    app_abi::AppStoreInstallCode store_install_code{app_abi::AppStoreInstallCode::invalid_argument};
    loader::ReceivedImageReadCode store_receive_code{loader::ReceivedImageReadCode::not_launch_ready};
    app_abi::AppStoreReadCode store_read_code{app_abi::AppStoreReadCode::invalid_argument};
    std::uint32_t store_install_target{kQspiStoreBaseOffset};
    std::uint32_t store_install_written{0};
    std::uint32_t store_install_erased{0};
    std::uint32_t store_receive_bytes{0};
    app_abi::AppStoreHeader store_header{};
    app_abi::AppStoreLookupResult store_lookup{};
    loader::ReceivedImageReadCode app_read_code{loader::ReceivedImageReadCode::not_launch_ready};
    app_abi::AppReceivedImageStageCode app_stage_code{app_abi::AppReceivedImageStageCode::not_verified};
    app_abi::AppImageFormat app_image_format{app_abi::AppImageFormat::elf};
    app_abi::AppElfProbeResult app_probe{};
    app_abi::AppElfLoadBackend app_elf_backend{};
    app_abi::AppModuleXLoadCode app_modulex_code{app_abi::AppModuleXLoadCode::invalid_argument};
    app_abi::AppModuleXLoadResult app_modulex_last{};
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
    std::array<char, app_abi::kAppReceivedImageMaxName> app_record_name_storage{};
    std::string_view app_name{kDefaultReceivedAppName};
    std::string_view app_record_name{kDefaultReceivedAppName};
    std::string_view app_source{"received"sv};
    std::string_view app_last_command{"none"sv};
};

Runtime& runtime() noexcept {
    static Runtime rt{};
    return rt;
}

std::array<std::byte, kDevRamCapacity>& dev_ram() noexcept {
    // .sdram is NOLOAD; keep this as explicitly written scratch storage only.
    alignas(32) __attribute__((section(".sdram.dev_loader_receive")))
    static std::array<std::byte, kDevRamCapacity> ram;
    return ram;
}

std::array<std::byte, kStageProbeScratchSize>& stage_probe_scratch() noexcept {
    // SDRAM smoke may touch this arena before commands run; never store static state here.
    alignas(32) __attribute__((section(".sdram.dev_loader_stage")))
    static std::array<std::byte, kStageProbeScratchSize> cache;
    return cache;
}

std::array<std::byte, kD1FallbackCapacity>& d1_receive_fallback() noexcept {
    alignas(32) static std::array<std::byte, kD1FallbackCapacity> ram{};
    return ram;
}

std::array<std::byte, kD1FallbackCapacity>& d1_stage_fallback() noexcept {
    alignas(32) static std::array<std::byte, kD1FallbackCapacity> cache{};
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

std::span<const std::byte> received_payload_view(loader::Storage storage,
                                                 const loader::ImageManifest& manifest) noexcept {
    if (manifest.load_address < storage.base_address ||
        manifest.size_bytes > (storage.capacity_bytes - (manifest.load_address - storage.base_address))) {
        return {};
    }
    const auto offset = manifest.load_address - storage.base_address;
    const auto memory = storage.base_address == kDevRamBase
        ? std::span<std::byte>{d1_receive_fallback()}
        : std::span<std::byte>{dev_ram()};
    return {memory.data() + offset, manifest.size_bytes};
}

EmmcStoreLayout emmc_store_layout() noexcept {
    const auto block_size = h747_storage_block_size();
    const auto blocks = h747_storage_block_count();
    EmmcStoreLayout layout{
        .ready = false,
        .block_size = block_size,
        .exposed_blocks = blocks,
    };
    if (block_size != kEmmcSupportedBlockSize || blocks == 0U) {
        return layout;
    }

    const std::uint32_t requested_blocks = kEmmcStoreSlotBytes / block_size;
    const std::uint32_t slot_blocks = std::min(blocks, requested_blocks);
    if (slot_blocks == 0U) {
        return layout;
    }

    layout.ready = h747_storage_state().ready != 0U;
    layout.slot_blocks = slot_blocks;
    layout.slot_lba = blocks - slot_blocks;
    layout.slot_bytes = slot_blocks * block_size;
    return layout;
}

bool emmc_read_slot_bytes(const EmmcStoreLayout& layout,
                          std::uint32_t offset,
                          std::span<std::byte> bytes) noexcept {
    if (!layout.ready || bytes.empty() || offset > layout.slot_bytes ||
        bytes.size() > (layout.slot_bytes - offset)) {
        return false;
    }

    alignas(4) std::array<std::uint8_t, kEmmcSupportedBlockSize> block{};
    std::uint32_t copied = 0U;
    while (copied < bytes.size()) {
        const auto absolute = offset + copied;
        const auto block_index = absolute / layout.block_size;
        const auto block_offset = absolute % layout.block_size;
        const auto chunk = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(bytes.size() - copied),
            layout.block_size - block_offset);
        if (h747_storage_read_blocks(layout.slot_lba + block_index, block.data(), layout.block_size) == 0U) {
            return false;
        }
        std::memcpy(bytes.data() + copied, block.data() + block_offset, chunk);
        copied += chunk;
    }
    return true;
}

bool emmc_write_slot_bytes(const EmmcStoreLayout& layout,
                           std::uint32_t offset,
                           std::span<const std::byte> bytes) noexcept {
    if (!layout.ready || bytes.empty() || offset > layout.slot_bytes ||
        bytes.size() > (layout.slot_bytes - offset)) {
        return false;
    }

    alignas(4) std::array<std::uint8_t, kEmmcSupportedBlockSize> block{};
    std::uint32_t copied = 0U;
    while (copied < bytes.size()) {
        const auto absolute = offset + copied;
        const auto block_index = absolute / layout.block_size;
        const auto block_offset = absolute % layout.block_size;
        const auto chunk = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(bytes.size() - copied),
            layout.block_size - block_offset);
        if ((block_offset != 0U || chunk != layout.block_size) &&
            h747_storage_read_blocks(layout.slot_lba + block_index, block.data(), layout.block_size) == 0U) {
            return false;
        }
        if (block_offset == 0U && chunk == layout.block_size) {
            std::memcpy(block.data(), bytes.data() + copied, chunk);
        } else {
            std::memcpy(block.data() + block_offset, bytes.data() + copied, chunk);
        }
        if (h747_storage_write_blocks(layout.slot_lba + block_index, block.data(), layout.block_size) == 0U) {
            return false;
        }
        copied += chunk;
    }
    return true;
}

bool emmc_erase_slot_bytes(const EmmcStoreLayout& layout, std::uint32_t offset, std::uint32_t size) noexcept {
    if (!layout.ready || size == 0U || offset > layout.slot_bytes || size > (layout.slot_bytes - offset)) {
        return false;
    }
    return h747_storage_flush() != 0U;
}

bool sdram_storage_write(void*, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto ram = std::span<std::byte>{dev_ram()};
    if (offset > ram.size() || bytes.size() > (ram.size() - offset)) {
        return false;
    }
    std::memcpy(ram.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool sdram_storage_read(void*, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto ram = std::span<std::byte>{dev_ram()};
    if (offset > ram.size() || bytes.size() > (ram.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), ram.data() + offset, bytes.size());
    return true;
}

bool d1_storage_write(void*, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto ram = std::span<std::byte>{d1_receive_fallback()};
    if (offset > ram.size() || bytes.size() > (ram.size() - offset)) {
        return false;
    }
    std::memcpy(ram.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool d1_storage_read(void*, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto ram = std::span<std::byte>{d1_receive_fallback()};
    if (offset > ram.size() || bytes.size() > (ram.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), ram.data() + offset, bytes.size());
    return true;
}

bool use_sdram_stage_arena() noexcept {
    return memory_probe_storage_state().sdram2_ready != 0U;
}

ImageStageArena receive_arena_descriptor(loader::Storage storage) noexcept {
    if (storage.base_address == kDevRamBase) {
        auto arena = kD1ReceiveArena;
        arena.base = reinterpret_cast<std::uintptr_t>(d1_receive_fallback().data());
        return arena;
    }
    return kReceiveArena;
}

ImageStageArena stage_arena_descriptor() noexcept {
    if (!use_sdram_stage_arena()) {
        auto arena = kD1StageArena;
        arena.base = reinterpret_cast<std::uintptr_t>(d1_stage_fallback().data());
        return arena;
    }
    return kImageStageArena;
}

std::span<std::byte> stage_scratch() noexcept {
    return use_sdram_stage_arena()
        ? std::span<std::byte>{stage_probe_scratch()}
        : std::span<std::byte>{d1_stage_fallback()};
}

loader::Storage ram_storage() noexcept {
    if (!use_sdram_stage_arena()) {
        return loader::Storage{
            .ctx = nullptr,
            .base_address = kDevRamBase,
            .capacity_bytes = kD1FallbackCapacity,
            .write = d1_storage_write,
            .read = d1_storage_read,
        };
    }
    return loader::Storage{
        .ctx = nullptr,
        .base_address = static_cast<std::uint32_t>(kReceiveArena.base),
        .capacity_bytes = kDevRamCapacity,
        .write = sdram_storage_write,
        .read = sdram_storage_read,
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
                       reinterpret_cast<const std::uint8_t*>(bytes.data()),
                       static_cast<std::uint32_t>(bytes.size())) != 0U;
        },
        .read = [](void*, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            return h747_qspi_nor_read(
                       offset,
                       reinterpret_cast<std::uint8_t*>(bytes.data()),
                       static_cast<std::uint32_t>(bytes.size())) != 0U;
        },
    };
}

app_abi::AppStoreReader qspi_store_reader() noexcept {
    return app_abi::AppStoreReader{
        .ctx = nullptr,
        .read = [](void*, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            return h747_qspi_nor_read(
                       kQspiStoreBaseOffset + offset,
                       reinterpret_cast<std::uint8_t*>(bytes.data()),
                       static_cast<std::uint32_t>(bytes.size())) != 0U;
        },
    };
}

EmmcStoreContext& emmc_store_context() noexcept {
    static EmmcStoreContext context{};
    context.layout = emmc_store_layout();
    return context;
}

app_abi::AppStoreWritableMedia emmc_store_media() noexcept {
    auto& context = emmc_store_context();
    return app_abi::AppStoreWritableMedia{
        .ctx = &context,
        .capacity = context.layout.slot_bytes,
        .erase_block_size = context.layout.block_size,
        .write_align = 1U,
        .erase = [](void* ctx, std::uint32_t offset, std::uint32_t size) noexcept -> bool {
            auto* local = static_cast<EmmcStoreContext*>(ctx);
            return local != nullptr && emmc_erase_slot_bytes(local->layout, offset, size);
        },
        .write = [](void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept -> bool {
            auto* local = static_cast<EmmcStoreContext*>(ctx);
            return local != nullptr && emmc_write_slot_bytes(local->layout, offset, bytes);
        },
        .read = [](void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            auto* local = static_cast<EmmcStoreContext*>(ctx);
            return local != nullptr && emmc_read_slot_bytes(local->layout, offset, bytes);
        },
    };
}

app_abi::AppStoreReader emmc_store_reader() noexcept {
    auto& context = emmc_store_context();
    return app_abi::AppStoreReader{
        .ctx = &context,
        .read = [](void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            auto* local = static_cast<EmmcStoreContext*>(ctx);
            return local != nullptr && emmc_read_slot_bytes(local->layout, offset, bytes);
        },
    };
}

app_abi::AppLoadResult load_runtime_elf(void* ctx,
                                        const app_abi::AppImage& image,
                                        const app_abi::AppLoadBuffer& buffer) noexcept {
    auto* backend = static_cast<app_abi::AppElfLoadBackend*>(ctx);
    if (backend == nullptr) {
        return {.code = app_abi::AppRunCode::image_not_found};
    }
    return app_abi::app_elf_load_image(backend, image, buffer);
}

app_abi::AppLoadResult load_runtime_modulex(void* ctx,
                                            const app_abi::AppImage& image,
                                            const app_abi::AppLoadBuffer& buffer) noexcept {
    auto* load_ctx = static_cast<ModuleXRuntimeLoadContext*>(ctx);
    auto* rt = load_ctx != nullptr ? load_ctx->rt : nullptr;
    if (buffer.base == nullptr || buffer.size == 0U || image.image_base == nullptr ||
        image.image_size == 0U || image.image_size > buffer.size) {
        if (rt != nullptr) {
            rt->app_modulex_code = app_abi::AppModuleXLoadCode::invalid_argument;
            rt->app_modulex_last = app_abi::AppModuleXLoadResult{
                .code = rt->app_modulex_code,
                .load = {
                    .code = app_abi::AppRunCode::load_failed,
                    .backend_error = static_cast<int>(rt->app_modulex_code),
                },
            };
            rt->app_plan_code = app_abi::AppRunCode::load_failed;
            rt->app_plan_backend_error = static_cast<int>(rt->app_modulex_code);
        }
        return {
            .code = app_abi::AppRunCode::load_failed,
            .backend_error = static_cast<int>(app_abi::AppModuleXLoadCode::invalid_argument),
        };
    }
    if (buffer.align != 0U &&
        (reinterpret_cast<std::uintptr_t>(buffer.base) % buffer.align) != 0U) {
        if (rt != nullptr) {
            rt->app_modulex_code = app_abi::AppModuleXLoadCode::invalid_argument;
            rt->app_modulex_last = app_abi::AppModuleXLoadResult{
                .code = rt->app_modulex_code,
                .load = {
                    .code = app_abi::AppRunCode::load_failed,
                    .backend_error = static_cast<int>(rt->app_modulex_code),
                },
            };
            rt->app_plan_code = app_abi::AppRunCode::load_failed;
            rt->app_plan_backend_error = static_cast<int>(rt->app_modulex_code);
        }
        return {
            .code = app_abi::AppRunCode::load_failed,
            .backend_error = static_cast<int>(app_abi::AppModuleXLoadCode::invalid_argument),
        };
    }

    std::memcpy(buffer.base, image.image_base, image.image_size);
    app_abi::AppImage materialized = image;
    materialized.image_base = buffer.base;
    if (load_ctx != nullptr) {
        load_ctx->materialized = materialized;
    }

    const auto loaded = app_abi::app_modulex_load_image(materialized);
    if (rt != nullptr) {
        rt->app_modulex_code = loaded.code;
        rt->app_modulex_last = loaded;
        rt->app_plan_code = loaded.load.code;
        rt->app_plan_backend_error = loaded.load.backend_error;
        rt->app_plan_load_base = reinterpret_cast<std::uintptr_t>(materialized.image_base);
        rt->app_plan_entry = 0;
        rt->app_probe = app_abi::AppElfProbeResult{
            .code = loaded.load.code == app_abi::AppRunCode::ok
                ? app_abi::AppElfProbeCode::ok
                : app_abi::AppElfProbeCode::format_mismatch,
            .entry_offset = 0,
            .load_span = static_cast<std::uint32_t>(materialized.image_size),
            .segment_count = loaded.load.code == app_abi::AppRunCode::ok ? 1U : 0U,
            .runnable = loaded.load.code == app_abi::AppRunCode::ok,
        };
    }
    if (loaded.load.code != app_abi::AppRunCode::ok) {
        return loaded.load;
    }
    auto result = loaded.load;
    auto raw = reinterpret_cast<std::uintptr_t>(result.image.entry);
    if (rt != nullptr) {
        rt->app_probe.entry_offset =
            static_cast<std::uint32_t>(raw - reinterpret_cast<std::uintptr_t>(materialized.image_base));
    }
    raw |= 1U;
    result.image.entry = reinterpret_cast<CharmAppMainFn>(raw);
    if (rt != nullptr) {
        rt->app_plan_code = result.code;
        rt->app_plan_backend_error = result.backend_error;
        rt->app_plan_entry = raw;
    }
    return result;
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

void prepare_modulex_app_buffer(void* ctx) noexcept {
    const auto* load_ctx = static_cast<const ModuleXRuntimeLoadContext*>(ctx);
    const auto* image = load_ctx != nullptr ? &load_ctx->materialized : nullptr;
    if (image == nullptr || image->image_base == nullptr || image->image_size == 0U) {
        return;
    }

    const auto start = reinterpret_cast<std::uintptr_t>(image->image_base);
    const auto end = start + image->image_size;
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

struct RuntimeLoaderBinding {
    void* load_ctx{nullptr};
    app_abi::AppLoadResult (*load)(void* ctx,
                                   const app_abi::AppImage& image,
                                   const app_abi::AppLoadBuffer& buffer) noexcept{nullptr};
    app_abi::AppLoadBuffer buffer{};
};

RuntimeLoaderBinding make_prepare_loader_binding(Runtime& rt,
                                                 const app_abi::AppImage& image,
                                                 ModuleXRuntimeLoadContext& modulex_ctx) noexcept {
    if (image.format == app_abi::AppImageFormat::modulex) {
        modulex_ctx = ModuleXRuntimeLoadContext{.rt = &rt};
        auto& load = elf_probe_load_buffer();
        return RuntimeLoaderBinding{
            .load_ctx = &modulex_ctx,
            .load = load_runtime_modulex,
            .buffer = app_abi::AppLoadBuffer{
                .base = load.data(),
                .size = load.size(),
                .align = 32U,
                .prepare = prepare_modulex_app_buffer,
                .prepare_ctx = &modulex_ctx,
            },
        };
    }

    auto& load = elf_probe_load_buffer();
    return RuntimeLoaderBinding{
        .load_ctx = &rt.app_elf_backend,
        .load = load_runtime_elf,
        .buffer = app_abi::AppLoadBuffer{
            .base = load.data(),
            .size = load.size(),
            .align = 16U,
        },
    };
}

RuntimeLoaderBinding make_run_loader_binding(Runtime& rt,
                                             const app_abi::AppImage& image,
                                             ModuleXRuntimeLoadContext& modulex_ctx) noexcept {
    const auto load = app_run_load_buffer();
    if (image.format == app_abi::AppImageFormat::modulex) {
        modulex_ctx = ModuleXRuntimeLoadContext{.rt = &rt};
        return RuntimeLoaderBinding{
            .load_ctx = &modulex_ctx,
            .load = load_runtime_modulex,
            .buffer = app_abi::AppLoadBuffer{
                .base = load.data(),
                .size = load.size(),
                .align = 32U,
                .prepare = prepare_modulex_app_buffer,
                .prepare_ctx = &modulex_ctx,
            },
        };
    }

    return RuntimeLoaderBinding{
        .load_ctx = &rt.app_elf_backend,
        .load = load_runtime_elf,
        .buffer = app_abi::AppLoadBuffer{
            .base = load.data(),
            .size = load.size(),
            .align = kAppRunLoadRegion.align,
            .prepare = prepare_loaded_app_buffer,
            .prepare_ctx = &rt.app_elf_backend,
        },
    };
}

void record_loader_plan(Runtime& rt, const app_abi::AppImage& image) noexcept {
    rt.app_image_format = image.format;
    if (image.format == app_abi::AppImageFormat::modulex) {
        return;
    }
    rt.app_probe = rt.app_elf_backend.last.plan.probe;
    rt.app_plan_code = rt.app_elf_backend.last.code;
    rt.app_plan_backend_error = rt.app_elf_backend.last.backend_error;
    rt.app_plan_load_base = rt.app_elf_backend.last.plan.load_base;
    rt.app_plan_entry = rt.app_elf_backend.last.plan.entry_address;
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
    const auto memory = memory_probe_storage_state();
    const auto receive_storage = runtime().receive_storage;
    const auto receive_arena = receive_arena_descriptor(receive_storage);
    emit<"dev: ram base=0x{:08x} capacity={} cursor={}\n">(
        receive_storage.base_address,
        receive_storage.capacity_bytes,
        command.cursor);
    emit<"dev: receive-arena name={} addr=0x{:08x} expected=0x{:08x} size={} align={}\n">(
        receive_arena.name,
        static_cast<std::uint32_t>(receive_arena.base),
        static_cast<std::uint32_t>(receive_arena.base),
        receive_arena.size,
        receive_arena.align);
    emit<"dev: sdram2 ready={} init={} smoke={} base=0x{:08x} size={}\n">(
        memory.sdram2_ready,
        memory.sdram2_init_ok,
        memory.sdram2_smoke_ok,
        memory.sdram2_base,
        memory.sdram2_size_bytes);
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
    emit<"dev: usb frontend packet_buffer={} max_payload={} read_chunk={} drain_limit={}\n">(
        kPacketBufferCapacity,
        kPacketMaxPayloadSize,
        kUsbReadChunkSize,
        kUsbDrainLimitBytes);
    emit<"dev: usb active={} exit={} bytes={} init={} started={} cdc_ready={} pcd_ready={} pcd={} usbd={} class={} iface={} start={}\n">(
        rt.usb_active ? 1U : 0U,
        usb_exit_reason_name(rt.usb_exit_reason),
        rt.usb_bytes,
        usb.init_called,
        usb.started,
        usb.cdc_ready,
        usb.pcd_ready,
        usb.pcd_init_status,
        usb.usbd_init_status,
        usb.register_class_status,
        usb.register_interface_status,
        usb.usbd_start_status);
    emit<"dev: usb usbd_state={} config={} class_id={} num_classes={} class_registered={} user_data={} class_data={} cdc_init={} cdc_deinit={}\n">(
        usb.usbd_dev_state,
        usb.usbd_dev_config,
        usb.usbd_class_id,
        usb.usbd_num_classes,
        usb.usbd_class_registered,
        usb.usbd_user_data_registered,
        usb.usbd_class_data_ready,
        usb.cdc_init_count,
        usb.cdc_deinit_count);
    emit<"dev: usb rx packets={} bytes={} read={} dropped={} overflow={} ctrl={} last_ctrl={}/{}\n">(
        usb.rx_packets,
        usb.rx_bytes,
        usb.bytes_read,
        usb.rx_dropped_bytes,
        usb.rx_overflow_count,
        usb.control_requests,
        usb.last_control_cmd,
        usb.last_control_length);
    emit<"dev: usb bus setup={} reset={} suspend={} resume={} connect={} disconnect={} ep0_out={} ep0_in={} ep1_out={} ep1_in={} last_setup={}\n">(
        usb.setup_count,
        usb.reset_count,
        usb.suspend_count,
        usb.resume_count,
        usb.connect_count,
        usb.disconnect_count,
        usb.out_ep0_hits,
        usb.in_ep0_hits,
        usb.out_ep1_hits,
        usb.in_ep1_hits,
        usb.last_setup_valid);
    emit<"dev: usb regs gusbcfg=0x{:08x} gahbcfg=0x{:08x} gintsts=0x{:08x} gintmsk=0x{:08x} dctl=0x{:08x} dsts=0x{:08x} gotgctl=0x{:08x} gccfg=0x{:08x}\n">(
        usb.gusbcfg,
        usb.gahbcfg,
        usb.gintsts,
        usb.gintmsk,
        usb.dctl,
        usb.dsts,
        usb.gotgctl,
        usb.gccfg);
    emit<"dev: usb ep0 diepctl=0x{:08x} diepint=0x{:08x} doepctl=0x{:08x} doepint=0x{:08x}\n">(
        usb.diepctl0,
        usb.diepint0,
        usb.doepctl0,
        usb.doepint0);
    if (usb.last_setup_valid != 0U) {
        emit_hex_bytes("dev: usb setup", usb.last_setup, sizeof(usb.last_setup));
    }
    emit<"dev: usb desc dev_len={} cfg_len={} dev_prefix={} cfg_prefix={}\n">(
        usb.dev_desc_len,
        usb.cfg_desc_len,
        usb.dev_desc_prefix_len,
        usb.cfg_desc_prefix_len);
    if (usb.dev_desc_prefix_len != 0U) {
        emit_hex_bytes("dev: usb dev-desc", usb.dev_desc_prefix, usb.dev_desc_prefix_len);
    }
    if (usb.cfg_desc_prefix_len != 0U) {
        emit_hex_bytes("dev: usb cfg-desc", usb.cfg_desc_prefix, usb.cfg_desc_prefix_len);
    }
    print_packet_result(rt.usb_last);
}

void print_store_status(Runtime& rt) noexcept {
    const auto qspi = h747_qspi_nor_state();
    const auto emmc = h747_storage_state();
    const auto emmc_layout = emmc_store_layout();
    rt.qspi_ready = qspi.ready != 0U;
    emit<"dev: store qspi ready={} power={} jedec_ok={} jedec=0x{:08x} capacity={} erase_block={} write_align={}\n">(
        qspi.ready,
        qspi.power_ok,
        qspi.jedec_ok,
        qspi.jedec_id,
        qspi.capacity_bytes,
        h747_qspi_nor_erase_block_size(),
        h747_qspi_nor_write_align());
    emit<"dev: store qspi reads={}/{} writes={}/{} erases={}/{} last=0x{:08x}:{} hal={} err={}\n">(
        qspi.read_count,
        qspi.read_fail_count,
        qspi.write_count,
        qspi.write_fail_count,
        qspi.erase_count,
        qspi.erase_fail_count,
        qspi.last_offset,
        qspi.last_bytes,
        qspi.last_hal_status,
        qspi.last_error);
    emit<"dev: store emmc ready={} init={} block_ready={} block_size={} raw_blocks={} exposed_blocks={} part_lba={} slot_lba={} slot_blocks={} slot_bytes={}\n">(
        emmc.ready,
        emmc.initialized,
        emmc.block_device_ready,
        emmc.block_size,
        emmc.block_count,
        emmc.exposed_block_count,
        emmc.partition_lba,
        emmc_layout.slot_lba,
        emmc_layout.slot_blocks,
        emmc_layout.slot_bytes);
    emit<"dev: store emmc reads={}/{} writes={}/{} last_lba={} count={} hal={} err={} card={} bus={} clkcr=0x{:08x} sta=0x{:08x} resp1=0x{:08x}\n">(
        emmc.read_count,
        emmc.read_fail_count,
        emmc.write_count,
        emmc.write_fail_count,
        emmc.last_lba,
        emmc.last_count,
        emmc.last_hal_status,
        emmc.last_error,
        emmc.card_state,
        emmc.selected_bus_width,
        emmc.clkcr,
        emmc.sta,
        emmc.resp1);
    emit<"dev: store install receive={} recv_bytes={} code={} target=0x{:08x} written={} erased={}\n">(
        loader::received_image_read_code_name(rt.store_receive_code),
        rt.store_receive_bytes,
        app_abi::app_store_install_code_name(rt.store_install_code),
        rt.store_install_target,
        rt.store_install_written,
        rt.store_install_erased);

    app_abi::AppStoreHeader header{};
    const auto header_code = app_abi::app_store_read_header(qspi_store_reader(), header);
    rt.store_read_code = header_code;
    rt.store_header = header;
    if (header_code != app_abi::AppStoreReadCode::ok) {
        emit<"dev: store header code={} readable=0 valid=0\n">(
            app_abi::app_store_read_code_name(header_code));
    } else {
        emit<"dev: store header code={} readable=1 valid={} magic=0x{:08x} version={} entries={} header_size={} entry_size={}\n">(
            app_abi::app_store_read_code_name(header_code),
            app_abi::app_store_header_valid(header) ? 1U : 0U,
            header.magic,
            static_cast<unsigned>(header.version),
            header.entry_count,
            header.header_size,
            header.entry_size);
    }

    app_abi::AppStoreHeader emmc_header{};
    const auto emmc_header_code = app_abi::app_store_read_header(emmc_store_reader(), emmc_header);
    if (emmc_header_code != app_abi::AppStoreReadCode::ok) {
        emit<"dev: store emmc header code={} readable=0 valid=0\n">(
            app_abi::app_store_read_code_name(emmc_header_code));
    } else {
        emit<"dev: store emmc header code={} readable=1 valid={} magic=0x{:08x} version={} entries={} header_size={} entry_size={}\n">(
            app_abi::app_store_read_code_name(emmc_header_code),
            app_abi::app_store_header_valid(emmc_header) ? 1U : 0U,
            emmc_header.magic,
            static_cast<unsigned>(emmc_header.version),
            emmc_header.entry_count,
            emmc_header.header_size,
            emmc_header.entry_size);
    }
}

void list_qspi_store(Runtime& rt) noexcept {
    app_abi::AppStoreHeader header{};
    const auto header_code = app_abi::app_store_read_header(qspi_store_reader(), header);
    rt.store_read_code = header_code;
    rt.store_header = header;
    if (header_code != app_abi::AppStoreReadCode::ok) {
        emit<"dev: store list code={} entries=0\n">(
            app_abi::app_store_read_code_name(header_code));
        return;
    }

    emit<"dev: store entries={}\n">(header.entry_count);
    for (std::uint32_t i = 0; i < header.entry_count; ++i) {
        app_abi::AppStoreEntry entry{};
        const auto entry_code = app_abi::app_store_read_entry(qspi_store_reader(), header, i, entry);
        if (entry_code != app_abi::AppStoreReadCode::ok) {
            rt.store_read_code = entry_code;
            emit<"  [{}] read_failed offset=0x{:08x} code={}\n">(
                i,
                app_abi::app_store_entry_offset(header, i),
                app_abi::app_store_read_code_name(entry_code));
            return;
        }
        emit<"  [{}] name={} format={} offset=0x{:08x} size={} flags=0x{:08x} runnable={}\n">(
            i,
            app_abi::app_store_entry_name(entry),
            app_image_format_name(app_abi::app_store_entry_format(entry)),
            entry.offset,
            entry.size,
            entry.flags,
            app_abi::app_store_entry_runnable(entry) ? 1U : 0U);
    }
}

void list_emmc_store(Runtime& rt) noexcept {
    app_abi::AppStoreHeader header{};
    const auto header_code = app_abi::app_store_read_header(emmc_store_reader(), header);
    rt.store_read_code = header_code;
    rt.store_header = header;
    if (header_code != app_abi::AppStoreReadCode::ok) {
        emit<"dev: store emmc list code={} entries=0\n">(
            app_abi::app_store_read_code_name(header_code));
        return;
    }

    emit<"dev: store emmc entries={}\n">(header.entry_count);
    for (std::uint32_t i = 0; i < header.entry_count; ++i) {
        app_abi::AppStoreEntry entry{};
        const auto entry_code = app_abi::app_store_read_entry(emmc_store_reader(), header, i, entry);
        if (entry_code != app_abi::AppStoreReadCode::ok) {
            rt.store_read_code = entry_code;
            emit<"  [{}] read_failed offset=0x{:08x} code={}\n">(
                i,
                app_abi::app_store_entry_offset(header, i),
                app_abi::app_store_read_code_name(entry_code));
            return;
        }
        emit<"  [{}] name={} format={} offset=0x{:08x} size={} flags=0x{:08x} runnable={}\n">(
            i,
            app_abi::app_store_entry_name(entry),
            app_image_format_name(app_abi::app_store_entry_format(entry)),
            entry.offset,
            entry.size,
            entry.flags,
            app_abi::app_store_entry_runnable(entry) ? 1U : 0U);
    }
}

void print_app_status(const Runtime& rt) noexcept {
    const auto run_state = (rt.app_last_command == "run"sv) ? "enabled"sv : "disabled"sv;
    const auto memory = memory_probe_storage_state();
    const auto arena = stage_arena_descriptor();
    const auto scratch = stage_scratch();
    emit<"dev: app command={} name={} run={}\n">(rt.app_last_command, rt.app_name, run_state);
    emit<"dev: app record source={} format={} name={} command={} argv={} load=0x{:08x} entry=0x{:08x} span={} segments={} run_stage={} run_code={} exited={} exit={} caps_console={} caps_present={} caps_input={}\n">(
        rt.app_source,
        app_image_format_name(rt.app_image_format),
        rt.app_record_name,
        rt.app_last_command,
        rt.app_prepare_argc,
        static_cast<std::uint32_t>(rt.app_plan_load_base),
        static_cast<std::uint32_t>(rt.app_plan_entry),
        rt.app_probe.load_span,
        rt.app_probe.segment_count,
        app_abi::stage_name(rt.app_run_stage),
        app_abi::code_name(rt.app_run_code),
        rt.app_run_exited ? 1U : 0U,
        rt.app_run_exit_code,
        rt.app_console_bytes,
        rt.app_display_present_count,
        rt.app_input_poll_count);
    emit<"dev: app stage-arena name={} addr=0x{:08x} expected=0x{:08x} size={} align={}\n">(
        arena.name,
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(scratch.data())),
        static_cast<std::uint32_t>(arena.base),
        arena.size,
        arena.align);
    emit<"dev: app sdram2 ready={} init={} smoke={} base=0x{:08x} size={}\n">(
        memory.sdram2_ready,
        memory.sdram2_init_ok,
        memory.sdram2_smoke_ok,
        memory.sdram2_base,
        memory.sdram2_size_bytes);
    emit<"dev: app run-region name={} base=0x{:08x} size={} align={} linked_elf_base=0x{:08x}\n">(
        kAppRunLoadRegion.name,
        static_cast<std::uint32_t>(kAppRunLoadRegion.base),
        kAppRunLoadRegion.size,
        kAppRunLoadRegion.align,
        static_cast<std::uint32_t>(kAppRunLoadRegion.linked_elf_base));
    emit<"dev: app capacity needed={} free={} fits={} region={} probe={}\n">(
        rt.app_probe.load_span,
        app_run_region_free(rt.app_probe.load_span),
        app_run_region_fits(rt.app_probe.load_span) ? 1U : 0U,
        kAppRunLoadRegion.size,
        app_abi::app_elf_probe_code_name(rt.app_probe.code));
    emit<"dev: app read={} bytes={}\n">(
        loader::received_image_read_code_name(rt.app_read_code),
        rt.app_read_bytes);
    emit<"dev: app stage={} bytes={}\n">(
        app_abi::app_received_image_stage_code_name(rt.app_stage_code),
        rt.app_stage_bytes);
    emit<"dev: app format={} modulex={}\n">(
        app_image_format_name(rt.app_image_format),
        app_abi::app_modulex_load_code_name(rt.app_modulex_code));
    emit<"dev: app modulex diag validate={} dep={} dep_index={} relocated={} entry_off=0x{:08x} span={}\n">(
        app_abi::app_modulex_image_error_name(rt.app_modulex_last.validate_error),
        app_abi::app_modulex_dep_error_name(rt.app_modulex_last.dependency_error),
        rt.app_modulex_last.dependency_index,
        rt.app_modulex_last.relocated ? 1U : 0U,
        rt.app_modulex_last.entry_offset,
        rt.app_modulex_last.image_span);
    emit<"dev: app store code={} lookup={} offset=0x{:08x} size={}\n">(
        app_abi::app_store_read_code_name(rt.store_read_code),
        app_abi::app_store_read_code_name(rt.store_lookup.code),
        rt.store_lookup.entry.offset,
        rt.store_lookup.entry.size);
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
    emit<"  dev store status           - Show QSPI/eMMC App Store diagnostics\n">();
    emit<"  dev store install qspi     - Install launch_ready .appstore.bin to QSPI\n">();
    emit<"  dev store install emmc     - Install launch_ready .appstore.bin to eMMC raw slot\n">();
    emit<"  dev store list qspi        - List QSPI App Store entries\n">();
    emit<"  dev store list emmc        - List eMMC App Store entries\n">();
    emit<"  dev store stage qspi:<name> - Stage named QSPI App image without running\n">();
    emit<"  dev store stage emmc:<name> - Stage named eMMC App image without running\n">();
    emit<"  dev app stage <name>       - Stage launch_ready payload as App image\n">();
    emit<"  dev app probe <name>       - Stage and load-probe payload without running\n">();
    emit<"  dev app prepare <name> [args...] - Prepare AppRuntime argv/ABI without running\n">();
    emit<"  dev app run <name>|qspi:<name>|emmc:<name> [args...] - Explicitly call charm_app_main from loaded ELF/ModuleX\n">();
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
        rt.usb_exit_reason = UsbExitReason::active;
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
        rt.usb_exit_reason = UsbExitReason::abort;
        h747::usb_dev_loader::stop();
        rt.packet_transport.reset_session();
        rt.usb_last = rt.packet_transport.status();
        print_usb_status(rt);
        return;
    }
    emit<"dev: usb usage error\n">();
    emit<"dev: use 'dev usb begin', 'dev usb status', or 'dev usb abort'\n">();
}

void handle_store_command(std::string_view line) noexcept {
    auto& rt = runtime();
    if (line == "dev store status") {
        print_store_status(rt);
        return;
    }
    if (line == "dev store install qspi") {
        const auto qspi = h747_qspi_nor_state();
        rt.qspi_ready = qspi.ready != 0U;
        rt.store_install_code = app_abi::AppStoreInstallCode::invalid_argument;
        rt.store_receive_code = loader::ReceivedImageReadCode::not_launch_ready;
        rt.store_read_code = app_abi::AppStoreReadCode::invalid_argument;
        rt.store_install_target = kQspiStoreBaseOffset;
        rt.store_install_written = 0;
        rt.store_install_erased = 0;
        rt.store_receive_bytes = 0;
        if (!rt.qspi_ready) {
            emit<"dev: store install backend=qspi ready=0 code={}\n">(
                app_abi::app_store_install_code_name(rt.store_install_code));
            print_store_status(rt);
            return;
        }
        auto scratch = stage_scratch();
        const auto installed = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
            .received = loader::ReceivedImageReadConfig{
                .status = rt.packet_transport.status().packet.receive,
                .manifest = rt.packet_transport.status().packet.manifest,
                .storage = rt.receive_storage,
                .output = scratch,
            },
            .media = qspi_store_media(),
            .target_offset = kQspiStoreBaseOffset,
        });
        rt.store_receive_code = installed.received.code;
        rt.store_receive_bytes = installed.received.bytes_read;
        rt.store_read_code = installed.store_code;
        rt.store_header = installed.header;
        rt.store_install_code = installed.install.code;
        rt.store_install_target = installed.install.target_offset;
        rt.store_install_written = installed.install.bytes_written;
        rt.store_install_erased = installed.install.bytes_erased;
        emit<"dev: store install qspi receive={} recv_bytes={} store={} code={} target=0x{:08x} written={} erased={}\n">(
            loader::received_image_read_code_name(rt.store_receive_code),
            rt.store_receive_bytes,
            app_abi::app_store_read_code_name(rt.store_read_code),
            app_abi::app_store_install_code_name(rt.store_install_code),
            rt.store_install_target,
            rt.store_install_written,
            rt.store_install_erased);
        return;
    }
    if (line == "dev store install emmc") {
        const auto media = emmc_store_media();
        rt.store_install_code = app_abi::AppStoreInstallCode::invalid_argument;
        rt.store_receive_code = loader::ReceivedImageReadCode::not_launch_ready;
        rt.store_read_code = app_abi::AppStoreReadCode::invalid_argument;
        rt.store_install_target = kEmmcStoreTargetOffset;
        rt.store_install_written = 0;
        rt.store_install_erased = 0;
        rt.store_receive_bytes = 0;
        if (media.capacity == 0U) {
            emit<"dev: store install backend=emmc ready=0 code={}\n">(
                app_abi::app_store_install_code_name(rt.store_install_code));
            print_store_status(rt);
            return;
        }
        auto scratch = stage_scratch();
        const auto installed = loader::store_install_received_image(loader::StoreInstallReceivedConfig{
            .received = loader::ReceivedImageReadConfig{
                .status = rt.packet_transport.status().packet.receive,
                .manifest = rt.packet_transport.status().packet.manifest,
                .storage = rt.receive_storage,
                .output = scratch,
            },
            .media = media,
            .target_offset = kEmmcStoreTargetOffset,
        });
        rt.store_receive_code = installed.received.code;
        rt.store_receive_bytes = installed.received.bytes_read;
        rt.store_read_code = installed.store_code;
        rt.store_header = installed.header;
        rt.store_install_code = installed.install.code;
        rt.store_install_target = installed.install.target_offset;
        rt.store_install_written = installed.install.bytes_written;
        rt.store_install_erased = installed.install.bytes_erased;
        emit<"dev: store install emmc receive={} recv_bytes={} store={} code={} target=0x{:08x} written={} erased={}\n">(
            loader::received_image_read_code_name(rt.store_receive_code),
            rt.store_receive_bytes,
            app_abi::app_store_read_code_name(rt.store_read_code),
            app_abi::app_store_install_code_name(rt.store_install_code),
            rt.store_install_target,
            rt.store_install_written,
            rt.store_install_erased);
        return;
    }
    if (line == "dev store list qspi") {
        list_qspi_store(rt);
        return;
    }
    if (line == "dev store list emmc") {
        list_emmc_store(rt);
        return;
    }
    if (line.starts_with("dev store stage ")) {
        auto spec = trim_left(line.substr(16));
        const auto image = spec.starts_with("emmc:"sv)
            ? stage_emmc_app(rt, "stage"sv, spec)
            : stage_qspi_app(rt, "stage"sv, spec);
        if (rt.store_read_code == app_abi::AppStoreReadCode::ok) {
            emit<"dev: store stage spec={} image={} bytes={}\n">(
                spec,
                image.name,
                static_cast<std::uint32_t>(image.image_size));
        }
        print_app_status(rt);
        return;
    }
    emit<"dev: store usage error\n">();
    emit<"dev: use 'dev store status', 'dev store install qspi|emmc', 'dev store list qspi|emmc', or 'dev store stage qspi:<name>|emmc:<name>'\n">();
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

bool set_app_record_name(Runtime& rt, std::string_view name) noexcept {
    name = name.empty() ? kDefaultReceivedAppName : name;
    rt.app_record_name_storage.fill('\0');
    if (name.size() + 1U > rt.app_record_name_storage.size()) {
        constexpr auto too_long = "name_too_long"sv;
        std::memcpy(rt.app_record_name_storage.data(), too_long.data(), too_long.size());
        rt.app_record_name = {rt.app_record_name_storage.data(), too_long.size()};
        return false;
    }
    std::memcpy(rt.app_record_name_storage.data(), name.data(), name.size());
    rt.app_record_name = {rt.app_record_name_storage.data(), name.size()};
    return true;
}

bool reset_app_diagnostics(Runtime& rt,
                           std::string_view command,
                           std::string_view name,
                           std::string_view source = "received"sv,
                           std::string_view record_name = {}) noexcept {
    rt.app_last_command = command;
    const bool name_ok = set_app_name(rt, name);
    const bool record_name_ok = set_app_record_name(rt, record_name.empty() ? name : record_name);
    rt.app_source = source;
    rt.app_read_code = loader::ReceivedImageReadCode::not_launch_ready;
    rt.app_stage_code = app_abi::AppReceivedImageStageCode::not_verified;
    rt.app_image_format = app_abi::AppImageFormat::elf;
    rt.app_probe = app_abi::AppElfProbeResult{.code = app_abi::AppElfProbeCode::invalid_argument};
    rt.app_elf_backend = {};
    rt.app_modulex_code = app_abi::AppModuleXLoadCode::invalid_argument;
    rt.app_modulex_last = app_abi::AppModuleXLoadResult{};
    rt.app_modulex_last.code = rt.app_modulex_code;
    rt.app_modulex_last.load.code = app_abi::AppRunCode::load_failed;
    rt.app_modulex_last.load.backend_error = static_cast<int>(rt.app_modulex_code);
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
    rt.store_read_code = app_abi::AppStoreReadCode::invalid_argument;
    rt.store_lookup = {};
    if (!name_ok || !record_name_ok) {
        rt.app_read_code = loader::ReceivedImageReadCode::invalid_argument;
        rt.app_stage_code = app_abi::AppReceivedImageStageCode::name_too_long;
    }
    return name_ok && record_name_ok;
}

bool parse_qspi_app_name(std::string_view spec, std::string_view& name) noexcept {
    if (!spec.starts_with("qspi:"sv)) {
        return false;
    }
    name = spec.substr(5U);
    return !name.empty() && !name.starts_with("@"sv);
}

bool parse_emmc_app_name(std::string_view spec, std::string_view& name) noexcept {
    if (!spec.starts_with("emmc:"sv)) {
        return false;
    }
    name = spec.substr(5U);
    return !name.empty() && !name.starts_with("@"sv);
}

app_abi::AppRunCode app_run_code_from_store_read(app_abi::AppStoreReadCode code) noexcept {
    return (code == app_abi::AppStoreReadCode::image_not_found ||
            code == app_abi::AppStoreReadCode::header_unreadable ||
            code == app_abi::AppStoreReadCode::header_invalid ||
            code == app_abi::AppStoreReadCode::entry_read_failed)
        ? app_abi::AppRunCode::image_not_found
        : app_abi::AppRunCode::load_failed;
}

constexpr std::string_view app_image_format_name(app_abi::AppImageFormat format) noexcept {
    switch (format) {
        case app_abi::AppImageFormat::function:
            return "function"sv;
        case app_abi::AppImageFormat::elf:
            return "elf"sv;
        case app_abi::AppImageFormat::modulex:
            return "modulex"sv;
    }
    return "unknown"sv;
}

app_abi::AppImageFormat detect_received_image_format(std::span<const std::byte> payload) noexcept {
    if (payload.size() >= sizeof(modulex::ImageHeader)) {
        const auto* header = reinterpret_cast<const modulex::ImageHeader*>(payload.data());
        if (header->magic == modulex::k_magic && header->version == modulex::k_version) {
            return app_abi::AppImageFormat::modulex;
        }
    }
    return app_abi::AppImageFormat::elf;
}

app_abi::AppImage stage_received_app(Runtime& rt, std::string_view command, std::string_view name) noexcept {
    if (!reset_app_diagnostics(rt, command, name)) {
        return {};
    }

    const auto status = rt.packet_transport.status();
    const auto payload = received_payload_view(rt.receive_storage, status.packet.manifest);
    auto scratch = stage_scratch();
    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = status.packet.receive,
        .manifest = status.packet.manifest,
        .storage = rt.receive_storage,
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
        .format = detect_received_image_format(payload),
        .image = payload,
        .verified = true,
        .cache = scratch,
    });
    rt.app_image_format = staged.image.format;
    rt.app_stage_code = staged.code;
    rt.app_stage_bytes = staged.bytes_copied;
    if (staged.code != app_abi::AppReceivedImageStageCode::ok) {
        return {};
    }
    return staged.image;
}

app_abi::AppImage stage_qspi_app(Runtime& rt, std::string_view command, std::string_view spec) noexcept {
    std::string_view name{};
    if (!parse_qspi_app_name(spec, name) || !reset_app_diagnostics(rt, command, spec, "qspi"sv, name)) {
        rt.store_read_code = app_abi::AppStoreReadCode::invalid_argument;
        rt.app_run_code = app_abi::AppRunCode::invalid_argument;
        return {};
    }

    const auto qspi = h747_qspi_nor_state();
    rt.qspi_ready = qspi.ready != 0U;
    if (!rt.qspi_ready) {
        rt.store_read_code = app_abi::AppStoreReadCode::header_unreadable;
        rt.app_run_code = app_abi::AppRunCode::image_not_found;
        return {};
    }

    auto scratch = stage_scratch();
    const auto staged = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = qspi_store_reader(),
        .name = name,
        .cache = scratch,
        .format = app_abi::AppImageFormat::elf,
    });
    rt.store_read_code = staged.code;
    rt.store_lookup = staged.lookup;
    if (staged.code != app_abi::AppStoreReadCode::ok) {
        rt.app_run_code = app_run_code_from_store_read(staged.code);
        return {};
    }
    rt.app_read_code = loader::ReceivedImageReadCode::ok;
    rt.app_stage_code = app_abi::AppReceivedImageStageCode::ok;
    rt.app_read_bytes = staged.lookup.entry.size;
    rt.app_stage_bytes = staged.lookup.entry.size;
    rt.app_image_format = staged.image.format;
    return staged.image;
}

app_abi::AppImage stage_emmc_app(Runtime& rt, std::string_view command, std::string_view spec) noexcept {
    std::string_view name{};
    if (!parse_emmc_app_name(spec, name) || !reset_app_diagnostics(rt, command, spec, "emmc"sv, name)) {
        rt.store_read_code = app_abi::AppStoreReadCode::invalid_argument;
        rt.app_run_code = app_abi::AppRunCode::invalid_argument;
        return {};
    }

    const auto layout = emmc_store_layout();
    if (!layout.ready) {
        rt.store_read_code = app_abi::AppStoreReadCode::header_unreadable;
        rt.app_run_code = app_abi::AppRunCode::image_not_found;
        return {};
    }

    auto scratch = stage_scratch();
    const auto staged = loader::store_stage_named_app_image(loader::StoreStageNamedConfig{
        .reader = emmc_store_reader(),
        .name = name,
        .cache = scratch,
        .format = app_abi::AppImageFormat::elf,
    });
    rt.store_read_code = staged.code;
    rt.store_lookup = staged.lookup;
    if (staged.code != app_abi::AppStoreReadCode::ok) {
        rt.app_run_code = app_run_code_from_store_read(staged.code);
        return {};
    }
    rt.app_read_code = loader::ReceivedImageReadCode::ok;
    rt.app_stage_code = app_abi::AppReceivedImageStageCode::ok;
    rt.app_read_bytes = staged.lookup.entry.size;
    rt.app_stage_bytes = staged.lookup.entry.size;
    rt.app_image_format = staged.image.format;
    return staged.image;
}

void prepare_received_app(Runtime& rt, std::string_view name, std::string_view args) noexcept {
    const auto image = stage_received_app(rt, "prepare"sv, name);
    if (rt.app_stage_code != app_abi::AppReceivedImageStageCode::ok) {
        return;
    }

    ModuleXRuntimeLoadContext modulex_ctx{};
    const auto loader = make_prepare_loader_binding(rt, image, modulex_ctx);
    app_abi::StagedAppImageSource source_ctx{
        .image = image,
        .load_ctx = loader.load_ctx,
        .load = loader.load,
    };
    auto source = app_abi::make_staged_app_image_source(source_ctx);
    CharmAppApi api = make_prepare_api();
    app_abi::AppRuntime<> app_runtime{};
    const auto prepared = app_runtime.prepare(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = loader.buffer,
        .api = &api,
        .name = rt.app_name,
        .arg_text = args,
    });

    record_loader_plan(rt, image);
    rt.app_prepare_stage = prepared.result.stage;
    rt.app_prepare_code = prepared.result.code;
    rt.app_prepare_backend_error = prepared.result.backend_error;
    rt.app_prepare_entry = reinterpret_cast<std::uintptr_t>(prepared.image.entry);
    rt.app_prepare_argc = prepared.argc;
    rt.app_prepare_ready = prepared.ready;
}

void probe_staged_app(Runtime& rt, const app_abi::AppImage& image) noexcept {
    if (image.format == app_abi::AppImageFormat::modulex) {
        ModuleXRuntimeLoadContext modulex_ctx{.rt = &rt};
        auto& load = elf_probe_load_buffer();
        (void)load_runtime_modulex(&modulex_ctx,
                                   image,
                                   app_abi::AppLoadBuffer{
                                       .base = load.data(),
                                       .size = load.size(),
                                       .align = 32U,
                                   });
        return;
    }

    auto& load = elf_probe_load_buffer();
    const auto loaded = app_abi::app_elf_load_image(&rt.app_elf_backend,
                                                    image,
                                                    app_abi::AppLoadBuffer{
                                                        .base = load.data(),
                                                        .size = load.size(),
                                                        .align = 16U,
                                                    });
    record_loader_plan(rt, image);
    rt.app_plan_code = loaded.code;
    rt.app_plan_backend_error = loaded.backend_error;
}

void run_staged_app(Runtime& rt, const app_abi::AppImage& image, std::string_view args) noexcept {
    ModuleXRuntimeLoadContext modulex_ctx{};
    const auto loader = make_run_loader_binding(rt, image, modulex_ctx);
    app_abi::StagedAppImageSource source_ctx{
        .image = image,
        .load_ctx = loader.load_ctx,
        .load = loader.load,
    };
    auto source = app_abi::make_staged_app_image_source(source_ctx);
    CharmAppApi api = make_run_api();
    app_abi::AppRuntime<> app_runtime{};
    const auto result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = loader.buffer,
        .api = &api,
        .name = image.name,
        .arg_text = args,
    });

    record_loader_plan(rt, image);
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

void run_received_app(Runtime& rt, std::string_view name, std::string_view args) noexcept {
    const auto image = stage_received_app(rt, "run"sv, name);
    if (rt.app_stage_code != app_abi::AppReceivedImageStageCode::ok) {
        return;
    }

    run_staged_app(rt, image, args);
}

void run_qspi_app(Runtime& rt, std::string_view spec, std::string_view args) noexcept {
    const auto image = stage_qspi_app(rt, "run"sv, spec);
    if (rt.store_read_code != app_abi::AppStoreReadCode::ok) {
        return;
    }
    run_staged_app(rt, image, args);
}

void run_emmc_app(Runtime& rt, std::string_view spec, std::string_view args) noexcept {
    const auto image = stage_emmc_app(rt, "run"sv, spec);
    if (rt.store_read_code != app_abi::AppStoreReadCode::ok) {
        return;
    }
    run_staged_app(rt, image, args);
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
            probe_staged_app(rt, image);
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
        if (name.starts_with("qspi:"sv)) {
            run_qspi_app(rt, name, args);
        } else if (name.starts_with("emmc:"sv)) {
            run_emmc_app(rt, name, args);
        } else {
            run_received_app(rt, name, args);
        }
        print_app_status(rt);
        return;
    }
    emit<"dev: app usage error\n">();
    emit<"dev: use 'dev app stage <name>', 'dev app probe <name>', 'dev app prepare <name> [args...]', 'dev app run <name>|qspi:<name>|emmc:<name> [args...]', or 'dev app status'\n">();
}

void handle_command(std::string_view line) noexcept {
    if (line.starts_with("dev app")) {
        handle_app_command(line);
        return;
    }
    if (line.starts_with("dev store")) {
        handle_store_command(line);
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
    std::array<std::uint8_t, kUsbReadChunkSize> raw{};
    std::uint32_t drained = 0;
    while (drained < kUsbDrainLimitBytes) {
        const auto capacity = static_cast<std::size_t>(
            std::min<std::uint32_t>(kUsbReadChunkSize, kUsbDrainLimitBytes - drained));
        const auto count = h747::usb_dev_loader::read(std::span<std::uint8_t>{raw.data(), capacity});
        if (count == 0U) {
            break;
        }

        drained += static_cast<std::uint32_t>(count);
        rt.usb_bytes += static_cast<std::uint32_t>(count);
        rt.usb_last = rt.packet_transport.ingest(
            std::as_bytes(std::span<const std::uint8_t>{raw.data(), count}));
        if (rt.usb_last.code != loader::ByteTransportCode::ok ||
            rt.usb_last.packet.kind == loader::PacketKind::abort ||
            rt.usb_last.packet.receive.stage == loader::Stage::launch_ready) {
            if (rt.usb_last.code != loader::ByteTransportCode::ok) {
                rt.usb_exit_reason = rt.usb_last.code == loader::ByteTransportCode::packet_failed
                                         ? UsbExitReason::packet_error
                                         : UsbExitReason::transport_error;
            } else if (rt.usb_last.packet.kind == loader::PacketKind::abort) {
                rt.usb_exit_reason = UsbExitReason::abort;
            } else {
                rt.usb_exit_reason = UsbExitReason::launch_ready;
            }
            rt.usb_active = false;
            emit<"\n">();
            print_usb_status(rt);
            rt.prompt_needed = true;
            break;
        }
    }
}

} // namespace

void init() {
    h747_qspi_nor_init();
    runtime().qspi_ready = h747_qspi_nor_state().ready != 0U;
    emit<"dev_loader: resident RAM dev-loader skeleton ready\n">();
    emit<"dev_loader: transport=console-test/raw-uart/usb-cdc store=qspi launch=app-run-explicit\n">();
    print_help();
    print_status(runtime().commands.status());
    print_store_status(runtime());
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
