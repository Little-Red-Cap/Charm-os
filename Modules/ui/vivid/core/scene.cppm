module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

export module charm.ui.scene;

export import charm.core.event;
export import charm.core.geometry;
export import charm.core.handle;
import charm.core.structured_view;
export import charm.core.style;
import charm.core.style_sheet;
export import charm.ui.scene.pill_surface;
export import charm.ui.scene.layer_runtime;
export import charm.ui.scene.builder_support;
export import charm.ui.scene.layer_support;
import :render_detail;
import charm.core.soa_factory;
import charm.core.soa_gui;
import charm.core.soa_kernel;
import charm.core.soa_payload;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.image;
import charm.gfx.pixel_ops;
export import charm.gfx.render_style;
export import charm.gfx.text_box;
import charm.gfx.draw_cmd;

export using ::ScrollBarOrientation;
export using ::SemanticAction;
export using ::SemanticActionAdmission;
export using ::SemanticActionAdmissionStatus;
export using ::SemanticActionMask;
export using ::SemanticActionRequest;
export using ::SemanticActionRequestLedger;
export using ::SemanticActionRequestRejectReason;
export using ::SemanticActionRequestStage;
export using ::SemanticActionRequestStatus;
export using ::SemanticActionSnapshot;
export using ::SemanticFocusAdmission;
export using ::SemanticFocusAdmissionStatus;
export using ::SemanticFocusQuery;
export using ::SemanticFocusQueryStatus;
export using ::SemanticFocusRequest;
export using ::SemanticFocusRequestLedger;
export using ::SemanticFocusRequestStage;
export using ::SemanticFocusRequestStatus;
export using ::SemanticFocusSnapshot;
export using ::SemanticIntentResolution;
export using ::SemanticIntentStatus;
export using ::SemanticRole;
export using ::SemanticTreeNode;
export using ::SemanticTreeSnapshot;
export using ::semantic_action_admission_status_name;
export using ::semantic_action_mask;
export using ::semantic_action_present;
export using ::semantic_action_request_reject_reason_name;
export using ::semantic_action_request_ledger;
export using ::semantic_action_request_stage_name;
export using ::semantic_action_request_status_name;
export using ::semantic_default_role_for_kind;
export using ::semantic_default_actions_for_role;
export using ::semantic_focus_admission_status_name;
export using ::semantic_focus_query_status_name;
export using ::semantic_focus_request_ledger;
export using ::semantic_focus_request_stage_name;
export using ::semantic_focus_request_status_name;
export using ::semantic_intent_status_name;
export using ::kSemanticTreeMaxNodes;
export using ::kSemanticTreeNoFocusIndex;
export using ::TableViewHeaderStyle;
export using ::TableViewColDividerStyle;

export namespace ui::scene {
    class SceneOverlay {
    public:
        explicit SceneOverlay(ui::draw_cmd::DefaultDrawCmdBuffer& buf) noexcept : buf_(buf) {}

