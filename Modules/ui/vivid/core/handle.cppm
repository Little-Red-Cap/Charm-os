module;
#include <cstdint>
export module charm.core.handle;

export
enum class WidgetKind : std::uint8_t {
    None = 0,
#include "widget_kind_enum.generated.inc"
};

export
struct alignas(4) WidgetHandle {
    WidgetKind kind{WidgetKind::None};
    std::uint16_t index{0};
    std::uint16_t generation{0};

    constexpr explicit operator bool() const noexcept { return kind != WidgetKind::None; }
};

static_assert(alignof(WidgetHandle) >= alignof(std::uint32_t));

export
inline const char* widget_kind_name(WidgetKind kind) noexcept {
    switch (kind) {
        case WidgetKind::None: return "None";
#include "widget_kind_name.generated.inc"
    }
    return "Unknown";
}

export
constexpr bool operator==(const WidgetHandle& a, const WidgetHandle& b) noexcept {
    return a.kind == b.kind && a.index == b.index && a.generation == b.generation;
}

export
constexpr bool operator!=(const WidgetHandle& a, const WidgetHandle& b) noexcept {
    return !(a == b);
}
