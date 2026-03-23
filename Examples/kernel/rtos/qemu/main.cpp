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
    volatile u32 g_tick_mod = 1000;
    volatile u32 timer_hits = 0;
    volatile u32 queue_hits = 0;

    using Queue = charm::system::rtos::SpscQueue<u32, 16>;
    Queue g_queue{};
    volatile u32 task_a_hits = 0;
    volatile u32 task_b_hits = 0;

    struct UartCmsdk {
        static constexpr std::uint32_t base = 0x40004000u;
        static constexpr std::uint32_t data = base + 0x00u;
        static constexpr std::uint32_t state = base + 0x04u;
        static constexpr std::uint32_t ctrl = base + 0x08u;
        static constexpr std::uint32_t state_txbf = 1u << 1;
        static constexpr std::uint32_t ctrl_tx_enable = 1u << 0;
        static constexpr std::uint32_t ctrl_rx_enable = 1u << 1;

        static void init() noexcept {
            auto* reg = reinterpret_cast<volatile std::uint32_t*>(ctrl);
            *reg = ctrl_tx_enable | ctrl_rx_enable;
        }

        static void write_byte(char ch) noexcept {
            auto* status = reinterpret_cast<volatile std::uint32_t*>(state);
            auto* out = reinterpret_cast<volatile std::uint32_t*>(data);
            while ((*status & state_txbf) != 0u) {
            }
            *out = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
        }

        static void write(const char* text) noexcept {
            static bool inited = false;
            if (!inited) {
                init();
                inited = true;
            }
            if (!text) return;
            while (*text) {
                write_byte(*text++);
            }
        }
    };

    u64 clock_now_ms(void*) noexcept {
        return g_tick_ms;
    }

    void task_a(void*) noexcept {
        ++task_a_hits;
        const u32 value = task_a_hits;
        (void)g_queue.push(value);
        Scheduler::current().sleep_ms(10);
    }

    void task_b(void*) noexcept {
        ++task_b_hits;
        u32 value = 0;
        if (g_queue.pop(value)) {
            ++queue_hits;
        }
        Scheduler::current().sleep_ms(25);
    }

    void timer_tick(void*) noexcept {
        ++timer_hits;
        (void)Scheduler::current().schedule_after(250, &timer_tick, nullptr);
    }

    struct Demo {
        std::array<TaskSlot, 4> slots{};
        std::array<charm::system::rtos::TimerSlot, 4> timers{};
        Clock clock{};
        Scheduler scheduler;

        Demo() noexcept
            : scheduler(SchedulerConfig{
                  std::span<TaskSlot>(slots.data(), slots.size()),
                  std::span<charm::system::rtos::TimerSlot>(timers.data(), timers.size())
              }) {
            clock.reset(nullptr, ClockOps{&clock_now_ms, nullptr});
            bind(clock);
            Scheduler::bind(scheduler);
            (void)scheduler.create(&task_a, nullptr);
            (void)scheduler.create(&task_b, nullptr);
            (void)scheduler.schedule_after(250, &timer_tick, nullptr);
        }
    };
}

int main() {
    demo::Demo demo{};
    demo::g_tick_mod = 100;
    while (true) {
        demo::g_tick_ms += 1;
        demo.scheduler.run_once();
        if ((demo::g_tick_ms % demo::g_tick_mod) == 0u) {
            demo::UartCmsdk::write("rtos tick\n");
        }
    }
    return 0;
}
