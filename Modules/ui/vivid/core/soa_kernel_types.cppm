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
