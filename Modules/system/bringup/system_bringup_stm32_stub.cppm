module;

#include <cstddef>
#include <new>
#include <utility>

export module charm.system.bringup.stm32_stub;

import charm.system.bringup;
import charm.system.caps;
import charm.system.reactor_pump;
import io.channel;
import input.pump;
import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import platform.board.stm32_stub;
import util.core;
import util.error;

export namespace charm::system {
    struct PumpConfig : kernel::KernelConfig {
        static constexpr std::size_t priority_levels = 1;
        static constexpr std::size_t evtq_capacity = 8;
    };

    using PumpCaps = charm::system::SystemCaps<
        kernel::NoopIrqGuard,
        kernel::NoopWakeup>;

    inline util::Result<void> bringup_minimal_stm32_stub() noexcept {
        auto caps = platform::board::stm32_stub::make_board_caps();
        using PumpTask = charm::system::ReactorPumpTask;
        using InputPumpTask = input::InputPumpTask;
        using Registry = kernel::TaskRegistry<PumpTask, InputPumpTask>;
        Registry registry{};
        PumpCaps pump_caps{};
        auto created = kernel::make_scheduler<PumpConfig>(registry, pump_caps);
        auto running = kernel::start(std::move(created));
        const auto pump_id = Registry::id_of<PumpTask>();
        const auto input_pump_id = Registry::id_of<InputPumpTask>();
        auto& pump = registry.get<PumpTask>();
        auto& input_pump = registry.get<InputPumpTask>();
        const auto input_desc = BringupMinimal<8, 16, 8, 64, 64>::make_input_desc(
            caps.input,
            input_pump,
            &input::scheduler_schedule_at<decltype(running)>,
            &running,
            input_pump_id);

        BringupMinimal<8, 16, 8, 64, 64> bringup{
            caps.uart1,
            caps.clock,
            caps.input,
            pump,
            &charm::system::scheduler_post<decltype(running)>,
            &running,
            pump_id,
            8,
            input_desc
        };
        auto r = bringup.start();
        if (!r) return r;

        auto* ch = bringup.registry().open_channel("io.uart1");
        if (!ch) return util::unexpected(util::Errc::noent);

        const char msg[] = "bringup ok\n";
        auto wr = ch->write(io::ByteView{
            reinterpret_cast<const util::u8*>(msg),
            sizeof(msg) - 1
        });
        if (!wr && wr.error() != util::Errc::would_block) {
            return util::unexpected(wr.error());
        }
        (void)running.run_once();
        return {};
    }
}
