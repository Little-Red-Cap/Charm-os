module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

export module charm.system.run_loop;

import charm.system.clock;
import io.reactor;
import util.core;

export namespace charm::system {
    enum class LoopPhase : util::u8 {
        io = 0,
        update = 1,
        render = 2,
        idle = 3
    };

    enum class SubmitProjection : util::u8 {
        event = 0,
        io_ready = 1,
        demand = 2,
        frame = 3,
    };

    [[nodiscard]] constexpr const char* to_text(LoopPhase phase) noexcept {
        switch (phase) {
            case LoopPhase::io: return "io";
            case LoopPhase::update: return "update";
            case LoopPhase::render: return "render";
            case LoopPhase::idle: return "idle";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr const char* to_text(SubmitProjection projection) noexcept {
        switch (projection) {
            case SubmitProjection::event: return "event-submit";
            case SubmitProjection::io_ready: return "io-ready-submit";
            case SubmitProjection::demand: return "demand-submit";
            case SubmitProjection::frame: return "frame-submit";
        }
        return "unknown-submit";
    }

    using LoopFn = void (*)(void* ctx, ClockTick now_us, ClockTick dt_us) noexcept;

    struct LoopStep {
        LoopPhase phase{};
        SubmitProjection submit_projection{SubmitProjection::event};
        void* ctx{nullptr};
        LoopFn fn{nullptr};
        const char* name{nullptr};
    };

    template <std::size_t Capacity>
    class RunLoop {
    public:
        constexpr RunLoop() noexcept = default;

        void bind_clock(Clock& clock) noexcept { clock_.reset(clock); }

        [[nodiscard]] bool add_step(LoopPhase phase,
                                    SubmitProjection projection,
                                    LoopFn fn,
                                    void* ctx,
                                    const char* name = nullptr) noexcept {
            if (count_ >= Capacity || fn == nullptr) {
                return false;
            }
            steps_[count_++] = LoopStep{phase, projection, ctx, fn, name};
            return true;
        }

        [[nodiscard]] std::size_t format_audit_json(char* out, std::size_t max) const noexcept {
            if (!out || max == 0) {
                return 0;
            }
            std::size_t offset = 0;
            offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "["));
            for (std::size_t i = 0; i < count_ && offset < max; ++i) {
                const auto& step = steps_[i];
                if (i > 0) {
                    offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, ","));
                }
                offset += static_cast<std::size_t>(std::snprintf(
                    out + offset,
                    max - offset,
                    "{\"index\":%llu,\"phase\":\"%s\",\"submit\":\"%s\",\"name\":\"%s\"}",
                    static_cast<unsigned long long>(i),
                    to_text(step.phase),
                    to_text(step.submit_projection),
                    step.name ? step.name : ""));
            }
            if (offset < max) {
                offset += static_cast<std::size_t>(std::snprintf(out + offset, max - offset, "]"));
            }
            if (offset >= max) {
                out[max - 1] = '\0';
                return max - 1;
            }
            return offset;
        }

        void reset_time() noexcept { last_us_ = 0; }

        void run_once() noexcept {
            const ClockTick now_us = clock_.now_us();
            const ClockTick dt_us = (last_us_ == 0) ? 0 : (now_us - last_us_);
            last_us_ = now_us;
            run_phase(LoopPhase::io, now_us, dt_us);
            run_phase(LoopPhase::update, now_us, dt_us);
            run_phase(LoopPhase::render, now_us, dt_us);
            run_phase(LoopPhase::idle, now_us, dt_us);
        }

    private:
        void run_phase(LoopPhase phase, ClockTick now_us, ClockTick dt_us) noexcept {
            for (std::size_t i = 0; i < count_; ++i) {
                const auto& step = steps_[i];
                if (step.phase != phase || step.fn == nullptr) {
                    continue;
                }
                step.fn(step.ctx, now_us, dt_us);
            }
        }

        ClockRef clock_{};
        std::array<LoopStep, Capacity> steps_{};
        std::size_t count_{0};
        ClockTick last_us_{0};
    };

    template <typename Scheduler>
    struct SchedulerLoopStep {
        Scheduler* scheduler{nullptr};
        std::size_t budget{0};

        static void run(void* ctx, ClockTick, ClockTick) noexcept {
            auto* self = static_cast<SchedulerLoopStep*>(ctx);
            if (!self || !self->scheduler) {
                return;
            }
            if (self->budget == 0) {
                (void)self->scheduler->run_once();
            } else {
                (void)self->scheduler->run_budget(self->budget);
            }
        }
    };

    template <typename Reactor>
    struct ReactorLoopStep {
        Reactor* reactor{nullptr};
        std::size_t budget{8};

        static void run(void* ctx, ClockTick, ClockTick) noexcept {
            auto* self = static_cast<ReactorLoopStep*>(ctx);
            if (!self || !self->reactor) {
                return;
            }
            (void)self->reactor->drain(static_cast<util::usize>(self->budget));
        }
    };

    template <std::size_t Capacity, typename Scheduler>
    [[nodiscard]] inline bool add_scheduler_step(RunLoop<Capacity>& loop,
                                                 Scheduler& scheduler,
                                                 SchedulerLoopStep<Scheduler>& step,
                                                 LoopPhase phase,
                                                 std::size_t budget = 0,
                                                 const char* name = nullptr) noexcept {
        step.scheduler = &scheduler;
        step.budget = budget;
        return loop.add_step(phase, SubmitProjection::event, &SchedulerLoopStep<Scheduler>::run, &step, name);
    }

    template <std::size_t Capacity, typename Reactor>
    [[nodiscard]] inline bool add_reactor_step(RunLoop<Capacity>& loop,
                                               Reactor& reactor,
                                               ReactorLoopStep<Reactor>& step,
                                               LoopPhase phase,
                                               std::size_t budget = 8,
                                               const char* name = nullptr) noexcept {
        step.reactor = &reactor;
        step.budget = budget;
        return loop.add_step(phase, SubmitProjection::io_ready, &ReactorLoopStep<Reactor>::run, &step, name);
    }
}