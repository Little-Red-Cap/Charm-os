module;

#include <cstdint>

export module charm.ui.scene.motion_page_transition;

export import charm.ui.scene.motion_execute;

export namespace ui::scene {
    struct PageMotionTransitionSpec {
        SnapshotSpec snapshot{};
        MotionRecipe recipe{};
        LayerProfile profile{LayerProfile::Rich};
        std::uint64_t start_ms{0};
        bool hide_live_root{true};
    };

    struct PageMotionTransitionFrame {
        MotionTransitionFrame transition{};
        MotionComposeExecuteResult compose{};
    };

    class PageMotionTransition {
    public:
        [[nodiscard]] LayerCaptureResult begin(Scene& scene,
                                               SceneAccess access,
                                               PageLayer& page,
                                               const PageMotionTransitionSpec& spec) noexcept {
            page_ = &page;
            snapshot_ = {};
            const auto capture = page.freeze(scene, access, spec.snapshot, spec.hide_live_root);
            if (!capture.ok()) {
                runner_.reset();
                page_ = nullptr;
                return capture;
            }
            snapshot_ = capture.handle;
            page.mark_transitioning();
            runner_.begin({
                .recipe = spec.recipe,
                .profile = spec.profile,
                .start_ms = spec.start_ms,
            });
            return capture;
        }

        [[nodiscard]] PageMotionTransitionFrame sample(Scene& scene,
                                                       std::uint64_t now_ms) noexcept {
            if (!page_ || !snapshot_) {
                return {};
            }
            const auto frame = runner_.sample(now_ms);
            const auto compose = execute_motion_compose(scene, {
                .source = snapshot_,
                .frame = frame,
            });
            return {
                .transition = frame,
                .compose = compose,
            };
        }

        void finish(Scene& scene, SceneAccess access, bool show_live_root = true) noexcept {
            if (page_) {
                page_->thaw(scene, access, show_live_root);
            }
            reset_tracking();
        }

        void cancel(Scene& scene, SceneAccess access, bool show_live_root = true) noexcept {
            runner_.cancel();
            finish(scene, access, show_live_root);
        }

        [[nodiscard]] bool active() const noexcept {
            return page_ && runner_.active();
        }

        [[nodiscard]] bool done() const noexcept {
            return runner_.done();
        }

        [[nodiscard]] SnapshotHandle snapshot() const noexcept {
            return snapshot_;
        }

        [[nodiscard]] MotionTransitionTrace trace() const noexcept {
            return runner_.trace();
        }

    private:
        void reset_tracking() noexcept {
            page_ = nullptr;
            snapshot_ = {};
            runner_.reset();
        }

        PageLayer* page_{nullptr};
        SnapshotHandle snapshot_{};
        MotionTransitionRunner runner_{};
    };
}
