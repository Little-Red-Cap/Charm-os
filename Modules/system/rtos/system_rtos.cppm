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
    struct TraceEvent;
    using TraceSinkFn = void (*)(const TraceEvent& ev) noexcept;
    using PortFn = void (*)() noexcept;
    using TickSetupFn = void (*)(util::u32 hz) noexcept;

    enum class RuntimePhase : util::u8 {
        registration,
        runtime,
    };

#if !defined(NDEBUG)
    inline void debug_trap() noexcept {
#if defined(__GNUC__)
        __builtin_trap();
#else
        while (true) {}
#endif
    }

    inline void debug_assert(bool cond) noexcept {
        if (!cond) debug_trap();
    }
#else
    inline void debug_assert(bool) noexcept {}
#endif

    inline util::u32 isr_depth_ = 0;
    inline util::u32 isr_violation_count_ = 0;
    inline util::u32 task_violation_count_ = 0;

    inline bool in_isr() noexcept { return isr_depth_ != 0; }

    inline void isr_enter() noexcept { ++isr_depth_; }
    inline void isr_exit() noexcept { if (isr_depth_ > 0) --isr_depth_; }

    inline void require_task_context() noexcept {
        if (in_isr()) {
            ++task_violation_count_;
            debug_assert(false);
        }
    }

    inline void require_isr_context() noexcept {
        if (!in_isr()) {
            ++isr_violation_count_;
            debug_assert(false);
        }
    }

    class IsrGuard {
    public:
        IsrGuard() noexcept { isr_enter(); }
        ~IsrGuard() { isr_exit(); }
        IsrGuard(const IsrGuard&) = delete;
        IsrGuard& operator=(const IsrGuard&) = delete;
    };

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

    enum class TraceKind : util::u8 {
        run,
        yield,
        block,
        wake,
        timeout,
        sleep,
        timer_fire,
        isr_poll,
    };

    struct TraceEvent {
        Tick ts{0};
        TraceKind kind{TraceKind::run};
        TaskId id{invalid_task_id};
        util::u32 data{0};
    };

    constexpr util::u32 trace_bit(TraceKind kind) noexcept {
        return 1u << static_cast<util::u32>(kind);
    }

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
        bool allow_task_create{true};
        bool allow_timer_create{true};
        bool allow_runtime_task_create{false};
        bool allow_runtime_timer_create{false};
        std::span<TraceEvent> trace{};
        util::u32 trace_mask{0xFFFFFFFFu};
    };

    class Scheduler {
    public:
        explicit Scheduler(SchedulerConfig cfg) noexcept
            : tasks_(cfg.tasks), timers_(cfg.timers), delay_list_(cfg.delay_list) {
            max_priority_ = cfg.max_priority > max_task_priority ? max_task_priority : cfg.max_priority;
            allow_task_create_ = cfg.allow_task_create;
            allow_timer_create_ = cfg.allow_timer_create;
            allow_runtime_task_create_ = cfg.allow_runtime_task_create;
            allow_runtime_timer_create_ = cfg.allow_runtime_timer_create;
            trace_ = cfg.trace;
            trace_mask_ = trace_.empty() ? 0u : cfg.trace_mask;
        }

        [[nodiscard]] util::Result<TaskId> create(TaskFn fn, void* ctx) noexcept;
        [[nodiscard]] util::Result<TaskId> create(TaskFn fn, void* ctx, TaskPriority priority, util::u32 slice) noexcept;
        [[nodiscard]] util::Result<TaskId> reserve(TaskFn fn, void* ctx, TaskPriority priority,
                                                   util::u32 slice) noexcept;
        [[nodiscard]] bool activate(TaskId id) noexcept;
        void enter_runtime() noexcept { phase_ = RuntimePhase::runtime; }
        [[nodiscard]] bool in_runtime() const noexcept { return phase_ == RuntimePhase::runtime; }
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
            util::u32 switch_count{0};
            util::u32 yield_count{0};
            util::u32 block_count{0};
            util::u32 wake_count{0};
            util::u32 timeout_count{0};
            util::u32 last_pick_prio{0};
            util::u32 create_denied{0};
            util::u32 runtime_create_denied{0};
            util::u32 timer_create_denied{0};
            util::u32 runtime_timer_denied{0};
            util::u32 isr_violation_count{0};
            util::u32 task_violation_count{0};
        };
        [[nodiscard]] Stats stats() const noexcept;
        [[nodiscard]] bool self_check() const noexcept;
        [[nodiscard]] util::u32 trace_mask() const noexcept { return trace_mask_; }
        void set_trace_mask(util::u32 mask) noexcept { trace_mask_ = mask; }
        [[nodiscard]] util::usize trace_count() const noexcept { return trace_count_; }
        [[nodiscard]] util::usize trace_dump(std::span<TraceEvent> out, util::u32 mask) const noexcept;

        [[nodiscard]] bool valid() const noexcept { return !tasks_.empty(); }
        void allow_task_create(bool allowed) noexcept { allow_task_create_ = allowed; }
        void allow_timer_create(bool allowed) noexcept { allow_timer_create_ = allowed; }
        void freeze_task_creation() noexcept { allow_task_create_ = false; }
        void freeze_timer_creation() noexcept { allow_timer_create_ = false; }

        static Scheduler& current() noexcept;
        static Scheduler* current_ptr() noexcept;
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
        void trace_push(TraceKind kind, TaskId id, util::u32 data) noexcept;

        std::span<TaskSlot> tasks_{};
        std::span<TimerSlot> timers_{};
        std::array<util::u16, max_task_priority + 1> ready_count_{};
        std::array<util::usize, max_task_priority + 1> rr_index_{};
        TaskPriority max_priority_{0};
        std::span<TaskId> delay_list_{};
        util::usize delay_count_{0};
        TaskId current_{invalid_task_id};
        util::u32 lock_count_{0};
        RuntimePhase phase_{RuntimePhase::registration};
        bool allow_task_create_{true};
        bool allow_timer_create_{true};
        bool allow_runtime_task_create_{false};
        bool allow_runtime_timer_create_{false};
        std::span<TraceEvent> trace_{};
        util::u32 trace_head_{0};
        util::u32 trace_count_{0};
        util::u32 trace_mask_{0};
        util::u32 switch_count_{0};
        util::u32 yield_count_{0};
        util::u32 block_count_{0};
        util::u32 wake_count_{0};
        util::u32 timeout_count_{0};
        TaskPriority last_pick_prio_{0};
        util::u32 create_denied_{0};
        util::u32 runtime_create_denied_{0};
        util::u32 timer_create_denied_{0};
        util::u32 runtime_timer_denied_{0};
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

    class PreemptGuard {
    public:
        PreemptGuard() noexcept
            : sched_(Scheduler::current_ptr()) {
            if (sched_) sched_->lock();
        }
        ~PreemptGuard() {
            if (sched_) sched_->unlock();
        }
        PreemptGuard(const PreemptGuard&) = delete;
        PreemptGuard& operator=(const PreemptGuard&) = delete;
    private:
        Scheduler* sched_{nullptr};
    };

    inline void preempt_disable() noexcept {
        if (auto* sched = Scheduler::current_ptr()) {
            sched->lock();
        }
    }

    inline void preempt_enable() noexcept {
        if (auto* sched = Scheduler::current_ptr()) {
            sched->unlock();
        }
    }

    struct RtosPort {
        CriticalFn enter_critical{nullptr};
        CriticalFn exit_critical{nullptr};
        TraceSinkFn trace_sink{nullptr};
        PortFn yield{nullptr};
        PortFn start_first_task{nullptr};
        TickSetupFn setup_tick{nullptr};
    };

    inline CriticalFn critical_enter_{nullptr};
    inline CriticalFn critical_exit_{nullptr};

    inline RtosPort port_{};

    inline const RtosPort& port() noexcept { return port_; }
    inline void bind_port(const RtosPort& port) noexcept {
        port_ = port;
        critical_enter_ = port_.enter_critical;
        critical_exit_ = port_.exit_critical;
    }

    inline void port_yield() noexcept {
        if (port_.yield) {
            port_.yield();
        }
    }

    inline void port_start_first_task() noexcept {
        if (port_.start_first_task) {
            port_.start_first_task();
        }
    }

    inline void port_setup_tick(util::u32 hz) noexcept {
        if (port_.setup_tick) {
            port_.setup_tick(hz);
        }
    }

    inline void bind_critical(CriticalFn enter, CriticalFn exit) noexcept {
        critical_enter_ = enter;
        critical_exit_ = exit;
        port_.enter_critical = enter;
        port_.exit_critical = exit;
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

    inline Scheduler* Scheduler::current_ptr() noexcept {
        return bound_;
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
        require_task_context();
        auto* slot = slot_from_id(current_);
        if (!slot) return;
        debug_assert(slot->state == TaskState::running);
        ++block_count_;
        trace_push(TraceKind::block, current_, 0);
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
        debug_assert(slot->state == TaskState::blocked);
        ++wake_count_;
        trace_push(TraceKind::wake, id, static_cast<util::u32>(result));
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
                ++timeout_count_;
                trace_push(TraceKind::timeout, id, 0);
                mark_ready(*slot);
            }
        }
    }

    inline util::Result<TaskId> Scheduler::create(TaskFn fn, void* ctx) noexcept {
        return create(fn, ctx, 0, 1);
    }

    inline util::Result<TaskId> Scheduler::create(TaskFn fn, void* ctx, TaskPriority priority, util::u32 slice) noexcept {
        if (!fn) return util::unexpected(util::Errc::invalid_arg);
        if (!allow_task_create_) {
            ++create_denied_;
            return util::unexpected(util::Errc::perm);
        }
        if (phase_ == RuntimePhase::runtime && !allow_runtime_task_create_) {
            ++runtime_create_denied_;
            return util::unexpected(util::Errc::perm);
        }
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

    inline util::Result<TaskId> Scheduler::reserve(TaskFn fn, void* ctx, TaskPriority priority,
                                                   util::u32 slice) noexcept {
        if (!fn) return util::unexpected(util::Errc::invalid_arg);
        if (!allow_task_create_) {
            ++create_denied_;
            return util::unexpected(util::Errc::perm);
        }
        if (phase_ == RuntimePhase::runtime && !allow_runtime_task_create_) {
            ++runtime_create_denied_;
            return util::unexpected(util::Errc::perm);
        }
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
                slot.state = TaskState::stopped;
                return static_cast<TaskId>(i + 1);
            }
        }
        return util::unexpected(util::Errc::no_memory);
    }

    inline bool Scheduler::activate(TaskId id) noexcept {
        auto* slot = slot_from_id(id);
        if (!slot) return false;
        if (slot->state != TaskState::stopped || !slot->fn) return false;
        slot->state = TaskState::ready;
        slot->slice_left = slot->slice_max;
        mark_ready(*slot);
        return true;
    }

    inline void Scheduler::yield() noexcept {
        require_task_context();
        auto* slot = slot_from_id(current_);
        if (!slot) return;
        debug_assert(slot->state == TaskState::running);
        ++yield_count_;
        trace_push(TraceKind::yield, current_, 0);
        if (slot->state == TaskState::running) {
            slot->state = TaskState::ready;
            slot->slice_left = slot->slice_max;
            mark_ready(*slot);
        }
    }

    inline void Scheduler::sleep_ms(Tick ms) noexcept {
        require_task_context();
        auto* slot = slot_from_id(current_);
        if (!slot) return;
        debug_assert(slot->state == TaskState::running);
        trace_push(TraceKind::sleep, current_, static_cast<util::u32>(ms));
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
            last_pick_prio_ = prio;
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
                trace_push(TraceKind::timer_fire, current_, static_cast<util::u32>(kind));
            }
        }
    }

    inline void Scheduler::trace_push(TraceKind kind, TaskId id, util::u32 data) noexcept {
        if (trace_.empty()) return;
        if ((trace_mask_ & trace_bit(kind)) == 0u) return;
        const auto idx = trace_head_ % static_cast<util::u32>(trace_.size());
        const TraceEvent ev{time::now_ms(), kind, id, data};
        trace_[idx] = ev;
        ++trace_head_;
        if (trace_count_ < trace_.size()) {
            ++trace_count_;
        }
        if (port_.trace_sink) {
            port_.trace_sink(ev);
        }
    }

    inline util::usize Scheduler::trace_dump(std::span<TraceEvent> out, util::u32 mask) const noexcept {
        if (trace_.empty() || out.empty() || trace_count_ == 0) return 0;
        const util::usize capacity = trace_.size();
        const util::usize available = trace_count_ < capacity ? trace_count_ : capacity;
        util::usize written = 0;
        for (util::usize offset = 0; offset < available && written < out.size(); ++offset) {
            const auto idx =
                static_cast<util::usize>((trace_head_ + capacity - 1u - offset) % capacity);
            const auto& ev = trace_[idx];
            if ((mask & trace_bit(ev.kind)) == 0u) continue;
            out[written++] = ev;
        }
        return written;
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
        debug_assert(slot->state == TaskState::ready);
        mark_unready(*slot);
        current_ = next;
        ++switch_count_;
        trace_push(TraceKind::run, current_, 0);
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
        if (!allow_timer_create_) {
            ++timer_create_denied_;
            return util::unexpected(util::Errc::perm);
        }
        if (phase_ == RuntimePhase::runtime && !allow_runtime_timer_create_) {
            ++runtime_timer_denied_;
            return util::unexpected(util::Errc::perm);
        }
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
        out.switch_count = switch_count_;
        out.yield_count = yield_count_;
        out.block_count = block_count_;
        out.wake_count = wake_count_;
        out.timeout_count = timeout_count_;
        out.last_pick_prio = last_pick_prio_;
        out.create_denied = create_denied_;
        out.runtime_create_denied = runtime_create_denied_;
        out.timer_create_denied = timer_create_denied_;
        out.runtime_timer_denied = runtime_timer_denied_;
        out.isr_violation_count = isr_violation_count_;
        out.task_violation_count = task_violation_count_;
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

    inline bool Scheduler::self_check() const noexcept {
        util::u32 ready = 0;
        util::u32 running = 0;
        util::u32 sleeping = 0;
        util::u32 blocked = 0;
        util::u32 stopped = 0;
        std::array<util::u32, max_task_priority + 1> ready_by_prio{};
        for (const auto& slot : tasks_) {
            switch (slot.state) {
            case TaskState::ready:
                ++ready;
                if (slot.priority <= max_task_priority) {
                    ++ready_by_prio[slot.priority];
                }
                break;
            case TaskState::running: ++running; break;
            case TaskState::sleeping: ++sleeping; break;
            case TaskState::blocked: ++blocked; break;
            case TaskState::stopped: ++stopped; break;
            case TaskState::unused: break;
            }
        }
        util::u32 ready_count = 0;
        for (auto count : ready_count_) {
            ready_count += count;
        }
        if (ready_count != ready) return false;
        if (running > 1) return false;
        if (delay_count_ > delay_list_.size()) return false;
        for (util::usize i = 0; i < delay_count_; ++i) {
            const auto id = delay_list_[i];
            auto* slot = const_cast<Scheduler*>(this)->slot_from_id(id);
            if (!slot) return false;
            if (slot->state != TaskState::sleeping && slot->state != TaskState::blocked) {
                return false;
            }
        }
        for (util::usize prio = 0; prio <= max_task_priority; ++prio) {
            if (prio > max_priority_) break;
            if (ready_by_prio[prio] != ready_count_[prio]) return false;
        }
        return true;
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

    template <util::usize MaxWaiters>
    class EventFlags {
    public:
        enum class AutoClearMode : util::u8 {
            none,
            mask,
            match_any,
            match_all,
        };

        [[nodiscard]] util::u32 get() const noexcept { return flags_; }
        void clear(util::u32 mask) noexcept { flags_ &= ~mask; }
        void set_auto_clear_any(AutoClearMode mode) noexcept { auto_clear_any_ = mode; }
        void set_auto_clear_all(AutoClearMode mode) noexcept { auto_clear_all_ = mode; }

        void set(util::u32 mask) noexcept {
            require_task_context();
            flags_ |= mask;
            if (!Scheduler::current().valid()) return;
            process_waiters(Scheduler::current());
        }

        void set_isr(util::u32 mask) noexcept {
            require_isr_context();
            flags_ |= mask;
            pending_wake_ = true;
        }

        void poll_wake(Scheduler& sched) noexcept {
            require_task_context();
            if (!pending_wake_) return;
            pending_wake_ = false;
            process_waiters(sched);
        }

        [[nodiscard]] WaitResult wait_any(util::u32 mask, Tick timeout_ms = 0) noexcept {
            require_task_context();
            return wait_impl(mask, false, auto_clear_any_, timeout_ms);
        }

        [[nodiscard]] WaitResult wait_any(util::u32 mask, AutoClearMode mode, Tick timeout_ms = 0) noexcept {
            require_task_context();
            return wait_impl(mask, false, mode, timeout_ms);
        }

        [[nodiscard]] WaitResult wait_all(util::u32 mask, Tick timeout_ms = 0) noexcept {
            require_task_context();
            return wait_impl(mask, true, auto_clear_all_, timeout_ms);
        }

        [[nodiscard]] WaitResult wait_all(util::u32 mask, AutoClearMode mode, Tick timeout_ms = 0) noexcept {
            require_task_context();
            return wait_impl(mask, true, mode, timeout_ms);
        }

    private:
        void process_waiters(Scheduler& sched) noexcept {
            util::usize i = 0;
            while (i < wait_count_) {
                const auto w = waiters_[i];
                if (w.id == invalid_task_id) {
                    remove_waiter(i);
                    continue;
                }
                const bool ready = w.all ? ((flags_ & w.mask) == w.mask)
                                         : ((flags_ & w.mask) != 0);
                if (ready) {
                    switch (w.auto_clear) {
                    case AutoClearMode::mask:
                        flags_ &= ~w.mask;
                        break;
                    case AutoClearMode::match_any:
                        flags_ &= ~(flags_ & w.mask);
                        break;
                    case AutoClearMode::match_all:
                        flags_ &= ~w.mask;
                        break;
                    case AutoClearMode::none:
                        break;
                    }
                    sched.wake(w.id, WaitResult::ok, true);
                    remove_waiter(i);
                    continue;
                }
                ++i;
            }
        }
        struct Waiter {
            TaskId id{invalid_task_id};
            util::u32 mask{0};
            bool all{false};
            AutoClearMode auto_clear{AutoClearMode::none};
        };

        void remove_waiter(util::usize index) noexcept {
            if (index >= wait_count_) return;
            for (util::usize i = index + 1; i < wait_count_; ++i) {
                waiters_[i - 1] = waiters_[i];
            }
            --wait_count_;
        }

        static void cancel_waiter(void* ctx, TaskId id) noexcept {
            auto* self = static_cast<EventFlags*>(ctx);
            if (!self) return;
            util::usize i = 0;
            while (i < self->wait_count_) {
                if (self->waiters_[i].id == id) {
                    self->remove_waiter(i);
                    return;
                }
                ++i;
            }
        }

        [[nodiscard]] WaitResult wait_impl(util::u32 mask, bool all, AutoClearMode auto_clear,
                                           Tick timeout_ms) noexcept {
            if (!Scheduler::current().valid()) return WaitResult::blocked;
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
            const bool ready = all ? ((flags_ & mask) == mask)
                                   : ((flags_ & mask) != 0);
            if (ready) {
                switch (auto_clear) {
                case AutoClearMode::mask:
                    flags_ &= ~mask;
                    break;
                case AutoClearMode::match_any:
                    flags_ &= ~(flags_ & mask);
                    break;
                case AutoClearMode::match_all:
                    flags_ &= ~mask;
                    break;
                case AutoClearMode::none:
                    break;
                }
                return WaitResult::ok;
            }
            if (wait_count_ >= MaxWaiters) return WaitResult::blocked;
            waiters_[wait_count_] = Waiter{id, mask, all, auto_clear};
            ++wait_count_;
            sched.block_current(timeout_ms, &EventFlags::cancel_waiter, this);
            return WaitResult::blocked;
        }

        util::u32 flags_{0};
        bool pending_wake_{false};
        AutoClearMode auto_clear_any_{AutoClearMode::none};
        AutoClearMode auto_clear_all_{AutoClearMode::none};
        std::array<Waiter, MaxWaiters> waiters_{};
        util::usize wait_count_{0};
    };

    template <CopyableValue T, util::usize Capacity, util::usize MaxWaiters>
    class MessageQueue {
    public:
        [[nodiscard]] WaitResult send(const T& value, Tick timeout_ms = 0) noexcept {
            require_task_context();
            if (!Scheduler::current().valid()) return WaitResult::blocked;
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
            if (!full()) {
                push(value);
                wake_receiver(sched);
                return WaitResult::ok;
            }
            if (send_wait_count_ >= MaxWaiters) return WaitResult::blocked;
            send_waiters_[send_wait_count_++] = id;
            sched.block_current(timeout_ms, &MessageQueue::cancel_send, this);
            return WaitResult::blocked;
        }

        [[nodiscard]] bool try_send(const T& value) noexcept {
            require_task_context();
            if (full()) return false;
            push(value);
            if (Scheduler::current().valid()) {
                wake_receiver(Scheduler::current());
            }
            return true;
        }

        [[nodiscard]] bool try_send_isr(const T& value) noexcept {
            require_isr_context();
            if (full()) return false;
            push(value);
            if (recv_wait_count_ > 0) {
                ++rx_lock_;
            }
            return true;
        }

        [[nodiscard]] WaitResult recv(T& out, Tick timeout_ms = 0) noexcept {
            require_task_context();
            if (!Scheduler::current().valid()) return WaitResult::blocked;
            auto& sched = Scheduler::current();
            const auto id = sched.current_id();
            if (id == invalid_task_id) return WaitResult::blocked;

            const auto prev = sched.take_wait_result(id);
            if (prev == WaitResult::timeout) {
                return WaitResult::timeout;
            }
            if (sched.take_grant(id) && !empty()) {
                pop(out);
                return WaitResult::ok;
            }
            if (!empty()) {
                pop(out);
                wake_sender(sched);
                return WaitResult::ok;
            }
            if (recv_wait_count_ >= MaxWaiters) return WaitResult::blocked;
            recv_waiters_[recv_wait_count_++] = id;
            sched.block_current(timeout_ms, &MessageQueue::cancel_recv, this);
            return WaitResult::blocked;
        }

        [[nodiscard]] bool try_recv(T& out) noexcept {
            require_task_context();
            if (empty()) return false;
            pop(out);
            if (Scheduler::current().valid()) {
                wake_sender(Scheduler::current());
            }
            return true;
        }

        [[nodiscard]] bool try_recv_isr(T& out) noexcept {
            require_isr_context();
            if (empty()) return false;
            pop(out);
            if (send_wait_count_ > 0) {
                ++tx_lock_;
            }
            return true;
        }

        void poll_wake(Scheduler& sched) noexcept {
            require_task_context();
            if (recv_wait_count_ == 0 || empty()) {
                rx_lock_ = 0;
            }
            while (rx_lock_ > 0 && recv_wait_count_ > 0 && !empty()) {
                wake_receiver(sched);
                --rx_lock_;
            }
            if (rx_lock_ > recv_wait_count_) rx_lock_ = recv_wait_count_;
            if (send_wait_count_ == 0 || full()) {
                tx_lock_ = 0;
            }
            while (tx_lock_ > 0 && send_wait_count_ > 0 && !full()) {
                wake_sender(sched);
                --tx_lock_;
            }
            if (tx_lock_ > send_wait_count_) tx_lock_ = send_wait_count_;
        }

        [[nodiscard]] util::usize send_batch(std::span<const T> items) noexcept {
            util::usize sent = 0;
            for (const auto& item : items) {
                if (!try_send(item)) break;
                ++sent;
            }
            return sent;
        }

        [[nodiscard]] util::usize recv_batch(std::span<T> items) noexcept {
            util::usize got = 0;
            for (auto& item : items) {
                if (!try_recv(item)) break;
                ++got;
            }
            return got;
        }

        [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
        [[nodiscard]] bool full() const noexcept { return count_ == Capacity; }

    private:
        static constexpr util::usize advance(util::usize value) noexcept {
            return (value + 1u) % Capacity;
        }

        void push(const T& value) noexcept {
            buffer_[tail_] = value;
            tail_ = advance(tail_);
            ++count_;
        }

        void pop(T& out) noexcept {
            out = buffer_[head_];
            head_ = advance(head_);
            --count_;
        }

        void wake_receiver(Scheduler& sched) noexcept {
            if (recv_wait_count_ == 0) return;
            const auto id = recv_waiters_[0];
            for (util::usize i = 1; i < recv_wait_count_; ++i) {
                recv_waiters_[i - 1] = recv_waiters_[i];
            }
            --recv_wait_count_;
            sched.wake(id, WaitResult::ok, true);
        }

        void wake_sender(Scheduler& sched) noexcept {
            if (send_wait_count_ == 0) return;
            const auto id = send_waiters_[0];
            for (util::usize i = 1; i < send_wait_count_; ++i) {
                send_waiters_[i - 1] = send_waiters_[i];
            }
            --send_wait_count_;
            sched.wake(id, WaitResult::ok, true);
        }

        static void cancel_send(void* ctx, TaskId id) noexcept {
            auto* self = static_cast<MessageQueue*>(ctx);
            if (!self) return;
            util::usize i = 0;
            while (i < self->send_wait_count_) {
                if (self->send_waiters_[i] == id) {
                    for (util::usize j = i + 1; j < self->send_wait_count_; ++j) {
                        self->send_waiters_[j - 1] = self->send_waiters_[j];
                    }
                    --self->send_wait_count_;
                    return;
                }
                ++i;
            }
        }

        static void cancel_recv(void* ctx, TaskId id) noexcept {
            auto* self = static_cast<MessageQueue*>(ctx);
            if (!self) return;
            util::usize i = 0;
            while (i < self->recv_wait_count_) {
                if (self->recv_waiters_[i] == id) {
                    for (util::usize j = i + 1; j < self->recv_wait_count_; ++j) {
                        self->recv_waiters_[j - 1] = self->recv_waiters_[j];
                    }
                    --self->recv_wait_count_;
                    return;
                }
                ++i;
            }
        }

        std::array<T, Capacity> buffer_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
        std::array<TaskId, MaxWaiters> send_waiters_{};
        std::array<TaskId, MaxWaiters> recv_waiters_{};
        util::usize send_wait_count_{0};
        util::usize recv_wait_count_{0};
        util::u32 rx_lock_{0};
        util::u32 tx_lock_{0};
    };

    template <util::usize MaxWaiters>
    class Mutex {
    public:
        [[nodiscard]] bool try_lock() noexcept {
            if (!Scheduler::current().valid()) return false;
            auto& sched = Scheduler::current();
            const auto id = sched.current_id();
            if (id == invalid_task_id) return false;
            if (owner_ == invalid_task_id || owner_ == id) {
                owner_ = id;
                return true;
            }
            return false;
        }

        [[nodiscard]] WaitResult lock(Tick timeout_ms = 0) noexcept {
            if (!Scheduler::current().valid()) return WaitResult::blocked;
            auto& sched = Scheduler::current();
            const auto id = sched.current_id();
            if (id == invalid_task_id) return WaitResult::blocked;

            const auto prev = sched.take_wait_result(id);
            if (prev == WaitResult::timeout) {
                return WaitResult::timeout;
            }
            if (sched.take_grant(id)) {
                owner_ = id;
                return WaitResult::ok;
            }
            if (owner_ == invalid_task_id || owner_ == id) {
                owner_ = id;
                return WaitResult::ok;
            }
            if (wait_count_ >= MaxWaiters) return WaitResult::blocked;
            waiters_[wait_count_++] = id;
            sched.block_current(timeout_ms, &Mutex::cancel_waiter, this);
            return WaitResult::blocked;
        }

        void unlock() noexcept {
            if (!Scheduler::current().valid()) return;
            auto& sched = Scheduler::current();
            const auto id = sched.current_id();
            if (owner_ != id) return;
            if (wait_count_ > 0) {
                const auto next = waiters_[0];
                for (util::usize i = 1; i < wait_count_; ++i) {
                    waiters_[i - 1] = waiters_[i];
                }
                --wait_count_;
                owner_ = next;
                sched.wake(next, WaitResult::ok, true);
                return;
            }
            owner_ = invalid_task_id;
        }

    private:
        static void cancel_waiter(void* ctx, TaskId id) noexcept {
            auto* self = static_cast<Mutex*>(ctx);
            if (!self) return;
            util::usize i = 0;
            while (i < self->wait_count_) {
                if (self->waiters_[i] == id) {
                    for (util::usize j = i + 1; j < self->wait_count_; ++j) {
                        self->waiters_[j - 1] = self->waiters_[j];
                    }
                    --self->wait_count_;
                    return;
                }
                ++i;
            }
        }

        TaskId owner_{invalid_task_id};
        std::array<TaskId, MaxWaiters> waiters_{};
        util::usize wait_count_{0};
    };

}
