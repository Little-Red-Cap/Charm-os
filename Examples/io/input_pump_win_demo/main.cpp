#include <cstdint>
#include <cstddef>
#include <cstdio>

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
}

int main() {
    StdoutSink sink{};
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
    const auto input_desc = charm::system::BringupMinimal<8, 16, 8, 64, 64>::make_input_desc(
        caps.input,
        input_pump,
        &input::scheduler_schedule_at<decltype(running)>,
        &running,
        input_pump_id);

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

    for (int i = 0; i < 8; ++i) {
        (void)running.run_once();
    }
    return 0;
}
