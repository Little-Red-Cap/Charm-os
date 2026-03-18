#include <expected>

import out.api;
import charm.port;
import charm.system.clock;
import charm.system.time;
import player.stm32h7.system_entry;

extern "C" {
void MX_I2S1_Init(void);
}

int main(void) {
    auto kit = charm::port::init();
    charm::system::Clock clock{kit.time_ctx, charm::system::ClockOps{&charm::port::now_ms, nullptr}};
    charm::system::time::bind(clock);
    MX_I2S1_Init();
    out::Scope scope{kit.console};
    out::println<"boot: system main">();

    system_run(kit.console.ctx);
}
