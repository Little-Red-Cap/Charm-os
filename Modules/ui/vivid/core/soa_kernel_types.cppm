module;
#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_kernel:types;

export import charm.core.geometry;
export import charm.core.handle;

export
enum class SoaNodeFlag : std::uint8_t {
    Used = 1 << 0,
    Visible = 1 << 1,
    Enabled = 1 << 2,
    Focusable = 1 << 3,
    HitTest = 1 << 4,
    ClipChildren = 1 << 5
};

export
enum class SoaStateFlag : std::uint8_t {
    Hovered = 1 << 0,
    Pressed = 1 << 1,
    Focused = 1 << 2
};

export
enum class SoaStateMask : std::uint8_t {
    Enabled = 1 << 0,
    Hovered = 1 << 1,
    Pressed = 1 << 2,
    Focused = 1 << 3
};

export
enum class SemanticRole : std::uint8_t {
    None = 0,
    Button,
    ListItem,
    Text,
    Container,
};

export
enum class SemanticAction : std::uint8_t {
    Activate = 1 << 0,
};

export
using SemanticActionMask = std::uint8_t;

export
struct SemanticFocusSnapshot {
    WidgetHandle handle{};
    const char* id{""};
    const char* role{"none"};
    const char* label{""};
    SemanticActionMask actions{0};
    bool found{false};
    bool focusable{false};
};

export
struct SemanticActionSnapshot {
    WidgetHandle handle{};
    const char* id{""};
    SemanticActionMask actions{0};
    bool found{false};
};

export
enum class SemanticIntentStatus : std::uint8_t {
    Resolved = 0,
    InvalidRoot,
    MissingId,
    NotFound,
    AmbiguousId,
    UnsupportedAction,
    Disabled,
};

export
struct SemanticIntentResolution {
    WidgetHandle handle{};
    const char* id{""};
    SemanticAction action{SemanticAction::Activate};
    SemanticIntentStatus status{SemanticIntentStatus::NotFound};
    SemanticActionMask actions{0};
    std::size_t visited_count{0};
    std::size_t match_count{0};
    bool found{false};
    bool executable{false};
};

export
enum class SemanticActionAdmissionStatus : std::uint8_t {
    Admitted = 0,
    InvalidRoot,
    MissingId,
    NotFound,
    AmbiguousId,
    UnsupportedAction,
    Disabled,
};

export
struct SemanticActionAdmission {
    SemanticIntentResolution resolution{};
    WidgetHandle handle{};
    WidgetHandle root{};
    const char* id{""};
    SemanticAction action{SemanticAction::Activate};
    SemanticIntentStatus intent_status{SemanticIntentStatus::NotFound};
    SemanticActionAdmissionStatus status{SemanticActionAdmissionStatus::NotFound};
    SemanticActionMask actions{0};
    std::size_t visited_count{0};
    std::size_t match_count{0};
    bool found{false};
    bool executable{false};
    bool admitted{false};
    bool will_request_focus{false};
    bool will_emit_click{false};
};

export
enum class SemanticActionRequestStatus : std::uint8_t {
    Executed = 0,
    Rejected,
};

export
enum class SemanticActionRequestRejectReason : std::uint8_t {
    None = 0,
    ActionAdmissionRejected,
    FocusRequestRejected,
    InputActionOverflow,
    NoActionEmitted,
};

export
enum class SemanticActionRequestStage : std::uint8_t {
    None = 0,
    ActionAdmission,
    FocusRequest,
    Execution,
};

export
enum class SemanticFocusQueryStatus : std::uint8_t {
    Resolved = 0,
    InvalidRoot,
    MissingId,
    NotFound,
    AmbiguousId,
    NotFocusable,
    Disabled,
    OutsideActiveScope,
};

export
struct SemanticFocusQuery {
    WidgetHandle handle{};
    WidgetHandle root{};
    WidgetHandle active_scope{};
    const char* id{""};
    SemanticFocusQueryStatus status{SemanticFocusQueryStatus::NotFound};
    std::size_t visited_count{0};
    std::size_t match_count{0};
    bool found{false};
    bool focusable{false};
    bool allowed_by_scope{false};
    bool focusable_now{false};
};

export
enum class SemanticFocusAdmissionStatus : std::uint8_t {
    Admitted = 0,
    AlreadyFocused,
    InvalidRoot,
    MissingId,
    NotFound,
    AmbiguousId,
    NotFocusable,
    Disabled,
    OutsideActiveScope,
};

