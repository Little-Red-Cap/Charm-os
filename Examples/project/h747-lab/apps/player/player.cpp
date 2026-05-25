#include "player.h"

#include "console.h"
#include "console_service.hpp"
#include "h747_world.hpp"
#include "memory_service.hpp"
#include "player_domain.hpp"

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

[[nodiscard]] bool command_from_line(const std::string_view line,
                                     h747::apps::player::PlayerCommand& out) noexcept {
    using h747::apps::player::PlayerCommandKind;
    if ((line == "play") || (line == "pause") || (line == "toggle")) {
        out = h747::apps::player::PlayerCommand{.kind = PlayerCommandKind::toggle_play};
        return true;
    }
    if ((line == "next") || (line == "n")) {
        out = h747::apps::player::PlayerCommand{.kind = PlayerCommandKind::next_track};
        return true;
    }
    if ((line == "prev") || (line == "previous") || (line == "p")) {
        out = h747::apps::player::PlayerCommand{.kind = PlayerCommandKind::previous_track};
        return true;
    }
    if ((line == "seek+") || (line == "+")) {
        out = h747::apps::player::PlayerCommand{
            .kind = PlayerCommandKind::seek_relative,
            .delta_percent = 10,
        };
        return true;
    }
    if ((line == "seek-") || (line == "-")) {
        out = h747::apps::player::PlayerCommand{
            .kind = PlayerCommandKind::seek_relative,
            .delta_percent = -10,
        };
        return true;
    }
    return false;
}

void poll_console_commands() noexcept {
    auto line = active_line_source().poll_line();
    if (!line) {
        return;
    }

    h747::apps::player::PlayerCommand command{};
    if (!command_from_line(*line, command)) {
        h747::console::write_line("player: commands: toggle next prev seek+ seek-");
        return;
    }

    active_runtime().dispatch(command);
    h747::console::write_line("player: command_ok");
}

void print_hex32(const char* label, const std::uint32_t value) {
    h747::console::write(label);
    h747::console::write_hex32(value);
}

void print_dec32(const char* label, const std::uint32_t value) {
    h747::console::write(label);
    h747::console::write_dec(value);
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

} // namespace

namespace h747::apps::player {

void init() {
    active_world().init();
    probe_resources_once();
    active_runtime().observe_board(capture_board_snapshot());
    print_raster_state("player");
    init(active_world(), active_runtime());
    print_raster_state("player");
}

void loop_once() noexcept {
    probe_resources_periodic();
    active_runtime().observe_board(capture_board_snapshot());
    poll_console_commands();
    loop_once(active_world(), active_runtime());
    static std::uint32_t last_present = 0U;
    const auto present = active_world().display().state().raw.present_count;
    if (present != last_present) {
        last_present = present;
        print_raster_state("player");
    }
}

} // namespace h747::apps::player
