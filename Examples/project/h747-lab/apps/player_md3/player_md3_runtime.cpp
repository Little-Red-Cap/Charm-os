#include <cstddef>
#include <cstdint>
#include <new>
#include <span>

#include "console.h"
#include "audio.h"
#include "display_raster.h"
#include "storage.h"
#include "stm32h7xx_hal.h"
#include "player_md3_runtime.hpp"
#include "player_md3_console.hpp"
#include "player_md3_diag.hpp"
#include "player_md3_input.hpp"
#include "player_md3_memory.hpp"
#include "player_md3_render_scheduler.hpp"
#include "player_md3_resource_probe.hpp"
#include "port.h"

import audio.player;
import charm.ui.scene;
import player.input;
import player.app;
import player.app_config;
import player.font_resource;
import player.platform;
import player.product_config;
import player.storage;
import player.ui;
import fs_block;
import fs_errno;
import fs_stream;
import util.core;

namespace {

::player::PlayerPlatform* g_platform{nullptr};
h747::apps::player_md3::PlayerRuntime* g_runtime{nullptr};
alignas(h747::apps::player_md3::PlayerRuntimeShell)
std::byte g_shell_storage[sizeof(h747::apps::player_md3::PlayerRuntimeShell)];
h747::apps::player_md3::PlayerRuntimeShell* g_shell{nullptr};

constexpr std::uint32_t kDwtCtrlCyccntena = 1UL;
constexpr std::uint32_t kCoreDebugDemcrTrcena = 1UL << 24U;

charm::system::ClockTick player_md3_now_us(void*) noexcept {
    return static_cast<charm::system::ClockTick>(HAL_GetTick()) * 1000ULL;
}

std::uint32_t clamp_u64_to_u32(const std::uint64_t value) noexcept {
    constexpr std::uint64_t max_u32 = 0xFFFFFFFFULL;
    return static_cast<std::uint32_t>(value > max_u32 ? max_u32 : value);
}

bool enable_dwt_cycle_counter() noexcept {
    CoreDebug->DEMCR |= kCoreDebugDemcrTrcena;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= kDwtCtrlCyccntena;
    return (DWT->CTRL & kDwtCtrlCyccntena) != 0U;
}

bool dwt_cycle_counter_ready() noexcept {
    return (CoreDebug->DEMCR & kCoreDebugDemcrTrcena) != 0U
        && (DWT->CTRL & kDwtCtrlCyccntena) != 0U
        && HAL_RCC_GetHCLKFreq() != 0U;
}

std::uint64_t dwt_cycles64() noexcept {
    static std::uint32_t last_cycles{0U};
    static std::uint64_t high_cycles{0ULL};
    const std::uint32_t cycles = DWT->CYCCNT;
    if (cycles < last_cycles) {
        high_cycles += 0x100000000ULL;
    }
    last_cycles = cycles;
    return high_cycles + static_cast<std::uint64_t>(cycles);
}

std::uint64_t dwt_now_us() noexcept {
    const std::uint32_t freq_hz = HAL_RCC_GetHCLKFreq();
    if (!dwt_cycle_counter_ready() || freq_hz == 0U) {
        return 0ULL;
    }
    return (dwt_cycles64() * 1000000ULL) /
           static_cast<std::uint64_t>(freq_hz);
}

std::uint32_t dwt_delta_us(const std::uint64_t start, const std::uint64_t end) noexcept {
    return end >= start ? clamp_u64_to_u32(end - start) : 0U;
}

struct TimedDisplaySinkState {
    ::player::PlayerDisplaySink* inner{nullptr};
    std::uint32_t present_us{0};
};

bool timed_display_present(void* ctx,
                           const ::player::PlayerDisplaySurface& surface,
                           ::player::PlayerDirtyRegion dirty) noexcept {
    auto* timed = static_cast<TimedDisplaySinkState*>(ctx);
    const bool timing_available = dwt_cycle_counter_ready();
    const std::uint64_t start = timing_available ? dwt_now_us() : 0ULL;
    const bool ok = timed && timed->inner && timed->inner->present(surface, dirty);
    const std::uint64_t end = timing_available ? dwt_now_us() : 0ULL;
    if (timed) {
        timed->present_us = timing_available ? dwt_delta_us(start, end) : 0U;
    }
    return ok;
}

::player::StorageConfig empty_storage_config() noexcept {
    return ::player::StorageConfig{};
}

::player::FontResourceConfig board_font_resource_config() noexcept {
#if defined(CHARM_PLAYER_FILE_FONTS) && CHARM_PLAYER_FILE_FONTS
    return ::player::make_file_font_resource_config(
        ::player::product_config::default_font_path,
        ::player::product_config::default_font_fallback_path);
#else
    return ::player::make_builtin_font_resource_config();
#endif
}

fs::Status emmc_read(void*, util::u64 lba, std::span<util::u8> data) noexcept {
    if (lba > 0xFFFFFFFFULL || data.empty()) {
        return fs::Status{fs::Errc::inval};
    }
    return h747_storage_read_blocks(static_cast<std::uint32_t>(lba),
                                    data.data(),
                                    static_cast<std::uint32_t>(data.size())) != 0U
        ? fs::Status{fs::Errc::ok}
        : fs::Status{fs::Errc::io};
}

fs::Status emmc_write(void*, util::u64 lba, std::span<const util::u8> data) noexcept {
    if (lba > 0xFFFFFFFFULL || data.empty()) {
        return fs::Status{fs::Errc::inval};
    }
    return h747_storage_write_blocks(static_cast<std::uint32_t>(lba),
                                     data.data(),
                                     static_cast<std::uint32_t>(data.size())) != 0U
        ? fs::Status{fs::Errc::ok}
        : fs::Status{fs::Errc::rofs};
}

fs::Status emmc_erase(void*, util::u64, util::u64) noexcept {
    return fs::Status{fs::Errc::notsup};
}

fs::Status emmc_flush(void*) noexcept {
    return h747_storage_flush() != 0U ? fs::Status{fs::Errc::ok} : fs::Status{fs::Errc::io};
}

fs::BlockDevice* board_emmc_block_device() noexcept {
    static fs::BlockDevice dev{};
    const auto block_size = h747_storage_block_size();
    const auto block_count = h747_storage_block_count();
    if (block_size == 0U || block_count == 0U) {
        return nullptr;
    }
    dev.ctx = nullptr;
    dev.read = &emmc_read;
    dev.write = &emmc_write;
    dev.erase = &emmc_erase;
    dev.flush = &emmc_flush;
    dev.block_size = block_size;
    dev.block_count = block_count;
    dev.caps = (1U << 0U) | (1U << 3U);
    return &dev;
}

void refresh_board_resource_state() noexcept {
    const auto storage = h747_storage_state();
    const auto audio = h747_audio_state();
    auto& st = h747::apps::player_md3::state();
    st.storage_attempted = storage.attempted;
    st.storage_initialized = storage.initialized;
    st.storage_ready = storage.ready;
    st.storage_block_device_ready = storage.block_device_ready;
    st.storage_fat_probe_ok = storage.fat_probe_ok;
    st.storage_partition_auto = storage.partition_auto;
    st.storage_init_status = storage.init_status;
    st.storage_last_hal_status = storage.last_hal_status;
    st.storage_last_error = storage.last_error;
    st.storage_card_state = storage.card_state;
    st.storage_block_size = storage.block_size;
    st.storage_blocks = storage.exposed_block_count;
    st.storage_part_lba = storage.partition_lba;
    st.storage_reads = storage.read_count;
    st.storage_read_fails = storage.read_fail_count;
    st.storage_wait_timeouts = storage.wait_timeout_count;
    st.storage_last_lba = storage.last_lba;
    st.storage_last_count = storage.last_count;
    st.storage_sta = storage.sta;
    st.storage_selected_bus_width = storage.selected_bus_width;
    st.storage_wide_status_8 = storage.wide_status_8;
    st.storage_wide_status_4 = storage.wide_status_4;
    st.storage_wide_status_1 = storage.wide_status_1;
    st.audio_ready = audio.i2s_ready;
    st.audio_dma_ready = audio.dma_ready;
    st.audio_i2s_status = audio.i2s_status;
    st.audio_dma_status = audio.dma_status;
    st.audio_dma_half_count = audio.dma_half_count;
    st.audio_dma_full_count = audio.dma_full_count;
    st.audio_underrun_count = audio.underrun_count;
}

::player::StorageConfig board_storage_config() noexcept {
    auto* block = board_emmc_block_device();
    if (block == nullptr) {
        refresh_board_resource_state();
        return empty_storage_config();
    }
    ::player::init_storage(*block);
    refresh_board_resource_state();
    return ::player::storage_config();
}

float scale_coord(std::uint16_t value, std::uint16_t max_value, std::uint32_t extent) noexcept {
    if (max_value <= 1U || extent == 0U) {
        return static_cast<float>(value);
    }

    const auto clamped = static_cast<std::uint32_t>((value < max_value) ? value : (max_value - 1U));
    return (static_cast<float>(clamped) * static_cast<float>(extent - 1U)) /
           static_cast<float>(max_value - 1U);
}

std::uint16_t clamp_touch_axis(const std::uint16_t value, const std::uint16_t max_value) noexcept {
    if (max_value <= 1U) {
        return value;
    }
    return value < max_value ? value : static_cast<std::uint16_t>(max_value - 1U);
}

void update_max(std::uint32_t& current, const std::uint32_t value) noexcept {
    current = value > current ? value : current;
}

charm::cap::PointerSample clamp_touch_sample(const charm::cap::PointerSample sample) noexcept {
    auto clamped = sample;
    clamped.x = clamp_touch_axis(sample.x, sample.max_x);
    clamped.y = clamp_touch_axis(sample.y, sample.max_y);
    return clamped;
}

struct MappedTouchSample {
    std::uint16_t raw_x{0};
    std::uint16_t raw_y{0};
    std::uint16_t ui_x{0};
    std::uint16_t ui_y{0};
    std::uint16_t raw_w{720};
    std::uint16_t raw_h{1280};
    std::uint16_t display_w{720};
    std::uint16_t display_h{1280};
};

MappedTouchSample map_touch_sample(const charm::cap::PointerSample sample) noexcept {
    constexpr std::uint16_t kDisplayWidth = 720U;
    constexpr std::uint16_t kDisplayHeight = 1280U;
    auto& st = h747::apps::player_md3::state();
    std::uint16_t x = sample.x;
    std::uint16_t y = sample.y;
    std::uint16_t source_w = sample.max_x;
    std::uint16_t source_h = sample.max_y;

    if (st.touch_map_swap_xy != 0U) {
        const auto old_x = x;
        x = y;
        y = old_x;
        const auto old_w = source_w;
        source_w = source_h;
        source_h = old_w;
    }
    if (st.touch_map_invert_x != 0U && source_w > 1U) {
        x = static_cast<std::uint16_t>((source_w - 1U) - clamp_touch_axis(x, source_w));
    }
    if (st.touch_map_invert_y != 0U && source_h > 1U) {
        y = static_cast<std::uint16_t>((source_h - 1U) - clamp_touch_axis(y, source_h));
    }

    const auto ui_x = static_cast<std::uint16_t>(
        scale_coord(clamp_touch_axis(x, source_w), source_w, kDisplayWidth));
    const auto ui_y = static_cast<std::uint16_t>(
        scale_coord(clamp_touch_axis(y, source_h), source_h, kDisplayHeight));
    st.touch_map_raw_x = sample.x;
    st.touch_map_raw_y = sample.y;
    st.touch_map_ui_x = ui_x;
    st.touch_map_ui_y = ui_y;
    st.touch_map_raw_w = sample.max_x;
    st.touch_map_raw_h = sample.max_y;
    st.touch_map_display_w = kDisplayWidth;
    st.touch_map_display_h = kDisplayHeight;
    return MappedTouchSample{
        .raw_x = sample.x,
        .raw_y = sample.y,
        .ui_x = ui_x,
        .ui_y = ui_y,
        .raw_w = sample.max_x,
        .raw_h = sample.max_y,
        .display_w = kDisplayWidth,
        .display_h = kDisplayHeight,
    };
}

void print_touch_ui_dispatch_trace(const charm::cap::PointerEvent& event,
                                   const MappedTouchSample mapped) noexcept {
    h747::console::write("touch_ui dispatch action=");
    h747::console::write_dec(static_cast<std::uint8_t>(event.action));
    h747::console::write(" raw=");
    h747::console::write_dec(mapped.raw_x);
    h747::console::write(",");
    h747::console::write_dec(mapped.raw_y);
    h747::console::write(" ui=");
    h747::console::write_dec(mapped.ui_x);
    h747::console::write(",");
    h747::console::write_dec(mapped.ui_y);
    h747::console::write(" id=");
    h747::console::write_dec(event.sample.id);
    h747::console::write(" sent=");
    h747::console::write_dec(h747::apps::player_md3::state().touch_ui_sent + 1U);
    h747::console::write("\n");
}

::player::PlayerPointerAction to_player_pointer_action(
    const charm::cap::PointerAction action) noexcept {
    using charm::cap::PointerAction;
    switch (action) {
    case PointerAction::down:
        return ::player::PlayerPointerAction::Down;
    case PointerAction::up:
        return ::player::PlayerPointerAction::Up;
    case PointerAction::cancel:
        return ::player::PlayerPointerAction::Cancel;
    case PointerAction::move:
    default:
        return ::player::PlayerPointerAction::Move;
    }
}

::player::PlayerInputCommand to_player_input_command(
    const h747::apps::player_md3::PlayerMd3InputCommand command) noexcept {
    using h747::apps::player_md3::PlayerMd3InputCommand;
    switch (command) {
    case PlayerMd3InputCommand::Up:
        return ::player::PlayerInputCommand::Up;
    case PlayerMd3InputCommand::Down:
        return ::player::PlayerInputCommand::Down;
    case PlayerMd3InputCommand::Left:
        return ::player::PlayerInputCommand::Left;
    case PlayerMd3InputCommand::Back:
        return ::player::PlayerInputCommand::Back;
    case PlayerMd3InputCommand::PlayToggle:
        return ::player::PlayerInputCommand::PlayToggle;
    case PlayerMd3InputCommand::Next:
        return ::player::PlayerInputCommand::Next;
    case PlayerMd3InputCommand::Prev:
        return ::player::PlayerInputCommand::Prev;
    case PlayerMd3InputCommand::Mode:
        return ::player::PlayerInputCommand::Mode;
    case PlayerMd3InputCommand::Enter:
    default:
        return ::player::PlayerInputCommand::Enter;
    }
}

bool render_pause_requested() noexcept {
    const auto& st = h747::apps::player_md3::state();
    return st.touch_monitor_pause_render != 0U || st.touch_sample_pause_render != 0U;
}

} // namespace

