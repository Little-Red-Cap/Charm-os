module;
#include <cstdint>

export module charm.ui.scene.focus_scope;

export import charm.core.handle;

export
enum class FocusScopeDecisionKind : std::uint8_t {
    Allow,
    RejectOutsideScope,
    RejectInvalidScope,
};

export
struct FocusScopeDecision {
    FocusScopeDecisionKind kind{FocusScopeDecisionKind::RejectInvalidScope};
    WidgetHandle scope{};
    WidgetHandle requested{};
    WidgetHandle fallback{};

    [[nodiscard]] constexpr bool allowed() const noexcept {
        return kind == FocusScopeDecisionKind::Allow;
    }
};

export
struct FocusScopeSpec {
    WidgetHandle scope{};
    WidgetHandle current{};
    WidgetHandle fallback{};
    bool trap{true};
};

export
using FocusScopeContainsFn = bool (*)(WidgetHandle node, WidgetHandle ancestor, void* ctx) noexcept;

export
[[nodiscard]] constexpr const char* focus_scope_decision_name(FocusScopeDecisionKind kind) noexcept {
    switch (kind) {
    case FocusScopeDecisionKind::Allow:
        return "allow";
    case FocusScopeDecisionKind::RejectOutsideScope:
        return "reject_outside_scope";
    case FocusScopeDecisionKind::RejectInvalidScope:
        return "reject_invalid_scope";
    }
    return "unknown";
}

export
[[nodiscard]] inline FocusScopeDecision decide_focus_scope_request(
    const FocusScopeSpec& spec,
    WidgetHandle requested,
    FocusScopeContainsFn contains,
    void* ctx = nullptr) noexcept {
    FocusScopeDecision decision{};
    decision.scope = spec.scope;
    decision.requested = requested;
    decision.fallback = spec.fallback ? spec.fallback : spec.current;

    if (!spec.scope || !requested || !contains) {
        decision.kind = FocusScopeDecisionKind::RejectInvalidScope;
        return decision;
    }

    if (!spec.trap) {
        decision.kind = FocusScopeDecisionKind::Allow;
        decision.fallback = requested;
        return decision;
    }

    if (contains(requested, spec.scope, ctx)) {
        decision.kind = FocusScopeDecisionKind::Allow;
        decision.fallback = requested;
        return decision;
    }

    decision.kind = FocusScopeDecisionKind::RejectOutsideScope;
    return decision;
}
