module;

#include <array>
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
    using TaskPriority = util::u8;
    using TimerId = util::u16;

    constexpr TaskPriority max_task_priority = 31;

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
        TaskPriority priority{0};
        util::u32 slice_max{1};
        util::u32 slice_left{1};
    };

    struct TimerSlot {
        TimerFn fn{nullptr};
        void* ctx{nullptr};
        Tick due_ms{0};
        bool active{false};
        enum class Kind : util::u8 { soft, hard } kind{Kind::soft};
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
        TaskPriority max_priority{0};
        std::span<TaskId> delay_list{};
    };

    class Scheduler {
    public:
        explicit Scheduler(SchedulerConfig cfg) noexcept
            : tasks_(cfg.tasks), timers_(cfg.timers), delay_list_(cfg.delay_list) {
            max_priority_ = cfg.max_priority > max_task_priority ? max_task_priority : cfg.max_priority;
        }

        [[nodiscard]] util::Result<TaskId> create(TaskFn fn, void* ctx) noexcept;
        [[nodiscard]] util::Result<TaskId> create(TaskFn fn, void* ctx, TaskPriority priority, util::u32 slice) noexcept;
        void run_once() noexcept;
        void yield() noexcept;
        void sleep_ms(Tick ms) noexcept;
        [[nodiscard]] util::Result<TimerId> schedule_at(Tick due_ms, TimerFn fn, void* ctx) noexcept;
        [[nodiscard]] util::Result<TimerId> schedule_after(Tick delay_ms, TimerFn fn, void* ctx) noexcept;
        [[nodiscard]] util::Result<TimerId> schedule_at(Tick due_ms, TimerFn fn, void* ctx, TimerSlot::Kind kind) noexcept;
        [[nodiscard]] util::Result<TimerId> schedule_after(Tick delay_ms, TimerFn fn, void* ctx, TimerSlot::Kind kind) noexcept;
        void cancel_timer(TimerId id) noexcept;
        void tick() noexcept;

        [[nodiscard]] bool valid() const noexcept { return !tasks_.empty(); }

        static Scheduler& current() noexcept;
        static void bind(Scheduler& scheduler) noexcept;

    private:
        TaskSlot* slot_from_id(TaskId id) noexcept;
        TimerSlot* timer_from_id(TimerId id) noexcept;
        void mark_ready(TaskSlot& slot) noexcept;
        void mark_unready(TaskSlot& slot) noexcept;
        TaskId pick_next_ready() noexcept;
        void delay_insert(TaskId id, Tick due_ms) noexcept;
        void delay_remove(TaskId id) noexcept;
        void delay_wake_ready(Tick now) noexcept;
        void process_timers(TimerSlot::Kind kind, Tick now) noexcept;

        std::span<TaskSlot> tasks_{};
        std::span<TimerSlot> timers_{};
        std::array<util::u16, max_task_priority + 1> ready_count_{};
        std::array<util::usize, max_task_priority + 1> rr_index_{};
        TaskPriority max_priority_{0};
        std::span<TaskId> delay_list_{};
        util::usize delay_count_{0};
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

    inline TimerSlot* Scheduler::timer_from_id(TimerId id) noexcept {
        if (id == 0u) return nullptr;
        const auto index = static_cast<util::usize>(id - 1u);
        if (index >= timers_.size()) return nullptr;
        return &timers_[index];
    }

    inline void Scheduler::delay_remove(TaskId id) noexcept {
        if (delay_count_ == 0 || delay_list_.empty()) return;
        for (util::usize i = 0; i < delay_count_; ++i) {
            if (delay_list_[i] == id) {
                for (util::usize j = i + 1; j < delay_count_; ++j) {
                    delay_list_[j - 1] = delay_list_[j];
                }
                --delay_count_;
                return;
            }
        }
    }

    inline void Scheduler::delay_insert(TaskId id, Tick due_ms) noexcept {
        if (delay_list_.empty()) return;
        delay_remove(id);
        if (delay_count_ >= delay_list_.size()) return;
        util::usize pos = delay_count_;
        for (util::usize i = 0; i < delay_count_; ++i) {
            auto* slot = slot_from_id(delay_list_[i]);
            if (!slot || due_ms < slot->wake_ms) {
                pos = i;
                break;
            }
        }
        for (util::usize i = delay_count_; i > pos; --i) {
            delay_list_[i] = delay_list_[i - 1];
        }
        delay_list_[pos] = id;
        ++delay_count_;
    }

    inline void Scheduler::delay_wake_ready(Tick now) noexcept {
        while (delay_count_ > 0) {
            const auto id = delay_list_[0];
            auto* slot = slot_from_id(id);
            if (!slot || slot->state != TaskState::sleeping) {
                delay_remove(id);
                continue;
            }
            if (now < slot->wake_ms) break;
            delay_remove(id);
            slot->state = TaskState::ready;
            mark_ready(*slot);
        }
    }

    inline util::Result<TaskId> Scheduler::create(TaskFn fn, void* ctx) noexcept {
        return create(fn, ctx, 0, 1);
    }

    inline util::Result<TaskId> Scheduler::create(TaskFn fn, void* ctx, TaskPriority priority, util::u32 slice) noexcept {
        if (!fn) return util::unexpected(util::Errc::invalid_arg);
        if (priority > max_task_priority) return util::unexpected(util::Errc::invalid_arg);
        if (slice == 0) slice = 1;
        for (util::usize i = 0; i < tasks_.size(); ++i) {
            auto& slot = tasks_[i];
            if (slot.state == TaskState::unused || slot.state == TaskState::stopped) {
                slot.fn = fn;
                slot.ctx = ctx;
                slot.wake_ms = 0;
                slot.priority = priority;
                slot.slice_max = slice;
                slot.slice_left = slice;
                slot.state = TaskState::ready;
                mark_ready(slot);
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
            slot->slice_left = slot->slice_max;
            mark_ready(*slot);
        }
    }

    inline void Scheduler::sleep_ms(Tick ms) noexcept {
        auto* slot = slot_from_id(current_);
        if (!slot) return;
        slot->wake_ms = time::now_ms() + ms;
        slot->state = TaskState::sleeping;
        slot->slice_left = slot->slice_max;
        delay_insert(current_, slot->wake_ms);
    }

    inline void Scheduler::mark_ready(TaskSlot& slot) noexcept {
        if (slot.state != TaskState::ready) return;
        const auto prio = slot.priority;
        if (prio > max_priority_) return;
        auto& count = ready_count_[prio];
        if (count < 0xFFFFu) {
            ++count;
        }
    }

    inline void Scheduler::mark_unready(TaskSlot& slot) noexcept {
        if (slot.state != TaskState::ready) return;
        const auto prio = slot.priority;
        if (prio > max_priority_) return;
        auto& count = ready_count_[prio];
        if (count > 0) {
            --count;
        }
    }

    inline TaskId Scheduler::pick_next_ready() noexcept {
        if (tasks_.empty()) return invalid_task_id;
        for (int p = static_cast<int>(max_priority_); p >= 0; --p) {
            const auto prio = static_cast<TaskPriority>(p);
            if (ready_count_[prio] == 0) continue;
            const auto start = rr_index_[prio] % tasks_.size();
            for (util::usize offset = 0; offset < tasks_.size(); ++offset) {
                const auto idx = (start + offset) % tasks_.size();
                auto& slot = tasks_[idx];
                if (slot.state == TaskState::ready && slot.priority == prio && slot.fn) {
                    rr_index_[prio] = (idx + 1) % tasks_.size();
                    return static_cast<TaskId>(idx + 1);
                }
            }
        }
        return invalid_task_id;
    }

    inline void Scheduler::process_timers(TimerSlot::Kind kind, Tick now) noexcept {
        for (util::usize i = 0; i < timers_.size(); ++i) {
            auto& timer = timers_[i];
            if (!timer.active || !timer.fn) continue;
            if (timer.kind != kind) continue;
            if (now >= timer.due_ms) {
                timer.active = false;
                timer.fn(timer.ctx);
            }
        }
    }

    inline void Scheduler::tick() noexcept {
        const auto now = time::now_ms();
        process_timers(TimerSlot::Kind::hard, now);
    }

    inline void Scheduler::run_once() noexcept {
        const auto now = time::now_ms();
        delay_wake_ready(now);
        process_timers(TimerSlot::Kind::soft, now);
        const auto next = pick_next_ready();
        if (next == invalid_task_id) {
            current_ = invalid_task_id;
            return;
        }
        auto* slot = slot_from_id(next);
        if (!slot) {
            current_ = invalid_task_id;
            return;
        }
        mark_unready(*slot);
        current_ = next;
        slot->state = TaskState::running;
        slot->fn(slot->ctx);
        if (slot->state == TaskState::running) {
            slot->state = TaskState::ready;
            if (slot->slice_left > 0) {
                --slot->slice_left;
            }
            if (slot->slice_left == 0) {
                slot->slice_left = slot->slice_max;
            }
            mark_ready(*slot);
        }
        current_ = invalid_task_id;
    }

    inline util::Result<TimerId> Scheduler::schedule_at(Tick due_ms, TimerFn fn, void* ctx) noexcept {
        return schedule_at(due_ms, fn, ctx, TimerSlot::Kind::soft);
    }

    inline util::Result<TimerId> Scheduler::schedule_after(Tick delay_ms, TimerFn fn, void* ctx) noexcept {
        return schedule_after(delay_ms, fn, ctx, TimerSlot::Kind::soft);
    }

    inline util::Result<TimerId> Scheduler::schedule_at(Tick due_ms, TimerFn fn, void* ctx, TimerSlot::Kind kind) noexcept {
        if (!fn) return util::unexpected(util::Errc::invalid_arg);
        for (util::usize i = 0; i < timers_.size(); ++i) {
            auto& timer = timers_[i];
            if (!timer.active) {
                timer.fn = fn;
                timer.ctx = ctx;
                timer.due_ms = due_ms;
                timer.kind = kind;
                timer.active = true;
                return static_cast<TimerId>(i + 1u);
            }
        }
        return util::unexpected(util::Errc::no_memory);
    }

    inline util::Result<TimerId> Scheduler::schedule_after(Tick delay_ms, TimerFn fn, void* ctx, TimerSlot::Kind kind) noexcept {
        return schedule_at(time::now_ms() + delay_ms, fn, ctx, kind);
    }

    inline void Scheduler::cancel_timer(TimerId id) noexcept {
        auto* timer = timer_from_id(id);
        if (!timer) return;
        timer->active = false;
        timer->fn = nullptr;
        timer->ctx = nullptr;
        timer->due_ms = 0;
    }
}
