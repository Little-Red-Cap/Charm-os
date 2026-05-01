module;

#include <cstdint>

export module charm.ui.scene.motion_transition;

export import charm.ui.scene.motion_recipe;

export namespace ui::scene {
    enum class MotionTransitionState : std::uint8_t {
        Idle,
        Running,
        Finished,
        Canceled,
    };

    struct MotionTransitionSpec {
        MotionRecipe recipe{};
        LayerProfile profile{LayerProfile::Rich};
        std::uint64_t start_ms{0};
    };

    struct MotionTransitionFrame {
        MotionTransitionState state{MotionTransitionState::Idle};
        LayerMotionFrame motion{};
    };

    class MotionTransitionRunner {
    public:
        constexpr void begin(const MotionTransitionSpec& spec) noexcept {
            spec_ = spec;
            state_ = MotionTransitionState::Running;
        }

        constexpr void cancel() noexcept {
            state_ = MotionTransitionState::Canceled;
        }

        [[nodiscard]] constexpr MotionTransitionFrame sample(std::uint64_t now_ms) noexcept {
            if (state_ == MotionTransitionState::Idle) {
                return {};
            }
            if (state_ == MotionTransitionState::Canceled) {
                return {
                    .state = state_,
                    .motion = sample_canceled(),
                };
            }
            if (state_ == MotionTransitionState::Finished) {
                return {
                    .state = state_,
                    .motion = sample_finished(),
                };
            }

            const auto motion = sample_motion_recipe(
                spec_.recipe,
                spec_.profile,
                spec_.start_ms,
                now_ms);
            state_ = motion.tick.finished
                ? MotionTransitionState::Finished
                : MotionTransitionState::Running;
            return {
                .state = state_,
                .motion = motion,
            };
        }

        constexpr void reset() noexcept {
            spec_ = {};
            state_ = MotionTransitionState::Idle;
        }

        [[nodiscard]] constexpr MotionTransitionState state() const noexcept {
            return state_;
        }

        [[nodiscard]] constexpr bool active() const noexcept {
            return state_ == MotionTransitionState::Running;
        }

        [[nodiscard]] constexpr bool done() const noexcept {
            return state_ == MotionTransitionState::Finished ||
                   state_ == MotionTransitionState::Canceled;
        }

    private:
        [[nodiscard]] constexpr LayerMotionFrame sample_canceled() const noexcept {
            const auto transform = motion_recipe_to_transform(spec_.recipe);
            return {
                .tick = {
                    .tier = motion_tier_from_layer_profile(spec_.profile),
                    .elapsed_ms = 0,
                    .sampled_elapsed_ms = spec_.recipe.duration_ms,
                    .progress = 1.0f,
                    .active = false,
                    .finished = true,
                    .should_sample = false,
                },
                .transform = transform,
                .compose = false,
            };
        }

        [[nodiscard]] constexpr LayerMotionFrame sample_finished() const noexcept {
            return sample_motion_recipe(
                spec_.recipe,
                spec_.profile,
                spec_.start_ms,
                spec_.start_ms + spec_.recipe.duration_ms);
        }

        MotionTransitionSpec spec_{};
        MotionTransitionState state_{MotionTransitionState::Idle};
    };
}
