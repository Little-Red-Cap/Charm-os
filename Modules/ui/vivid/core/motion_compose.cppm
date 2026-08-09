export module charm.ui.scene.motion_compose;

export import charm.ui.scene.motion_transition;

export namespace ui::scene {
    struct MotionComposeRequest {
        SnapshotHandle source{};
        MotionTransitionFrame frame{};
        Rect clip{};
        bool has_clip{false};
    };

    struct MotionComposeBridgeResult {
        bool valid{false};
        LayerComposeSpec spec{};
    };

    struct MotionComposeDryRunResult {
        bool valid{false};
        MotionComposeBridgeResult bridge{};
        LayerComposePlan plan{};
        LayerBudgetResult budget{};
    };

    struct MotionComposeProfileDecision {
        bool valid{false};
        LayerProfileDecision profile{};
    };

    constexpr MotionComposeBridgeResult make_motion_compose_spec(
        const MotionComposeRequest& request) noexcept {
        if (!request.source || !request.frame.motion.compose) {
            return {};
        }
        return {
            .valid = true,
            .spec = {
                .source = request.source,
                .transform = request.frame.motion.transform,
                .clip = request.clip,
                .has_clip = request.has_clip,
            },
        };
    }

    constexpr MotionComposeProfileDecision decide_motion_compose_profile(
        LayerProfile requested,
        const MotionComposeDryRunResult& dry_run) noexcept {
        if (!dry_run.valid) {
            return {};
        }
        return {
            .valid = true,
            .profile = decide_layer_profile(requested, dry_run.budget),
        };
    }
}
