#include <array>
#include <cstdint>
#include <span>

import charm.system.clock;
import charm.system.rtos;
import charm.system.time;
import util.core;

namespace demo {
    using charm::system::Clock;
    using charm::system::ClockOps;
    using charm::system::rtos::Scheduler;
    using charm::system::rtos::SchedulerConfig;
    using charm::system::rtos::TaskSlot;
    using charm::system::rtos::TaskState;
    using charm::system::time::bind;
    using util::u32;
    using util::u64;

    volatile u64 g_tick_ms = 0;
    volatile u32 task_a_hits = 0;
    volatile u32 task_b_hits = 0;

    inline void semihosting_write0(const char* text) noexcept {
        constexpr std::uint32_t kSysWrite0 = 0x04;
        register std::uint32_t r0 asm("r0") = kSysWrite0;
        register const char* r1 asm("r1") = text;
        asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
    }

    u64 clock_now_ms(void*) noexcept {
        return g_tick_ms;
    }

    void task_a(void*) noexcept {
        ++task_a_hits;
        Scheduler::current().sleep_ms(10);
    }

    void task_b(void*) noexcept {
        ++task_b_hits;
        Scheduler::current().sleep_ms(25);
    }

    struct Demo {
        std::array<TaskSlot, 4> slots{};
        Clock clock{};
        Scheduler scheduler;

        Demo() noexcept
            : scheduler(SchedulerConfig{std::span<TaskSlot>(slots.data(), slots.size())}) {
            clock.reset(nullptr, ClockOps{&clock_now_ms, nullptr});
            bind(clock);
            Scheduler::bind(scheduler);
            (void)scheduler.create(&task_a, nullptr);
            (void)scheduler.create(&task_b, nullptr);
        }
    };
}

int main() {
    demo::Demo demo{};
    while (true) {
        demo::g_tick_ms += 1;
        demo.scheduler.run_once();
        if ((demo::g_tick_ms % 1000u) == 0u) {
            demo::semihosting_write0("rtos tick\n");
        }
    }
    return 0;
}
