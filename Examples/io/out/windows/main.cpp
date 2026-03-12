#include <cstdint>

import charm.system.bringup.console;
import out.channel;
import out.api;
import util.core;
import platform.board.win_stub;

extern "C" void example(out::channel_sink& sink, util::u64 now_ms);

int main() {
    auto caps = platform::board::win_stub::make_console_caps();
    charm::system::BringupConsole<8, 12, 8, 64, 64> bringup{caps};
    auto r = bringup.start();
    if (!r) return 1;
    auto* ch = bringup.console_channel();
    if (!ch) return 1;
    auto sink = out::make_channel_sink(*ch);
    example(sink, bringup.clock().now_ms());
    return 0;
}
