#include <new>
#include <type_traits>

import player.stm32h7.fs_demo;
import player.stm32h7.audio_mp3_demo;
import out.api;

bool player_app_boot_and_run() noexcept {
    static out::port::console_sink uart_sink;
    out::println<"boot: init ok">(uart_sink);

    if (!fs_boot_init()) {
        return false;
    }

    audio_mp3_demo_run();
    return true;
}