export
struct SemanticFocusAdmission {
    WidgetHandle handle{};
    WidgetHandle root{};
    WidgetHandle current_focus{};
    WidgetHandle active_scope{};
    const char* id{""};
    SemanticFocusQueryStatus query_status{SemanticFocusQueryStatus::NotFound};
    SemanticFocusAdmissionStatus status{SemanticFocusAdmissionStatus::NotFound};
    std::size_t visited_count{0};
    std::size_t match_count{0};
    bool found{false};
    bool focusable{false};
    bool allowed_by_scope{false};
    bool focusable_now{false};
    bool admitted{false};
    bool transfer_needed{false};
    bool will_emit_focus_out{false};
    bool will_emit_focus_in{false};
};

export
enum class SemanticFocusRequestStatus : std::uint8_t {
    Committed = 0,
    AlreadyFocused,
    Rejected,
};

export
enum class SemanticFocusRequestStage : std::uint8_t {
    None = 0,
    FocusAdmission,
    AlreadyFocused,
    Execution,
};

export
struct SemanticFocusRequest {
    SemanticFocusAdmission admission{};
    WidgetHandle before_focus{};
    WidgetHandle after_focus{};
    std::size_t events_before{0};
    std::size_t events_after{0};
    SemanticFocusRequestStatus status{SemanticFocusRequestStatus::Rejected};
    bool committed{false};
    bool focus_changed{false};
    bool emitted_focus_out{false};
    bool emitted_focus_in{false};
};

export
struct SemanticFocusRequestLedger {
    SemanticFocusRequestStage stage{SemanticFocusRequestStage::None};
    SemanticFocusRequestStatus status{SemanticFocusRequestStatus::Rejected};
    SemanticFocusAdmissionStatus admission_status{SemanticFocusAdmissionStatus::NotFound};
    SemanticFocusQueryStatus query_status{SemanticFocusQueryStatus::NotFound};
    const char* id{""};
    std::size_t events_before{0};
    std::size_t events_after{0};
    bool admitted{false};
    bool transfer_needed{false};
    bool committed{false};
    bool focus_changed{false};
    bool emitted_focus_out{false};
    bool emitted_focus_in{false};
    bool focus_started_on_target{false};
    bool focus_ended_on_target{false};
};

export
struct SemanticActionRequest {
    SemanticActionAdmission admission{};
    SemanticFocusRequest focus_request{};
    WidgetHandle target{};
    WidgetHandle before_focus{};
    WidgetHandle after_focus{};
    std::size_t events_before{0};
    std::size_t events_after{0};
    SemanticActionRequestStatus status{SemanticActionRequestStatus::Rejected};
    SemanticActionRequestRejectReason reject_reason{SemanticActionRequestRejectReason::None};
    bool resolved{false};
    bool admitted{false};
    bool focus_ready{false};
    bool focus_changed{false};
    bool emitted_focus_out{false};
    bool emitted_focus_in{false};
    bool emitted_click{false};
    bool executed{false};
};

export
struct SemanticActionRequestLedger {
    SemanticActionRequestStage stage{SemanticActionRequestStage::None};
    SemanticActionRequestStatus status{SemanticActionRequestStatus::Rejected};
    SemanticActionRequestRejectReason reject_reason{SemanticActionRequestRejectReason::None};
    SemanticIntentStatus intent_status{SemanticIntentStatus::NotFound};
    SemanticActionAdmissionStatus action_admission_status{SemanticActionAdmissionStatus::NotFound};
    SemanticFocusRequestStatus focus_request_status{SemanticFocusRequestStatus::Rejected};
    const char* id{""};
    std::size_t events_before{0};
    std::size_t events_after{0};
    bool admitted{false};
    bool focus_ready{false};
    bool executed{false};
    bool emitted_click{false};
    bool focus_started_on_target{false};
    bool focus_ended_on_target{false};
};

export
constexpr std::size_t kSemanticTreeMaxNodes = 32;

export
constexpr std::uint16_t kSemanticTreeNoFocusIndex = 0xFFFF;

export
struct SemanticTreeNode {
    WidgetHandle handle{};
    const char* id{""};
    const char* role{"none"};
    const char* label{""};
    SemanticActionMask actions{0};
    Rect bounds{};
    std::uint16_t depth{0};
    std::uint16_t preorder{0};
    bool focused{false};
    bool focusable{false};
};

