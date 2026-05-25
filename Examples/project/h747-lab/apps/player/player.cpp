#include "player.h"

#include "console.h"
#include "console_service.hpp"
#include "h747_world.hpp"
#include "memory_service.hpp"
#include "player_display_hal.hpp"
#include "player_domain.hpp"
#include "player_input.hpp"

#include <string_view>

namespace {

h747::world::DiyBoardWorld& active_world() noexcept {
    static h747::world::DiyBoardWorld instance{};
    return instance;
}

h747::apps::player::PlayerRuntime& active_runtime() noexcept {
    static h747::apps::player::PlayerRuntime instance{};
    return instance;
}

h747::memory::StorageProbe& active_storage_probe() noexcept {
    static h747::memory::StorageProbe instance{};
    return instance;
}

h747::console::ConsoleLineSource& active_line_source() noexcept {
    static h747::console::ConsoleLineSource instance{};
    return instance;
}

h747::apps::player::PlayerRasterDisplaySinkState& active_player_display_sink_state() noexcept {
    static h747::apps::player::PlayerRasterDisplaySinkState instance{};
    return instance;
}

player::PlayerDisplaySink active_player_display_sink() noexcept {
    return h747::apps::player::make_player_raster_display_sink(
        active_player_display_sink_state(),
        active_world().display());
}

h747::apps::player::PlayerBoardSnapshot capture_board_snapshot() noexcept {
    const auto display = active_world().display().state().raw;
    const auto storage = active_storage_probe().snapshot().raw;
    return h747::apps::player::PlayerBoardSnapshot{
        .display_ready = display.init_ok != 0U,
        .framebuffer_ready = display.framebuffer_ready != 0U,
        .sdram_ready = display.sdram_ready != 0U,
        .sdram_smoke_ok = display.sdram_smoke_ok != 0U,
        .qspi_power_good = storage.qspi_power_good != 0U,
        .qspi_jedec_ok = storage.qspi_jedec_ok != 0U,
        .qspi_read_ok = storage.qspi_read_ok != 0U,
    };
}

void probe_resources_once() noexcept {
    auto& storage = active_storage_probe();
    const auto snapshot = storage.snapshot().raw;
    if ((snapshot.qspi_power_good != 0U) && (snapshot.qspi_jedec_ok == 0U) && (snapshot.qspi_read_ok == 0U)) {
        (void)storage.probe_qspi();
    }
}

void probe_resources_periodic() noexcept {
    static std::uint32_t last_probe_ms = 0U;
    const std::uint32_t now = active_world().clock().tick_ms().value;
    if ((now - last_probe_ms) < 5000U) {
        return;
    }
    last_probe_ms = now;
    probe_resources_once();
}

void print_hex32(const char* label, const std::uint32_t value) {
    h747::console::write(label);
    h747::console::write_hex32(value);
}

void print_dec32(const char* label, const std::uint32_t value) {
    h747::console::write(label);
    h747::console::write_dec(value);
}

void print_raster_state(const char* prefix);

void print_bool(const char* label, const bool value) {
    h747::console::write(label);
    h747::console::write(value ? "1" : "0");
}

void print_help() {
    h747::console::write_line("player commands:");
    h747::console::write_line("  status  - print player/display state");
    h747::console::write_line("  toggle  - toggle play/pause");
    h747::console::write_line("  next    - jump progress forward");
    h747::console::write_line("  prev    - jump progress backward");
    h747::console::write_line("  seek+   - seek forward");
    h747::console::write_line("  seek-   - seek backward");
}

void print_player_status() {
    const auto& view = active_runtime().view();
    h747::console::write("player_status:");
    print_bool(" playing=", view.playing);
    print_dec32(" progress=", view.progress_percent);
    print_bool(" storage=", view.storage_ready);
    print_bool(" cover=", view.cover_ready);
    h747::console::write(" subtitle=");
    for (const char ch : view.subtitle) {
        h747::console::write_char(ch);
    }
    h747::console::write("\n");
    print_raster_state("player_status");
}

void print_raster_state(const char* prefix) {
    const auto state = active_world().display().state().raw;
    const auto mode = active_world().display().mode();

    h747::console::write(prefix);
    h747::console::write(" mode=");
    h747::console::write_dec(mode.extent.width);
    h747::console::write("x");
    h747::console::write_dec(mode.extent.height);
    h747::console::write(" fmt=argb8888");
    print_hex32(" fb=", state.framebuffer_base);
    print_hex32(" bytes=", state.framebuffer_bytes);
    h747::console::write(" init=");
    h747::console::write_dec(state.init_ok);
    h747::console::write(" sdram=");
    h747::console::write_dec(state.sdram_ready);
    h747::console::write(" smoke=");
    h747::console::write_dec(state.sdram_smoke_ok);
    print_dec32(" words=", state.sdram_tested_words);
    print_hex32(" first_err=", static_cast<std::uint32_t>(state.sdram_first_error_addr));
    h747::console::write("\n");

    h747::console::write(prefix);
    h747::console::write("_regs");
    h747::console::write(" layer=");
    h747::console::write_dec(state.ltdc_layer_ready);
    h747::console::write(" present=");
    h747::console::write_dec(state.present_count);
    h747::console::write(" clean=");
    h747::console::write_dec(state.cache_clean_count);
    print_hex32(" hal=", state.last_hal_status);
    print_hex32(" sdram_hal=", state.sdram_last_hal_status);
    print_hex32(" dsi_err=", state.dsi_error);
    print_hex32(" WCR=", state.dsi_wcr);
    print_hex32(" WISR=", state.dsi_wisr);
    print_hex32(" LTDC_ISR=", state.ltdc_isr);
    h747::console::write("\n");
}

void print_player_display_surface(const char* prefix) {
    const auto surface = h747::apps::player::make_player_display_surface(active_world().display());
    h747::console::write(prefix);
    h747::console::write("_surface");
    h747::console::write(" valid=");
    h747::console::write_dec(surface.valid() ? 1U : 0U);
    h747::console::write(" size=");
    h747::console::write_dec(static_cast<std::uint32_t>(surface.width));
    h747::console::write("x");
    h747::console::write_dec(static_cast<std::uint32_t>(surface.height));
    h747::console::write(" stride=");
    h747::console::write_dec(static_cast<std::uint32_t>(surface.stride_bytes));
    h747::console::write(" fmt=argb8888");
    h747::console::write(" pixels=");
    h747::console::write_hex32(reinterpret_cast<std::uintptr_t>(surface.pixels));
    h747::console::write("\n");
}

void present_player_platform_probe() noexcept {
    const auto surface = h747::apps::player::make_player_display_surface(active_world().display());
    const auto sink = active_player_display_sink();
    const bool ok = sink.present(surface, player::full_player_dirty_region(surface));
    h747::console::write("player_display_hal: present=");
    h747::console::write_dec(ok ? 1U : 0U);
    h747::console::write("\n");
}

void handle_player_command(const std::string_view line) {
    const auto event = h747::apps::player::parse_player_input_event(line);
    using h747::apps::player::PlayerInputCommand;

    switch (event.command) {
    case PlayerInputCommand::none:
        return;
    case PlayerInputCommand::help:
        print_help();
        return;
    case PlayerInputCommand::status:
        print_player_status();
        return;
    case PlayerInputCommand::toggle:
    case PlayerInputCommand::next:
    case PlayerInputCommand::previous:
    case PlayerInputCommand::seek_forward:
    case PlayerInputCommand::seek_backward:
        active_runtime().observe_input(event.frame);
        h747::console::write("player_cmd: ");
        h747::console::write(h747::apps::player::player_input_command_name(event.command));
        h747::console::write("\n");
        return;
    case PlayerInputCommand::unknown:
        h747::console::write_line("player_cmd: unknown");
        print_help();
        return;
    }
}

} // namespace

namespace h747::apps::player {

void init() {
    active_world().init();
    probe_resources_once();
    active_runtime().observe_board(capture_board_snapshot());
    print_raster_state("player");
    print_player_display_surface("player");
    present_player_platform_probe();
    init(active_world(), active_runtime());
    print_raster_state("player");
    print_help();
}

void loop_once() noexcept {
    probe_resources_periodic();
    if (const auto line = active_line_source().poll_line()) {
        handle_player_command(*line);
    }
    active_runtime().observe_board(capture_board_snapshot());
    loop_once(active_world(), active_runtime());
    static std::uint32_t last_present = 0U;
    const auto present = active_world().display().state().raw.present_count;
    if (present != last_present) {
        last_present = present;
        print_raster_state("player");
    }
}

} // namespace h747::apps::player
