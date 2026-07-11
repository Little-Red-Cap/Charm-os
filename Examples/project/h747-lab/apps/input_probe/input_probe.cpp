#include "input_probe.h"

#include "console_service.hpp"
#include "input_service.hpp"
#include "port.h"
#include "stm32h7xx_hal.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

import out.core;
import out.format;

namespace h747::apps::input_probe {
namespace {

using namespace std::literals::string_view_literals;

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

h747::console::ConsoleLineSource line_source;
h747::input::Service input;
std::uint32_t last_tick_ms = 0U;
std::uint32_t alive_count = 0U;
std::int32_t encoder1_accum = 0;
std::int32_t encoder2_accum = 0;
bool monitor_enabled = false;

constexpr bool printable_ascii(const std::uint8_t value) noexcept {
    return (value >= 32U) && (value < 127U);
}

void print_prompt() {
    emit<"\r\nh747-input> ">();
}

void print_help() {
    emit<"Commands:\n">();
    emit<"  help        - Show help\n">();
    emit<"  status      - Print touch and encoder evidence\n">();
    emit<"  touch probe - Reset and probe GT970/GT9xx over I2C4\n">();
    emit<"  touch debug - Print GT9xx register snapshot\n">();
    emit<"  touch reset14/reset5d - Reset GT9xx with address-select INT level\n">();
    emit<"  touch cfg luat - Write Luat GT9157 config table\n">();
    emit<"  touch cfg luat reset - Write Luat config then soft-reset\n">();
    emit<"  touch softreset - Write GT9157 command 0x02 then wake\n">();
    emit<"  monitor on/off - Enable or disable periodic status output\n">();
    emit<"  reboot      - Reboot\n">();
}

void print_version(const input_touch_snapshot_t& touch) {
    emit<" version_hex=">();
    for (std::uint32_t index = 0; index < sizeof(touch.version); ++index) {
        if (index != 0U) {
            emit<" ">();
        }
        emit<"{:02X}">(static_cast<unsigned>(touch.version[index]));
    }

    emit<" version_ascii=\"">();
    for (const auto value : touch.version) {
        h747::console::write_char(printable_ascii(value) ? static_cast<char>(value) : '.');
    }
    emit<"\"">();
}

void print_encoder(const std::string_view name,
                   const input_encoder_snapshot_t& encoder,
                   const std::int32_t accumulated) {
    emit<"{}: count={} delta={} detent={} accum={} pressed={} level={} queue={} last_ab=0x{:02X}\n">(
        name,
        encoder.count,
        encoder.delta_counts,
        static_cast<int>(encoder.detent_delta),
        accumulated,
        encoder.button_pressed,
        encoder.button_level,
        encoder.phase_queue_depth,
        static_cast<unsigned>(encoder.last_ab));
}

void print_touch(const input_touch_snapshot_t& touch, const std::uint8_t probe_attempted) {
    emit<"touch: attempted={} ready={} detected={} down={} addr=0x{:02X} contacts={} int={} rst={} id={} status=0x{:02X} hal=0x{:08X} i2c_err=0x{:08X} i2c_state=0x{:08X} probe0=0x{:02X}:0x{:08X} probe1=0x{:02X}:0x{:08X} max={}x{} xy={},{} pressure={}">(
        probe_attempted,
        touch.ready,
        touch.detected,
        touch.down,
        static_cast<unsigned>(touch.addr7),
        touch.contacts,
        touch.int_level,
        touch.reset_pin_level,
        touch.last_id,
        static_cast<unsigned>(touch.last_status),
        touch.last_hal_status,
        touch.i2c_error_code,
        touch.i2c_state,
        static_cast<unsigned>(touch.probe_addr0),
        touch.probe_status0,
        static_cast<unsigned>(touch.probe_addr1),
        touch.probe_status1,
        touch.max_x,
        touch.max_y,
        touch.x,
        touch.y,
        touch.pressure);
    print_version(touch);
    emit<"\n">();
}

void print_hex_bytes(const char* label, const std::uint8_t* data, const std::uint32_t size) {
    emit<" {}=">(label);
    for (std::uint32_t index = 0; index < size; ++index) {
        if (index != 0U) {
            emit<",">();
        }
        emit<"0x{:02X}">(static_cast<unsigned>(data[index]));
    }
}

void print_touch_debug() {
    input_touch_debug_snapshot_t dbg{};
    const auto ok = input_touch_debug_snapshot(&dbg);
    emit<"touch_debug ok={} ready={} addr=0x{:02X} cmd=0x{:02X} status=0x{:02X} int={} rst={} hal=0x{:08X}/0x{:08X}/0x{:08X}/0x{:08X} i2c=0x{:08X}/0x{:08X}">(
        ok,
        dbg.ready,
        static_cast<unsigned>(dbg.addr7),
        static_cast<unsigned>(dbg.command),
        static_cast<unsigned>(dbg.status),
        dbg.int_level,
        dbg.reset_pin_level,
        dbg.command_hal_status,
        dbg.status_hal_status,
        dbg.version_hal_status,
        dbg.config_hal_status,
        dbg.i2c_error_code,
        dbg.i2c_state);
    print_hex_bytes("ver", dbg.version, sizeof(dbg.version));
    print_hex_bytes("cfg", dbg.config, sizeof(dbg.config));
    print_hex_bytes("point", dbg.point_data, sizeof(dbg.point_data));
    emit<"\n">();
}

void print_status() {
    const auto state = input.snapshot().raw;
    const auto rx = h747::console::rx_stats();
    emit<"input: profile=input_probe tick={} initialized={} encoder_started={} touch_probe_attempted={}\n">(
        h747::port::tick_ms(),
        state.initialized,
        state.encoder_started,
        state.touch_probe_attempted);
    emit<"console_rx: bytes={} lines={} overrun={} last=0x{:02X}\n">(
        rx.bytes,
        rx.lines,
        rx.overrun_clears,
        static_cast<unsigned>(rx.last_byte));
    print_touch(state.touch, state.touch_probe_attempted);
    print_encoder("encoder1"sv, state.encoder1, encoder1_accum);
    print_encoder("encoder2"sv, state.encoder2, encoder2_accum);
}

void poll_input() {
    input.poll();
    const auto state = input.snapshot().raw;
    encoder1_accum += state.encoder1.detent_delta;
    encoder2_accum += state.encoder2.detent_delta;
}

void run_touch_probe() {
    const bool ok = input.probe_touch();
    emit<"touch_probe: {}\n">(ok ? "ok" : "failed");
    print_status();
}

void run_touch_reset(const std::uint8_t addr7) {
    const auto ok = input_touch_debug_reset_address(addr7);
    emit<"touch_reset addr=0x{:02X} {}\n">(static_cast<unsigned>(addr7), ok ? "ok" : "failed");
    print_touch_debug();
}

void run_touch_cfg_luat(const bool do_reset) {
    std::uint8_t checksum = 0U;
    const auto load_ok = input_touch_debug_load_luat_config(720U, 1280U, &checksum);
    const auto reset_ok = (do_reset && load_ok != 0U) ? input_touch_debug_soft_reset() : 0U;
    emit<"touch_cfg_luat: load={} reset={} checksum=0x{:02X}\n">(
        load_ok,
        reset_ok,
        static_cast<unsigned>(checksum));
    print_touch_debug();
}

void run_touch_softreset() {
    const auto ok = input_touch_debug_soft_reset();
    emit<"touch_softreset: {}\n">(ok ? "ok" : "failed");
    print_touch_debug();
}

void handle_command(const std::string_view line) {
    if (line.empty()) {
        return;
    }
    if (line == "help"sv) {
        print_help();
    } else if (line == "status"sv) {
        print_status();
    } else if (line == "touch probe"sv) {
        run_touch_probe();
    } else if (line == "touch debug"sv) {
        print_touch_debug();
    } else if (line == "touch reset14"sv) {
        run_touch_reset(0x14U);
    } else if (line == "touch reset5d"sv) {
        run_touch_reset(0x5DU);
    } else if (line == "touch cfg luat"sv) {
        run_touch_cfg_luat(false);
    } else if (line == "touch cfg luat reset"sv) {
        run_touch_cfg_luat(true);
    } else if (line == "touch softreset"sv) {
        run_touch_softreset();
    } else if (line == "monitor on"sv) {
        monitor_enabled = true;
        emit<"monitor: on\n">();
    } else if (line == "monitor off"sv) {
        monitor_enabled = false;
        emit<"monitor: off\n">();
    } else if (line == "reboot"sv) {
        emit<"rebooting...\n">();
        HAL_Delay(20U);
        NVIC_SystemReset();
    } else {
        emit<"unknown command\n">();
    }
}

} // namespace

void init() {
    emit<"input_probe: init role=touch_input gt970_i2c4 + dual_encoder\n">();
    const bool touch_ok = input.probe_touch();
    emit<"input_probe: touch initial_probe={}\n">(touch_ok ? "ok" : "failed");
    print_help();
    print_status();
    print_prompt();
    last_tick_ms = h747::port::tick_ms();
}

void loop_once() noexcept {
    poll_input();

    if (const auto line = line_source.poll_line()) {
        handle_command(*line);
        print_prompt();
    }

    const std::uint32_t now = h747::port::tick_ms();
    if (monitor_enabled && ((now - last_tick_ms) >= 1000U)) {
        last_tick_ms = now;
        ++alive_count;
        emit<"input_probe: alive tick={} e1_accum={} e2_accum={}\n">(
            alive_count,
            encoder1_accum,
            encoder2_accum);
        print_status();
        print_prompt();
    }
}

} // namespace h747::apps::input_probe