export
struct SemanticTreeSnapshot {
    std::array<SemanticTreeNode, kSemanticTreeMaxNodes> nodes{};
    std::size_t node_count{0};
    std::size_t total_semantic_count{0};
    std::size_t visited_count{0};
    std::uint16_t focus_index{kSemanticTreeNoFocusIndex};
    WidgetHandle focused_handle{};
    const char* focus_id{""};
    bool focus_found{false};
    bool overflowed{false};
    std::uint32_t semantic_hash{2166136261u};
};

export
inline const char* semantic_role_name(SemanticRole role) noexcept {
    switch (role) {
    case SemanticRole::None:
        return "none";
    case SemanticRole::Button:
        return "button";
    case SemanticRole::ListItem:
        return "list_item";
    case SemanticRole::Text:
        return "text";
    case SemanticRole::Container:
        return "container";
    }
    return "unknown";
}

export
inline SemanticRole semantic_default_role_for_kind(WidgetKind kind) noexcept {
    switch (kind) {
    case WidgetKind::Button:
    case WidgetKind::IconButton:
    case WidgetKind::MenuItem:
    case WidgetKind::Checkbox:
    case WidgetKind::Radio:
    case WidgetKind::Switch:
        return SemanticRole::Button;
    case WidgetKind::ListItem:
        return SemanticRole::ListItem;
    case WidgetKind::Label:
    case WidgetKind::TextBox:
        return SemanticRole::Text;
    case WidgetKind::Container:
    case WidgetKind::ScrollContainer:
    case WidgetKind::List:
    case WidgetKind::ListView:
    case WidgetKind::Menu:
    case WidgetKind::PopupLayer:
    case WidgetKind::ModalDialog:
        return SemanticRole::Container;
    default:
        return SemanticRole::None;
    }
}

export
inline constexpr SemanticActionMask semantic_action_mask(SemanticAction action) noexcept {
    return static_cast<SemanticActionMask>(action);
}

export
inline constexpr bool semantic_action_present(SemanticActionMask mask,
                                              SemanticAction action) noexcept {
    return (mask & semantic_action_mask(action)) != 0;
}

export
inline constexpr SemanticActionMask semantic_default_actions_for_role(SemanticRole role) noexcept {
    switch (role) {
    case SemanticRole::Button:
    case SemanticRole::ListItem:
        return semantic_action_mask(SemanticAction::Activate);
    case SemanticRole::None:
    case SemanticRole::Text:
    case SemanticRole::Container:
        return 0;
    }
    return 0;
}

export
inline const char* semantic_intent_status_name(SemanticIntentStatus status) noexcept {
    switch (status) {
    case SemanticIntentStatus::Resolved:
        return "resolved";
    case SemanticIntentStatus::InvalidRoot:
        return "invalid_root";
    case SemanticIntentStatus::MissingId:
        return "missing_id";
    case SemanticIntentStatus::NotFound:
        return "not_found";
    case SemanticIntentStatus::AmbiguousId:
        return "ambiguous_id";
    case SemanticIntentStatus::UnsupportedAction:
        return "unsupported_action";
    case SemanticIntentStatus::Disabled:
        return "disabled";
    }
    return "unknown";
}

export
inline const char* semantic_action_admission_status_name(SemanticActionAdmissionStatus status) noexcept {
    switch (status) {
    case SemanticActionAdmissionStatus::Admitted:
        return "admitted";
    case SemanticActionAdmissionStatus::InvalidRoot:
        return "invalid_root";
    case SemanticActionAdmissionStatus::MissingId:
        return "missing_id";
    case SemanticActionAdmissionStatus::NotFound:
        return "not_found";
    case SemanticActionAdmissionStatus::AmbiguousId:
        return "ambiguous_id";
    case SemanticActionAdmissionStatus::UnsupportedAction:
        return "unsupported_action";
    case SemanticActionAdmissionStatus::Disabled:
        return "disabled";
    }
    return "unknown";
}

export
inline const char* semantic_action_request_status_name(SemanticActionRequestStatus status) noexcept {
    switch (status) {
    case SemanticActionRequestStatus::Executed:
        return "executed";
    case SemanticActionRequestStatus::Rejected:
        return "rejected";
    }
    return "unknown";
}

export
inline const char* semantic_action_request_reject_reason_name(
    SemanticActionRequestRejectReason reason) noexcept {
    switch (reason) {
    case SemanticActionRequestRejectReason::None:
        return "none";
    case SemanticActionRequestRejectReason::ActionAdmissionRejected:
        return "action_admission_rejected";
    case SemanticActionRequestRejectReason::FocusRequestRejected:
        return "focus_request_rejected";
    case SemanticActionRequestRejectReason::InputActionOverflow:
        return "input_action_overflow";
    case SemanticActionRequestRejectReason::NoActionEmitted:
        return "no_action_emitted";
    }
    return "unknown";
}

