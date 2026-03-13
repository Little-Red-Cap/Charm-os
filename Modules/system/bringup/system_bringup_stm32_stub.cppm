module;

#include <cstddef>
#include <new>
#include <utility>

export module charm.system.bringup.stm32_stub;

import charm.system.bringup;
import charm.system.app_host;
import charm.system.caps;
import charm.system.reactor_pump;
import io.channel;
import kernel.capabilities;
import platform.board.stm32_stub;
import util.core;
import util.error;

export namespace charm::system {
    using PumpCaps = charm::system::SystemCaps<
        kernel::NoopIrqGuard,
        kernel::NoopWakeup>;

    inline util::Result<void> bringup_minimal_stm32_stub() noexcept {
        auto caps = platform::board::stm32_stub::make_board_caps();
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
}
