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
    using CriticalFn = void (*)() noexcept;

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

    enum class WaitResult : util::u8 {
        ok,
        timeout,
        blocked,
    };

    struct TaskSlot {
        TaskFn fn{nullptr};
        void* ctx{nullptr};
        Tick wake_ms{0};
        TaskState state{TaskState::unused};
        TaskPriority priority{0};
        util::u32 slice_max{1};
        util::u32 slice_left{1};
        bool grant{false};
        WaitResult wait_result{WaitResult::ok};
        void* wait_ctx{nullptr};
        void (*wait_cancel)(void* ctx, TaskId id) noexcept {nullptr};
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

    class Scheduler;

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
        void lock() noexcept { ++lock_count_; }
        void unlock() noexcept { if (lock_count_ > 0) --lock_count_; }
        [[nodiscard]] bool locked() const noexcept { return lock_count_ != 0; }
        [[nodiscard]] TaskId current_id() const noexcept { return current_; }
        [[nodiscard]] WaitResult take_wait_result(TaskId id) noexcept;
        [[nodiscard]] bool take_grant(TaskId id) noexcept;
        [[nodiscard]] bool is_blocked(TaskId id) noexcept;
        void block_current(Tick timeout_ms, void (*cancel)(void* ctx, TaskId id) noexcept, void* ctx) noexcept;
        void wake(TaskId id, WaitResult result, bool grant) noexcept;
        struct Stats {
            util::u32 ready{0};
            util::u32 sleeping{0};
            util::u32 running{0};
            util::u32 blocked{0};
            util::u32 stopped{0};
            util::u32 timers_soft{0};
            util::u32 timers_hard{0};
            util::u32 lock_depth{0};
            util::u32 delay_count{0};
        };
        [[nodiscard]] Stats stats() const noexcept;

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
        util::u32 lock_count_{0};
        inline static Scheduler* bound_{nullptr};
    };

    class SchedulerLockGuard {
    public:
        explicit SchedulerLockGuard(Scheduler& sched) noexcept : sched_(sched) { sched_.lock(); }
        ~SchedulerLockGuard() { sched_.unlock(); }
        SchedulerLockGuard(const SchedulerLockGuard&) = delete;
        SchedulerLockGuard& operator=(const SchedulerLockGuard&) = delete;
    private:
        Scheduler& sched_;
    };

    inline CriticalFn critical_enter_{nullptr};
    inline CriticalFn critical_exit_{nullptr};

    inline void bind_critical(CriticalFn enter, CriticalFn exit) noexcept {
        critical_enter_ = enter;
        critical_exit_ = exit;
    }

    class CriticalGuard {
    public:
        CriticalGuard() noexcept { if (critical_enter_) critical_enter_(); }
        ~CriticalGuard() { if (critical_exit_) critical_exit_(); }
        CriticalGuard(const CriticalGuard&) = delete;
        CriticalGuard& operator=(const CriticalGuard&) = delete;
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

    inline WaitResult Scheduler::take_wait_result(TaskId id) noexcept {
        auto* slot = slot_from_id(id);
        if (!slot) return WaitResult::blocked;
        const auto result = slot->wait_result;
        if (result != WaitResult::ok) {
            slot->wait_result = WaitResult::ok;
        }
        return result;
    }

    inline bool Scheduler::take_grant(TaskId id) noexcept {
        auto* slot = slot_from_id(id);
        if (!slot) return false;
        if (slot->grant) {
            slot->grant = false;
            return true;
        }
        return false;
    }

    inline bool Scheduler::is_blocked(TaskId id) noexcept {
        auto* slot = slot_from_id(id);
        return slot && slot->state == TaskState::blocked;
    }

    inline void Scheduler::block_current(Tick timeout_ms, void (*cancel)(void* ctx, TaskId id) noexcept, void* ctx) noexcept {
        auto* slot = slot_from_id(current_);
        if (!slot) return;
        slot->state = TaskState::blocked;
        slot->slice_left = slot->slice_max;
        slot->wait_result = WaitResult::blocked;
        slot->wait_ctx = ctx;
        slot->wait_cancel = cancel;
        if (timeout_ms > 0) {
            slot->wake_ms = time::now_ms() + timeout_ms;
            delay_insert(current_, slot->wake_ms);
        } else {
            slot->wake_ms = 0;
        }
    }

    inline void Scheduler::wake(TaskId id, WaitResult result, bool grant) noexcept {
        auto* slot = slot_from_id(id);
        if (!slot) return;
        delay_remove(id);
        slot->state = TaskState::ready;
        slot->wait_result = result;
        slot->grant = grant;
        slot->wait_ctx = nullptr;
        slot->wait_cancel = nullptr;
        mark_ready(*slot);
    }

    inline void Scheduler::delay_remove(TaskId id) noexcept {
        CriticalGuard guard{};
        if (delay_count_ == 0 || delay_list_.empty()) return;
        for (util::usize i = 0; i < delay_count_; ++i) {
            if (delay_list_[i] == id) {
                auto* removed = slot_from_id(id);
                const Tick removed_delta = removed ? removed->wake_ms : 0;
                if (i + 1 < delay_count_) {
                    auto* next = slot_from_id(delay_list_[i + 1]);
                    if (next) {
                        next->wake_ms += removed_delta;
                    }
                }
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
        CriticalGuard guard{};
        delay_remove(id);
        if (delay_count_ >= delay_list_.size()) return;
        auto* slot = slot_from_id(id);
        if (!slot) return;

        util::usize pos = delay_count_;
        Tick accum = 0;
        for (util::usize i = 0; i < delay_count_; ++i) {
            auto* other = slot_from_id(delay_list_[i]);
            if (!other) {
                pos = i;
                break;
            }
            const Tick delta = other->wake_ms;
            if (accum + delta > due_ms) {
                pos = i;
                break;
            }
            accum += delta;
        }

        for (util::usize i = delay_count_; i > pos; --i) {
            delay_list_[i] = delay_list_[i - 1];
        }
        delay_list_[pos] = id;
        slot->wake_ms = due_ms - accum;

        for (util::usize i = pos + 1; i <= delay_count_; ++i) {
            auto* adjust = slot_from_id(delay_list_[i]);
            if (!adjust) continue;
            if (adjust->wake_ms >= slot->wake_ms) {
                adjust->wake_ms -= slot->wake_ms;
            } else {
                adjust->wake_ms = 0;
            }
            break;
        }

        ++delay_count_;
    }

    inline void Scheduler::delay_wake_ready(Tick now) noexcept {
        CriticalGuard guard{};
        Tick remain = now;
        while (delay_count_ > 0) {
            const auto id = delay_list_[0];
            auto* slot = slot_from_id(id);
            if (!slot) {
                delay_remove(id);
                continue;
            }
            if (slot->wake_ms > remain) {
                slot->wake_ms -= remain;
                break;
            }
            remain -= slot->wake_ms;
            delay_remove(id);
            if (slot->state == TaskState::sleeping) {
                slot->state = TaskState::ready;
                mark_ready(*slot);
            } else if (slot->state == TaskState::blocked) {
                if (slot->wait_cancel && slot->wait_ctx) {
                    slot->wait_cancel(slot->wait_ctx, id);
                }
                slot->state = TaskState::ready;
                slot->wait_result = WaitResult::timeout;
                slot->wait_ctx = nullptr;
                slot->wait_cancel = nullptr;
                slot->grant = false;
                mark_ready(*slot);
            }
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
        CriticalGuard guard{};
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
        if (locked()) {
            current_ = invalid_task_id;
            return;
        }
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
        CriticalGuard guard{};
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
        CriticalGuard guard{};
        auto* timer = timer_from_id(id);
        if (!timer) return;
        timer->active = false;
        timer->fn = nullptr;
        timer->ctx = nullptr;
        timer->due_ms = 0;
    }

    inline Scheduler::Stats Scheduler::stats() const noexcept {
        Stats out{};
        out.lock_depth = lock_count_;
        out.delay_count = static_cast<util::u32>(delay_count_);
        for (const auto& slot : tasks_) {
            switch (slot.state) {
            case TaskState::ready:
                ++out.ready;
                break;
            case TaskState::running:
                ++out.running;
                break;
            case TaskState::sleeping:
                ++out.sleeping;
                break;
            case TaskState::blocked:
                ++out.blocked;
                break;
            case TaskState::stopped:
                ++out.stopped;
                break;
            case TaskState::unused:
                break;
            }
        }
        for (const auto& timer : timers_) {
            if (!timer.active) continue;
            if (timer.kind == TimerSlot::Kind::hard) {
                ++out.timers_hard;
            } else {
                ++out.timers_soft;
            }
        }
        return out;
    }

    template <util::usize Capacity>
    class Semaphore {
    public:
        explicit Semaphore(util::u32 initial = 0, util::u32 max = 0xFFFFFFFFu) noexcept
            : count_(initial), max_(max) {}

        [[nodiscard]] bool post() noexcept {
            if (!Scheduler::current().valid()) return false;
            auto& sched = Scheduler::current();
            while (wait_count_ > 0) {
                const auto id = waiters_[head_];
                head_ = advance(head_);
                --wait_count_;
                if (!sched.is_blocked(id)) {
                    continue;
                }
                sched.wake(id, WaitResult::ok, true);
                return true;
            }
            if (count_ < max_) {
                ++count_;
                return true;
            }
            return false;
        }

        [[nodiscard]] WaitResult wait(Tick timeout_ms = 0) noexcept {
            auto& sched = Scheduler::current();
            const auto id = sched.current_id();
            if (id == invalid_task_id) return WaitResult::blocked;

            const auto prev = sched.take_wait_result(id);
            if (prev == WaitResult::timeout) {
                return WaitResult::timeout;
            }
            if (sched.take_grant(id)) {
                return WaitResult::ok;
            }
            if (count_ > 0) {
                --count_;
                return WaitResult::ok;
            }
            if (full()) return WaitResult::blocked;
            waiters_[tail_] = id;
            tail_ = advance(tail_);
            ++wait_count_;
            sched.block_current(timeout_ms, &Semaphore::cancel_waiter, this);
            return WaitResult::blocked;
        }

    private:
        static constexpr util::usize advance(util::usize value) noexcept {
            return (value + 1u) % Capacity;
        }

        static void cancel_waiter(void* ctx, TaskId id) noexcept {
            auto* self = static_cast<Semaphore*>(ctx);
            if (!self) return;
            util::usize new_count = 0;
            for (util::usize i = 0; i < self->wait_count_; ++i) {
                const auto idx = (self->head_ + i) % Capacity;
                const auto value = self->waiters_[idx];
                if (value == id) continue;
                self->waiters_[(self->head_ + new_count) % Capacity] = value;
                ++new_count;
            }
            self->tail_ = (self->head_ + new_count) % Capacity;
            self->wait_count_ = new_count;
        }

        [[nodiscard]] bool full() const noexcept { return wait_count_ == Capacity; }

        TaskId waiters_[Capacity]{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize wait_count_{0};
        util::u32 count_{0};
        util::u32 max_{0xFFFFFFFFu};
    };

}
