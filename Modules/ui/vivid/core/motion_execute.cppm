module;

export module charm.ui.scene.motion_execute;

export import charm.ui.scene;
export import charm.ui.scene.motion_compose;

export namespace ui::scene {
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

    [[nodiscard]] MotionComposeDryRunResult dry_run_motion_compose(
        const MotionComposeRequest& request,
        Scene& scene,
        const LayerBudget& budget = {}) noexcept {
        const auto bridge = make_motion_compose_spec(request);
        if (!bridge.valid) {
            return {};
        }
        const auto plan = scene.make_snapshot_compose_plan(bridge.spec);
        const auto budget_result = scene.check_layer_budget(plan, budget);
        return {
            .valid = plan.valid,
            .bridge = bridge,
            .plan = plan,
            .budget = budget_result,
        };
    }

    struct MotionComposeExecuteResult {
        bool valid{false};
        MotionComposeBridgeResult bridge{};
        LayerComposePlan plan{};
        LayerReplayResult replay{};
    };

    [[nodiscard]] MotionComposeExecuteResult execute_motion_compose(
        Scene& scene,
        const MotionComposeRequest& request) noexcept {
        const auto bridge = make_motion_compose_spec(request);
        if (!bridge.valid) {
            return {};
        }
        const auto plan = scene.make_snapshot_compose_plan(bridge.spec);
        if (!plan.valid) {
            return {
                .valid = false,
                .bridge = bridge,
                .plan = plan,
            };
        }
        LayerReplayResult replay{};
        switch (plan.kind) {
        case SnapshotKind::CommandBuffer:
            replay = scene.replay_command_snapshot(plan);
            break;
        case SnapshotKind::PixelSurface:
            replay = scene.compose_pixel_snapshot(plan);
            break;
        case SnapshotKind::EmptyFallback:
            replay = {};
            break;
        }
        return {
            .valid = replay.ok(),
            .bridge = bridge,
            .plan = plan,
            .replay = replay,
        };
    }
}
