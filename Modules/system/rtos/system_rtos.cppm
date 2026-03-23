module;

#include <cstdint>
#include <span>
#include <type_traits>

export module charm.system.rtos;

import charm.system.time;
import util.core;
import util.error;

export namespace charm::system::rtos {
    using Tick = util::u64;
    using TaskId = util::u16;
    using TaskFn = void (*)(void* ctx) noexcept;
    using TimerFn = void (*)(void* ctx) noexcept;

    constexpr TaskId invalid_task_id = 0;

    enum class TaskState : util::u8 {
        unused,
        ready,
        running,
        sleeping,
        blocked,
        stopped,
    };

    struct TaskSlot {
        TaskFn fn{nullptr};
        void* ctx{nullptr};
        Tick wake_ms{0};
        TaskState state{TaskState::unused};
    };

    struct TimerSlot {
        TimerFn fn{nullptr};
        void* ctx{nullptr};
        Tick due_ms{0};
        bool active{false};
    };

    template <class T>
    concept CopyableValue = std::is_trivially_copyable_v<T>;

    template <CopyableValue T, util::usize Capacity>
    class SpscQueue {
    public:
        [[nodiscard]] bool push(const T& value) noexcept {
            const auto next = advance(head_);
            if (next == tail_) return false;
            buffer_[head_] = value;
            head_ = next;
            return true;
        }

        [[nodiscard]] bool pop(T& out) noexcept {
            if (tail_ == head_) return false;
            out = buffer_[tail_];
            tail_ = advance(tail_);
            return true;
        }

        [[nodiscard]] bool empty() const noexcept { return head_ == tail_; }
        [[nodiscard]] bool full() const noexcept { return advance(head_) == tail_; }

    private:
        static constexpr util::usize advance(util::usize value) noexcept {
            return (value + 1u) % Capacity;
        }

        T buffer_[Capacity]{};
        util::usize head_{0};
        util::usize tail_{0};
    };

    struct SchedulerConfig {
        std::span<TaskSlot> tasks{};
        std::span<TimerSlot> timers{};
    };

    class Scheduler {
    public:
        explicit Scheduler(SchedulerConfig cfg) noexcept : tasks_(cfg.tasks), timers_(cfg.timers) {}

        [[nodiscard]] util::Result<TaskId> create(TaskFn fn, void* ctx) noexcept;
        void run_once() noexcept;
        void yield() noexcept;
        void sleep_ms(Tick ms) noexcept;
        [[nodiscard]] util::Result<util::u16> schedule_at(Tick due_ms, TimerFn fn, void* ctx) noexcept;
        [[nodiscard]] util::Result<util::u16> schedule_after(Tick delay_ms, TimerFn fn, void* ctx) noexcept;
        void cancel_timer(util::u16 id) noexcept;

        [[nodiscard]] bool valid() const noexcept { return !tasks_.empty(); }

        static Scheduler& current() noexcept;
        static void bind(Scheduler& scheduler) noexcept;

    private:
        TaskSlot* slot_from_id(TaskId id) noexcept;
        TimerSlot* timer_from_id(util::u16 id) noexcept;

        std::span<TaskSlot> tasks_{};
        std::span<TimerSlot> timers_{};
        TaskId current_{invalid_task_id};
        inline static Scheduler* bound_{nullptr};
    };

    inline Scheduler& Scheduler::current() noexcept {
        return *bound_;
    }

    inline void Scheduler::bind(Scheduler& scheduler) noexcept {
        bound_ = &scheduler;
    }

    inline TaskSlot* Scheduler::slot_from_id(TaskId id) noexcept {
        if (id == invalid_task_id) return nullptr;
        const auto index = static_cast<util::usize>(id - 1);
        if (index >= tasks_.size()) return nullptr;
        return &tasks_[index];
    }

    inline TimerSlot* Scheduler::timer_from_id(util::u16 id) noexcept {
        if (id == 0u) return nullptr;
        const auto index = static_cast<util::usize>(id - 1u);
        if (index >= timers_.size()) return nullptr;
        return &timers_[index];
    }

    inline util::Result<TaskId> Scheduler::create(TaskFn fn, void* ctx) noexcept {
        if (!fn) return util::unexpected(util::Errc::invalid_arg);
        for (util::usize i = 0; i < tasks_.size(); ++i) {
            auto& slot = tasks_[i];
            if (slot.state == TaskState::unused || slot.state == TaskState::stopped) {
                slot.fn = fn;
                slot.ctx = ctx;
                slot.wake_ms = 0;
                slot.state = TaskState::ready;
                return static_cast<TaskId>(i + 1);
            }
        }
        return util::unexpected(util::Errc::no_memory);
    }

    inline void Scheduler::yield() noexcept {
        auto* slot = slot_from_id(current_);
        if (!slot) return;
        if (slot->state == TaskState::running) {
            slot->state = TaskState::ready;
        }
    }

    inline void Scheduler::sleep_ms(Tick ms) noexcept {
        auto* slot = slot_from_id(current_);
        if (!slot) return;
        slot->wake_ms = time::now_ms() + ms;
        slot->state = TaskState::sleeping;
    }

    inline void Scheduler::run_once() noexcept {
        const auto now = time::now_ms();
        for (util::usize i = 0; i < timers_.size(); ++i) {
            auto& timer = timers_[i];
            if (!timer.active || !timer.fn) continue;
            if (now >= timer.due_ms) {
                timer.active = false;
                timer.fn(timer.ctx);
            }
        }
        for (util::usize i = 0; i < tasks_.size(); ++i) {
            auto& slot = tasks_[i];
            if (slot.state == TaskState::sleeping && now >= slot.wake_ms) {
                slot.state = TaskState::ready;
            }
            if (slot.state != TaskState::ready || !slot.fn) {
                continue;
            }
            current_ = static_cast<TaskId>(i + 1);
            slot.state = TaskState::running;
            slot.fn(slot.ctx);
            if (slot.state == TaskState::running) {
                slot.state = TaskState::ready;
            }
        }
        current_ = invalid_task_id;
    }

    inline util::Result<util::u16> Scheduler::schedule_at(Tick due_ms, TimerFn fn, void* ctx) noexcept {
        if (!fn) return util::unexpected(util::Errc::invalid_arg);
        for (util::usize i = 0; i < timers_.size(); ++i) {
            auto& timer = timers_[i];
            if (!timer.active) {
                timer.fn = fn;
                timer.ctx = ctx;
                timer.due_ms = due_ms;
                timer.active = true;
                return static_cast<util::u16>(i + 1u);
            }
        }
        return util::unexpected(util::Errc::no_memory);
    }

    inline util::Result<util::u16> Scheduler::schedule_after(Tick delay_ms, TimerFn fn, void* ctx) noexcept {
        return schedule_at(time::now_ms() + delay_ms, fn, ctx);
    }

    inline void Scheduler::cancel_timer(util::u16 id) noexcept {
        auto* timer = timer_from_id(id);
        if (!timer) return;
        timer->active = false;
        timer->fn = nullptr;
        timer->ctx = nullptr;
        timer->due_ms = 0;
    }
}
