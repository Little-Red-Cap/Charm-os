module;
#include <cstdint>
export module charm.core.handle;

export
enum class WidgetKind : std::uint8_t {
    None,
    Container,
    ScrollContainer,
    Dial,
    Arc,
    Image,
    Label,
    Button,
    Checkbox,
    Slider,
    Switch,
    Progress,
    List,
    ListItem,
    ListView,
    ScrollBar,
    TextArea,
    TextInput,
    NumberInput,
    Dropdown,
    TabView,
    Roller,
    Spinner,
    Bar,
    PopupLayer,
    Menu,
    MenuItem,
    Radio,
    RadioGroup
    ,Chart
    ,Gauge
    ,PrimitivesCanvas
};

export
struct WidgetHandle {
    WidgetKind kind{WidgetKind::None};
    std::uint16_t index{0};
    std::uint16_t generation{0};

    constexpr explicit operator bool() const noexcept { return kind != WidgetKind::None; }
};

export
inline const char* widget_kind_name(WidgetKind kind) noexcept {
    switch (kind) {
        case WidgetKind::None: return "None";
        case WidgetKind::Container: return "Container";
        case WidgetKind::ScrollContainer: return "ScrollContainer";
        case WidgetKind::Dial: return "Dial";
        case WidgetKind::Arc: return "Arc";
        case WidgetKind::Image: return "Image";
        case WidgetKind::Label: return "Label";
        case WidgetKind::Button: return "Button";
        case WidgetKind::Checkbox: return "Checkbox";
        case WidgetKind::Slider: return "Slider";
        case WidgetKind::Switch: return "Switch";
        case WidgetKind::Progress: return "Progress";
        case WidgetKind::List: return "List";
        case WidgetKind::ListItem: return "ListItem";
        case WidgetKind::ListView: return "ListView";
        case WidgetKind::ScrollBar: return "ScrollBar";
        case WidgetKind::TextArea: return "TextArea";
        case WidgetKind::TextInput: return "TextInput";
        case WidgetKind::NumberInput: return "NumberInput";
        case WidgetKind::Dropdown: return "Dropdown";
        case WidgetKind::TabView: return "TabView";
        case WidgetKind::Roller: return "Roller";
        case WidgetKind::Spinner: return "Spinner";
        case WidgetKind::Bar: return "Bar";
        case WidgetKind::PopupLayer: return "PopupLayer";
        case WidgetKind::Menu: return "Menu";
        case WidgetKind::MenuItem: return "MenuItem";
        case WidgetKind::Radio: return "Radio";
        case WidgetKind::RadioGroup: return "RadioGroup";
        case WidgetKind::Chart: return "Chart";
        case WidgetKind::Gauge: return "Gauge";
        case WidgetKind::PrimitivesCanvas: return "PrimitivesCanvas";
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
