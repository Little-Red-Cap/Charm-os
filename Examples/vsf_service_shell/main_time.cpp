#include <cstdint>
#include <cstdio>

import util.core;
import hal_core;
import hal_time;
import shell_time;
import shell_core;
import shell_stdio;

struct DummyTime {
    using Tick = hal::tick_t;
    static Tick now() noexcept { return 123; }
};

struct DummyDelay {
    static void delay_ms(hal::tick_t) noexcept {}
};

static util::usize console_write(void*, shell::Buffer buf) noexcept {
    return std::fwrite(buf.data, 1, buf.size, stdout);
}

int main() {
    shell::Console con = shell::make_console(&console_write);
    auto now = shell::TimeApi<DummyTime>::now();
    (void)now;
    shell::DelayApi<DummyDelay>::delay_ms(1);
    (void)shell::write(con, "[shell_time] ok\n");
    return 0;
}
