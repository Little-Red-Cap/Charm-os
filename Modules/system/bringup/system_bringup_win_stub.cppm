module;

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

export module charm.system.bringup.win_stub;

import charm.system.bringup;
import charm.system.app_host;
import charm.system.caps;
import charm.system.init_canopen;
import charm.system.reactor_pump;
import canopen.nmt;
import canopen.od;
import canopen.pump;
import canopen.sdo;
import io.channel;
import io.registry;
import init.node;
import init.plan;
import platform.board.win_stub;
import platform.win.irq_guard;
import platform.win.time_source;
import platform.win.wakeup;
import util.core;
import util.error;

export namespace charm::system {
    using PumpCaps = charm::system::SystemCaps<
        platform::win::SpinIrqGuard,
        platform::win::NoopWakeup>;

    inline util::Result<void> bringup_minimal_win_stub() noexcept {
        auto caps = platform::board::win_stub::make_board_caps();
        PumpCaps pump_caps{};
        AppHost<PumpCaps> host{pump_caps};
        BringupMinimal<8, 16, 8, 64, 64> bringup{caps, host};
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
        (void)host.run_once();
        return {};
    }

    inline util::Result<void> bringup_minimal_win_stub_canopen() noexcept {
        auto caps = platform::board::win_stub::make_board_caps();
        PumpCaps pump_caps{};
        using Host = AppHost<PumpCaps, AppHostConfig, canopen::CanopenPumpTask>;
        Host host{pump_caps};
        auto& canopen_pump = host.task<canopen::CanopenPumpTask>();
        constexpr auto canopen_pump_id = Host::task_id<canopen::CanopenPumpTask>();
        BringupMinimal<8, 16, 8, 64, 64> bringup{caps, host};

        util::u32 value = 0x12345678u;
        std::array<canopen::Entry, 1> entries{
            canopen::make_entry(0x2000, 0x00, value, canopen::Access::read_write)
        };
        canopen::ObjectDictionary od{entries};
        canopen::SdoServerConfig sdo_cfg{};
        sdo_cfg.node_id = 1;
        canopen::SdoServer sdo{od, sdo_cfg};
        canopen::NmtConfig nmt_cfg{};
        nmt_cfg.node_id = 1;
        canopen::NmtNode nmt{nmt_cfg};

        auto& running = host.scheduler();
        using Scheduler = std::remove_reference_t<decltype(running)>;
        charm::system::CanopenInitChain<io::Registry<8>, Scheduler> canopen_chain{
            bringup.registry(),
            sdo,
            nmt,
            bringup.clock(),
            running,
            canopen_pump,
            canopen_pump_id
        };

        auto r = bringup.start_plan(
            canopen_chain.plan(),
            static_cast<util::u32>(init::Runlevel::all),
            init::Phase::app);
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
        (void)host.run_once();
        return {};
    }
}
