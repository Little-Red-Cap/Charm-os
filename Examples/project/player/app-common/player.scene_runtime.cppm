module;

#include <cstdint>

export module player.scene_runtime;

import charm.core.geometry;
import charm.core.handle;
export import charm.ui.scene;

export namespace player {
    struct PlayerLayerCaptureResult {
        ::ui::scene::LayerCaptureStatus status{::ui::scene::LayerCaptureStatus::NoSnapshotSlot};
        ::ui::scene::SnapshotHandle handle{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == ::ui::scene::LayerCaptureStatus::Ok;
        }
    };

    struct PlayerLayerReplayResult {
        ::ui::scene::LayerReplayStatus status{::ui::scene::LayerReplayStatus::InvalidPlan};
        ::ui::scene::SnapshotHandle source{};
        ::ui::scene::SnapshotKind kind{::ui::scene::SnapshotKind::EmptyFallback};
        Rect target_bounds{};
        std::uint64_t alpha_blend_count{0};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == ::ui::scene::LayerReplayStatus::Ok;
        }
    };

    struct PlayerSceneRuntime {
        using ReleaseSnapshotFn = bool (*)(void*, ::ui::scene::SnapshotHandle) noexcept;
        using MarkSnapshotStaleFn = bool (*)(void*, ::ui::scene::SnapshotHandle) noexcept;
        using LayerStatsFn = ::ui::scene::LayerStats (*)(void*) noexcept;
        using CaptureSnapshotFn = PlayerLayerCaptureResult (*)(void*, const ::ui::scene::SnapshotSpec&) noexcept;
        using MakeComposePlanFn = ::ui::scene::LayerComposePlan (*)(void*, const ::ui::scene::LayerComposeSpec&) noexcept;
        using ComposePixelSnapshotFn = PlayerLayerReplayResult (*)(void*, const ::ui::scene::LayerComposePlan&) noexcept;

        void* ctx{nullptr};
        ::ui::scene::SceneAccess access{};
        ReleaseSnapshotFn release_snapshot_fn{nullptr};
        MarkSnapshotStaleFn mark_snapshot_stale_fn{nullptr};
        LayerStatsFn layer_stats_fn{nullptr};
        CaptureSnapshotFn capture_snapshot_fn{nullptr};
        MakeComposePlanFn make_compose_plan_fn{nullptr};
        ComposePixelSnapshotFn compose_pixel_snapshot_fn{nullptr};

        [[nodiscard]] bool valid() const noexcept {
            return ctx != nullptr
                && access.valid()
                && release_snapshot_fn
                && mark_snapshot_stale_fn
                && layer_stats_fn
                && capture_snapshot_fn
                && make_compose_plan_fn
                && compose_pixel_snapshot_fn;
        }

        [[nodiscard]] bool release_snapshot(::ui::scene::SnapshotHandle handle) const noexcept {
            return valid() && release_snapshot_fn(ctx, handle);
        }

        [[nodiscard]] bool mark_snapshot_stale(::ui::scene::SnapshotHandle handle) const noexcept {
            return valid() && mark_snapshot_stale_fn(ctx, handle);
        }

        [[nodiscard]] ::ui::scene::LayerStats layer_stats() const noexcept {
            return valid() ? layer_stats_fn(ctx) : ::ui::scene::LayerStats{};
        }

        [[nodiscard]] PlayerLayerCaptureResult capture_snapshot_result(
            const ::ui::scene::SnapshotSpec& spec) const noexcept {
            return valid() ? capture_snapshot_fn(ctx, spec) : PlayerLayerCaptureResult{};
        }

        [[nodiscard]] ::ui::scene::LayerComposePlan make_snapshot_compose_plan(
            const ::ui::scene::LayerComposeSpec& spec) const noexcept {
            return valid() ? make_compose_plan_fn(ctx, spec) : ::ui::scene::LayerComposePlan{};
        }

        [[nodiscard]] PlayerLayerReplayResult compose_pixel_snapshot(
            const ::ui::scene::LayerComposePlan& plan) const noexcept {
            return valid() ? compose_pixel_snapshot_fn(ctx, plan) : PlayerLayerReplayResult{};
        }
    };

    class PlayerPageLayer {
    public:
        constexpr PlayerPageLayer() noexcept {}
        explicit PlayerPageLayer(WidgetHandle root) noexcept : root_(root) {}

        void set_root(WidgetHandle root) noexcept {
            root_ = root;
            if (!root_) {
                visible_ = false;
                snapshot_ = {};
                state_ = ::ui::scene::LayerState::Hidden;
            } else if (state_ == ::ui::scene::LayerState::Hidden || !snapshot_) {
                state_ = visible_ ? ::ui::scene::LayerState::Live : ::ui::scene::LayerState::Hidden;
            }
        }

        WidgetHandle root() const noexcept { return root_; }
        void set_hooks(const ::ui::scene::PageHooks& hooks) noexcept { hooks_ = hooks; }
        bool visible() const noexcept { return visible_; }
        ::ui::scene::LayerState state() const noexcept { return state_; }
        bool live() const noexcept { return state_ == ::ui::scene::LayerState::Live; }
        bool frozen() const noexcept { return state_ == ::ui::scene::LayerState::Frozen; }
        bool transitioning() const noexcept { return state_ == ::ui::scene::LayerState::Transitioning; }
        bool stale_snapshot() const noexcept { return state_ == ::ui::scene::LayerState::StaleSnapshot; }
        ::ui::scene::SnapshotHandle snapshot() const noexcept { return snapshot_; }

        void show(::ui::scene::SceneAccess& access) noexcept { set_visible(access, true); }
        void hide(::ui::scene::SceneAccess& access) noexcept { set_visible(access, false); }

        void set_visible(::ui::scene::SceneAccess& access, bool on) noexcept {
            if (!root_) return;
            access.set_visible(root_, on);
            const bool changed = (visible_ != on);
            visible_ = on;
            if (!snapshot_ || state_ == ::ui::scene::LayerState::Live ||
                state_ == ::ui::scene::LayerState::Hidden) {
                state_ = on ? ::ui::scene::LayerState::Live : ::ui::scene::LayerState::Hidden;
            }
            if (changed) {
                if (on) {
                    if (hooks_.on_show) hooks_.on_show(access, root_, hooks_.ctx);
                } else {
                    if (hooks_.on_hide) hooks_.on_hide(access, root_, hooks_.ctx);
                }
            }
        }

        [[nodiscard]] PlayerLayerCaptureResult freeze(PlayerSceneRuntime& runtime,
                                                      const ::ui::scene::SnapshotSpec& spec) noexcept {
            if (!root_) return {};
            (void)release_snapshot(runtime);
            PlayerLayerCaptureResult result = runtime.capture_snapshot_result(spec);
            if (result.ok() && result.handle) {
                snapshot_ = result.handle;
                state_ = ::ui::scene::LayerState::Frozen;
            }
            return result;
        }

        [[nodiscard]] PlayerLayerCaptureResult freeze(PlayerSceneRuntime& runtime,
                                                      ::ui::scene::SceneAccess access,
                                                      const ::ui::scene::SnapshotSpec& spec,
                                                      bool hide_live_root) noexcept {
            const auto result = freeze(runtime, spec);
            if (result.ok() && hide_live_root) {
                set_visible(access, false);
                state_ = ::ui::scene::LayerState::Frozen;
            }
            return result;
        }

        void mark_transitioning() noexcept {
            if (snapshot_) state_ = ::ui::scene::LayerState::Transitioning;
        }

        bool mark_stale(PlayerSceneRuntime& runtime) noexcept {
            if (!snapshot_) return false;
            const bool marked = runtime.mark_snapshot_stale(snapshot_);
            if (marked) state_ = ::ui::scene::LayerState::StaleSnapshot;
            return marked;
        }

        bool release_snapshot(PlayerSceneRuntime& runtime) noexcept {
            if (!snapshot_) return false;
            const auto handle = snapshot_;
            snapshot_ = {};
            const bool released = runtime.release_snapshot(handle);
            state_ = visible_ ? ::ui::scene::LayerState::Live : ::ui::scene::LayerState::Hidden;
            return released;
        }

        void thaw(PlayerSceneRuntime& runtime,
                  ::ui::scene::SceneAccess access,
                  bool show_live_root = true) noexcept {
            (void)release_snapshot(runtime);
            if (show_live_root) {
                set_visible(access, true);
            } else {
                state_ = visible_ ? ::ui::scene::LayerState::Live : ::ui::scene::LayerState::Hidden;
            }
        }

        void reset_snapshot_tracking() noexcept {
            snapshot_ = {};
            state_ = visible_ ? ::ui::scene::LayerState::Live : ::ui::scene::LayerState::Hidden;
        }

    private:
        WidgetHandle root_{};
        bool visible_{false};
        ::ui::scene::LayerState state_{::ui::scene::LayerState::Hidden};
        ::ui::scene::SnapshotHandle snapshot_{};
        ::ui::scene::PageHooks hooks_{};
    };
}