export
inline const char* semantic_action_request_stage_name(SemanticActionRequestStage stage) noexcept {
    switch (stage) {
    case SemanticActionRequestStage::None:
        return "none";
    case SemanticActionRequestStage::ActionAdmission:
        return "action_admission";
    case SemanticActionRequestStage::FocusRequest:
        return "focus_request";
    case SemanticActionRequestStage::Execution:
        return "execution";
    }
    return "unknown";
}

export
inline SemanticActionRequestStage semantic_action_request_stage(
    const SemanticActionRequest& request) noexcept {
    if (request.status == SemanticActionRequestStatus::Executed) {
        return SemanticActionRequestStage::Execution;
    }
    switch (request.reject_reason) {
    case SemanticActionRequestRejectReason::None:
        return SemanticActionRequestStage::None;
    case SemanticActionRequestRejectReason::ActionAdmissionRejected:
        return SemanticActionRequestStage::ActionAdmission;
    case SemanticActionRequestRejectReason::FocusRequestRejected:
        return SemanticActionRequestStage::FocusRequest;
    case SemanticActionRequestRejectReason::InputActionOverflow:
    case SemanticActionRequestRejectReason::NoActionEmitted:
        return SemanticActionRequestStage::Execution;
    }
    return SemanticActionRequestStage::None;
}

export
inline SemanticActionRequestLedger semantic_action_request_ledger(
    const SemanticActionRequest& request) noexcept {
    return SemanticActionRequestLedger{
        .stage = semantic_action_request_stage(request),
        .status = request.status,
        .reject_reason = request.reject_reason,
        .intent_status = request.admission.intent_status,
        .action_admission_status = request.admission.status,
        .focus_request_status = request.focus_request.status,
        .id = request.admission.id,
        .events_before = request.events_before,
        .events_after = request.events_after,
        .admitted = request.admitted,
        .focus_ready = request.focus_ready,
        .executed = request.executed,
        .emitted_click = request.emitted_click,
        .focus_started_on_target = request.before_focus == request.target,
        .focus_ended_on_target = request.after_focus == request.target,
    };
}

export
inline const char* semantic_focus_query_status_name(SemanticFocusQueryStatus status) noexcept {
    switch (status) {
    case SemanticFocusQueryStatus::Resolved:
        return "resolved";
    case SemanticFocusQueryStatus::InvalidRoot:
        return "invalid_root";
    case SemanticFocusQueryStatus::MissingId:
        return "missing_id";
    case SemanticFocusQueryStatus::NotFound:
        return "not_found";
    case SemanticFocusQueryStatus::AmbiguousId:
        return "ambiguous_id";
    case SemanticFocusQueryStatus::NotFocusable:
        return "not_focusable";
    case SemanticFocusQueryStatus::Disabled:
        return "disabled";
    case SemanticFocusQueryStatus::OutsideActiveScope:
        return "outside_active_scope";
    }
    return "unknown";
}

export
inline const char* semantic_focus_admission_status_name(SemanticFocusAdmissionStatus status) noexcept {
    switch (status) {
    case SemanticFocusAdmissionStatus::Admitted:
        return "admitted";
    case SemanticFocusAdmissionStatus::AlreadyFocused:
        return "already_focused";
    case SemanticFocusAdmissionStatus::InvalidRoot:
        return "invalid_root";
    case SemanticFocusAdmissionStatus::MissingId:
        return "missing_id";
    case SemanticFocusAdmissionStatus::NotFound:
        return "not_found";
    case SemanticFocusAdmissionStatus::AmbiguousId:
        return "ambiguous_id";
    case SemanticFocusAdmissionStatus::NotFocusable:
        return "not_focusable";
    case SemanticFocusAdmissionStatus::Disabled:
        return "disabled";
    case SemanticFocusAdmissionStatus::OutsideActiveScope:
        return "outside_active_scope";
    }
    return "unknown";
}

export
inline const char* semantic_focus_request_status_name(SemanticFocusRequestStatus status) noexcept {
    switch (status) {
    case SemanticFocusRequestStatus::Committed:
        return "committed";
    case SemanticFocusRequestStatus::AlreadyFocused:
        return "already_focused";
    case SemanticFocusRequestStatus::Rejected:
        return "rejected";
    }
    return "unknown";
}

