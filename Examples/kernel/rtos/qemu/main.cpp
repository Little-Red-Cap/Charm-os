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
    using charm::system::rtos::bind_port;
    using charm::system::rtos::CriticalGuard;
    using charm::system::rtos::EventFlags;
    using charm::system::rtos::IsrGuard;
    using charm::system::rtos::IsrPollEntry;
    using charm::system::rtos::MessageQueue;
    using charm::system::rtos::Semaphore;
    using charm::system::rtos::RtosPort;
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
    EventFlags<2> g_timeout_flags{};
    MessageQueue<u32, 8, 4> g_mq{};
    Semaphore<4> g_sem{};
    Semaphore<2> g_cancel_sem{};
    volatile u32 task_a_hits = 0;
    volatile u32 task_b_hits = 0;
    volatile u32 cancel_hits = 0;
    volatile u32 cleanup_hits = 0;

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
    void port_yield_impl() noexcept {}
    void port_start_impl() noexcept {}
    void port_setup_tick_impl(u32) noexcept {}

    void poll_flags(void* ctx, Scheduler& sched) noexcept {
        auto* flags = static_cast<EventFlags<4>*>(ctx);
        if (!flags) return;
        flags->poll_wake(sched);
    }

    void poll_mq(void* ctx, Scheduler& sched) noexcept {
        auto* mq = static_cast<MessageQueue<u32, 8, 4>*>(ctx);
        if (!mq) return;
        mq->poll_wake(sched);
    }

    void poll_sem(void* ctx, Scheduler& sched) noexcept {
        auto* sem = static_cast<Semaphore<4>*>(ctx);
        if (!sem) return;
        sem->poll_wake(sched);
    }

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
        if ((task_a_hits % 13u) == 0u) {
            g_cancel_sem.cancel_waiters(Scheduler::current());
        }
        if ((task_a_hits % 17u) == 0u) {
            Scheduler::current().cleanup_all();
            ++cleanup_hits;
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
        if ((task_b_hits % 6u) == 0u) {
            if (g_timeout_flags.wait_any(0x1, 2) != charm::system::rtos::WaitResult::ok) {
                return;
            }
        }
        if ((task_b_hits % 7u) == 0u) {
            if (g_sem.wait(15) != charm::system::rtos::WaitResult::ok) {
                return;
            }
        }
        if ((task_b_hits % 9u) == 0u) {
            const auto result = g_cancel_sem.wait(50);
            if (result == charm::system::rtos::WaitResult::cancelled) {
                ++cancel_hits;
                return;
            }
            if (result != charm::system::rtos::WaitResult::ok) {
                return;
            }
        }
        std::array<u32, 2> out{};
        (void)g_mq.recv_batch(std::span<u32>(out.data(), out.size()));
        if ((task_b_hits % 11u) == 0u) {
            PreemptGuard guard{};
            {
                // 模拟 ISR 调度硬定时器
                IsrGuard isr{};
                (void)Scheduler::current().schedule_after(7, &timer_tick_hard, nullptr,
                    charm::system::rtos::TimerSlot::Kind::hard);
            }
        }
        Scheduler::current().sleep_ms(25);
    }

    void timer_tick(void*) noexcept {
        ++timer_hits;
        (void)Scheduler::current().schedule_after(250, &timer_tick, nullptr);
    }

    void timer_tick_hard(void*) noexcept {
        ++timer_hits_hard;
        {
            IsrGuard guard{};
            g_flags.set_isr(0x2);
            const u32 isr_value = timer_hits_hard;
            (void)g_mq.try_send_isr(isr_value);
            (void)g_sem.post_isr();
        }
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
        std::array<IsrPollEntry, 3> pollers{};
        std::array<charm::system::rtos::TraceEvent, 64> trace{};
        Clock clock{};
        Scheduler scheduler;
        u32 last_stats_tick{0};
        u32 last_timeout_count{0};

        Demo() noexcept
            : pollers{IsrPollEntry{&g_flags, &poll_flags},
                      IsrPollEntry{&g_mq, &poll_mq},
                      IsrPollEntry{&g_sem, &poll_sem}},
              scheduler(SchedulerConfig{
                  std::span<TaskSlot>(slots.data(), slots.size()),
                  std::span<charm::system::rtos::TimerSlot>(timers.data(), timers.size()),
                  1,
              std::span<TaskId>(delays.data(), delays.size()),
              true,
              true,
              false,
              true,
              std::span<charm::system::rtos::TraceEvent>(trace.data(), trace.size()),
              charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::run) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::block) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::wake) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::timeout) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::task_violation) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::isr_violation) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::pi_detected) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::lock_reenter) |
                  charm::system::rtos::trace_bit(charm::system::rtos::TraceKind::pi_boost),
              std::span<IsrPollEntry>(pollers.data(), pollers.size())
          }) {
            clock.reset(nullptr, ClockOps{&clock_now_ms, nullptr});
            bind(clock);
            Scheduler::bind(scheduler);
            bind_port(RtosPort{
                &disable_irqs,
                &enable_irqs,
                nullptr,
                &port_yield_impl,
                &port_start_impl,
                &port_setup_tick_impl
            });
            g_flags.set_auto_clear_any(EventFlags<4>::AutoClearMode::match_any);
            g_flags.set_auto_clear_all(EventFlags<4>::AutoClearMode::mask);
            (void)scheduler.create(&task_a, nullptr, 1, 1);
            (void)scheduler.create(&task_b, nullptr, 0, 1);
            scheduler.freeze_task_creation();
            (void)scheduler.schedule_after(250, &timer_tick, nullptr);
            {
                // 模拟 ISR 调度硬定时器
                IsrGuard isr{};
                (void)scheduler.schedule_after(
                    500,
                    &timer_tick_hard,
                    nullptr,
                    charm::system::rtos::TimerSlot::Kind::hard
                );
            }
            scheduler.enter_runtime();
        }

        void dump_stats(u32 now_tick) noexcept {
            if (now_tick - last_stats_tick < 1000u) return;
            last_stats_tick = now_tick;
            const auto st = scheduler.stats();
            char buf[160]{};
            const int n = std::snprintf(
                buf,
                sizeof(buf),
                "rtos stats ready=%u run=%u blk=%u slp=%u lock=%u delay=%u sw=%u y=%u b=%u w=%u tmo=%u prio=%u den=%u rden=%u tden=%u rtden=%u isr=%u task=%u reent=%u\n",
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
                static_cast<unsigned>(st.last_pick_prio),
                static_cast<unsigned>(st.create_denied),
                static_cast<unsigned>(st.runtime_create_denied),
                static_cast<unsigned>(st.timer_create_denied),
                static_cast<unsigned>(st.runtime_timer_denied),
                static_cast<unsigned>(st.isr_violation_count),
                static_cast<unsigned>(st.task_violation_count),
                static_cast<unsigned>(st.lock_reenter_count));
        if (n > 0) {
            UartCmsdk::write(buf);
        }
        if (cancel_hits > 0) {
            UartCmsdk::write("rtos cancelled\n");
        }
        if (st.timeout_count > last_timeout_count) {
            last_timeout_count = st.timeout_count;
            UartCmsdk::write("rtos timeout\n");
        }
        if (cleanup_hits > 0) {
            UartCmsdk::write("rtos cleanup\n");
        }
        if (!scheduler.self_check()) {
            UartCmsdk::write("rtos check failed\n");
        }
            dump_trace();
        }

        void dump_trace() noexcept {
            if (scheduler.trace_count() == 0) return;
            std::array<charm::system::rtos::TraceEvent, 4> out{};
            const auto count = scheduler.trace_dump(
                std::span<charm::system::rtos::TraceEvent>(out.data(), out.size()),
                scheduler.trace_mask());
            for (util::usize i = 0; i < count; ++i) {
                const auto& ev = out[i];
                const char kind = trace_kind_short(ev.kind);
                char buf[96]{};
                const int n = std::snprintf(
                    buf,
                    sizeof(buf),
                    "trace %c id=%u data=%u ts=%llu\n",
                    kind,
                    static_cast<unsigned>(ev.id),
                    static_cast<unsigned>(ev.data),
                    static_cast<unsigned long long>(ev.ts));
                if (n > 0) {
                    UartCmsdk::write(buf);
                }
            }
        }

        static char trace_kind_short(charm::system::rtos::TraceKind kind) noexcept {
            using charm::system::rtos::TraceKind;
            switch (kind) {
            case TraceKind::run: return 'R';
            case TraceKind::yield: return 'Y';
            case TraceKind::block: return 'B';
            case TraceKind::wake: return 'W';
            case TraceKind::timeout: return 'T';
            case TraceKind::sleep: return 'S';
            case TraceKind::timer_fire: return 'F';
            case TraceKind::isr_poll: return 'I';
            case TraceKind::task_violation: return 'v';
            case TraceKind::isr_violation: return 'V';
            case TraceKind::create_denied: return 'C';
            case TraceKind::timer_create_denied: return 'D';
            case TraceKind::pi_detected: return 'P';
            case TraceKind::lock_reenter: return 'L';
            case TraceKind::pi_boost: return 'B';
            }
            return '?';
        }
    };
}

int main() {
    demo::Demo demo{};
    demo::g_tick_mod = 100;
    while (true) {
        demo::g_tick_ms += 1;
        {
            demo::IsrGuard isr{};
            demo.scheduler.tick();
        }
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
