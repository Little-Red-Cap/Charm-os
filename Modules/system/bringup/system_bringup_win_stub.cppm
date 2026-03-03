module;

export module charm.system.bringup.win_stub;

import charm.system.bringup;
import io.channel;
import platform.board.win_stub;
import util.core;
import util.error;

export namespace charm::system {
    inline util::Result<void> bringup_minimal_win_stub() noexcept {
        auto caps = platform::board::win_stub::make_board_caps();
        BringupMinimal<8, 16, 8, 64, 64> bringup{caps.uart1};
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

        bringup.reactor().drain();
        return {};
    }
}
