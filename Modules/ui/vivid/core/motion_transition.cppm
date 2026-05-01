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

    struct MotionTransitionTrace {
        MotionRecipeKind recipe_kind{MotionRecipeKind::Cut};
        LayerProfile profile{LayerProfile::Rich};
        MotionTier tier{MotionTier::Rich60Fps};
        MotionTransitionState last_state{MotionTransitionState::Idle};
        std::uint16_t begin_count{0};
        std::uint16_t sample_count{0};
        std::uint16_t compose_count{0};
        std::uint16_t finish_count{0};
        std::uint16_t cancel_count{0};
        std::uint64_t last_now_ms{0};
        std::uint64_t last_elapsed_ms{0};
        std::uint64_t last_sampled_elapsed_ms{0};
        LayerTransform last_transform{};
    };

    class MotionTransitionRunner {
    public:
        constexpr void begin(const MotionTransitionSpec& spec) noexcept {
            spec_ = spec;
            state_ = MotionTransitionState::Running;
            trace_ = {
                .recipe_kind = spec.recipe.kind,
                .profile = spec.profile,
                .tier = motion_tier_from_layer_profile(spec.profile),
                .last_state = state_,
                .begin_count = 1,
            };
        }

        constexpr void cancel() noexcept {
            if (state_ != MotionTransitionState::Running) return;
            state_ = MotionTransitionState::Canceled;
            trace_.last_state = state_;
            ++trace_.cancel_count;
        }

        [[nodiscard]] constexpr MotionTransitionFrame sample(std::uint64_t now_ms) noexcept {
            if (state_ == MotionTransitionState::Idle) {
                return {};
            }
            if (state_ == MotionTransitionState::Canceled) {
                return record_frame(now_ms, {
                    .state = state_,
                    .motion = sample_canceled(),
                });
            }
            if (state_ == MotionTransitionState::Finished) {
                return record_frame(now_ms, {
                    .state = state_,
                    .motion = sample_finished(),
                });
            }

            const auto motion = sample_motion_recipe(
                spec_.recipe,
                spec_.profile,
                spec_.start_ms,
                now_ms);
            state_ = motion.tick.finished
                ? MotionTransitionState::Finished
                : MotionTransitionState::Running;
            auto frame = MotionTransitionFrame{
                .state = state_,
                .motion = motion,
            };
            if (state_ == MotionTransitionState::Finished) {
                ++trace_.finish_count;
            }
            return record_frame(now_ms, frame);
        }

        constexpr void reset() noexcept {
            spec_ = {};
            state_ = MotionTransitionState::Idle;
            trace_ = {};
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

        [[nodiscard]] constexpr MotionTransitionTrace trace() const noexcept {
            return trace_;
        }

    private:
        [[nodiscard]] constexpr MotionTransitionFrame record_frame(
            std::uint64_t now_ms,
            MotionTransitionFrame frame) noexcept {
            ++trace_.sample_count;
            if (frame.motion.compose) {
                ++trace_.compose_count;
            }
            trace_.last_state = frame.state;
            trace_.last_now_ms = now_ms;
            trace_.last_elapsed_ms = frame.motion.tick.elapsed_ms;
            trace_.last_sampled_elapsed_ms = frame.motion.tick.sampled_elapsed_ms;
            trace_.last_transform = frame.motion.transform;
            return frame;
        }

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
        MotionTransitionTrace trace_{};
    };
}