export
inline const char* semantic_focus_request_stage_name(SemanticFocusRequestStage stage) noexcept {
    switch (stage) {
    case SemanticFocusRequestStage::None:
        return "none";
    case SemanticFocusRequestStage::FocusAdmission:
        return "focus_admission";
    case SemanticFocusRequestStage::AlreadyFocused:
        return "already_focused";
    case SemanticFocusRequestStage::Execution:
        return "execution";
    }
    return "unknown";
}

export
inline SemanticFocusRequestStage semantic_focus_request_stage(
    const SemanticFocusRequest& request) noexcept {
    switch (request.status) {
    case SemanticFocusRequestStatus::Committed:
        return SemanticFocusRequestStage::Execution;
    case SemanticFocusRequestStatus::AlreadyFocused:
        return SemanticFocusRequestStage::AlreadyFocused;
    case SemanticFocusRequestStatus::Rejected:
        if (!request.admission.admitted) {
            return SemanticFocusRequestStage::FocusAdmission;
        }
        return SemanticFocusRequestStage::Execution;
    }
    return SemanticFocusRequestStage::None;
}

export
inline SemanticFocusRequestLedger semantic_focus_request_ledger(
    const SemanticFocusRequest& request) noexcept {
    return SemanticFocusRequestLedger{
        .stage = semantic_focus_request_stage(request),
        .status = request.status,
        .admission_status = request.admission.status,
        .query_status = request.admission.query_status,
        .id = request.admission.id,
        .events_before = request.events_before,
        .events_after = request.events_after,
        .admitted = request.admission.admitted,
        .transfer_needed = request.admission.transfer_needed,
        .committed = request.committed,
        .focus_changed = request.focus_changed,
        .emitted_focus_out = request.emitted_focus_out,
        .emitted_focus_in = request.emitted_focus_in,
        .focus_started_on_target = request.before_focus == request.admission.handle,
        .focus_ended_on_target = request.after_focus == request.admission.handle,
    };
}

export
inline std::uint32_t semantic_tree_hash_mix(std::uint32_t hash, std::uint32_t value) noexcept {
    hash ^= value;
    hash *= 16777619u;
    return hash;
}

export
inline std::uint32_t semantic_tree_hash_text(std::uint32_t hash, const char* text) noexcept {
    if (!text) return semantic_tree_hash_mix(hash, 0xFFu);
    for (const char* p = text; *p; ++p) {
        hash = semantic_tree_hash_mix(hash, static_cast<std::uint8_t>(*p));
    }
    return semantic_tree_hash_mix(hash, 0u);
}

export
inline std::uint32_t semantic_tree_hash_node(std::uint32_t hash,
                                             const SemanticTreeNode& node) noexcept {
    hash = semantic_tree_hash_mix(hash, node.depth);
    hash = semantic_tree_hash_mix(hash, node.preorder);
    hash = semantic_tree_hash_text(hash, node.id);
    hash = semantic_tree_hash_text(hash, node.role);
    hash = semantic_tree_hash_text(hash, node.label);
    hash = semantic_tree_hash_mix(hash, node.actions);
    hash = semantic_tree_hash_mix(hash, static_cast<std::uint32_t>(node.bounds.x));
    hash = semantic_tree_hash_mix(hash, static_cast<std::uint32_t>(node.bounds.y));
    hash = semantic_tree_hash_mix(hash, static_cast<std::uint32_t>(node.bounds.w));
    hash = semantic_tree_hash_mix(hash, static_cast<std::uint32_t>(node.bounds.h));
    hash = semantic_tree_hash_mix(hash, node.focused ? 1u : 0u);
    hash = semantic_tree_hash_mix(hash, node.focusable ? 1u : 0u);
    return hash;
}

export
enum class ScrollBarOrientation : std::uint8_t {
    Horizontal = 0,
    Vertical = 1
};

export
enum class TableViewHeaderStyle : std::uint8_t {
    Default = 0,
    Accent = 1,
    Muted = 2
};

export
enum class TableViewColDividerStyle : std::uint8_t {
    None = 0,
    HeaderOnly = 1,
    BodyOnly = 2,
    Full = 3
};

export
struct StateCompact {
    std::uint8_t bits{0};
    std::uint8_t variant{0};

    bool enabled() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Enabled)) != 0;
    }

    bool hovered() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Hovered)) != 0;
    }

    bool pressed() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Pressed)) != 0;
    }

    bool focused() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Focused)) != 0;
    }
};
