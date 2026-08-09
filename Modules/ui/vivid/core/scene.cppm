module;

#include "vivid_features.generated.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#ifndef CHARM_VIVID_MEMORY_PROFILE_SYMBOLS
#define CHARM_VIVID_MEMORY_PROFILE_SYMBOLS 0
#endif

export module charm.ui.scene;

import charm.core.config;
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
import charm.core.soa_factory;
import charm.core.soa_gui;
import charm.core.soa_kernel;
import charm.core.soa_payload;
#if !defined(CHARM_VIVID_FEATURESET_MCU_MIN)
import charm.ui.vivid.perf_overlay_runtime;
#endif
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.image;
import charm.gfx.pixel_ops;
export import charm.gfx.render_style;
export import charm.gfx.text_box;
import charm.gfx.draw_cmd;
import :render_detail;

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
    struct SceneTimingSource {
        using NowUsFn = std::uint64_t (*)(void* ctx) noexcept;

        void* ctx{nullptr};
        NowUsFn now_us{nullptr};

        [[nodiscard]] bool available() const noexcept { return now_us != nullptr; }
        [[nodiscard]] std::uint64_t sample_us() const noexcept {
            return now_us ? now_us(ctx) : 0ULL;
        }
    };

    struct SceneRenderTiming {
        std::uint32_t available{0};
        std::uint32_t record_us{0};
        std::uint32_t execute_us{0};
        std::uint32_t render_us{0};
    };

    inline constexpr std::uint32_t clamp_scene_timing_us(std::uint64_t value) noexcept {
        constexpr std::uint64_t max_u32 = 0xFFFFFFFFULL;
        return static_cast<std::uint32_t>(value > max_u32 ? max_u32 : value);
    }

    inline constexpr std::uint32_t scene_timing_delta_us(std::uint64_t start,
                                                         std::uint64_t end) noexcept {
        return end >= start ? clamp_scene_timing_us(end - start) : 0U;
    }

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
            const int radius = std::max<int>(0, static_cast<int>(std::min(rect.w, rect.h)) / 2);
            buf_.fill_circle(rect.x + rect.w / 2, rect.y + rect.h / 2, radius, color);
        }
        void stroke_circle(const Rect& rect, const rgba& color) noexcept {
            const int radius = std::max<int>(0, static_cast<int>(std::min(rect.w, rect.h)) / 2);
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
              gui_(canvas_, kernel_, {}, cmd_buf_, compaction_workspace_, cmd_exec_) {}

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

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

        void set_timing_source(SceneTimingSource timing_source) noexcept {
            timing_source_ = timing_source;
            if (!timing_source_.available()) {
                last_render_timing_ = {};
            }
        }

        void render() {
            const bool timing = timing_source_.available();
            const std::uint64_t frame_start = timing ? timing_source_.sample_us() : 0ULL;
            record_current_scene();
            const std::uint64_t record_end = timing ? timing_source_.sample_us() : 0ULL;
            reset_alpha_blend_count();
            last_exec_stats_ = detail::to_scene_stats(cmd_exec_.execute(canvas_, cmd_buf_));
            last_exec_stats_.alpha_blend_count = alpha_blend_count();
            const std::uint64_t execute_end = timing ? timing_source_.sample_us() : 0ULL;
            last_render_timing_ = timing
                ? SceneRenderTiming{
                    .available = 1U,
                    .record_us = scene_timing_delta_us(frame_start, record_end),
                    .execute_us = scene_timing_delta_us(record_end, execute_end),
                    .render_us = scene_timing_delta_us(frame_start, execute_end),
                }
                : SceneRenderTiming{};
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
        SceneRenderTiming last_render_timing() const noexcept { return last_render_timing_; }
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        ui::draw_cmd::DrawCmdDetailStats last_draw_detail_stats() const noexcept {
            return cmd_exec_.last_detail_stats();
        }
#else
        ui::draw_cmd::DrawCmdDetailStats last_draw_detail_stats() const noexcept { return {}; }
#endif
        LayerStats layer_stats() const noexcept { return snapshot_store_.stats(); }
        LayerEpoch current_layer_epoch() const noexcept {
            return make_layer_epoch();
        }
        SnapshotHandle reserve_snapshot(const SnapshotSpec& spec) noexcept {
            if ((spec.preferred_kind == SnapshotKind::CommandBuffer
                 && !snapshot_command_enabled)
                || (spec.preferred_kind == SnapshotKind::PixelSurface
                    && !snapshot_pixel_enabled)) {
                return {};
            }
            return snapshot_store_.reserve(spec, make_layer_epoch());
        }
        bool release_snapshot(SnapshotHandle handle) noexcept {
            if (const auto* record = snapshot_store_.record(handle);
                record && record->payload_slot != kInvalidSnapshotPayloadSlot) {
                (void)snapshot_payloads_.release(record->payload_slot);
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
            if constexpr (!snapshot_command_enabled) {
                (void)handle;
                return false;
            } else {
                return snapshot_store_.update_command_snapshot(
                    handle,
                    static_cast<std::uint32_t>(last_cmd_stats_.cmd_count),
                    static_cast<std::uint32_t>(last_cmd_stats_.cmd_bytes));
            }
        }
        LayerCaptureResult capture_command_snapshot_result(const SnapshotSpec& spec) noexcept {
            return capture_command_snapshot_result_for_root(spec, {});
        }
        SnapshotHandle capture_command_snapshot(const SnapshotSpec& spec) noexcept {
            return capture_command_snapshot_result(spec).handle;
        }
        LayerCaptureResult capture_pixel_snapshot_result(const SnapshotSpec& spec) noexcept {
            LayerCaptureResult result{};
            if constexpr (!snapshot_pixel_enabled) {
                result.status = LayerCaptureStatus::UnsupportedKind;
                return result;
            }
            SnapshotSpec pixel_spec = spec;
            pixel_spec.preferred_kind = SnapshotKind::PixelSurface;
            pixel_spec.preferred_format = screen_pixel_format;
            const auto handle = reserve_snapshot(pixel_spec);
            if (!handle) return result;
            result.handle = handle;
            const auto payload_slot = static_cast<std::uint32_t>(handle.slot);
            if (!snapshot_payloads_.store_pixel(payload_slot, canvas_, pixel_spec.bounds)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::StoreFailed;
                return result;
            }
            if (auto* record = snapshot_store_.mutable_record(handle)) {
                record->payload_slot = payload_slot;
            }
            const auto bytes = static_cast<std::uint32_t>(
                snapshot_pixel_bytes(screen_pixel_format, pixel_spec.bounds.w, pixel_spec.bounds.h));
            if (!snapshot_store_.update_pixel_snapshot(handle, screen_pixel_format, bytes)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::RecordFailed;
                return result;
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
            if constexpr (!snapshot_pixel_enabled) {
                (void)handle;
                (void)format;
                (void)bytes;
                return false;
            } else {
                return snapshot_store_.update_pixel_snapshot(handle, format, bytes);
            }
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
            if constexpr (!snapshot_command_enabled) {
                out.status = LayerReplayStatus::UnsupportedKind;
                return out;
            }
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
            const auto* payload = snapshot_payloads_.command(record->payload_slot);
            if (!payload) {
                out.status = LayerReplayStatus::MissingPayload;
                return out;
            }
            if (plan.transform.opacity == 0) {
                reset_alpha_blend_count();
                out.status = LayerReplayStatus::Ok;
                return out;
            }
            if (plan.transform.opacity != 255) {
                const auto origin = canvas_.save_origin();
                const Rect canvas_bounds{
                    -origin.x,
                    -origin.y,
                    canvas_.width(),
                    canvas_.height(),
                };
                const Rect visible_target =
                    layer_intersect_rect(plan.target_bounds, canvas_bounds);
                out.stats.cmd_count = payload->size();
                out.stats.cmd_bytes = payload->cmd_bytes();
                out.stats.overflowed = payload->overflowed();
                reset_alpha_blend_count();
                if (layer_rect_empty(visible_target)) {
                    out.status = LayerReplayStatus::Ok;
                    return out;
                }

                const auto base_clip = canvas_.save_clip();
                canvas_.clear_clip();
                std::uint64_t blended_pixels = 0;
                constexpr int tile_width = DefaultCommandReplayWorkspace::tile_width;
                constexpr int tile_height = DefaultCommandReplayWorkspace::tile_height;
                for (int tile_y = visible_target.y;
                     tile_y < visible_target.y + visible_target.h;
                     tile_y += tile_height) {
                    const int height = std::min(tile_height,
                                                visible_target.y + visible_target.h - tile_y);
                    for (int tile_x = visible_target.x;
                         tile_x < visible_target.x + visible_target.w;
                         tile_x += tile_width) {
                        const int width = std::min(tile_width,
                                                   visible_target.x + visible_target.w - tile_x);
                        const Rect tile{tile_x, tile_y, width, height};
                        RuntimeCanvas scratch{
                            command_replay_workspace_.data(),
                            width,
                            height,
                            screen_pixel_format,
                        };
                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                scratch.set_pixel(x,
                                                  y,
                                                  canvas_.get_pixel(tile_x + x, tile_y + y));
                            }
                        }

                        scratch.set_origin(static_cast<int>(plan.transform.x) - tile_x,
                                           static_cast<int>(plan.transform.y) - tile_y);
                        Rect source_tile = layer_inverse_translate_rect(tile, plan.transform);
                        source_tile = layer_intersect_rect(source_tile, plan.source_visible);
                        const auto tile_stats = cmd_exec_.execute(scratch, *payload, &source_tile);
                        detail::accumulate_scene_stats(out.stats, tile_stats);
                        scratch.clear_origin();

                        for (int y = 0; y < height; ++y) {
                            for (int x = 0; x < width; ++x) {
                                const int target_x = tile_x + x;
                                const int target_y = tile_y + y;
                                const auto background = canvas_.get_pixel(target_x, target_y);
                                const auto foreground = scratch.get_pixel(x, y);
                                canvas_.set_pixel(
                                    target_x,
                                    target_y,
                                    detail::blend_opaque_pixels(background,
                                                                foreground,
                                                                plan.transform.opacity));
                                ++blended_pixels;
                            }
                        }
                    }
                }
                canvas_.restore_clip(base_clip);
                canvas_.mark_dirty(visible_target);
                reset_alpha_blend_count();
                out.stats.alpha_blend_count = blended_pixels;
                out.status = out.stats.failed_cmds == 0
                    ? LayerReplayStatus::Ok
                    : LayerReplayStatus::ExecuteFailed;
                return out;
            }
            reset_alpha_blend_count();
            const auto origin = canvas_.save_origin();
            canvas_.set_origin(origin.x + static_cast<int>(plan.transform.x),
                               origin.y + static_cast<int>(plan.transform.y));
            out.stats = detail::to_scene_stats(cmd_exec_.execute(canvas_,
                                                                 *payload,
                                                                 &plan.source_visible));
            canvas_.restore_origin(origin);
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
            if constexpr (!snapshot_pixel_enabled) {
                out.status = LayerReplayStatus::UnsupportedKind;
                return out;
            }
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
                || snapshot_payloads_.pixel_width(record->payload_slot) <= 0
                || snapshot_payloads_.pixel_height(record->payload_slot) <= 0) {
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
                const auto* src = snapshot_payloads_.pixel_row(record->payload_slot, source_y);
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
        friend class PageLayer;

        LayerCaptureResult capture_command_snapshot_result_for_root(
            const SnapshotSpec& spec,
            WidgetHandle record_root) noexcept {
            LayerCaptureResult result{};
            if constexpr (!snapshot_command_enabled) {
                result.status = LayerCaptureStatus::UnsupportedKind;
                return result;
            }
            SnapshotSpec command_spec = spec;
            command_spec.preferred_kind = SnapshotKind::CommandBuffer;
            const auto handle = reserve_snapshot(command_spec);
            if (!handle) return result;
            result.handle = handle;
            record_current_scene(record_root);
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

        void record_current_scene(WidgetHandle record_root = {}) noexcept {
            cmd_buf_.clear();
            last_cmd_stats_ = detail::to_scene_stats(
                gui_.record_commands(cmd_buf_, record_root));
            if (!record_root && overlay_fn_) {
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
            const auto payload_slot = static_cast<std::uint32_t>(handle.slot);
            if (!snapshot_payloads_.store_command(payload_slot, cmd_buf_)) {
                return false;
            }
            record->payload_slot = payload_slot;
            return true;
        }

        CanvasBase& canvas_;
        SoaKernel kernel_{};
        SoaFactory factory_;
        ui::draw_cmd::DefaultDrawCmdBuffer cmd_buf_{};
        ui::draw_cmd::DefaultDrawCmdCompactionWorkspace compaction_workspace_{};
        ui::draw_cmd::DrawCmdExecutor cmd_exec_{};
        [[no_unique_address]] DefaultCommandReplayWorkspace command_replay_workspace_{};
        SoaGui gui_;
        WidgetHandle root_{};
        DefaultSnapshotPayloadStore snapshot_payloads_{};
        CmdStats last_cmd_stats_{};
        ExecStats last_exec_stats_{};
        SceneTimingSource timing_source_{};
        SceneRenderTiming last_render_timing_{};
        DefaultSnapshotStore snapshot_store_{};
        OverlayFn overlay_fn_{nullptr};
        void* overlay_ctx_{nullptr};
    };

    static_assert(!std::is_copy_constructible_v<Scene>
                  && !std::is_copy_assignable_v<Scene>
                  && !std::is_move_constructible_v<Scene>
                  && !std::is_move_assignable_v<Scene>,
                  "Scene must keep its runtime references bound to its own storage");

    class PageLayer {
    public:
        constexpr PageLayer() noexcept {}
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
                : scene.capture_command_snapshot_result_for_root(spec, root_);
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

namespace {
    struct VividStaticMemoryProfile {
        std::uint64_t scene_instances{0};
        std::uint64_t scene_bytes{0};
        std::uint64_t soa_kernel_bytes{0};
        std::uint64_t payload_manager_bytes{0};
        std::uint64_t draw_cmd_buffer_bytes{0};
        std::uint64_t draw_cmd_buffer_instances_per_scene{0};
        std::uint64_t draw_cmd_compaction_workspace_bytes{0};
        std::uint64_t draw_cmd_executor_bytes{0};
        std::uint64_t command_replay_workspace_bytes{0};
        std::uint64_t soa_traversal_workspace_bytes{0};
        std::uint64_t snapshot_payload_bytes{0};
        std::uint64_t command_snapshot_bytes{0};
        std::uint64_t pixel_snapshot_bytes{0};
        std::uint64_t theme_bytes{0};
        std::uint64_t stylesheet_bytes{0};
        std::uint64_t image_registry_bytes{0};
        std::uint64_t object_style_reserve_bytes{0};
        std::uint64_t style_state_table_bytes{0};
        std::uint64_t perf_overlay_runtime_bytes{0};
        std::uint64_t canvas_profile_bytes{0};
        std::uint64_t text_profile_bytes{0};
        std::uint64_t draw_cmd_policy_bytes{0};
        std::uint64_t runtime_global_bytes{0};
        std::uint64_t global_bytes{0};
        std::uint64_t total_bytes{0};
        std::uint64_t upper_bound_bytes{0};
        std::uint64_t budget_bytes{0};
        std::uint64_t min_headroom_bytes{0};
        std::uint64_t exact_headroom_bytes{0};
    };

    inline constexpr VividStaticMemoryProfile vivid_static_memory_profile() noexcept {
        constexpr auto style_profile = style_sheet_memory_profile();
        constexpr std::uint64_t scene_instances = CHARM_VIVID_RUNTIME_SCENE_INSTANCES;
        constexpr std::uint64_t scene_bytes = sizeof(ui::scene::Scene);
        constexpr std::uint64_t theme_bytes =
            sizeof(Theme) + sizeof(ThemeTokens) + sizeof(const Font*);
        constexpr std::uint64_t stylesheet_bytes = sizeof(StyleSheet);
        constexpr std::uint64_t image_registry_bytes = sizeof(ui::gfx::ImageRegistry);
        constexpr std::uint64_t widget_kind_count = style_profile.widget_kind_count;
        constexpr std::uint64_t object_style_reserve_bytes = widget_kind_count * sizeof(Style);
        constexpr std::uint64_t style_state_table_bytes = widget_kind_count
            * (sizeof(std::uint8_t) + sizeof(std::uint8_t) + sizeof(std::uint16_t));
#if defined(CHARM_VIVID_FEATURESET_MCU_MIN)
        constexpr std::uint64_t perf_overlay_runtime_bytes = 0;
#else
        constexpr std::uint64_t perf_overlay_runtime_bytes =
            ui::perf_overlay_runtime::resident_bytes;
#endif
        constexpr std::uint64_t canvas_profile_bytes = alpha_blend_counter_resident_bytes;
        constexpr std::uint64_t text_profile_bytes = ::text_profile_resident_bytes;
        constexpr std::uint64_t draw_cmd_policy_bytes =
            ui::draw_cmd::compaction_policy_resident_bytes;
        constexpr std::uint64_t runtime_global_bytes = perf_overlay_runtime_bytes
            + canvas_profile_bytes
            + text_profile_bytes
            + draw_cmd_policy_bytes;
        constexpr std::uint64_t global_bytes = theme_bytes
            + stylesheet_bytes
            + image_registry_bytes
            + object_style_reserve_bytes
            + style_state_table_bytes
            + runtime_global_bytes;
        constexpr std::uint64_t total_bytes = scene_instances * scene_bytes + global_bytes;
        constexpr std::uint64_t budget_bytes = CHARM_VIVID_STATIC_MEMORY_BUDGET_BYTES;
        constexpr std::uint64_t exact_headroom_bytes =
            budget_bytes > total_bytes ? budget_bytes - total_bytes : 0;
        return VividStaticMemoryProfile{
            scene_instances,
            scene_bytes,
            sizeof(SoaKernel),
            sizeof(soa_detail::PayloadManager),
            sizeof(ui::draw_cmd::DefaultDrawCmdBuffer),
            1,
            sizeof(ui::draw_cmd::DefaultDrawCmdCompactionWorkspace),
            sizeof(ui::draw_cmd::DrawCmdExecutor),
            ui::scene::DefaultCommandReplayWorkspace::storage_bytes,
            SoaGui::kTraversalWorkspaceBytes,
            sizeof(ui::scene::DefaultSnapshotPayloadStore),
            ui::scene::DefaultSnapshotPayloadStore::command_capacity_bytes,
            ui::scene::DefaultSnapshotPayloadStore::pixel_capacity_bytes,
            theme_bytes,
            stylesheet_bytes,
            image_registry_bytes,
            object_style_reserve_bytes,
            style_state_table_bytes,
            perf_overlay_runtime_bytes,
            canvas_profile_bytes,
            text_profile_bytes,
            draw_cmd_policy_bytes,
            runtime_global_bytes,
            global_bytes,
            total_bytes,
            CHARM_VIVID_STATIC_MEMORY_UPPER_BOUND_BYTES,
            budget_bytes,
            CHARM_VIVID_STATIC_MEMORY_MIN_HEADROOM_BYTES,
            exact_headroom_bytes,
        };
    }

    inline constexpr auto kVividStaticMemoryProfile = vivid_static_memory_profile();
    static_assert(kVividStaticMemoryProfile.runtime_global_bytes
                  <= CHARM_VIVID_RUNTIME_GLOBALS_UPPER_BYTES,
                  "Vivid target-ABI runtime globals exceed their configure-time upper bound");
    static_assert(kVividStaticMemoryProfile.snapshot_payload_bytes
                  <= CHARM_VIVID_SNAPSHOT_PAYLOAD_UPPER_BYTES,
                  "Vivid target-ABI snapshot payload store exceeds its configure-time upper bound");
    static_assert(kVividStaticMemoryProfile.command_replay_workspace_bytes
                  <= CHARM_VIVID_COMMAND_REPLAY_WORKSPACE_UPPER_BYTES,
                  "Vivid command replay workspace exceeds its configure-time upper bound");
    static_assert(kVividStaticMemoryProfile.total_bytes
                  <= kVividStaticMemoryProfile.upper_bound_bytes,
                  "Vivid configure-time static memory model undercounted target-ABI resident bytes");
#if CHARM_VIVID_STATIC_MEMORY_ADMISSION_REQUIRED
    static_assert(kVividStaticMemoryProfile.budget_bytes > 0,
                  "Vivid static memory admission requires a non-zero budget");
    static_assert(kVividStaticMemoryProfile.total_bytes <= kVividStaticMemoryProfile.budget_bytes
                      && kVividStaticMemoryProfile.budget_bytes
                              - kVividStaticMemoryProfile.total_bytes
                          >= kVividStaticMemoryProfile.min_headroom_bytes,
                  "Vivid target-ABI resident bytes do not leave the required budget headroom");
#endif
}

#if CHARM_VIVID_MEMORY_PROFILE_SYMBOLS && defined(__GNUC__)
extern "C" [[gnu::used]] void charm_vivid_static_memory_profile_symbols() noexcept {
    constexpr auto profile = kVividStaticMemoryProfile;
    asm volatile(
        ".global charm_vivid_static_profile_scene_bytes\n"
        ".set charm_vivid_static_profile_scene_bytes, %c0\n"
        ".global charm_vivid_static_profile_soa_kernel_bytes\n"
        ".set charm_vivid_static_profile_soa_kernel_bytes, %c1\n"
        ".global charm_vivid_static_profile_payload_manager_bytes\n"
        ".set charm_vivid_static_profile_payload_manager_bytes, %c2\n"
        ".global charm_vivid_static_profile_draw_cmd_buffer_bytes\n"
        ".set charm_vivid_static_profile_draw_cmd_buffer_bytes, %c3\n"
        ".global charm_vivid_static_profile_draw_cmd_buffer_instances_per_scene\n"
        ".set charm_vivid_static_profile_draw_cmd_buffer_instances_per_scene, %c4\n"
        ".global charm_vivid_static_profile_draw_cmd_compaction_workspace_bytes\n"
        ".set charm_vivid_static_profile_draw_cmd_compaction_workspace_bytes, %c5\n"
        ".global charm_vivid_static_profile_draw_cmd_executor_bytes\n"
        ".set charm_vivid_static_profile_draw_cmd_executor_bytes, %c6\n"
        ".global charm_vivid_static_profile_command_replay_workspace_bytes\n"
        ".set charm_vivid_static_profile_command_replay_workspace_bytes, %c7\n"
        ".global charm_vivid_static_profile_soa_traversal_workspace_bytes\n"
        ".set charm_vivid_static_profile_soa_traversal_workspace_bytes, %c8\n"
        ".global charm_vivid_static_profile_snapshot_payload_bytes\n"
        ".set charm_vivid_static_profile_snapshot_payload_bytes, %c9\n"
        ".global charm_vivid_static_profile_command_snapshot_bytes\n"
        ".set charm_vivid_static_profile_command_snapshot_bytes, %c10\n"
        ".global charm_vivid_static_profile_pixel_snapshot_bytes\n"
        ".set charm_vivid_static_profile_pixel_snapshot_bytes, %c11\n"
        ".global charm_vivid_static_profile_runtime_global_bytes\n"
        ".set charm_vivid_static_profile_runtime_global_bytes, %c12\n"
        ".global charm_vivid_static_profile_global_bytes\n"
        ".set charm_vivid_static_profile_global_bytes, %c13\n"
        ".global charm_vivid_static_profile_total_bytes\n"
        ".set charm_vivid_static_profile_total_bytes, %c14\n"
        ".global charm_vivid_static_profile_upper_bound_bytes\n"
        ".set charm_vivid_static_profile_upper_bound_bytes, %c15\n"
        ".global charm_vivid_static_profile_budget_bytes\n"
        ".set charm_vivid_static_profile_budget_bytes, %c16\n"
        ".global charm_vivid_static_profile_min_headroom_bytes\n"
        ".set charm_vivid_static_profile_min_headroom_bytes, %c17\n"
        ".global charm_vivid_static_profile_exact_headroom_bytes\n"
        ".set charm_vivid_static_profile_exact_headroom_bytes, %c18\n"
        :
        : "i"(profile.scene_bytes),
          "i"(profile.soa_kernel_bytes),
          "i"(profile.payload_manager_bytes),
          "i"(profile.draw_cmd_buffer_bytes),
          "i"(profile.draw_cmd_buffer_instances_per_scene),
          "i"(profile.draw_cmd_compaction_workspace_bytes),
          "i"(profile.draw_cmd_executor_bytes),
          "i"(profile.command_replay_workspace_bytes),
          "i"(profile.soa_traversal_workspace_bytes),
          "i"(profile.snapshot_payload_bytes),
          "i"(profile.command_snapshot_bytes),
          "i"(profile.pixel_snapshot_bytes),
          "i"(profile.runtime_global_bytes),
          "i"(profile.global_bytes),
          "i"(profile.total_bytes),
          "i"(profile.upper_bound_bytes),
          "i"(profile.budget_bytes),
          "i"(profile.min_headroom_bytes),
          "i"(profile.exact_headroom_bytes));
}
#endif
