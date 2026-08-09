module;

#include <cstdint>

export module charm.ui.scene.page_transition;

export import charm.ui.scene.motion_execute;

export namespace ui::scene {
    enum class PageTransitionState : std::uint8_t {
        Idle,
        Admitting,
        FreezingSource,
        PreparingDestination,
        FreezingDestination,
        Composing,
        Committing,
        Aborting,
    };

    enum class PageTransitionBeginStatus : std::uint8_t {
        Started,
        StaticCut,
        Rejected,
        PrepareFailed,
        SourceCaptureFailed,
        DestinationCaptureFailed,
    };

    using PageTransitionPrepareFn =
        bool (*)(Scene& scene, SceneAccess access, PageLayer& destination, void* ctx) noexcept;

    struct PageTransitionSpec {
        PageLayer* source{nullptr};
        PageLayer* destination{nullptr};
        SnapshotSpec source_snapshot{};
        SnapshotSpec destination_snapshot{};
        MotionRecipe recipe{};
        LayerProfile requested_profile{LayerProfile::Rich};
        LayerBudget budget{};
        std::uint64_t start_ms{0};
        bool hide_source_live_root{true};
        bool hide_destination_live_root{true};
        PageTransitionPrepareFn prepare_destination{nullptr};
        void* prepare_ctx{nullptr};
    };

    struct PageTransitionBeginResult {
        PageTransitionBeginStatus status{PageTransitionBeginStatus::Rejected};
        PageTransitionState state{PageTransitionState::Idle};
        LayerAdmission admission{LayerAdmission::Reject};
        LayerCaptureResult source_capture{};
        LayerCaptureResult destination_capture{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == PageTransitionBeginStatus::Started
                || status == PageTransitionBeginStatus::StaticCut;
        }

        [[nodiscard]] constexpr bool started() const noexcept {
            return status == PageTransitionBeginStatus::Started;
        }

        [[nodiscard]] constexpr bool static_cut() const noexcept {
            return status == PageTransitionBeginStatus::StaticCut;
        }
    };

    struct PageTransitionFrame {
        bool valid{false};
        MotionTransitionFrame transition{};
        MotionComposeExecuteResult destination{};
        MotionComposeExecuteResult source{};
    };

    struct PageTransitionTrace {
        PageTransitionBeginStatus begin_status{PageTransitionBeginStatus::Rejected};
        PageTransitionState last_state{PageTransitionState::Idle};
        LayerAdmission admission{LayerAdmission::Reject};
        LayerFallbackReason fallback_reason{LayerFallbackReason::None};
        LayerProfile requested_profile{LayerProfile::Rich};
        LayerProfile effective_profile{LayerProfile::Rich};
        std::uint16_t begin_count{0};
        std::uint16_t interrupt_count{0};
        std::uint16_t sample_count{0};
        std::uint16_t commit_count{0};
        std::uint16_t abort_count{0};
        std::uint16_t static_cut_count{0};
        std::uint16_t source_capture_count{0};
        std::uint16_t destination_capture_count{0};
        LayerCaptureStatus source_capture_status{LayerCaptureStatus::NoSnapshotSlot};
        LayerCaptureStatus destination_capture_status{LayerCaptureStatus::NoSnapshotSlot};
        MotionTransitionTrace motion{};
    };

    struct PageTransitionLedger {
        PageTransitionBeginStatus begin_status{PageTransitionBeginStatus::Rejected};
        PageTransitionState final_state{PageTransitionState::Idle};
        LayerAdmission admission{LayerAdmission::Reject};
        LayerFallbackReason fallback_reason{LayerFallbackReason::None};
        LayerProfile requested_profile{LayerProfile::Rich};
        LayerProfile effective_profile{LayerProfile::Rich};
        std::uint16_t begin_count{0};
        std::uint16_t interrupt_count{0};
        std::uint16_t sample_count{0};
        std::uint16_t commit_count{0};
        std::uint16_t abort_count{0};
        std::uint16_t static_cut_count{0};
        std::uint16_t source_capture_count{0};
        std::uint16_t destination_capture_count{0};
        LayerCaptureStatus source_capture_status{LayerCaptureStatus::NoSnapshotSlot};
        LayerCaptureStatus destination_capture_status{LayerCaptureStatus::NoSnapshotSlot};
        std::uint32_t source_bytes{0};
        std::uint32_t destination_bytes{0};
        std::uint32_t peak_layer_bytes{0};
        std::uint32_t destination_composite_pixels{0};
        std::uint32_t source_composite_pixels{0};
        std::uint32_t total_composite_pixels{0};
        bool committed{false};
        bool aborted{false};
        bool static_cut{false};
        bool interrupted{false};
        bool snapshots_released{true};
    };

