#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <thread>

import charm.foundation;
import charm.runtime;
import charm.system.bringup;
import charm.system.bringup.win_stub;
import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import out.api;
import platform.board.win_stub;
import platform.win.irq_guard;
import platform.win.time_source;
import platform.win.wakeup;
import util.expected;

namespace {
    struct StdoutSink {
        out::result<std::size_t> write(out::bytes b) noexcept {
            if (std::fwrite(b.data(), 1, b.size(), stdout) != b.size()) {
                return util::unexpected(out::errc::io_error);
            }
            return out::ok(b.size());
        }
        out::result<std::size_t> flush() noexcept {
            return std::fflush(stdout) == 0 ? out::ok<std::size_t>(0u)
                                            : util::unexpected(out::errc::io_error);
        }
    };

    struct ScriptedInput {
        util::u64 start_us{0};
        bool enabled{true};
    };

    bool is_down(void* ctx, input::Button b) noexcept {
        auto* s = static_cast<ScriptedInput*>(ctx);
        if (!s || !s->enabled) return false;
        if (b != input::Button::Enter) return false;
        const auto now = platform::win::SteadyClock::now();
        const auto ms = (now - s->start_us) / 1000u;
        return ((ms / 300u) % 2u) != 0u;
    }

    input::PointerRaw read_pointer(void* ctx) noexcept {
        (void)ctx;
        return input::PointerRaw{.down = false, .x = -1, .y = -1, .id = 0};
    }

    input::AxisRaw read_axis(void* ctx) noexcept {
        (void)ctx;
        return input::AxisRaw{};
    }

    std::optional<std::uint8_t> pop_encoder_ab(void* ctx) noexcept {
        (void)ctx;
        return std::nullopt;
    }

    struct RawPrintCtx {
        StdoutSink* sink{nullptr};
    };

    bool on_raw(void* ctx, const input::RawInputEvent& ev) noexcept {
        auto* c = static_cast<RawPrintCtx*>(ctx);
        if (!c || !c->sink) return true;
        int code = 0;
        int value = 0;
        if (ev.type == input::RawInputEventType::Button) {
            code = static_cast<int>(ev.button);
            value = ev.pressed ? 1 : 0;
        }
        (void)out::println<"[raw] t={} type={} code={} value={}">(
            *c->sink,
            ev.ms,
            static_cast<int>(ev.type),
            code,
            value);
        return true;
    }
}

int main(int argc, char** argv) {
    StdoutSink sink{};
    bool enable_input = true;
    if (argc > 1 && std::strcmp(argv[1], "--no-input") == 0) {
        enable_input = false;
    }
    auto caps = platform::board::win_stub::make_board_caps();
    using PumpTask = charm::system::ReactorPumpTask;
    using InputPumpTask = input::InputPumpTask;
    using Registry = kernel::TaskRegistry<PumpTask, InputPumpTask>;
    Registry registry{};
    charm::system::PumpCaps pump_caps{};
    auto created = kernel::make_scheduler<charm::system::PumpConfig>(registry, pump_caps);
    auto running = kernel::start(std::move(created));
    const auto pump_id = Registry::id_of<PumpTask>();
    const auto input_pump_id = Registry::id_of<InputPumpTask>();
    auto& pump = registry.get<PumpTask>();
    auto& input_pump = registry.get<InputPumpTask>();
    ScriptedInput scripted{platform::win::SteadyClock::now(), enable_input};
    const hal::RawInputDriver kDriver{
        .ctx = &scripted,
        .is_down = &is_down,
        .read_pointer = &read_pointer,
        .read_axis = &read_axis,
        .pop_encoder_ab = &pop_encoder_ab
    };
    platform::board::InputDesc input_desc_caps = caps.input;
    input_desc_caps.driver = &kDriver;

    RawPrintCtx print_ctx{&sink};
    const auto input_desc = charm::system::BringupMinimal<8, 16, 8, 64, 64>::make_input_desc(
        input_desc_caps,
        input_pump,
        &input::scheduler_schedule_at<decltype(running)>,
        &running,
        input_pump_id,
        &on_raw,
        &print_ctx);

    charm::system::BringupMinimal<8, 16, 8, 64, 64> bringup{
        caps.uart1,
        caps.clock,
        caps.input,
        caps.spi1,
        caps.i2c1,
        caps.can0,
        pump,
        &charm::system::scheduler_post<decltype(running)>,
        &running,
        pump_id,
        8,
        input_desc
    };

    auto r = bringup.start();
    if (!r) {
        (void)out::println<"[input_pump] bringup failed err={}">(sink, static_cast<int>(r.error()));
        return 1;
    }
    (void)out::println<"[input_pump] bringup ok">(sink);

    const auto start = platform::win::SteadyClock::now();
    while ((platform::win::SteadyClock::now() - start) < 500000u) {
        (void)running.run_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}