namespace h747::apps::player_md3 {

PlayerMd3State& state() noexcept {
    static PlayerMd3State s{};
    return s;
}

void set_touch_monitor_enabled(const bool enabled, const bool pause_render) noexcept {
    auto& st = state();
    st.touch_monitor_enabled = enabled ? 1U : 0U;
    st.touch_monitor_pause_render = (enabled && pause_render) ? 1U : 0U;
    st.touch_monitor_events = 0U;
}

bool touch_monitor_enabled() noexcept {
    return state().touch_monitor_enabled != 0U;
}

std::uint32_t touch_monitor_event_count() noexcept {
    return state().touch_monitor_events;
}

std::uint32_t record_touch_monitor_event() noexcept {
    return ++state().touch_monitor_events;
}

::player::PlayerController& controller_ref() noexcept {
    static ::player::PlayerController controller{};
    return controller;
}

charm::system::Clock& clock_ref() noexcept {
    static charm::system::Clock clock{nullptr, {.now_us = &player_md3_now_us}};
    return clock;
}

::player::PlayerRuntimeConfig<::player::PlayerPage> runtime_config() noexcept {
    audio::PlayerConfig audio_cfg{};
    audio_cfg.output_mode = audio::OutputMode::fixed_rate;
    audio_cfg.fixed_rate = 48000;

    ::player::AppConfig app_cfg{};
    app_cfg.player_config = audio_cfg;
    app_cfg.font_resources = board_font_resource_config();
    app_cfg.icon_pixels = ::player::PixelArenaConfig{
        reinterpret_cast<std::byte*>(state().icon_pixel_arena),
        state().icon_pixel_arena_bytes,
    };

    return ::player::PlayerRuntimeConfig<::player::PlayerPage>{
        .app_config = app_cfg,
        .storage_config = board_storage_config(),
        .start_page = ::player::PlayerPage::Home,
        .initial_track_index = 0,
        .auto_start = false,
        .clear_color = ::player::ui::kUiBackground,
    };
}

::player::PlayerPlatform& platform_ref() noexcept {
    if (g_platform == nullptr) {
        g_platform = ::new (reinterpret_cast<void*>(state().platform_storage))
            ::player::PlayerPlatform{render_surface_ref()};
    }
    return *g_platform;
}

::player::PlayerDisplaySink& sink_ref() noexcept {
    static ::player::PlayerDisplaySink sink =
        h747::apps::player::make_player_raster_display_sink(state().sink_state, state().panel);
    return sink;
}

PlayerRuntime* runtime_ref() noexcept {
    return g_runtime;
}

PlayerRuntimeShell* shell_ref() noexcept {
    return g_shell;
}

PlayerRuntime& runtime_emplace() noexcept {
    if (g_runtime == nullptr) {
        g_runtime = ::new (reinterpret_cast<void*>(state().runtime_storage))
            PlayerRuntime{clock_ref(), platform_ref(), controller_ref(), runtime_config()};
    }
    return *g_runtime;
}

void dispatch_player_input_event(const ::player::PlayerInputEvent& event) noexcept {
    auto* shell = shell_ref();
    if (shell == nullptr) {
        return;
    }

    shell->dispatch_input(event);
    ++state().input_events;
    render_scheduler_mark_dirty();
}

void dispatch_runtime_pointer(const charm::cap::PointerEvent event) noexcept {
    const auto sample = clamp_touch_sample(event.sample);
    const auto mapped = map_touch_sample(sample);
    print_touch_ui_dispatch_trace(event, mapped);
    record_touch_runtime_dispatch_sent(event.action);
    record_touch_latency_dispatch(h747::port::tick_ms());
    dispatch_player_input_event(::player::PlayerInputEvent::make_pointer(
        h747::port::tick_ms(),
        to_player_pointer_action(event.action),
        ::player::PlayerPointerSample{
            sample.down,
            static_cast<float>(mapped.ui_x),
            static_cast<float>(mapped.ui_y),
            sample.id,
        }));
}

void dispatch_runtime_command(const std::uint32_t ms, const PlayerMd3InputCommand command) noexcept {
    dispatch_player_input_event(::player::PlayerInputEvent::make_command(
        ms,
        to_player_input_command(command)));
}

void reset_input_route_evidence() noexcept {
    auto& st = state();
    st.input_route_console_commands = 0U;
    st.input_route_touch_pointers = 0U;
    st.input_route_encoder_commands = 0U;
    st.input_route_button_commands = 0U;
    st.input_route_last_source = 0U;
    st.input_route_last_kind = 0U;
    st.input_route_last_code = 0U;
}

void record_input_route(const PlayerMd3InputRouteSource source,
                        const PlayerMd3InputCommand command) noexcept {
    auto& st = state();
    switch (source) {
    case PlayerMd3InputRouteSource::Console:
        ++st.input_route_console_commands;
        break;
    case PlayerMd3InputRouteSource::Encoder:
        ++st.input_route_encoder_commands;
        break;
    case PlayerMd3InputRouteSource::Button:
        ++st.input_route_button_commands;
        break;
    case PlayerMd3InputRouteSource::Touch:
    case PlayerMd3InputRouteSource::Unknown:
    default:
        break;
    }
    st.input_route_last_source = static_cast<std::uint8_t>(source);
    st.input_route_last_kind = 1U;
    st.input_route_last_code = static_cast<std::uint8_t>(command);
}

void record_input_route(const PlayerMd3InputRouteSource source,
                        const charm::cap::PointerAction action) noexcept {
    auto& st = state();
    if (source == PlayerMd3InputRouteSource::Touch) {
        ++st.input_route_touch_pointers;
    }
    st.input_route_last_source = static_cast<std::uint8_t>(source);
    st.input_route_last_kind = 2U;
    st.input_route_last_code = static_cast<std::uint8_t>(action);
}

void set_touch_runtime_dispatch_enabled(const bool enabled) noexcept {
    auto& st = state();
    st.touch_dispatch_enabled = enabled ? 1U : 0U;
    if (enabled) {
        st.touch_dispatch_once_active = 0U;
        st.touch_dispatch_once_in_sequence = 0U;
    }
}

void set_touch_runtime_dispatch_once() noexcept {
    auto& st = state();
    st.touch_dispatch_enabled = 0U;
    st.touch_dispatch_once_active = 1U;
    st.touch_dispatch_once_in_sequence = 0U;
}

bool touch_runtime_dispatch_enabled() noexcept {
    const auto& st = state();
    return st.touch_dispatch_enabled != 0U || st.touch_dispatch_once_active != 0U;
}

bool touch_runtime_dispatch_allows(const charm::cap::PointerAction action) noexcept {
    auto& st = state();
    if (st.touch_dispatch_enabled != 0U) {
        return true;
    }
    if (st.touch_dispatch_once_active == 0U) {
        return false;
    }

    if (!st.touch_dispatch_once_in_sequence) {
        if (action != charm::cap::PointerAction::down) {
            return false;
        }
        st.touch_dispatch_once_in_sequence = 1U;
        return true;
    }

    if (action == charm::cap::PointerAction::up || action == charm::cap::PointerAction::cancel) {
        st.touch_dispatch_once_active = 0U;
        st.touch_dispatch_once_in_sequence = 0U;
    }
    return true;
}

std::uint32_t touch_runtime_dispatch_blocked_count() noexcept {
    return state().touch_dispatch_blocked;
}

std::uint8_t touch_runtime_dispatch_last_action() noexcept {
    return state().touch_dispatch_last_action;
}

void record_touch_runtime_dispatch_blocked(const charm::cap::PointerAction action) noexcept {
    auto& st = state();
    ++st.touch_dispatch_blocked;
    ++st.touch_ui_blocked;
    st.touch_dispatch_last_action = static_cast<std::uint8_t>(action);
    st.touch_ui_last_action = static_cast<std::uint8_t>(action);
}

void record_touch_runtime_dispatch_sent(const charm::cap::PointerAction action) noexcept {
    auto& st = state();
    ++st.touch_ui_sent;
    st.touch_ui_last_action = static_cast<std::uint8_t>(action);
}

void record_touch_ui_fault_guard(const charm::cap::PointerAction action) noexcept {
    auto& st = state();
    ++st.touch_ui_fault_guard;
    st.touch_ui_last_action = static_cast<std::uint8_t>(action);
}

void record_touch_latency_poll(const std::uint32_t poll_ms,
                               const std::uint32_t int_edge_ms) noexcept {
    auto& st = state();
    st.touch_latency_last_poll_ms = poll_ms;
    if (int_edge_ms != 0U && poll_ms >= int_edge_ms) {
        st.touch_latency_last_int_to_poll_ms = poll_ms - int_edge_ms;
        update_max(st.touch_latency_max_int_to_poll_ms, st.touch_latency_last_int_to_poll_ms);
    }
}

void record_touch_latency_dispatch(const std::uint32_t dispatch_ms) noexcept {
    auto& st = state();
    st.touch_latency_last_dispatch_ms = dispatch_ms;
    ++st.touch_latency_samples;
    if (st.touch_latency_last_poll_ms != 0U && dispatch_ms >= st.touch_latency_last_poll_ms) {
        st.touch_latency_last_poll_to_dispatch_ms = dispatch_ms - st.touch_latency_last_poll_ms;
        update_max(st.touch_latency_max_poll_to_dispatch_ms,
                   st.touch_latency_last_poll_to_dispatch_ms);
    }
}

void note_touch_render_frame() noexcept {
    auto& st = state();
    const auto now_ms = h747::port::tick_ms();
    st.touch_latency_last_frame_ms = now_ms;
    if (st.touch_latency_last_dispatch_ms != 0U && now_ms >= st.touch_latency_last_dispatch_ms) {
        st.touch_latency_last_dispatch_to_frame_ms = now_ms - st.touch_latency_last_dispatch_ms;
        update_max(st.touch_latency_max_dispatch_to_frame_ms,
                   st.touch_latency_last_dispatch_to_frame_ms);
    }
}

void reset_touch_latency_evidence() noexcept {
    auto& st = state();
    st.touch_latency_samples = 0U;
    st.touch_latency_last_int_to_poll_ms = 0U;
    st.touch_latency_last_poll_to_dispatch_ms = 0U;
    st.touch_latency_last_dispatch_to_frame_ms = 0U;
    st.touch_latency_max_int_to_poll_ms = 0U;
    st.touch_latency_max_poll_to_dispatch_ms = 0U;
    st.touch_latency_max_dispatch_to_frame_ms = 0U;
    st.touch_latency_last_poll_ms = 0U;
    st.touch_latency_last_dispatch_ms = 0U;
    st.touch_latency_last_frame_ms = 0U;
}

void set_touch_map_mode(const PlayerMd3TouchMapMode mode) noexcept {
    auto& st = state();
    st.touch_map_mode = static_cast<std::uint8_t>(mode);
    st.touch_map_swap_xy = 0U;
    st.touch_map_invert_x = 0U;
    st.touch_map_invert_y = 0U;
    switch (mode) {
    case PlayerMd3TouchMapMode::Swap:
        st.touch_map_swap_xy = 1U;
        break;
    case PlayerMd3TouchMapMode::InvertX:
        st.touch_map_invert_x = 1U;
        break;
    case PlayerMd3TouchMapMode::InvertY:
        st.touch_map_invert_y = 1U;
        break;
    case PlayerMd3TouchMapMode::Rot90:
        st.touch_map_swap_xy = 1U;
        st.touch_map_invert_x = 1U;
        break;
    case PlayerMd3TouchMapMode::Rot270:
        st.touch_map_swap_xy = 1U;
        st.touch_map_invert_y = 1U;
        break;
    case PlayerMd3TouchMapMode::Normal:
    default:
        break;
    }
}

void print_touch_runtime_dispatch_status() noexcept {
    const auto& st = state();
    h747::console::write("touch_dispatch enabled=");
    h747::console::write_dec(st.touch_dispatch_enabled);
    h747::console::write(" once=");
    h747::console::write_dec(st.touch_dispatch_once_active);
    h747::console::write("/");
    h747::console::write_dec(st.touch_dispatch_once_in_sequence);
    h747::console::write(" blocked=");
    h747::console::write_dec(st.touch_dispatch_blocked);
    h747::console::write(" sent=");
    h747::console::write_dec(st.touch_ui_sent);
    h747::console::write(" last_action=");
    h747::console::write_dec(st.touch_dispatch_last_action);
    h747::console::write("\n");
}

void print_touch_map_status() noexcept {
    const auto& st = state();
    h747::console::write("touch_map mode=");
    h747::console::write_dec(st.touch_map_mode);
    h747::console::write(" flags=");
    h747::console::write_dec(st.touch_map_swap_xy);
    h747::console::write("/");
    h747::console::write_dec(st.touch_map_invert_x);
    h747::console::write("/");
    h747::console::write_dec(st.touch_map_invert_y);
    h747::console::write(" raw=");
    h747::console::write_dec(st.touch_map_raw_x);
    h747::console::write(",");
    h747::console::write_dec(st.touch_map_raw_y);
    h747::console::write(" ui=");
    h747::console::write_dec(st.touch_map_ui_x);
    h747::console::write(",");
    h747::console::write_dec(st.touch_map_ui_y);
    h747::console::write(" raw_max=");
    h747::console::write_dec(st.touch_map_raw_w);
    h747::console::write("x");
    h747::console::write_dec(st.touch_map_raw_h);
    h747::console::write(" display=");
    h747::console::write_dec(st.touch_map_display_w);
    h747::console::write("x");
    h747::console::write_dec(st.touch_map_display_h);
    h747::console::write("\n");
}

void print_touch_latency_status() noexcept {
    const auto& st = state();
    h747::console::write("touch_latency samples=");
    h747::console::write_dec(st.touch_latency_samples);
    h747::console::write(" last=");
    h747::console::write_dec(st.touch_latency_last_int_to_poll_ms);
    h747::console::write("/");
    h747::console::write_dec(st.touch_latency_last_poll_to_dispatch_ms);
    h747::console::write("/");
    h747::console::write_dec(st.touch_latency_last_dispatch_to_frame_ms);
    h747::console::write(" max=");
    h747::console::write_dec(st.touch_latency_max_int_to_poll_ms);
    h747::console::write("/");
    h747::console::write_dec(st.touch_latency_max_poll_to_dispatch_ms);
    h747::console::write("/");
    h747::console::write_dec(st.touch_latency_max_dispatch_to_frame_ms);
    h747::console::write(" ms poll=");
    h747::console::write_dec(st.touch_latency_last_poll_ms);
    h747::console::write(" dispatch=");
    h747::console::write_dec(st.touch_latency_last_dispatch_ms);
    h747::console::write(" frame=");
    h747::console::write_dec(st.touch_latency_last_frame_ms);
    h747::console::write("\n");
}

void record_input_snapshot(const PlayerMd3InputSnapshot snapshot) noexcept {
    auto& st = state();
    st.input_touch_ready = snapshot.touch_ready;
    st.input_touch_down = snapshot.touch_down;
    st.input_last_id = snapshot.touch_id;
    st.input_touch_profile = snapshot.touch_profile;
    st.input_touch_int_exti = snapshot.touch_int_exti;
    st.input_touch_int_level = snapshot.touch_int_level;
    st.input_touch_int_last_level = snapshot.touch_int_last_level;
    st.input_last_x = snapshot.touch_x;
    st.input_last_y = snapshot.touch_y;
    st.input_touch_int_rise = snapshot.touch_int_rise;
    st.input_touch_int_fall = snapshot.touch_int_fall;
    st.input_touch_int_last_ms = snapshot.touch_int_last_ms;
    st.input_encoder1_delta = snapshot.encoder1_delta;
    st.input_encoder2_delta = snapshot.encoder2_delta;
    st.input_encoder1_button = snapshot.encoder1_button;
    st.input_encoder2_button = snapshot.encoder2_button;
}

void record_input_bridge_init(const std::uint8_t touch_probe_ok,
                              const PlayerMd3InputSnapshot snapshot) noexcept {
    state().input_touch_probe_ok = touch_probe_ok;
    record_input_snapshot(snapshot);
}

void record_input_bridge_poll(const PlayerMd3InputSnapshot snapshot) noexcept {
    ++state().input_polls;
    record_input_snapshot(snapshot);
}

void record_input_touch_event() noexcept {
    ++state().input_touch_events;
}

void record_input_encoder_event() noexcept {
    ++state().input_encoder_events;
}

void record_input_button_event() noexcept {
    ++state().input_button_events;
}

std::uint32_t touch_oob_count() noexcept {
    return state().touch_oob_count;
}

void record_touch_oob(const std::uint16_t raw_x,
                      const std::uint16_t raw_y,
                      const std::uint16_t clamped_x,
                      const std::uint16_t clamped_y,
                      const std::uint16_t max_x,
                      const std::uint16_t max_y) noexcept {
    auto& st = state();
    ++st.touch_oob_count;
    st.touch_oob_raw_x = raw_x;
    st.touch_oob_raw_y = raw_y;
    st.touch_oob_clamped_x = clamped_x;
    st.touch_oob_clamped_y = clamped_y;
    st.touch_oob_max_x = max_x;
    st.touch_oob_max_y = max_y;
}

void reset_touch_sample_evidence(const bool enabled,
                                 const std::uint8_t last_int,
                                 const std::uint32_t now_ms) noexcept {
    auto& st = state();
    st.touch_sample_enabled = enabled ? 1U : 0U;
    st.touch_sample_pause_render = enabled ? 1U : 0U;
    if (!enabled) {
        return;
    }
    st.touch_sample_count = 0U;
    st.touch_sample_ready_hits = 0U;
    st.touch_sample_int_changes = 0U;
    st.touch_sample_last_ms = now_ms;
    st.touch_sample_last_int = last_int;
    st.touch_sample_raw_min_x = 0U;
    st.touch_sample_raw_min_y = 0U;
    st.touch_sample_raw_max_x = 0U;
    st.touch_sample_raw_max_y = 0U;
    st.touch_sample_filtered_x = 0U;
    st.touch_sample_filtered_y = 0U;
}

void record_touch_sample_poll(const std::uint32_t now_ms,
                              const std::uint8_t int_level,
                              const bool ready_hit,
                              const std::uint16_t raw_x,
                              const std::uint16_t raw_y,
                              const std::uint16_t max_x,
                              const std::uint16_t max_y) noexcept {
    auto& st = state();
    ++st.touch_sample_count;
    st.touch_sample_last_ms = now_ms;
    if (int_level != st.touch_sample_last_int) {
        ++st.touch_sample_int_changes;
        st.touch_sample_last_int = int_level;
    }
    if (!ready_hit) {
        return;
    }

    if (st.touch_sample_ready_hits == 0U) {
        st.touch_sample_raw_min_x = raw_x;
        st.touch_sample_raw_min_y = raw_y;
        st.touch_sample_raw_max_x = raw_x;
        st.touch_sample_raw_max_y = raw_y;
    } else {
        st.touch_sample_raw_min_x = raw_x < st.touch_sample_raw_min_x ? raw_x : st.touch_sample_raw_min_x;
        st.touch_sample_raw_min_y = raw_y < st.touch_sample_raw_min_y ? raw_y : st.touch_sample_raw_min_y;
        st.touch_sample_raw_max_x = raw_x > st.touch_sample_raw_max_x ? raw_x : st.touch_sample_raw_max_x;
        st.touch_sample_raw_max_y = raw_y > st.touch_sample_raw_max_y ? raw_y : st.touch_sample_raw_max_y;
    }
    ++st.touch_sample_ready_hits;
    st.touch_sample_filtered_x = clamp_touch_axis(raw_x, max_x);
    st.touch_sample_filtered_y = clamp_touch_axis(raw_y, max_y);
}

PlayerMd3TouchSampleEvidence touch_sample_evidence() noexcept {
    const auto& st = state();
    return PlayerMd3TouchSampleEvidence{
        .enabled = st.touch_sample_enabled,
        .pause_render = st.touch_sample_pause_render,
        .samples = st.touch_sample_count,
        .ready_hits = st.touch_sample_ready_hits,
        .int_changes = st.touch_sample_int_changes,
        .raw_min_x = st.touch_sample_raw_min_x,
        .raw_min_y = st.touch_sample_raw_min_y,
        .raw_max_x = st.touch_sample_raw_max_x,
        .raw_max_y = st.touch_sample_raw_max_y,
        .filtered_x = st.touch_sample_filtered_x,
        .filtered_y = st.touch_sample_filtered_y,
        .oob_count = st.touch_oob_count,
        .oob_raw_x = st.touch_oob_raw_x,
        .oob_raw_y = st.touch_oob_raw_y,
        .oob_clamped_x = st.touch_oob_clamped_x,
        .oob_clamped_y = st.touch_oob_clamped_y,
        .oob_max_x = st.touch_oob_max_x,
        .oob_max_y = st.touch_oob_max_y,
    };
}

void record_touch_config_auto_evidence(const std::uint8_t attempted,
                                       const std::uint8_t written,
                                       const std::uint8_t verify_ok,
                                       const std::uint8_t stage,
                                       const std::uint8_t invalid_reason,
                                       const std::uint8_t before_valid,
                                       const std::uint8_t after_valid,
                                       const std::uint8_t force,
                                       const std::uint32_t error_code) noexcept {
    auto& st = state();
    st.touch_cfg_auto_attempted = attempted;
    st.touch_cfg_auto_written = written;
    st.touch_cfg_auto_verify = verify_ok;
    st.touch_cfg_auto_stage = stage;
    st.touch_cfg_auto_err = invalid_reason;
    st.touch_cfg_auto_before_valid = before_valid;
    st.touch_cfg_auto_after_valid = after_valid;
    st.touch_cfg_auto_force = force;
    st.touch_cfg_auto_error_code = error_code;
}

void refresh_playback_probe_state() noexcept {
    auto& st = state();
    auto* shell = shell_ref();
    auto* app = shell ? shell->app() : nullptr;
    const auto audio_state = h747_audio_state();
    st.playback_dma_callbacks = audio_state.dma_half_count + audio_state.dma_full_count;
    st.playback_underruns = audio_state.underrun_count;
    st.playback_track_ready = controller_ref().track_ready() ? 1U : 0U;
    if (!app) {
        st.playback_player_state = 0U;
        st.playback_running = 0U;
        st.playback_last_error_stage = 0;
        st.playback_last_error = 0;
        return;
    }
    const auto& player = app->player();
    st.playback_player_state = static_cast<std::uint8_t>(player.state());
    st.playback_running = player.is_running() ? 1U : 0U;
    st.playback_last_error_stage = static_cast<std::int32_t>(player.last_error_stage());
    st.playback_last_error = static_cast<std::int32_t>(player.last_error());
}

bool render_frame() noexcept {
    auto* shell = shell_ref();
    if (shell == nullptr) {
        return false;
    }
    const bool timing_available = dwt_cycle_counter_ready();
    const std::uint64_t frame_start = timing_available ? dwt_now_us() : 0ULL;
    const std::uint64_t tick_start = frame_start;
    shell->step(clock_ref().now_us());
    const std::uint64_t tick_end = timing_available ? dwt_now_us() : 0ULL;
    TimedDisplaySinkState timed_sink_state{&sink_ref(), 0U};
    ::player::PlayerDisplaySink timed_sink{&timed_sink_state, &timed_display_present};
    const bool ok = shell->runtime() && shell->runtime()->render(&timed_sink);
    const std::uint64_t render_end = timing_available ? dwt_now_us() : 0ULL;
    refresh_board_resource_state();
    if ((state().frames % 120U) == 0U) {
        refresh_resource_probe_state();
    }
    state().last_render_ok = ok;
    sample_render_surface();
    if (ok && ((state().frames < 2U) || ((state().frames % 30U) == 0U))) {
        sample_render_content_bounds();
    }
    sample_scene_stats();
    const auto scene_timing = shell->scene()
        ? shell->scene()->last_render_timing()
        : ::ui::scene::SceneRenderTiming{};
    auto& st = state();
    st.perf_time_available = timing_available ? 1U : 0U;
    st.perf_time_frame_us = timing_available ? dwt_delta_us(frame_start, render_end) : 0U;
    st.perf_time_tick_us = timing_available ? dwt_delta_us(tick_start, tick_end) : 0U;
    st.perf_time_render_us = timing_available ? dwt_delta_us(tick_end, render_end) : 0U;
    st.perf_time_record_us = scene_timing.record_us;
    st.perf_time_execute_us = scene_timing.execute_us;
    st.perf_time_present_us = timed_sink_state.present_us;
    refresh_playback_probe_state();
    if (ok) {
        ++state().frames;
        render_scheduler_note_full_render();
        note_touch_render_frame();
    }
    update_smoke_verdict();
    return ok;
}

bool force_render_frame() noexcept {
    render_scheduler_note_forced_render();
    return render_frame();
}

void runtime_step_only() noexcept {
    auto* shell = shell_ref();
    if (shell != nullptr) {
        shell->step(clock_ref().now_us());
    }
}

void init_runtime() noexcept {
    h747::console::write_line("player_md3: real MD3 PlayerRuntime");
    ::player::ui::set_player_system_font_fallback_enabled(false);

    const auto raster = display_raster_state();
    if (raster.init_ok == 0U || raster.framebuffer_ready == 0U || raster.ltdc_layer_ready == 0U) {
        h747::console::write_line("player_md3: display_raster service is not ready");
        print_status("player_md3");
        return;
    }

    auto& st = state();
    st.display_ready = true;
    if (!ensure_runtime_storage_ready()) {
        h747::console::write_line("player_md3: SDRAM1 runtime storage is not ready");
        print_status("player_md3");
        return;
    }

    auto& runtime = runtime_emplace();
    ::player::PlayerRuntimeShellConfig shell_cfg{};
    shell_cfg.display_sink = &sink_ref();
    if (g_shell == nullptr) {
        g_shell = ::new (static_cast<void*>(g_shell_storage)) PlayerRuntimeShell{runtime, shell_cfg};
    }
    const bool timing_ready = enable_dwt_cycle_counter();
    g_shell->scene_ref().set_timing_source(::ui::scene::SceneTimingSource{
        nullptr,
        [](void*) noexcept -> std::uint64_t {
            return dwt_now_us();
        },
    });
    st.perf_time_available = timing_ready ? 1U : 0U;
    init_input_bridge();

    h747::console::write_line("player_md3: bootstrap begin");
    (void)g_shell->bootstrap();
    st.runtime_bootstrapped = g_shell->app() != nullptr;
    h747::console::write_line(st.runtime_bootstrapped ? "player_md3: bootstrap ok"
                                                      : "player_md3: bootstrap failed");
    refresh_resource_probe_state();
    (void)force_render_frame();
    h747::console::write_line(st.last_render_ok ? "player_md3: first render ok"
                                                : "player_md3: first render failed");
    print_status("player_md3");
    init_console_bridge();
}

void loop_runtime() noexcept {
    if (!state().runtime_bootstrapped) {
        return;
    }
    poll_console_bridge();
    poll_input_bridge();
    if (render_pause_requested()) {
        return;
    }
    if (render_scheduler_should_render(h747::port::tick_ms())) {
        (void)render_frame();
    } else {
        runtime_step_only();
        render_scheduler_note_skip();
    }
    maybe_print_loop_status();
}

} // namespace h747::apps::player_md3