    [[nodiscard]] constexpr const char* page_transition_begin_status_name(
        PageTransitionBeginStatus status) noexcept {
        switch (status) {
        case PageTransitionBeginStatus::Started: return "started";
        case PageTransitionBeginStatus::StaticCut: return "static_cut";
        case PageTransitionBeginStatus::Rejected: return "rejected";
        case PageTransitionBeginStatus::PrepareFailed: return "prepare_failed";
        case PageTransitionBeginStatus::SourceCaptureFailed: return "source_capture_failed";
        case PageTransitionBeginStatus::DestinationCaptureFailed: return "destination_capture_failed";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr const char* page_transition_state_name(
        PageTransitionState state) noexcept {
        switch (state) {
        case PageTransitionState::Idle: return "idle";
        case PageTransitionState::Admitting: return "admitting";
        case PageTransitionState::FreezingSource: return "freezing_source";
        case PageTransitionState::PreparingDestination: return "preparing_destination";
        case PageTransitionState::FreezingDestination: return "freezing_destination";
        case PageTransitionState::Composing: return "composing";
        case PageTransitionState::Committing: return "committing";
        case PageTransitionState::Aborting: return "aborting";
        }
        return "unknown";
    }

    class PageTransitionRunner {
    public:
        [[nodiscard]] PageTransitionBeginResult begin(Scene& scene,
                                                     SceneAccess access,
                                                     const PageTransitionSpec& spec) noexcept {
            const bool interrupted = state_ != PageTransitionState::Idle;
            const auto previous_interrupts = trace_.interrupt_count;
            if (state_ != PageTransitionState::Idle) {
                cancel(scene, access);
            }
            reset_transition_trace(spec.requested_profile);
            trace_.interrupt_count = previous_interrupts + (interrupted ? 1u : 0u);
            source_ = spec.source;
            destination_ = spec.destination;
            if (!source_ || !destination_) {
                trace_.begin_status = PageTransitionBeginStatus::Rejected;
                clear_tracking();
                return begin_result(PageTransitionBeginStatus::Rejected);
            }

            source_was_visible_ = source_->visible();
            destination_was_visible_ = destination_->visible();
            state_ = PageTransitionState::Admitting;
            trace_.last_state = state_;
            trace_.admission = decide_admission(spec);
            if (trace_.admission == LayerAdmission::Reject) {
                trace_.begin_status = PageTransitionBeginStatus::Rejected;
                clear_tracking();
                return begin_result(PageTransitionBeginStatus::Rejected);
            }
            if (trace_.admission == LayerAdmission::StaticCut) {
                state_ = PageTransitionState::PreparingDestination;
                trace_.last_state = state_;
                if (spec.prepare_destination &&
                    !spec.prepare_destination(scene, access, *destination_, spec.prepare_ctx)) {
                    trace_.begin_status = PageTransitionBeginStatus::PrepareFailed;
                    abort_begin_failure(scene, access);
                    return begin_result(PageTransitionBeginStatus::PrepareFailed);
                }
                return complete_static_cut(scene,
                                           access,
                                           {},
                                           static_cut_fallback_reason(spec));
            }

            state_ = PageTransitionState::FreezingSource;
            trace_.last_state = state_;
            const auto source_capture =
                source_->freeze(scene,
                                access,
                                admitted_snapshot_spec(spec.source_snapshot, trace_.admission),
                                spec.hide_source_live_root);
            trace_.source_capture_status = source_capture.status;
            if (!source_capture.ok()) {
                trace_.begin_status = PageTransitionBeginStatus::SourceCaptureFailed;
                abort_begin_failure(scene, access);
                return begin_result(PageTransitionBeginStatus::SourceCaptureFailed,
                                    source_capture);
            }
            source_snapshot_ = source_capture.handle;
            ++trace_.source_capture_count;
            ledger_.source_bytes = source_capture.bytes;
            ledger_.peak_layer_bytes = ledger_.source_bytes + ledger_.destination_bytes;
            source_->mark_transitioning();

            state_ = PageTransitionState::PreparingDestination;
            trace_.last_state = state_;
            if (spec.prepare_destination &&
                !spec.prepare_destination(scene, access, *destination_, spec.prepare_ctx)) {
                trace_.begin_status = PageTransitionBeginStatus::PrepareFailed;
                abort_begin_failure(scene, access);
                return begin_result(PageTransitionBeginStatus::PrepareFailed, source_capture);
            }

            if (trace_.admission == LayerAdmission::CommandSnapshot) {
                const auto fallback_reason = command_snapshot_fallback_reason(scene, spec);
                if (fallback_reason != LayerFallbackReason::None) {
                    return complete_static_cut(scene,
                                               access,
                                               source_capture,
                                               fallback_reason);
                }
            }

            if (trace_.admission == LayerAdmission::PixelSingle
                || trace_.admission == LayerAdmission::CommandSnapshot) {
                destination_->thaw(scene, access, true);
                runner_.begin({
                    .recipe = spec.recipe,
                    .profile = spec.requested_profile,
                    .start_ms = spec.start_ms,
                });
                state_ = PageTransitionState::Composing;
                trace_.last_state = state_;
                trace_.begin_status = PageTransitionBeginStatus::Started;
                return begin_result(PageTransitionBeginStatus::Started,
                                    source_capture);
            }

            state_ = PageTransitionState::FreezingDestination;
            trace_.last_state = state_;
            const auto destination_capture =
                destination_->freeze(scene,
                                     access,
                                     admitted_snapshot_spec(spec.destination_snapshot,
                                                            LayerAdmission::PixelDouble),
                                     spec.hide_destination_live_root);
            trace_.destination_capture_status = destination_capture.status;
            if (!destination_capture.ok()) {
                trace_.begin_status = PageTransitionBeginStatus::DestinationCaptureFailed;
                abort_begin_failure(scene, access);
                return begin_result(PageTransitionBeginStatus::DestinationCaptureFailed,
                                    source_capture,
                                    destination_capture);
            }
            destination_snapshot_ = destination_capture.handle;
            ++trace_.destination_capture_count;
            ledger_.destination_bytes = destination_capture.bytes;
            ledger_.peak_layer_bytes = ledger_.source_bytes + ledger_.destination_bytes;
            destination_->mark_transitioning();

            runner_.begin({
                .recipe = spec.recipe,
                .profile = spec.requested_profile,
                .start_ms = spec.start_ms,
            });
            state_ = PageTransitionState::Composing;
            trace_.last_state = state_;
            trace_.begin_status = PageTransitionBeginStatus::Started;
            return begin_result(PageTransitionBeginStatus::Started,
                                source_capture,
                                destination_capture);
        }

        [[nodiscard]] PageTransitionFrame sample(Scene& scene,
                                                 std::uint64_t now_ms) noexcept {
            if (state_ != PageTransitionState::Composing ||
                !source_snapshot_) {
                return {};
            }
            const auto frame = runner_.sample(now_ms);
            trace_.motion = runner_.trace();
            ++trace_.sample_count;
            const MotionTransitionFrame destination_frame{
                .state = frame.state,
                .motion = {
                    .tick = frame.motion.tick,
                    .transform = {},
                    .compose = frame.motion.tick.should_sample,
                },
            };
            const auto destination = destination_snapshot_
                ? execute_motion_compose(scene, {
                      .source = destination_snapshot_,
                      .frame = destination_frame,
                  })
                : MotionComposeExecuteResult{};
            const auto source = execute_motion_compose(scene, {
                .source = source_snapshot_,
                .frame = frame,
            });
            ledger_.sample_count = trace_.sample_count;
            ledger_.destination_composite_pixels += destination.plan.composite_pixels;
            ledger_.source_composite_pixels += source.plan.composite_pixels;
            ledger_.total_composite_pixels =
                ledger_.destination_composite_pixels + ledger_.source_composite_pixels;
            return {
                .valid = destination.valid || source.valid,
                .transition = frame,
                .destination = destination,
                .source = source,
            };
        }

        void commit(Scene& scene, SceneAccess access) noexcept {
            if (state_ == PageTransitionState::Idle) return;
            state_ = PageTransitionState::Committing;
            trace_.last_state = state_;
            trace_.motion = runner_.trace();
            ++trace_.commit_count;
            ledger_.final_state = state_;
            ledger_.commit_count = trace_.commit_count;
            ledger_.committed = true;
            commit_page_truth(scene, access);
            runner_.reset();
            clear_tracking();
        }

        void cancel(Scene& scene, SceneAccess access) noexcept {
            if (state_ == PageTransitionState::Idle) return;
            state_ = PageTransitionState::Aborting;
            trace_.last_state = state_;
            runner_.cancel();
            trace_.motion = runner_.trace();
            ++trace_.abort_count;
            ledger_.final_state = state_;
            ledger_.abort_count = trace_.abort_count;
            ledger_.aborted = true;
            restore_page_truth(scene, access);
            runner_.reset();
            clear_tracking();
        }

        void reset() noexcept {
            clear_tracking();
            runner_.reset();
            trace_ = {};
            ledger_ = {};
        }

        [[nodiscard]] bool active() const noexcept {
            return state_ == PageTransitionState::Composing;
        }

        [[nodiscard]] bool idle() const noexcept {
            return state_ == PageTransitionState::Idle;
        }

        [[nodiscard]] bool done() const noexcept {
            return runner_.done();
        }

        [[nodiscard]] PageTransitionState state() const noexcept {
            return state_;
        }

        [[nodiscard]] SnapshotHandle source_snapshot() const noexcept {
            return source_snapshot_;
        }

        [[nodiscard]] SnapshotHandle destination_snapshot() const noexcept {
            return destination_snapshot_;
        }

        [[nodiscard]] PageTransitionTrace trace() const noexcept {
            return trace_;
        }

        [[nodiscard]] PageTransitionLedger ledger() const noexcept {
            return ledger_;
        }

    private:
        static constexpr SnapshotSpec admitted_snapshot_spec(
            SnapshotSpec spec,
            LayerAdmission admission) noexcept {
            if (admission == LayerAdmission::CommandSnapshot) {
                spec.preferred_kind = SnapshotKind::CommandBuffer;
            } else {
                spec.preferred_kind = SnapshotKind::PixelSurface;
                spec.preferred_format = screen_pixel_format;
            }
            return spec;
        }

        static constexpr std::uint32_t pixel_snapshot_bytes(const SnapshotSpec& spec) noexcept {
            return static_cast<std::uint32_t>(
                snapshot_pixel_bytes(screen_pixel_format, spec.bounds.w, spec.bounds.h));
        }

        static constexpr std::uint32_t max_pixel_snapshot_bytes(
            const PageTransitionSpec& spec) noexcept {
            const auto source = pixel_snapshot_bytes(spec.source_snapshot);
            const auto destination = pixel_snapshot_bytes(spec.destination_snapshot);
            return source > destination ? source : destination;
        }

        static constexpr bool command_compose_supported(
            const PageTransitionSpec& spec) noexcept {
            const auto caps = layer_profile_caps(spec.requested_profile);
            return (spec.recipe.kind != MotionRecipeKind::Slide
                    && spec.recipe.kind != MotionRecipeKind::FadeSlide)
                || caps.allow_slide;
        }

        LayerFallbackReason command_snapshot_fallback_reason(
            Scene& scene,
            const PageTransitionSpec& spec) const noexcept {
            if (!scene.snapshot_current(source_snapshot_)) {
                return LayerFallbackReason::StaleSnapshot;
            }
            const auto frame = sample_motion_recipe(spec.recipe,
                                                    spec.requested_profile,
                                                    spec.start_ms,
                                                    spec.start_ms);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = source_snapshot_,
                .transform = frame.transform,
            });
            if (!plan.valid) return LayerFallbackReason::Disabled;
            return layer_budget_fallback_reason(scene.check_layer_budget(plan, spec.budget));
        }

        static constexpr LayerAdmission decide_admission(
            const PageTransitionSpec& spec) noexcept {
            return decide_layer_admission({
                .profile = spec.requested_profile,
                .budget = spec.budget,
                .pixel_snapshot_bytes = max_pixel_snapshot_bytes(spec),
                .cache_slots = static_cast<std::uint16_t>(layer_cache_slots),
                .need_double_snapshot = true,
                .command_snapshot_enabled =
                    snapshot_command_enabled && command_compose_supported(spec),
                .pixel_snapshot_enabled = snapshot_pixel_enabled,
            });
        }

        static constexpr LayerFallbackReason static_cut_fallback_reason(
            const PageTransitionSpec& spec) noexcept {
            if (spec.requested_profile == LayerProfile::Static) {
                return LayerFallbackReason::None;
            }
            if (layer_cache_slots == 0) {
                return LayerFallbackReason::DoubleSnapshotUnsupported;
            }
            if (snapshot_command_enabled && !snapshot_pixel_enabled
                && !command_compose_supported(spec)) {
                return LayerFallbackReason::Disabled;
            }
            if (snapshot_pixel_enabled && spec.budget.max_layer_bytes > 0
                && max_pixel_snapshot_bytes(spec) > spec.budget.max_layer_bytes) {
                return LayerFallbackReason::LayerBytesOver;
            }
            return LayerFallbackReason::Disabled;
        }

        PageTransitionBeginResult complete_static_cut(
            Scene& scene,
            SceneAccess access,
            LayerCaptureResult source_capture = {},
            LayerFallbackReason fallback_reason = LayerFallbackReason::None) noexcept {
            state_ = PageTransitionState::Committing;
            trace_.last_state = state_;
            trace_.admission = LayerAdmission::StaticCut;
            trace_.fallback_reason = fallback_reason;
            trace_.begin_status = PageTransitionBeginStatus::StaticCut;
            trace_.effective_profile = LayerProfile::Static;
            ++trace_.static_cut_count;
            ++trace_.commit_count;
            ledger_.committed = true;
            commit_page_truth(scene, access);
            clear_tracking();
            source_capture.handle = {};
            return begin_result(PageTransitionBeginStatus::StaticCut, source_capture);
        }

        void reset_transition_trace(LayerProfile requested) noexcept {
            trace_ = {
                .begin_status = PageTransitionBeginStatus::Rejected,
                .last_state = PageTransitionState::Idle,
                .admission = LayerAdmission::Reject,
                .requested_profile = requested,
                .effective_profile = requested,
                .begin_count = 1,
            };
            ledger_ = {
                .begin_status = PageTransitionBeginStatus::Rejected,
                .final_state = PageTransitionState::Idle,
                .admission = LayerAdmission::Reject,
                .requested_profile = requested,
                .effective_profile = requested,
                .begin_count = 1,
                .snapshots_released = true,
            };
            state_ = PageTransitionState::Idle;
            runner_.reset();
        }

        [[nodiscard]] PageTransitionBeginResult begin_result(
            PageTransitionBeginStatus status,
            LayerCaptureResult source_capture = {},
            LayerCaptureResult destination_capture = {}) noexcept {
            ledger_.begin_status = status;
            ledger_.final_state = state_;
            ledger_.admission = trace_.admission;
            ledger_.fallback_reason = trace_.fallback_reason;
            ledger_.effective_profile = trace_.effective_profile;
            ledger_.interrupt_count = trace_.interrupt_count;
            ledger_.sample_count = trace_.sample_count;
            ledger_.commit_count = trace_.commit_count;
            ledger_.abort_count = trace_.abort_count;
            ledger_.static_cut_count = trace_.static_cut_count;
            ledger_.source_capture_count = trace_.source_capture_count;
            ledger_.destination_capture_count = trace_.destination_capture_count;
            ledger_.source_capture_status = source_capture.status;
            ledger_.destination_capture_status = destination_capture.status;
            ledger_.static_cut = status == PageTransitionBeginStatus::StaticCut;
            ledger_.interrupted = trace_.interrupt_count > 0;
            ledger_.peak_layer_bytes = ledger_.source_bytes + ledger_.destination_bytes;
            ledger_.snapshots_released = !source_snapshot_ && !destination_snapshot_;
            return {
                .status = status,
                .state = state_,
                .admission = trace_.admission,
                .source_capture = source_capture,
                .destination_capture = destination_capture,
            };
        }

        void abort_begin_failure(Scene& scene, SceneAccess access) noexcept {
            ++trace_.abort_count;
            ledger_.abort_count = trace_.abort_count;
            ledger_.aborted = true;
            restore_page_truth(scene, access);
            runner_.reset();
            clear_tracking();
        }

        void commit_page_truth(Scene& scene, SceneAccess access) noexcept {
            if (source_) {
                source_->thaw(scene, access, false);
                source_->set_visible(access, false);
            }
            if (destination_) {
                destination_->thaw(scene, access, true);
                destination_->set_visible(access, true);
            }
        }

        void restore_page_truth(Scene& scene, SceneAccess access) noexcept {
            if (source_) {
                source_->thaw(scene, access, source_was_visible_);
                source_->set_visible(access, source_was_visible_);
            }
            if (destination_) {
                destination_->thaw(scene, access, destination_was_visible_);
                destination_->set_visible(access, destination_was_visible_);
            }
        }

        void clear_tracking() noexcept {
            source_ = nullptr;
            destination_ = nullptr;
            source_snapshot_ = {};
            destination_snapshot_ = {};
            source_was_visible_ = false;
            destination_was_visible_ = false;
            state_ = PageTransitionState::Idle;
            trace_.last_state = state_;
            ledger_.final_state = state_;
            ledger_.snapshots_released = !source_snapshot_ && !destination_snapshot_;
        }

        PageLayer* source_{nullptr};
        PageLayer* destination_{nullptr};
        SnapshotHandle source_snapshot_{};
        SnapshotHandle destination_snapshot_{};
        bool source_was_visible_{false};
        bool destination_was_visible_{false};
        PageTransitionState state_{PageTransitionState::Idle};
        PageTransitionTrace trace_{};
        PageTransitionLedger ledger_{};
        MotionTransitionRunner runner_{};
    };
}
