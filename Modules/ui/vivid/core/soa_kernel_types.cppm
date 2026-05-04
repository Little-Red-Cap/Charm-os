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
struct SemanticFocusSnapshot {
    WidgetHandle handle{};
    const char* id{""};
    const char* role{"none"};
    const char* label{""};
    bool found{false};
    bool focusable{false};
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
