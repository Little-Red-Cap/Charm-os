module;

#include <array>
#include <cstddef>
#include <cstdint>

module charm.core.soa_kernel:semantic;

import :kernel_class;
import :types;
import :input;
import charm.core.event;
import charm.core.geometry;
import charm.core.soa_payload;

    void SoaKernel::set_semantic(WidgetHandle h,
                                 SemanticRole role,
                                 const char* id,
                                 const char* label) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.semantic_role[idx] = role;
        common_.semantic_id[idx] = payloads_.store_text(id);
        common_.semantic_label[idx] = payloads_.store_text(label);
        common_.semantic_actions[idx] = semantic_default_actions_for_role(role);
    }

    void SoaKernel::set_semantic_default(WidgetHandle h,
                                         const char* id,
                                         const char* label) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const SemanticRole role = semantic_default_role_for_kind(common_.kind[idx]);
        if (role == SemanticRole::None) {
            clear_semantic(h);
            return;
        }
        const char* resolved_label = label;
        if (!resolved_label || resolved_label[0] == '\0') {
            resolved_label = text(h);
        }
        set_semantic(h, role, id, resolved_label);
    }

    void SoaKernel::clear_semantic(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.semantic_role[idx] = SemanticRole::None;
        common_.semantic_id[idx] = soa_detail::empty_text_id();
        common_.semantic_label[idx] = soa_detail::empty_text_id();
        common_.semantic_actions[idx] = 0;
    }

    void SoaKernel::set_semantic_actions(WidgetHandle h, SemanticActionMask actions) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.semantic_actions[idx] = actions;
    }

    SemanticFocusSnapshot SoaKernel::semantic_snapshot(WidgetHandle h) const noexcept {
        SemanticFocusSnapshot out{};
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return out;
        const SemanticRole role = common_.semantic_role[idx];
        out.handle = h;
        out.id = payloads_.text_c_str(common_.semantic_id[idx]);
        out.role = semantic_role_name(role);
        out.label = payloads_.text_c_str(common_.semantic_label[idx]);
        out.actions = common_.semantic_actions[idx];
        out.focusable = focusable(h);
        out.found = role != SemanticRole::None && out.id && out.id[0] != '\0';
        return out;
    }

    SemanticActionSnapshot SoaKernel::semantic_action_snapshot(WidgetHandle h) const noexcept {
        SemanticActionSnapshot out{};
        const auto semantic = semantic_snapshot(h);
        out.handle = semantic.handle;
        out.id = semantic.id;
        out.actions = semantic.actions;
        out.found = semantic.found;
        return out;
    }

    SemanticFocusSnapshot SoaKernel::semantic_focus_snapshot() const noexcept {
        return semantic_snapshot(input_.focused);
    }

    SemanticIntentResolution SoaKernel::resolve_semantic_intent(WidgetHandle root,
                                                                const char* id,
                                                                SemanticAction action) const noexcept {
        SemanticIntentResolution out{};
        out.id = id ? id : "";
        out.action = action;
        if (!root || !valid(root)) {
            out.status = SemanticIntentStatus::InvalidRoot;
            return out;
        }
        if (!id || id[0] == '\0') {
            out.status = SemanticIntentStatus::MissingId;
            return out;
        }

        const auto workspace = acquire_traversal(TraversalPhase::Semantic);
        if (!workspace) return out;
        auto& stack = workspace.stack();
        std::size_t sp = 0;
        stack[sp++] = TraversalFrame{.h = root};

        while (sp > 0) {
            const WidgetHandle h = stack[--sp].h;
            if (!valid(h) || !visible(h)) continue;
            ++out.visited_count;

            const auto semantic = semantic_snapshot(h);
            if (semantic.found && text_equal(semantic.id, id)) {
                ++out.match_count;
                if (out.match_count == 1) {
                    out.handle = h;
                    out.actions = semantic.actions;
                    out.found = true;
                } else {
                    out.status = SemanticIntentStatus::AmbiguousId;
                    out.executable = false;
                    return out;
                }
            }

            for (auto child = last_child(h); child; child = prev_sibling(child)) {
                if (sp >= stack.size()) {
                    note_workspace_overflow();
                    break;
                }
                stack[sp++] = TraversalFrame{.h = child};
            }
        }

        if (!out.found) {
            out.status = SemanticIntentStatus::NotFound;
            return out;
        }
        if (!semantic_action_present(out.actions, action)) {
            out.status = SemanticIntentStatus::UnsupportedAction;
            return out;
        }
        if (!enabled(out.handle)) {
            out.status = SemanticIntentStatus::Disabled;
            return out;
        }
        out.status = SemanticIntentStatus::Resolved;
        out.executable = true;
        return out;
    }

    SemanticActionAdmission SoaKernel::admit_semantic_action(WidgetHandle root,
                                                             const char* id,
                                                             SemanticAction action) const noexcept {
        SemanticActionAdmission out{};
        out.root = root;
        out.id = id ? id : "";
        out.action = action;
        out.resolution = resolve_semantic_intent(root, id, action);
        out.handle = out.resolution.handle;
        out.intent_status = out.resolution.status;
        out.actions = out.resolution.actions;
        out.visited_count = out.resolution.visited_count;
        out.match_count = out.resolution.match_count;
        out.found = out.resolution.found;
        out.executable = out.resolution.executable;

        switch (out.resolution.status) {
        case SemanticIntentStatus::Resolved:
            break;
        case SemanticIntentStatus::InvalidRoot:
            out.status = SemanticActionAdmissionStatus::InvalidRoot;
            return out;
        case SemanticIntentStatus::MissingId:
            out.status = SemanticActionAdmissionStatus::MissingId;
            return out;
        case SemanticIntentStatus::NotFound:
            out.status = SemanticActionAdmissionStatus::NotFound;
            return out;
        case SemanticIntentStatus::AmbiguousId:
            out.status = SemanticActionAdmissionStatus::AmbiguousId;
            return out;
        case SemanticIntentStatus::UnsupportedAction:
            out.status = SemanticActionAdmissionStatus::UnsupportedAction;
            return out;
        case SemanticIntentStatus::Disabled:
            out.status = SemanticActionAdmissionStatus::Disabled;
            return out;
        }

        out.status = SemanticActionAdmissionStatus::Admitted;
        out.admitted = true;
        out.will_request_focus = focusable(out.handle);
        out.will_emit_click = action == SemanticAction::Activate;
        return out;
    }

    SemanticActionRequest SoaKernel::request_semantic_action(WidgetHandle root,
                                                             const char* id,
                                                             SemanticAction action) noexcept {
        SemanticActionRequest out{};
        input_events_.clear();
        input_actions_.clear();
        out.before_focus = input_.focused;
        out.events_before = input_events_.count;
        out.admission = admit_semantic_action(root, id, action);
        out.target = out.admission.handle;
        out.resolved = out.admission.intent_status == SemanticIntentStatus::Resolved
            && out.admission.executable;
        out.admitted = out.admission.admitted;

        if (!out.admitted) {
            out.after_focus = input_.focused;
            out.events_after = input_events_.count;
            out.status = SemanticActionRequestStatus::Rejected;
            out.reject_reason = SemanticActionRequestRejectReason::ActionAdmissionRejected;
            return out;
        }

        out.focus_request = request_semantic_focus(root, id);
        out.before_focus = out.focus_request.before_focus;
        out.after_focus = out.focus_request.after_focus;
        out.events_before = out.focus_request.events_before;
        out.events_after = out.focus_request.events_after;
        out.focus_changed = out.focus_request.focus_changed;
        out.emitted_focus_out = out.focus_request.emitted_focus_out;
        out.emitted_focus_in = out.focus_request.emitted_focus_in;
        out.focus_ready = out.focus_request.status == SemanticFocusRequestStatus::Committed
            || out.focus_request.status == SemanticFocusRequestStatus::AlreadyFocused;

        if (!out.focus_ready) {
            out.status = SemanticActionRequestStatus::Rejected;
            out.reject_reason = SemanticActionRequestRejectReason::FocusRequestRejected;
            return out;
        }

        if (action == SemanticAction::Activate) {
            const Rect r = world_rect(out.target);
            const int x = r.x + r.w / 2;
            const int y = r.y + r.h / 2;
            input_emit_event(out.target, Event::mouse(Event::Type::Click, x, y, 1, input_.last_ms));
            input_handle_click(out.target, x, y);
            if (input_actions_.overflowed) {
                input_handle_action_overflow();
                out.after_focus = input_.focused;
                out.events_after = input_events_.count;
                out.status = SemanticActionRequestStatus::Rejected;
                out.reject_reason = SemanticActionRequestRejectReason::InputActionOverflow;
                return out;
            }
            input_apply_actions();
            for (std::size_t index = out.events_after; index < input_events_.count; ++index) {
                const auto& event = input_events_.events[index];
                if (event.target == out.target && event.event.type == Event::Type::Click) {
                    out.emitted_click = true;
                }
            }
        }

        out.after_focus = input_.focused;
        out.events_after = input_events_.count;
        out.executed = out.emitted_click;
        out.status = out.executed
            ? SemanticActionRequestStatus::Executed
            : SemanticActionRequestStatus::Rejected;
        out.reject_reason = out.executed
            ? SemanticActionRequestRejectReason::None
            : SemanticActionRequestRejectReason::NoActionEmitted;
        return out;
    }

    SemanticFocusQuery SoaKernel::query_semantic_focus(WidgetHandle root, const char* id) const noexcept {
        SemanticFocusQuery out{};
        out.root = root;
        out.active_scope = input_.focus_scope;
        out.id = id ? id : "";
        if (!root || !valid(root)) {
            out.status = SemanticFocusQueryStatus::InvalidRoot;
            return out;
        }
        if (!id || id[0] == '\0') {
            out.status = SemanticFocusQueryStatus::MissingId;
            return out;
        }

        const auto workspace = acquire_traversal(TraversalPhase::Semantic);
        if (!workspace) return out;
        auto& stack = workspace.stack();
        std::size_t sp = 0;
        stack[sp++] = TraversalFrame{.h = root};

        while (sp > 0) {
            const WidgetHandle h = stack[--sp].h;
            if (!valid(h) || !visible(h)) continue;
            ++out.visited_count;

            const auto semantic = semantic_snapshot(h);
            if (semantic.found && text_equal(semantic.id, id)) {
                ++out.match_count;
                if (out.match_count == 1) {
                    out.handle = h;
                    out.found = true;
                    out.focusable = semantic.focusable;
                } else {
                    out.status = SemanticFocusQueryStatus::AmbiguousId;
                    out.allowed_by_scope = false;
                    out.focusable_now = false;
                    return out;
                }
            }

            for (auto child = last_child(h); child; child = prev_sibling(child)) {
                if (sp >= stack.size()) {
                    note_workspace_overflow();
                    break;
                }
                stack[sp++] = TraversalFrame{.h = child};
            }
        }

        if (!out.found) {
            out.status = SemanticFocusQueryStatus::NotFound;
            return out;
        }
        if (!out.focusable) {
            out.status = SemanticFocusQueryStatus::NotFocusable;
            return out;
        }
        if (!enabled(out.handle)) {
            out.status = SemanticFocusQueryStatus::Disabled;
            return out;
        }
        out.allowed_by_scope = !input_.focus_scope
            || !input_.focus_scope_trap
            || input_is_descendant(out.handle, input_.focus_scope);
        if (!out.allowed_by_scope) {
            out.status = SemanticFocusQueryStatus::OutsideActiveScope;
            return out;
        }
        out.status = SemanticFocusQueryStatus::Resolved;
        out.focusable_now = true;
        return out;
    }

    SemanticFocusAdmission SoaKernel::admit_semantic_focus(WidgetHandle root, const char* id) const noexcept {
        const SemanticFocusQuery query = query_semantic_focus(root, id);
        SemanticFocusAdmission out{};
        out.handle = query.handle;
        out.root = query.root;
        out.current_focus = input_.focused;
        out.active_scope = query.active_scope;
        out.id = query.id;
        out.query_status = query.status;
        out.visited_count = query.visited_count;
        out.match_count = query.match_count;
        out.found = query.found;
        out.focusable = query.focusable;
        out.allowed_by_scope = query.allowed_by_scope;
        out.focusable_now = query.focusable_now;

        switch (query.status) {
        case SemanticFocusQueryStatus::Resolved:
            break;
        case SemanticFocusQueryStatus::InvalidRoot:
            out.status = SemanticFocusAdmissionStatus::InvalidRoot;
            return out;
        case SemanticFocusQueryStatus::MissingId:
            out.status = SemanticFocusAdmissionStatus::MissingId;
            return out;
        case SemanticFocusQueryStatus::NotFound:
            out.status = SemanticFocusAdmissionStatus::NotFound;
            return out;
        case SemanticFocusQueryStatus::AmbiguousId:
            out.status = SemanticFocusAdmissionStatus::AmbiguousId;
            return out;
        case SemanticFocusQueryStatus::NotFocusable:
            out.status = SemanticFocusAdmissionStatus::NotFocusable;
            return out;
        case SemanticFocusQueryStatus::Disabled:
            out.status = SemanticFocusAdmissionStatus::Disabled;
            return out;
        case SemanticFocusQueryStatus::OutsideActiveScope:
            out.status = SemanticFocusAdmissionStatus::OutsideActiveScope;
            return out;
        }

        if (input_.focused == query.handle) {
            out.status = SemanticFocusAdmissionStatus::AlreadyFocused;
            out.admitted = true;
            return out;
        }

        out.status = SemanticFocusAdmissionStatus::Admitted;
        out.admitted = true;
        out.transfer_needed = true;
        out.will_emit_focus_out = static_cast<bool>(input_.focused);
        out.will_emit_focus_in = static_cast<bool>(query.handle);
        return out;
    }

    SemanticFocusRequest SoaKernel::request_semantic_focus(WidgetHandle root, const char* id) noexcept {
        SemanticFocusRequest out{};
        input_events_.clear();
        input_actions_.clear();
        out.before_focus = input_.focused;
        out.events_before = input_events_.count;
        out.admission = admit_semantic_focus(root, id);

        if (!out.admission.admitted) {
            out.after_focus = input_.focused;
            out.events_after = input_events_.count;
            out.status = SemanticFocusRequestStatus::Rejected;
            return out;
        }
        if (!out.admission.transfer_needed) {
            out.after_focus = input_.focused;
            out.events_after = input_events_.count;
            out.status = SemanticFocusRequestStatus::AlreadyFocused;
            return out;
        }

        input_set_focus(out.admission.handle);
        const std::size_t emitted_begin = out.events_before;
        for (std::size_t index = emitted_begin; index < input_events_.count; ++index) {
            const auto type = input_events_.events[index].event.type;
            out.emitted_focus_out = out.emitted_focus_out || type == Event::Type::FocusOut;
            out.emitted_focus_in = out.emitted_focus_in || type == Event::Type::FocusIn;
        }
        input_apply_actions();

        out.after_focus = input_.focused;
        out.events_after = input_events_.count;
        out.focus_changed = out.before_focus != out.after_focus;
        out.committed = out.focus_changed && out.after_focus == out.admission.handle;
        out.status = out.committed
            ? SemanticFocusRequestStatus::Committed
            : SemanticFocusRequestStatus::Rejected;
        return out;
    }

    SemanticTreeSnapshot SoaKernel::semantic_tree_snapshot(
        WidgetHandle root,
        std::size_t max_nodes) const noexcept {
        SemanticTreeSnapshot out{};
        if (!root || !valid(root)) {
            return out;
        }
        if (max_nodes > kSemanticTreeMaxNodes) {
            max_nodes = kSemanticTreeMaxNodes;
        }

        const auto workspace = acquire_traversal(TraversalPhase::Semantic);
        if (!workspace) {
            out.overflowed = true;
            return out;
        }
        auto& stack = workspace.stack();
        std::size_t sp = 0;
        stack[sp++] = TraversalFrame{.h = root};

        while (sp > 0) {
            const TraversalFrame entry = stack[--sp];
            const WidgetHandle h = entry.h;
            if (!valid(h) || !visible(h)) continue;
            ++out.visited_count;

            const auto semantic = semantic_snapshot(h);
            if (semantic.found) {
                const bool focused = h == input_.focused;
                ++out.total_semantic_count;
                if (focused) {
                    out.focused_handle = h;
                    out.focus_id = semantic.id;
                    out.focus_found = true;
                }
                if (out.node_count < max_nodes) {
                    SemanticTreeNode node{};
                    node.handle = h;
                    node.id = semantic.id;
                    node.role = semantic.role;
                    node.label = semantic.label;
                    node.actions = semantic.actions;
                    node.bounds = world_rect(h);
                    node.depth = entry.depth;
                    node.preorder = static_cast<std::uint16_t>(out.total_semantic_count - 1);
                    node.focused = focused;
                    node.focusable = semantic.focusable;
                    out.nodes[out.node_count] = node;
                    out.semantic_hash = semantic_tree_hash_node(out.semantic_hash, node);
                    if (focused) {
                        out.focus_index = static_cast<std::uint16_t>(out.node_count);
                    }
                    ++out.node_count;
                } else {
                    out.overflowed = true;
                }
            }

            for (auto child = last_child(h); child; child = prev_sibling(child)) {
                if (sp >= stack.size()) {
                    out.overflowed = true;
                    note_workspace_overflow();
                    break;
                }
                stack[sp++] = TraversalFrame{
                    .h = child,
                    .depth = static_cast<std::uint16_t>(entry.depth + 1),
                };
            }
        }

        out.semantic_hash = semantic_tree_hash_mix(
            out.semantic_hash,
            static_cast<std::uint32_t>(out.node_count));
        out.semantic_hash = semantic_tree_hash_mix(
            out.semantic_hash,
            static_cast<std::uint32_t>(out.total_semantic_count));
        out.semantic_hash = semantic_tree_hash_mix(
            out.semantic_hash,
            out.focus_found ? static_cast<std::uint32_t>(out.focus_index) : 0xFFFFu);
        out.semantic_hash = semantic_tree_hash_text(out.semantic_hash, out.focus_found ? out.focus_id : "");
        out.semantic_hash = semantic_tree_hash_mix(out.semantic_hash, out.overflowed ? 1u : 0u);
        return out;
    }
