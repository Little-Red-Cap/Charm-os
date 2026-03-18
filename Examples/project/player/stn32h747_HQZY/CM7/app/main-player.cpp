#include <expected>

import out.api;
import charm.port;
import charm.system.clock;
import charm.system.time;
import player.stm32h7.player_entry;
import util.core;

extern "C" void Error_Handler(void);

int main(void) {
    auto kit = charm::port::init();
    charm::system::Clock clock{kit.time_ctx, charm::system::ClockOps{&charm::port::now_ms, nullptr}};
    charm::system::time::bind(clock);
    out::Scope scope{kit.console};

    player_boot();
    out::println<"boot: player main">();
    if (!player_init_fs()) {
        out::println<"boot: fs mount failed">();
    } else {
        out::println<"boot: fs mount ok">();
    }
    player_init_audio();
    out::println<"audio: i2s init ok">();
    if (!player_init_usb()) {
        out::println<"usb: start failed">();
        Error_Handler();
    }
    out::println<"usb: pcd start ok">();
    player_loop();
}
