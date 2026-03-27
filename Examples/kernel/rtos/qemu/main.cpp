#include <array>
#include <cstdint>
#include <cstdio>
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
    using charm::system::rtos::SchedulerLockGuard;
    using charm::system::rtos::TaskSlot;
    using charm::system::rtos::TaskState;
    using charm::system::rtos::TaskId;
    using charm::system::rtos::PreemptGuard;
    using charm::system::rtos::bind_critical;
    using charm::system::rtos::CriticalGuard;
    using charm::system::rtos::EventFlags;
    using charm::system::rtos::MessageQueue;
    using charm::system::time::bind;
    using util::u32;
    using util::u64;

    volatile u64 g_tick_ms = 0;
    volatile u32 g_tick_mod = 1000;
    volatile u32 timer_hits = 0;
    volatile u32 timer_hits_hard = 0;
    volatile u32 queue_hits = 0;

    using Queue = charm::system::rtos::SpscQueue<u32, 16>;
    Queue g_queue{};
    EventFlags<4> g_flags{};
    MessageQueue<u32, 8, 4> g_mq{};
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

    void disable_irqs() noexcept {}
    void enable_irqs() noexcept {}

    void timer_tick(void*) noexcept;
    void timer_tick_hard(void*) noexcept;

    void task_a(void*) noexcept {
        ++task_a_hits;
        const u32 value = task_a_hits;
        (void)g_queue.push(value);
        if ((task_a_hits % 4u) == 0u) {
            g_flags.set(0x1);
            std::array<u32, 2> batch{value, static_cast<u32>(value + 1)};
            (void)g_mq.send_batch(std::span<const u32>(batch.data(), batch.size()));
        }
        if ((task_a_hits % 9u) == 0u) {
            SchedulerLockGuard guard{Scheduler::current()};
            (void)Scheduler::current().schedule_after(5, &timer_tick, nullptr);
        }
        Scheduler::current().sleep_ms(10);
    }

    void task_b(void*) noexcept {
        ++task_b_hits;
        u32 value = 0;
        if (g_queue.pop(value)) {
            ++queue_hits;
        }
        if ((task_b_hits % 5u) == 0u) {
            (void)g_flags.wait_any(0x1, 20);
        }
        std::array<u32, 2> out{};
        (void)g_mq.recv_batch(std::span<u32>(out.data(), out.size()));
        if ((task_b_hits % 11u) == 0u) {
            PreemptGuard guard{};
            (void)Scheduler::current().schedule_after(7, &timer_tick_hard, nullptr,
                charm::system::rtos::TimerSlot::Kind::hard);
        }
        Scheduler::current().sleep_ms(25);
    }

    void timer_tick(void*) noexcept {
        ++timer_hits;
        (void)Scheduler::current().schedule_after(250, &timer_tick, nullptr);
    }

    void timer_tick_hard(void*) noexcept {
        ++timer_hits_hard;
        (void)Scheduler::current().schedule_after(
            500,
            &timer_tick_hard,
            nullptr,
            charm::system::rtos::TimerSlot::Kind::hard
        );
    }

    struct Demo {
        std::array<TaskSlot, 4> slots{};
        std::array<charm::system::rtos::TimerSlot, 4> timers{};
        std::array<TaskId, 4> delays{};
        std::array<charm::system::rtos::TraceEvent, 64> trace{};
        Clock clock{};
        Scheduler scheduler;
        u32 last_stats_tick{0};

        Demo() noexcept
            : scheduler(SchedulerConfig{
                  std::span<TaskSlot>(slots.data(), slots.size()),
                  std::span<charm::system::rtos::TimerSlot>(timers.data(), timers.size()),
                  1,
                  std::span<TaskId>(delays.data(), delays.size()),
                  true,
                  true,
                  std::span<charm::system::rtos::TraceEvent>(trace.data(), trace.size())
              }) {
            clock.reset(nullptr, ClockOps{&clock_now_ms, nullptr});
            bind(clock);
            Scheduler::bind(scheduler);
            bind_critical(&disable_irqs, &enable_irqs);
            g_flags.set_auto_clear_any(EventFlags<4>::AutoClearMode::match_any);
            g_flags.set_auto_clear_all(EventFlags<4>::AutoClearMode::mask);
            (void)scheduler.create(&task_a, nullptr, 1, 1);
            (void)scheduler.create(&task_b, nullptr, 0, 1);
            scheduler.freeze_task_creation();
            (void)scheduler.schedule_after(250, &timer_tick, nullptr);
            (void)scheduler.schedule_after(
                500,
                &timer_tick_hard,
                nullptr,
                charm::system::rtos::TimerSlot::Kind::hard
            );
        }

        void dump_stats(u32 now_tick) noexcept {
            if (now_tick - last_stats_tick < 1000u) return;
            last_stats_tick = now_tick;
            const auto st = scheduler.stats();
            char buf[160]{};
            const int n = std::snprintf(
                buf,
                sizeof(buf),
                "rtos stats ready=%u run=%u blk=%u slp=%u lock=%u delay=%u sw=%u y=%u b=%u w=%u tmo=%u prio=%u\n",
                static_cast<unsigned>(st.ready),
                static_cast<unsigned>(st.running),
                static_cast<unsigned>(st.blocked),
                static_cast<unsigned>(st.sleeping),
                static_cast<unsigned>(st.lock_depth),
                static_cast<unsigned>(st.delay_count),
                static_cast<unsigned>(st.switch_count),
                static_cast<unsigned>(st.yield_count),
                static_cast<unsigned>(st.block_count),
                static_cast<unsigned>(st.wake_count),
                static_cast<unsigned>(st.timeout_count),
                static_cast<unsigned>(st.last_pick_prio));
            if (n > 0) {
                UartCmsdk::write(buf);
            }
            if (!scheduler.self_check()) {
                UartCmsdk::write("rtos check failed\n");
            }
        }
    };
}

int main() {
    demo::Demo demo{};
    demo::g_tick_mod = 100;
    while (true) {
        demo::g_tick_ms += 1;
        demo::g_flags.poll_wake(demo.scheduler);
        demo::g_mq.poll_wake(demo.scheduler);
        demo.scheduler.tick();
        demo.scheduler.run_once();
        demo.dump_stats(static_cast<demo::u32>(demo::g_tick_ms));
        if ((demo::g_tick_ms % demo::g_tick_mod) == 0u) {
            {
                demo::CriticalGuard guard{};
                demo::UartCmsdk::write("rtos tick\n");
            }
        }
    }
    return 0;
}
