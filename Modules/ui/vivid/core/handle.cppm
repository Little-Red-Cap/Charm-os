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
    IconButton,
    Checkbox,
    Led,
    Slider,
    Switch,
    Progress,
    List,
    ListItem,
    ListView,
    IconList,
    TextTrackingList,
    TextList,
    ModalDialog,
    ProgressBarSimple,
    DynamicNebula,
    CrtScreen,
    ScrollBar,
    SegmentedControl,
    TextArea,
    TextInput,
    NumberInput,
    ToggleGroup,
    TableView,
    TreeView,
    Dropdown,
    TabView,
    Roller,
    Spinner,
    Bar,
    PopupLayer,
    MessageBox,
    Menu,
    MenuItem,
    Radio,
    RadioGroup
    ,Chart
    ,Waveform
    ,Gauge
    ,PrimitivesCanvas
    ,PerfOverlay
    ,Stepper
    ,Timeline
    ,RichText
    ,CodeBlock
    ,ProgressWheel
    ,WaveformView
    ,BatteryGauge
    ,HistogramView
    ,RingIndication
    ,TextBox
    ,FoldablePanel
    ,ProgressFlowing
    ,CloudyGlass
    ,NumberList
    ,ProgressBarRound
    ,SpinZoomWidget
    ,SpinningWheel
    ,ImageBox
    ,MeterPointer
    ,ProgressBarDrill
    ,SpectrumView
    ,BusyWheel
    ,ConsoleBox
    ,BatteryGasGauge
    ,Histogram
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
        case WidgetKind::IconButton: return "IconButton";
        case WidgetKind::Checkbox: return "Checkbox";
        case WidgetKind::Led: return "Led";
        case WidgetKind::Slider: return "Slider";
        case WidgetKind::Switch: return "Switch";
        case WidgetKind::Progress: return "Progress";
        case WidgetKind::List: return "List";
        case WidgetKind::ListItem: return "ListItem";
        case WidgetKind::ListView: return "ListView";
        case WidgetKind::IconList: return "IconList";
        case WidgetKind::TextTrackingList: return "TextTrackingList";
        case WidgetKind::TextList: return "TextList";
        case WidgetKind::ModalDialog: return "ModalDialog";
        case WidgetKind::ProgressBarSimple: return "ProgressBarSimple";
        case WidgetKind::DynamicNebula: return "DynamicNebula";
        case WidgetKind::CrtScreen: return "CrtScreen";
        case WidgetKind::ScrollBar: return "ScrollBar";
        case WidgetKind::SegmentedControl: return "SegmentedControl";
        case WidgetKind::TextArea: return "TextArea";
        case WidgetKind::TextInput: return "TextInput";
        case WidgetKind::NumberInput: return "NumberInput";
        case WidgetKind::ToggleGroup: return "ToggleGroup";
        case WidgetKind::TableView: return "TableView";
        case WidgetKind::TreeView: return "TreeView";
        case WidgetKind::Dropdown: return "Dropdown";
        case WidgetKind::TabView: return "TabView";
        case WidgetKind::Roller: return "Roller";
        case WidgetKind::Spinner: return "Spinner";
        case WidgetKind::Bar: return "Bar";
        case WidgetKind::PopupLayer: return "PopupLayer";
        case WidgetKind::MessageBox: return "MessageBox";
        case WidgetKind::Menu: return "Menu";
        case WidgetKind::MenuItem: return "MenuItem";
        case WidgetKind::Radio: return "Radio";
        case WidgetKind::RadioGroup: return "RadioGroup";
        case WidgetKind::Chart: return "Chart";
        case WidgetKind::Waveform: return "Waveform";
        case WidgetKind::Gauge: return "Gauge";
        case WidgetKind::PrimitivesCanvas: return "PrimitivesCanvas";
        case WidgetKind::PerfOverlay: return "PerfOverlay";
        case WidgetKind::Stepper: return "Stepper";
        case WidgetKind::Timeline: return "Timeline";
        case WidgetKind::RichText: return "RichText";
        case WidgetKind::CodeBlock: return "CodeBlock";
        case WidgetKind::ProgressWheel: return "ProgressWheel";
        case WidgetKind::WaveformView: return "WaveformView";
        case WidgetKind::BatteryGauge: return "BatteryGauge";
        case WidgetKind::HistogramView: return "HistogramView";
        case WidgetKind::RingIndication: return "RingIndication";
        case WidgetKind::TextBox: return "TextBox";
        case WidgetKind::FoldablePanel: return "FoldablePanel";
        case WidgetKind::ProgressFlowing: return "ProgressFlowing";
        case WidgetKind::CloudyGlass: return "CloudyGlass";
        case WidgetKind::NumberList: return "NumberList";
        case WidgetKind::ProgressBarRound: return "ProgressBarRound";
        case WidgetKind::SpinZoomWidget: return "SpinZoomWidget";
        case WidgetKind::SpinningWheel: return "SpinningWheel";
        case WidgetKind::ImageBox: return "ImageBox";
        case WidgetKind::MeterPointer: return "MeterPointer";
        case WidgetKind::ProgressBarDrill: return "ProgressBarDrill";
        case WidgetKind::SpectrumView: return "SpectrumView";
        case WidgetKind::BusyWheel: return "BusyWheel";
        case WidgetKind::ConsoleBox: return "ConsoleBox";
        case WidgetKind::BatteryGasGauge: return "BatteryGasGauge";
        case WidgetKind::Histogram: return "Histogram";
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
