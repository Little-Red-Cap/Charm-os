module;

#include <array>
#include <cstddef>
#include <cstdint>

export module charm.system.run_loop;

import charm.system.clock;
import util.core;

export namespace charm::system {
    enum class LoopPhase : util::u8 {
        io = 0,
        update = 1,
        render = 2,
        idle = 3
    };

    using LoopFn = void (*)(void* ctx, ClockTick now_us, ClockTick dt_us) noexcept;

    struct LoopStep {
        LoopPhase phase{};
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
                                    LoopFn fn,
                                    void* ctx,
                                    const char* name = nullptr) noexcept {
            if (count_ >= Capacity || fn == nullptr) {
                return false;
            }
            steps_[count_++] = LoopStep{phase, ctx, fn, name};
            return true;
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
}