        void fill_rect(const Rect& rect, const rgba& color) noexcept { buf_.fill_rect(rect, color); }
        void stroke_rect(const Rect& rect, const rgba& color) noexcept { buf_.stroke_rect(rect, color); }
        void fill_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            buf_.fill_round_rect(rect, radius, color);
        }
        void stroke_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            buf_.stroke_round_rect(rect, radius, color);
        }
        void fill_circle(const Rect& rect, const rgba& color) noexcept {
            const int radius = std::max(0, std::min(rect.w, rect.h) / 2);
            buf_.fill_circle(rect.x + rect.w / 2, rect.y + rect.h / 2, radius, color);
        }
        void stroke_circle(const Rect& rect, const rgba& color) noexcept {
            const int radius = std::max(0, std::min(rect.w, rect.h) / 2);
            buf_.stroke_circle(rect.x + rect.w / 2, rect.y + rect.h / 2, radius, color);
        }

    private:
        ui::draw_cmd::DefaultDrawCmdBuffer& buf_;
    };

    class Scene {
    public:
        using OverlayFn = void(*)(SceneOverlay& overlay, void* ctx) noexcept;

        explicit Scene(CanvasBase& canvas) noexcept
            : canvas_(canvas),
              factory_(kernel_),
              gui_(canvas_, kernel_, {}) {}

        SceneBuilder begin() noexcept { return SceneBuilder(kernel_, factory_); }
        void end(const SceneBuilder& builder) noexcept { set_root(builder.root()); }
        template <typename Fn>
        void build(Fn&& fn) noexcept {
            SceneBuilder builder{kernel_, factory_};
            fn(builder);
            set_root(builder.root());
        }

        void set_root(WidgetHandle root) noexcept {
            root_ = root;
            gui_.set_root(root_);
        }
        WidgetHandle root() const noexcept { return root_; }

        void set_overlay(OverlayFn fn, void* ctx) noexcept {
            overlay_fn_ = fn;
            overlay_ctx_ = ctx;
        }

        void render() {
            record_current_scene();
            reset_alpha_blend_count();
            last_exec_stats_ = detail::to_scene_stats(cmd_exec_.execute(canvas_, cmd_buf_));
            last_exec_stats_.alpha_blend_count = alpha_blend_count();
        }

        template <ui::RenderBackend Backend>
        TileStats render_tiles(Backend& backend,
                               const FrameBufferView& tile_buffer,
                               const TileConfig& config) {
            record_current_scene();
            const ui::draw_cmd::DrawCmdTileConfig cfg{
                config.tile_width,
                config.tile_height,
                config.clear_color,
                config.clear_tile
            };
            reset_alpha_blend_count();
            auto stats = detail::to_scene_stats(cmd_exec_.execute_tiles(backend, tile_buffer, cmd_buf_, cfg));
            stats.alpha_blend_count = alpha_blend_count();
            return stats;
        }

        void dispatch_event(const Event& e) { gui_.dispatch_event(e); }
        WidgetHandle hit_test(int x, int y) noexcept { return gui_.hit_test(x, y); }

        Rect world_rect(WidgetHandle h) const noexcept { return kernel_.world_rect(h); }
        const char* text(WidgetHandle h) const noexcept { return kernel_.text(h); }
        SemanticFocusSnapshot semantic_snapshot(WidgetHandle h) const noexcept {
            return kernel_.semantic_snapshot(h);
        }
        SemanticActionSnapshot semantic_action_snapshot(WidgetHandle h) const noexcept {
            return kernel_.semantic_action_snapshot(h);
        }
        SemanticIntentResolution resolve_semantic_intent(
            WidgetHandle root,
            const char* id,
            SemanticAction action) const noexcept {
            return kernel_.resolve_semantic_intent(root, id, action);
        }
        SemanticActionAdmission admit_semantic_action(
            WidgetHandle root,
            const char* id,
            SemanticAction action) const noexcept {
            return kernel_.admit_semantic_action(root, id, action);
        }
        SemanticActionRequest request_semantic_action(
            WidgetHandle root,
            const char* id,
            SemanticAction action) noexcept {
            return kernel_.request_semantic_action(root, id, action);
        }
        SemanticFocusQuery query_semantic_focus(WidgetHandle root, const char* id) const noexcept {
            return kernel_.query_semantic_focus(root, id);
        }
        SemanticFocusAdmission admit_semantic_focus(WidgetHandle root, const char* id) const noexcept {
            return kernel_.admit_semantic_focus(root, id);
        }
        SemanticFocusRequest request_semantic_focus(WidgetHandle root, const char* id) noexcept {
            return kernel_.request_semantic_focus(root, id);
        }
        SemanticFocusSnapshot semantic_focus_snapshot() const noexcept {
            return kernel_.semantic_focus_snapshot();
        }
        SemanticTreeSnapshot semantic_tree_snapshot(
            WidgetHandle root,
            std::size_t max_nodes = kSemanticTreeMaxNodes) const noexcept {
            return kernel_.semantic_tree_snapshot(root, max_nodes);
        }
        SceneAccess access() noexcept { return SceneAccess(kernel_); }

        CmdStats last_cmd_stats() const noexcept { return last_cmd_stats_; }
        ExecStats last_exec_stats() const noexcept { return last_exec_stats_; }
        LayerStats layer_stats() const noexcept { return snapshot_store_.stats(); }
        LayerEpoch current_layer_epoch() const noexcept {
            return make_layer_epoch();
        }
        SnapshotHandle reserve_snapshot(const SnapshotSpec& spec) noexcept {
            return snapshot_store_.reserve(spec, make_layer_epoch());
        }
        bool release_snapshot(SnapshotHandle handle) noexcept {
            if (const auto* record = snapshot_store_.record(handle);
                record && record->payload_slot != kInvalidSnapshotPayloadSlot) {
                if (record->kind == SnapshotKind::CommandBuffer) {
                    (void)command_snapshot_payloads_.release(record->payload_slot);
                } else if (record->kind == SnapshotKind::PixelSurface) {
                    (void)pixel_snapshot_payloads_.release(record->payload_slot);
                }
            }
            return snapshot_store_.release(handle);
        }
        bool mark_snapshot_stale(SnapshotHandle handle) noexcept {
            return snapshot_store_.mark_stale(handle);
        }
        bool refresh_snapshot_epoch(SnapshotHandle handle) noexcept {
            return snapshot_store_.refresh_epoch(handle, make_layer_epoch());
        }
        const SnapshotRecord* snapshot_record(SnapshotHandle handle) const noexcept {
            return snapshot_store_.record(handle);
        }
        bool snapshot_current(SnapshotHandle handle) const noexcept {
            const auto* record = snapshot_store_.record(handle);
            return record && !record->stale && record->epoch == make_layer_epoch();
        }
        bool validate_snapshot(SnapshotHandle handle) noexcept {
            if (snapshot_current(handle)) return true;
            if (snapshot_store_.record(handle)) {
                (void)snapshot_store_.mark_stale(handle);
            }
            return false;
        }
        bool validate_snapshot_for_compose(SnapshotHandle handle) noexcept {
            const auto* record = snapshot_store_.record(handle);
            if (!record || record->stale) return false;
            if (record->kind == SnapshotKind::PixelSurface) return true;
            return validate_snapshot(handle);
        }
        bool update_command_snapshot(SnapshotHandle handle) noexcept {
            return snapshot_store_.update_command_snapshot(
                handle,
                static_cast<std::uint32_t>(last_cmd_stats_.cmd_count),
                static_cast<std::uint32_t>(last_cmd_stats_.cmd_bytes));
        }
        LayerCaptureResult capture_command_snapshot_result(const SnapshotSpec& spec) noexcept {
            LayerCaptureResult result{};
            SnapshotSpec command_spec = spec;
            command_spec.preferred_kind = SnapshotKind::CommandBuffer;
            const auto handle = reserve_snapshot(command_spec);
            if (!handle) return result;
            result.handle = handle;
            record_current_scene();
            if (!update_command_snapshot(handle)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::RecordFailed;
                return result;
            }
            if (!store_command_snapshot_payload(handle)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::StoreFailed;
                return result;
            }
            result.status = LayerCaptureStatus::Ok;
            return result;
        }
        SnapshotHandle capture_command_snapshot(const SnapshotSpec& spec) noexcept {
            return capture_command_snapshot_result(spec).handle;
        }
        LayerCaptureResult capture_pixel_snapshot_result(const SnapshotSpec& spec) noexcept {
            LayerCaptureResult result{};
            SnapshotSpec pixel_spec = spec;
            pixel_spec.preferred_kind = SnapshotKind::PixelSurface;
            pixel_spec.preferred_format = screen_pixel_format;
            const auto handle = reserve_snapshot(pixel_spec);
            if (!handle) return result;
            result.handle = handle;
            const auto payload_slot = pixel_snapshot_payloads_.store(canvas_, pixel_spec.bounds);
            if (payload_slot == kInvalidSnapshotPayloadSlot) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::StoreFailed;
                return result;
            }
            const auto bytes = static_cast<std::uint32_t>(
                snapshot_pixel_bytes(screen_pixel_format, pixel_spec.bounds.w, pixel_spec.bounds.h));
            if (!snapshot_store_.update_pixel_snapshot(handle, screen_pixel_format, bytes)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::RecordFailed;
                return result;
            }
            if (auto* record = snapshot_store_.mutable_record(handle)) {
                record->payload_slot = payload_slot;
            }
            result.status = LayerCaptureStatus::Ok;
            return result;
        }
        SnapshotHandle capture_pixel_snapshot(const SnapshotSpec& spec) noexcept {
            return capture_pixel_snapshot_result(spec).handle;
        }
        bool update_pixel_snapshot(SnapshotHandle handle,
                                   PixelFormat format,
                                   std::uint32_t bytes) noexcept {
            return snapshot_store_.update_pixel_snapshot(handle, format, bytes);
        }
        LayerComposeResult compose_snapshot_dry_run(const LayerComposeSpec& spec) noexcept {
            if (!validate_snapshot_for_compose(spec.source)) {
                LayerComposeResult result{};
                if (const auto* record = snapshot_store_.record(spec.source)) {
                    result.stale = record->stale;
                    result.kind = record->kind;
                    result.source_bounds = record->bounds;
                    result.source_bytes = record->bytes;
                }
                return result;
            }
            return snapshot_store_.compose_dry_run(spec);
        }
        LayerComposePlan make_snapshot_compose_plan(const LayerComposeSpec& spec) noexcept {
            if (!validate_snapshot_for_compose(spec.source)) return {};
            return snapshot_store_.make_compose_plan(spec);
        }
        LayerBudgetResult check_layer_budget(const LayerComposePlan& plan,
                                             const LayerBudget& budget) const noexcept {
            return snapshot_store_.check_budget(plan, budget);
        }
        LayerReplayResult replay_command_snapshot(const LayerComposePlan& plan) noexcept {
            LayerReplayResult out{};
            out.source = plan.source;
            out.kind = plan.kind;
            out.target_bounds = plan.target_bounds;
            if (!plan.valid) return out;
            if (plan.kind != SnapshotKind::CommandBuffer) {
                out.status = LayerReplayStatus::UnsupportedKind;
                return out;
            }
            const auto* record = snapshot_store_.record(plan.source);
            if (!record) {
                out.status = LayerReplayStatus::MissingSnapshot;
                return out;
            }
            if (record->stale || record->epoch != make_layer_epoch()) {
                (void)snapshot_store_.mark_stale(plan.source);
                out.status = LayerReplayStatus::StaleSnapshot;
                return out;
            }
            const auto* payload = command_snapshot_payloads_.get(record->payload_slot);
            if (!payload) {
                out.status = LayerReplayStatus::MissingPayload;
                return out;
            }
            reset_alpha_blend_count();
            out.stats = detail::to_scene_stats(cmd_exec_.execute(canvas_,
                                                                 *payload,
                                                                 &plan.target_bounds));
            out.stats.alpha_blend_count = alpha_blend_count();
            out.status = out.stats.failed_cmds == 0
                ? LayerReplayStatus::Ok
                : LayerReplayStatus::ExecuteFailed;
            return out;
        }
        LayerReplayResult compose_pixel_snapshot(const LayerComposePlan& plan) noexcept {
            LayerReplayResult out{};
            out.source = plan.source;
            out.kind = plan.kind;
            out.target_bounds = plan.target_bounds;
            if (!plan.valid) return out;
            if (plan.kind != SnapshotKind::PixelSurface) {
                out.status = LayerReplayStatus::UnsupportedKind;
                return out;
            }
            const auto* record = snapshot_store_.record(plan.source);
            if (!record) {
                out.status = LayerReplayStatus::MissingSnapshot;
                return out;
            }
            if (record->stale) {
                (void)snapshot_store_.mark_stale(plan.source);
                out.status = LayerReplayStatus::StaleSnapshot;
                return out;
            }
            if (record->payload_slot == kInvalidSnapshotPayloadSlot
                || pixel_snapshot_payloads_.width(record->payload_slot) <= 0
                || pixel_snapshot_payloads_.height(record->payload_slot) <= 0) {
                out.status = LayerReplayStatus::MissingPayload;
                return out;
            }
            if (plan.target_bounds.x < 0 || plan.target_bounds.y < 0 ||
                plan.target_bounds.x + plan.target_bounds.w > canvas_.width() ||
                plan.target_bounds.y + plan.target_bounds.h > canvas_.height()) {
                out.status = LayerReplayStatus::ExecuteFailed;
                return out;
            }
            if (plan.transform.opacity == 0) {
                out.stats.cmd_count = 0;
                out.stats.cmd_bytes = 0;
                out.status = LayerReplayStatus::Ok;
                return out;
            }
            std::uint64_t alpha_blend_pixels = 0;
            const auto row_bytes =
                static_cast<std::size_t>(plan.source_visible.w) * canvas_.bytes_per_pixel();
            for (int y = 0; y < plan.source_visible.h; ++y) {
                const int source_y = plan.source_visible.y - plan.source_bounds.y + y;
                const int source_x = plan.source_visible.x - plan.source_bounds.x;
                const auto* src = pixel_snapshot_payloads_.row(record->payload_slot, source_y);
                if (!src) {
                    out.status = LayerReplayStatus::ExecuteFailed;
                    return out;
                }
                const auto src_offset =
                    static_cast<std::size_t>(source_x) * canvas_.bytes_per_pixel();
                const auto* src_line = src + src_offset;
                if (plan.transform.opacity == 255) {
                    canvas_.blit_span(plan.target_bounds.x,
                                      plan.target_bounds.y + y,
                                      src_line,
                                      row_bytes);
                } else {
                    alpha_blend_pixels += detail::blend_pixel_snapshot_row(canvas_,
                                                                           plan.target_bounds.x,
                                                                           plan.target_bounds.y + y,
                                                                           src_line,
                                                                           plan.source_visible.w,
                                                                           plan.transform.opacity);
                }
            }
            canvas_.mark_dirty(plan.target_bounds);
            snapshot_store_.note_pixel_blit(plan.composite_pixels);
            out.stats.cmd_count = 0;
            out.stats.cmd_bytes = 0;
            out.stats.alpha_blend_count = alpha_blend_pixels;
            out.status = LayerReplayStatus::Ok;
            return out;
        }

    private:
        void record_current_scene() noexcept {
            cmd_buf_.clear();
            last_cmd_stats_ = detail::to_scene_stats(gui_.record_commands(cmd_buf_));
            if (overlay_fn_) {
                SceneOverlay overlay{cmd_buf_};
                overlay_fn_(overlay, overlay_ctx_);
            }
        }

        LayerEpoch make_layer_epoch() const noexcept {
            const auto& tokens = Theme::instance().get_tokens();
            LayerEpoch epoch{};
            epoch.layout = kernel_.layout_dirty_version();
            epoch.style = StyleSheet::instance().stylesheet_version();
            epoch.theme = tokens.version;
            return epoch;
        }

        bool store_command_snapshot_payload(SnapshotHandle handle) noexcept {
            auto* record = snapshot_store_.mutable_record(handle);
            if (!record) return false;
            const auto payload_slot = command_snapshot_payloads_.store(cmd_buf_);
            if (payload_slot == kInvalidSnapshotPayloadSlot) {
                return false;
            }
            record->payload_slot = payload_slot;
            return true;
        }

        CanvasBase& canvas_;
        SoaKernel kernel_{};
        SoaFactory factory_;
        SoaGui gui_;
        WidgetHandle root_{};
        ui::draw_cmd::DefaultDrawCmdBuffer cmd_buf_{};
        CommandSnapshotPayloadStore<static_cast<std::size_t>(layer_cache_slots)> command_snapshot_payloads_{};
        PixelSnapshotPayloadStore<static_cast<std::size_t>(layer_cache_slots)> pixel_snapshot_payloads_{};
        ui::draw_cmd::DrawCmdExecutor cmd_exec_{};
        CmdStats last_cmd_stats_{};
        ExecStats last_exec_stats_{};
        DefaultSnapshotStore snapshot_store_{};
        OverlayFn overlay_fn_{nullptr};
        void* overlay_ctx_{nullptr};
    };

    class PageLayer {
    public:
        PageLayer() noexcept = default;
        explicit PageLayer(WidgetHandle root) noexcept : root_(root) {}

        void set_root(WidgetHandle root) noexcept {
            root_ = root;
            if (!root_) {
                visible_ = false;
                snapshot_ = {};
                state_ = LayerState::Hidden;
            } else if (state_ == LayerState::Hidden || !snapshot_) {
                state_ = visible_ ? LayerState::Live : LayerState::Hidden;
            }
        }

        WidgetHandle root() const noexcept { return root_; }
        void set_hooks(const PageHooks& hooks) noexcept { hooks_ = hooks; }
        bool visible() const noexcept { return visible_; }
        LayerState state() const noexcept { return state_; }
        bool live() const noexcept { return state_ == LayerState::Live; }
        bool frozen() const noexcept { return state_ == LayerState::Frozen; }
        bool transitioning() const noexcept { return state_ == LayerState::Transitioning; }
        bool stale_snapshot() const noexcept { return state_ == LayerState::StaleSnapshot; }
        SnapshotHandle snapshot() const noexcept { return snapshot_; }

        void show(SceneAccess& access) noexcept { set_visible(access, true); }
        void hide(SceneAccess& access) noexcept { set_visible(access, false); }

        void set_visible(SceneAccess& access, bool on) noexcept {
            if (!root_) return;
            access.set_visible(root_, on);
            const bool changed = (visible_ != on);
            visible_ = on;
            if (!snapshot_ || state_ == LayerState::Live || state_ == LayerState::Hidden) {
                state_ = on ? LayerState::Live : LayerState::Hidden;
            }
            if (changed) {
                if (on) {
                    if (hooks_.on_show) hooks_.on_show(access, root_, hooks_.ctx);
                } else {
                    if (hooks_.on_hide) hooks_.on_hide(access, root_, hooks_.ctx);
                }
            }
        }

        [[nodiscard]] LayerCaptureResult freeze(Scene& scene,
                                                const SnapshotSpec& spec) noexcept {
            if (!root_) return {};
            release_snapshot(scene);
            LayerCaptureResult result = (spec.preferred_kind == SnapshotKind::PixelSurface)
                ? scene.capture_pixel_snapshot_result(spec)
                : scene.capture_command_snapshot_result(spec);
            if (result.ok() && result.handle) {
                snapshot_ = result.handle;
                state_ = LayerState::Frozen;
            }
            return result;
        }

        [[nodiscard]] LayerCaptureResult freeze(Scene& scene,
                                                SceneAccess access,
                                                const SnapshotSpec& spec,
                                                bool hide_live_root) noexcept {
            const auto result = freeze(scene, spec);
            if (result.ok() && hide_live_root) {
                set_visible(access, false);
                state_ = LayerState::Frozen;
            }
            return result;
        }

        void mark_transitioning() noexcept {
            if (snapshot_) state_ = LayerState::Transitioning;
        }

        bool mark_stale(Scene& scene) noexcept {
            if (!snapshot_) return false;
            const bool marked = scene.mark_snapshot_stale(snapshot_);
            if (marked) state_ = LayerState::StaleSnapshot;
            return marked;
        }

        bool release_snapshot(Scene& scene) noexcept {
            if (!snapshot_) return false;
            const auto handle = snapshot_;
            snapshot_ = {};
            const bool released = scene.release_snapshot(handle);
            state_ = visible_ ? LayerState::Live : LayerState::Hidden;
            return released;
        }

        void thaw(Scene& scene, SceneAccess access, bool show_live_root = true) noexcept {
            (void)release_snapshot(scene);
            if (show_live_root) {
                set_visible(access, true);
            } else {
                state_ = visible_ ? LayerState::Live : LayerState::Hidden;
            }
        }

        void reset_snapshot_tracking() noexcept {
            snapshot_ = {};
            state_ = visible_ ? LayerState::Live : LayerState::Hidden;
        }

    private:
        WidgetHandle root_{};
        bool visible_{false};
        LayerState state_{LayerState::Hidden};
        SnapshotHandle snapshot_{};
        PageHooks hooks_{};
    };
}
