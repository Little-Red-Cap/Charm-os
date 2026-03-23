module;

#include <cstdint>
#include <span>

export module charm.system.rtos;

import charm.system.time;
import util.core;
import util.error;

export namespace charm::system::rtos {
    using Tick = util::u64;
    using TaskId = util::u16;
    using TaskFn = void (*)(void* ctx) noexcept;

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

    struct SchedulerConfig {
        std::span<TaskSlot> tasks{};
    };

    class Scheduler {
    public:
        explicit Scheduler(SchedulerConfig cfg) noexcept : tasks_(cfg.tasks) {}

        [[nodiscard]] util::Result<TaskId> create(TaskFn fn, void* ctx) noexcept;
        void run_once() noexcept;
        void yield() noexcept;
        void sleep_ms(Tick ms) noexcept;

        [[nodiscard]] bool valid() const noexcept { return !tasks_.empty(); }

        static Scheduler& current() noexcept;
        static void bind(Scheduler& scheduler) noexcept;

    private:
        TaskSlot* slot_from_id(TaskId id) noexcept;

        std::span<TaskSlot> tasks_{};
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
}
