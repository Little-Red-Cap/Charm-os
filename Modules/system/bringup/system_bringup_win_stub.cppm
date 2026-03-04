module;

#include <cstddef>
#include <utility>

export module charm.system.bringup.win_stub;

import charm.system.bringup;
import charm.system.clock;
import charm.system.reactor_pump;
import io.channel;
import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import platform.board.win_stub;
import platform.win.irq_guard;
import platform.win.time_source;
import platform.win.wakeup;
import util.core;
import util.error;

export namespace charm::system {
    struct PumpConfig : kernel::KernelConfig {
        static constexpr std::size_t priority_levels = 1;
        static constexpr std::size_t evtq_capacity = 8;
    };

    struct PumpCaps {
        using TimeSource = charm::system::ClockCaps::TimeSource;
        using IrqGuard = platform::win::SpinIrqGuard;
        using Wakeup = platform::win::NoopWakeup;
        using SwiTrigger = kernel::NoopSwiTrigger;
    };

    inline util::Result<void> bringup_minimal_win_stub() noexcept {
        auto caps = platform::board::win_stub::make_board_caps();
        using PumpTask = charm::system::ReactorPumpTask;
        using Registry = kernel::TaskRegistry<PumpTask>;
        Registry registry{};
        PumpCaps pump_caps{};
        auto created = kernel::make_scheduler<PumpConfig>(registry, pump_caps);
        auto running = kernel::start(std::move(created));
        const auto pump_id = Registry::id_of<PumpTask>();
        auto& pump = registry.get<PumpTask>();

        BringupMinimal<8, 16, 8, 64, 64> bringup{
            caps.uart1,
            caps.clock,
            pump,
            &charm::system::scheduler_post<decltype(running)>,
            &running,
            pump_id,
            8
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
