#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace vivid::evidence {
    struct RenderEvidence {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::size_t exec_cmds{0};
        std::size_t failed_cmds{0};
        std::size_t dirty_count{0};
        std::uint32_t dirty_hash{0};
        std::uint32_t cmd_hash{0};
        std::uint32_t pixel_hash{0};
    };

    struct StateDeltaEvidence {
        const char* id{""};
        const char* key{""};
        const char* source{""};
        int old_value{0};
        int new_value{0};
        const char* reason{""};

        [[nodiscard]] bool changed() const noexcept {
            return old_value != new_value;
        }
    };

    struct InvalidationEvidence {
        const char* kind{"none"};
        const char* dirty_scope{"none"};
        Rect component_bounds{};
        bool layout_changed{false};
    };

    struct RenderArtifactDeltaEvidence {
        bool changed{false};
        bool dirty_within_component{false};
        bool single_dirty_rect{false};
    };

    struct RenderArtifactEvidenceCapture {
        RenderEvidence evidence{};
        RenderArtifactDeltaEvidence delta{};
    };

    struct CausalChainEvidence {
        const char* name{""};
        bool request_ok{false};
        bool state_delta_ok{false};
        bool invalidation_ok{false};
        bool artifact_ok{false};
        bool rejected_no_mutation{false};

        [[nodiscard]] bool ok() const noexcept {
            return request_ok && state_delta_ok && invalidation_ok && artifact_ok;
        }
    };

    struct FocusMoveTrace {
        int focus_out{0};
        int focus_in{0};
        bool focus_out_expected{false};
        bool focus_in_expected{false};
    };

    struct PointerFocusTrace {
        int mouse_down{0};
        int focus_out{0};
        int focus_in{0};
        bool mouse_down_expected{false};
        bool focus_out_expected{false};
        bool focus_in_expected{false};
    };

    class RunLog {
    public:
        constexpr RunLog(const char* tag, const char* run) noexcept
            : tag_(tag), run_(run) {}

        void begin() const noexcept {
            std::printf("[%s] run=%s phase=begin\n", tag_, run_);
        }

        void end(bool ok) const noexcept {
            std::printf("[%s] run=%s phase=end result=%s cases=%u\n",
                        tag_,
                        run_,
                        ok ? "ok" : "fail",
                        case_count_);
        }

        void case_begin(const char* name) noexcept {
            ++case_count_;
            std::printf("[%s] case=%s", tag_, name);
        }

        [[nodiscard]] unsigned case_count() const noexcept {
            return case_count_;
        }

    private:
        const char* tag_;
        const char* run_;
        unsigned case_count_{0};
    };

    [[nodiscard]] inline bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] inline bool same_handle(WidgetHandle lhs, WidgetHandle rhs) noexcept {
        return lhs == rhs;
    }

    inline void click_center(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1, ms));
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1, ms + 1));
    }

    inline void mouse_down_center(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1, ms));
    }

    inline void mouse_up_center(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1, ms + 1));
    }

    [[nodiscard]] inline FocusMoveTrace collect_focus_move(
        ::ui::scene::SceneAccess& access,
        WidgetHandle old_target,
        WidgetHandle new_target) noexcept {
        FocusMoveTrace out{};
        for (std::size_t index = 0; index < access.input_event_count(); ++index) {
            const auto& event = access.input_event(index);
            if (event.event.type == Event::Type::FocusOut) {
                ++out.focus_out;
                out.focus_out_expected = out.focus_out_expected || same_handle(event.target, old_target);
            } else if (event.event.type == Event::Type::FocusIn) {
                ++out.focus_in;
                out.focus_in_expected = out.focus_in_expected || same_handle(event.target, new_target);
            }
        }
        return out;
    }

    [[nodiscard]] inline PointerFocusTrace collect_pointer_focus_trace(
        ::ui::scene::SceneAccess& access,
        WidgetHandle mouse_target,
        WidgetHandle focus_out_target,
        WidgetHandle focus_in_target) noexcept {
        PointerFocusTrace out{};
        for (std::size_t index = 0; index < access.input_event_count(); ++index) {
            const auto& event = access.input_event(index);
            if (event.event.type == Event::Type::MouseDown) {
                ++out.mouse_down;
                out.mouse_down_expected = out.mouse_down_expected || same_handle(event.target, mouse_target);
            } else if (event.event.type == Event::Type::FocusOut) {
                ++out.focus_out;
                out.focus_out_expected = out.focus_out_expected || same_handle(event.target, focus_out_target);
            } else if (event.event.type == Event::Type::FocusIn) {
                ++out.focus_in;
                out.focus_in_expected = out.focus_in_expected || same_handle(event.target, focus_in_target);
            }
        }
        return out;
    }

    [[nodiscard]] inline std::uint32_t hash_mix(std::uint32_t hash, std::uint32_t value) noexcept {
        hash ^= value;
        hash *= 16777619u;
        return hash;
    }

    inline void print_state_delta(const StateDeltaEvidence& delta) noexcept {
        std::printf(" state_delta=%d id=%s key=%s old=%d new=%d changed=%d source=%s",
                    delta.changed() ? 1 : 0,
                    delta.id ? delta.id : "",
                    delta.key ? delta.key : "",
                    delta.old_value,
                    delta.new_value,
                    delta.changed() ? 1 : 0,
                    delta.source ? delta.source : "");
        if (delta.reason && delta.reason[0] != '\0') {
            std::printf(" reason=%s", delta.reason);
        }
    }

    inline void print_named_state_delta(const char* name,
                                        const StateDeltaEvidence& delta) noexcept {
        const char* prefix = name ? name : "delta";
        std::printf(" %s_state_delta=%d %s_id=%s %s_key=%s %s_old=%d %s_new=%d %s_changed=%d %s_source=%s",
                    prefix,
                    delta.changed() ? 1 : 0,
                    prefix,
                    delta.id ? delta.id : "",
                    prefix,
                    delta.key ? delta.key : "",
                    prefix,
                    delta.old_value,
                    prefix,
                    delta.new_value,
                    prefix,
                    delta.changed() ? 1 : 0,
                    prefix,
                    delta.source ? delta.source : "");
        if (delta.reason && delta.reason[0] != '\0') {
            std::printf(" %s_reason=%s", prefix, delta.reason);
        }
    }

    inline void print_invalidation(const InvalidationEvidence& evidence) noexcept {
        std::printf(" invalidation=1 kind=%s dirty_scope=%s component_x=%d component_y=%d component_w=%d component_h=%d layout_changed=%d",
                    evidence.kind ? evidence.kind : "",
                    evidence.dirty_scope ? evidence.dirty_scope : "",
                    evidence.component_bounds.x,
                    evidence.component_bounds.y,
                    evidence.component_bounds.w,
                    evidence.component_bounds.h,
                    evidence.layout_changed ? 1 : 0);
    }

    inline void print_render_evidence(const char* prefix,
                                      const RenderEvidence& evidence) noexcept {
        const char* p = prefix ? prefix : "render";
        std::printf(" %s_dirty_count=%zu %s_dirty_hash=%u %s_cmd_count=%zu %s_cmd_bytes=%zu %s_exec_cmds=%zu %s_failed=%zu %s_cmd_hash=%u %s_pixel_hash=%u",
                    p,
                    evidence.dirty_count,
                    p,
                    evidence.dirty_hash,
                    p,
                    evidence.cmd_count,
                    p,
                    evidence.cmd_bytes,
                    p,
                    evidence.exec_cmds,
                    p,
                    evidence.failed_cmds,
                    p,
                    evidence.cmd_hash,
                    p,
                    evidence.pixel_hash);
    }

    [[nodiscard]] inline bool render_artifact_same(const RenderEvidence& lhs,
                                                   const RenderEvidence& rhs) noexcept {
        return lhs.dirty_count == rhs.dirty_count
            && lhs.dirty_hash == rhs.dirty_hash
            && lhs.cmd_hash == rhs.cmd_hash
            && lhs.pixel_hash == rhs.pixel_hash
            && lhs.cmd_count == rhs.cmd_count
            && lhs.cmd_bytes == rhs.cmd_bytes
            && lhs.exec_cmds == rhs.exec_cmds
            && lhs.failed_cmds == rhs.failed_cmds;
    }

    [[nodiscard]] inline RenderArtifactDeltaEvidence make_render_artifact_delta(
        const RenderEvidence& before,
        const RenderEvidence& after,
        bool dirty_within_component) noexcept {
        return RenderArtifactDeltaEvidence{
            .changed = !render_artifact_same(before, after),
            .dirty_within_component = dirty_within_component,
            .single_dirty_rect = after.dirty_count == 1,
        };
    }

    inline void print_render_artifact_delta(
        const RenderArtifactDeltaEvidence& delta) noexcept {
        std::printf(" artifact_delta=%d changed=%d dirty_within_component=%d single_dirty_rect=%d",
                    delta.changed ? 1 : 0,
                    delta.changed ? 1 : 0,
                    delta.dirty_within_component ? 1 : 0,
                    delta.single_dirty_rect ? 1 : 0);
    }

    inline void print_render_artifact_verdict(
        const RenderArtifactDeltaEvidence& delta,
        const char* prefix,
        const RenderEvidence& evidence) noexcept {
        print_render_artifact_delta(delta);
        print_render_evidence(prefix, evidence);
    }

    inline void print_render_artifact_comparison(
        const RenderArtifactDeltaEvidence& delta,
        const RenderEvidence& before,
        const RenderEvidence& after) noexcept {
        print_render_artifact_delta(delta);
        print_render_evidence("before", before);
        print_render_evidence("after", after);
    }

    inline void print_causal_chain(const CausalChainEvidence& evidence) noexcept {
        std::printf(" causal_chain=1 name=%s ok=%d request_ok=%d state_delta_ok=%d invalidation_ok=%d artifact_ok=%d rejected_no_mutation=%d",
                    evidence.name ? evidence.name : "",
                    evidence.ok() ? 1 : 0,
                    evidence.request_ok ? 1 : 0,
                    evidence.state_delta_ok ? 1 : 0,
                    evidence.invalidation_ok ? 1 : 0,
                    evidence.artifact_ok ? 1 : 0,
                    evidence.rejected_no_mutation ? 1 : 0);
    }

    template <typename StyleStateEvidenceT>
    inline void print_style_state_mask(const char* widget,
                                       const char* law,
                                       const StyleStateEvidenceT& evidence) noexcept {
        std::printf(" widget=%s mask=%u hovered=%d pressed=%d disabled=%d focused_in_style_mask=%d state_count=%u law=%s",
                    widget ? widget : "",
                    evidence.mask,
                    evidence.includes_hovered ? 1 : 0,
                    evidence.includes_pressed ? 1 : 0,
                    evidence.includes_disabled ? 1 : 0,
                    evidence.includes_focused ? 1 : 0,
                    evidence.state_count,
                    law ? law : "");
    }

    template <typename ResolvedStyleEvidenceT>
    inline void print_resolved_style_evidence(const char* widget,
                                              const char* state,
                                              const ResolvedStyleEvidenceT& evidence) noexcept {
        std::printf(" widget=%s state=%s style_key=%u color_hash=%u metrics_hash=%u",
                    widget ? widget : "",
                    state ? state : "",
                    evidence.style_key,
                    evidence.color_hash,
                    evidence.metrics_hash);
    }

    template <typename ResolvedStyleEvidenceT>
    inline void print_focus_style_evidence(const char* widget,
                                           bool focused,
                                           const ResolvedStyleEvidenceT& evidence,
                                           bool style_same,
                                           bool focused_in_style_mask) noexcept {
        std::printf(" widget=%s focus=%d style_key=%u color_hash=%u metrics_hash=%u style_same=%d focused_in_style_mask=%d",
                    widget ? widget : "",
                    focused ? 1 : 0,
                    evidence.style_key,
                    evidence.color_hash,
                    evidence.metrics_hash,
                    style_same ? 1 : 0,
                    focused_in_style_mask ? 1 : 0);
    }

    inline void print_semantic_intent_resolution(const SemanticIntentResolution& resolution) noexcept {
        std::printf(" intent_resolution=1 status=%s found=%d executable=%d id=%s actions=%u visited=%zu matches=%zu",
                    semantic_intent_status_name(resolution.status),
                    resolution.found ? 1 : 0,
                    resolution.executable ? 1 : 0,
                    resolution.id,
                    resolution.actions,
                    resolution.visited_count,
                    resolution.match_count);
    }

    inline void print_focus_query_ledger(const SemanticFocusQuery& query) noexcept {
        std::printf(" ledger=focus_query status=%s found=%d focusable=%d allowed=%d focusable_now=%d id=%s visited=%zu matches=%zu",
                    semantic_focus_query_status_name(query.status),
                    query.found ? 1 : 0,
                    query.focusable ? 1 : 0,
                    query.allowed_by_scope ? 1 : 0,
                    query.focusable_now ? 1 : 0,
                    query.id,
                    query.visited_count,
                    query.match_count);
    }

    inline void print_focus_admission_ledger(const SemanticFocusAdmission& admission) noexcept {
        std::printf(" ledger=focus_admission status=%s query=%s admitted=%d transfer_needed=%d focus_out=%d focus_in=%d found=%d focusable=%d allowed=%d id=%s visited=%zu matches=%zu",
                    semantic_focus_admission_status_name(admission.status),
                    semantic_focus_query_status_name(admission.query_status),
                    admission.admitted ? 1 : 0,
                    admission.transfer_needed ? 1 : 0,
                    admission.will_emit_focus_out ? 1 : 0,
                    admission.will_emit_focus_in ? 1 : 0,
                    admission.found ? 1 : 0,
                    admission.focusable ? 1 : 0,
                    admission.allowed_by_scope ? 1 : 0,
                    admission.id,
                    admission.visited_count,
                    admission.match_count);
    }

    inline void print_action_admission_ledger(const SemanticActionAdmission& admission) noexcept {
        std::printf(" ledger=action_admission status=%s intent=%s admitted=%d executable=%d focus_plan=%d click_plan=%d found=%d id=%s visited=%zu matches=%zu actions=%u",
                    semantic_action_admission_status_name(admission.status),
                    semantic_intent_status_name(admission.intent_status),
                    admission.admitted ? 1 : 0,
                    admission.executable ? 1 : 0,
                    admission.will_request_focus ? 1 : 0,
                    admission.will_emit_click ? 1 : 0,
                    admission.found ? 1 : 0,
                    admission.id,
                    admission.visited_count,
                    admission.match_count,
                    admission.actions);
    }

    inline void print_focus_request_ledger(const SemanticFocusRequest& request) noexcept {
        const SemanticFocusRequestLedger ledger = semantic_focus_request_ledger(request);
        std::printf(" ledger=focus_request stage=%s status=%s admission=%s query=%s admitted=%d transfer_needed=%d committed=%d focus_changed=%d focus_out=%d focus_in=%d events_before=%zu events_after=%zu focus_before=%s focus_after=%s id=%s",
                    semantic_focus_request_stage_name(ledger.stage),
                    semantic_focus_request_status_name(ledger.status),
                    semantic_focus_admission_status_name(ledger.admission_status),
                    semantic_focus_query_status_name(ledger.query_status),
                    ledger.admitted ? 1 : 0,
                    ledger.transfer_needed ? 1 : 0,
                    ledger.committed ? 1 : 0,
                    ledger.focus_changed ? 1 : 0,
                    ledger.emitted_focus_out ? 1 : 0,
                    ledger.emitted_focus_in ? 1 : 0,
                    ledger.events_before,
                    ledger.events_after,
                    ledger.focus_started_on_target ? "target" : "other",
                    ledger.focus_ended_on_target ? "target" : "other",
                    ledger.id);
    }

    inline void print_action_request_ledger(const SemanticActionRequest& request) noexcept {
        const SemanticActionRequestLedger ledger = semantic_action_request_ledger(request);
        std::printf(" ledger=action_request stage=%s status=%s reason=%s intent=%s action_admission=%s focus=%s admitted=%d focus_ready=%d executed=%d click=%d events_before=%zu events_after=%zu focus_before=%s focus_after=%s id=%s",
                    semantic_action_request_stage_name(ledger.stage),
                    semantic_action_request_status_name(ledger.status),
                    semantic_action_request_reject_reason_name(ledger.reject_reason),
                    semantic_intent_status_name(ledger.intent_status),
                    semantic_action_admission_status_name(ledger.action_admission_status),
                    semantic_focus_request_status_name(ledger.focus_request_status),
                    ledger.admitted ? 1 : 0,
                    ledger.focus_ready ? 1 : 0,
                    ledger.executed ? 1 : 0,
                    ledger.emitted_click ? 1 : 0,
                    ledger.events_before,
                    ledger.events_after,
                    ledger.focus_started_on_target ? "target" : "other",
                    ledger.focus_ended_on_target ? "target" : "other",
                    ledger.id);
    }

    [[nodiscard]] inline std::uint32_t hash_bytes(const std::byte* data, std::size_t len) noexcept {
        std::uint32_t hash = 2166136261u;
        if (!data) return hash;
        for (std::size_t index = 0; index < len; ++index) {
            hash ^= static_cast<std::uint8_t>(data[index]);
            hash *= 16777619u;
        }
        return hash;
    }

    [[nodiscard]] inline std::uint32_t hash_dirty(const DefaultCanvas& canvas) noexcept {
        std::uint32_t hash = 2166136261u;
        const auto& dirty = canvas.dirty_list();
        hash = hash_mix(hash, static_cast<std::uint32_t>(dirty.full() ? 1u : 0u));
        hash = hash_mix(hash, static_cast<std::uint32_t>(dirty.size()));
        for (std::size_t index = 0; index < dirty.size(); ++index) {
            const Rect rect = dirty[index];
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.x));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.y));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.w));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.h));
        }
        return hash;
    }

    [[nodiscard]] inline std::uint32_t hash_cmd_stats(const ui::scene::CmdStats& cmd,
                                                      const ui::scene::ExecStats& exec) noexcept {
        std::uint32_t hash = 2166136261u;
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.cmd_count));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.cmd_bytes));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.text_used));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.blob_used));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_count));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_rect));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_text));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_other));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.failed_cmds));
        return hash;
    }

    [[nodiscard]] inline bool rect_contains(Rect outer, Rect inner) noexcept {
        return inner.x >= outer.x
            && inner.y >= outer.y
            && inner.x + inner.w <= outer.x + outer.w
            && inner.y + inner.h <= outer.y + outer.h;
    }

    [[nodiscard]] inline bool dirty_stays_inside(const DefaultCanvas& canvas, Rect bounds) noexcept {
        const auto& dirty = canvas.dirty_list();
        if (dirty.full()) return false;
        for (std::size_t index = 0; index < dirty.size(); ++index) {
            if (!rect_contains(bounds, dirty[index])) return false;
        }
        return true;
    }

    inline void prepare_style_sheet() noexcept {
        apply_baseline_theme_preset(make_style_from_tokens(Theme::instance().get_tokens()));
    }

    [[nodiscard]] inline RenderEvidence render_scene(::ui::scene::Scene& scene,
                                                     DefaultCanvas& canvas,
                                                     Rect dirty_hint) noexcept {
        canvas.begin_frame();
        canvas.mark_dirty(dirty_hint);
        scene.render();
        const auto cmd = scene.last_cmd_stats();
        const auto exec = scene.last_exec_stats();
        return RenderEvidence{
            .cmd_count = cmd.cmd_count,
            .cmd_bytes = cmd.cmd_bytes,
            .exec_cmds = exec.cmd_count,
            .failed_cmds = exec.failed_cmds,
            .dirty_count = canvas.dirty_list().size(),
            .dirty_hash = hash_dirty(canvas),
            .cmd_hash = hash_cmd_stats(cmd, exec),
            .pixel_hash = hash_bytes(canvas.data(),
                                     static_cast<std::size_t>(canvas.height()) * canvas.stride_bytes()),
        };
    }

    [[nodiscard]] inline RenderArtifactEvidenceCapture render_component_artifact_delta(
        ::ui::scene::Scene& scene,
        DefaultCanvas& canvas,
        Rect component_bounds,
        const RenderEvidence& before) noexcept {
        const auto evidence = render_scene(scene, canvas, component_bounds);
        return RenderArtifactEvidenceCapture{
            .evidence = evidence,
            .delta = make_render_artifact_delta(before,
                                                evidence,
                                                dirty_stays_inside(canvas, component_bounds)),
        };
    }
}
