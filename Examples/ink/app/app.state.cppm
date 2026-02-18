// app.state.cppm
// 应用级状态 + 页面栈（零动态分配）
// 约定：
//  - 每个页面拥有自己的 PageState（focus / viewport 等）
//  - AppState 只负责：页面栈、共享业务数据、退出请求

module;
#include <array>
#include <cstdint>

export module app.state;

import gui.list_view;
import gui.ui_tree;
import gui.ui_semantics;
import gui.motion;
import gui.perf;
import gui.ui_settings;
import gui.ui_input_policy;
import app.data;
import gui.ui_list;
import gui.ui_focus_bookmark;
import gui.ui_list_shell;
import gui.ui_vtree;
import gui.qr_widget;


export namespace app
{
    struct AppState;

    // -----------------------------
    // 业务数据（跨页面共享）
    // -----------------------------

    enum class PageId : std::uint8_t {
        Main = 0,
        Battery = 1,
        Chart = 2,
        Settings = 3,
        ChartWave = 4,
        Icon = 5,
        Qr = 6,
        Widgets = 7,
    };

    enum class PopupContentKind : std::uint8_t {
        None = 0,
        Lamp = 1,
        Setting = 2,
    };

    enum class SettingField : std::uint8_t {
        ListSpeed = 0,
        WinSpeed = 1,
        SpotSpeed = 2,
        SpringOmega = 3,
        SpringZeta = 4,
    };

    inline constexpr gui::ui::NodeId kPopupDemoOwner = gui::ui::fnv1a("demo_popup");
    inline constexpr gui::ui::NodeId kPopupLampOwner = gui::ui::fnv1a("lamp_popup");
    inline constexpr gui::ui::NodeId kPopupSettingsOwner = gui::ui::fnv1a("settings_popup");

    // -----------------------------
    // 各页面自己的状态
    // -----------------------------

    struct MainPageState {
        // TODO: derive item_count from kMainItems without module cycles (e.g., items_meta module).
        static constexpr std::int16_t item_count = 15;
        gui::ListViewport viewport{};
        gui::ui::ListWidget<AppState, item_count> list{};
        std::int16_t      saved_focus{0};
        std::uint32_t     last_activate_ms{0}; // 用于短暂激活态反馈
        gui::motion::Spring1D highlight_spring{};
        gui::motion::Anim1D expand_anim{};
        std::int16_t      highlight_prev_first_y{0};
        bool             highlight_valid{false};
        std::uint32_t     highlight_last_ms{0};
        bool             expand_valid{false};
        gui::ui::FocusBookmark focus_bk{};
        gui::ui::ListPageShell<8> shell{};
    };

    struct BatteryPageState {
        // 预留：未来可做“步进编辑模式”等；当前仅用于 demo
        std::uint8_t step{1};
        gui::ui::VTree<12> vtree{};
        bool vtree_valid{false};
    };

    struct ChartPageState {
        std::uint32_t last_activate_ms{0};
        std::uint32_t last_sample_ms{0};
        static constexpr std::int16_t scope_count = 128;
        std::uint16_t scope[scope_count]{};
        bool scope_valid{false};
        gui::ui::ListPageShell<8> shell{};
    };

    struct IconPageState {
        std::int16_t index{0};
        std::uint32_t last_ms{0};
        gui::ui::ListPageShell<12> shell{};
    };

    struct QrPageState {
        gui::qr::QrCode code{};
    };

    struct WidgetsPageState {
        static constexpr std::int16_t item_count = 8;
        gui::ListViewport viewport{};
        gui::ui::ListWidget<AppState, item_count> list{};
        std::uint32_t last_activate_ms{0};
        gui::ui::FocusBookmark focus_bk{};
        gui::ui::ListPageShell<8> shell{};
        std::uint8_t range_value{42};
        std::uint16_t stepper_value{12};
        std::uint8_t segmented_index{0};
        const char* toast_text{nullptr};
        std::uint32_t toast_until_ms{0};
    };

    struct SettingsPageState {
        static constexpr std::int16_t kBaseCount = 14;
        static constexpr std::int16_t item_count =
            (std::int16_t)(kBaseCount +
                           gui::motion::anim_preset_count() +
                           gui::motion::spring_preset_count());
        gui::ListViewport viewport{};
        gui::ui::ListWidget<AppState, item_count> list{};
        std::int16_t      saved_focus{0};
        std::uint32_t     last_activate_ms{0};
        gui::motion::Spring1D highlight_spring{};
        gui::motion::Anim1D expand_anim{};
        std::int16_t      highlight_prev_first_y{0};
        bool             highlight_valid{false};
        std::uint32_t     highlight_last_ms{0};
        bool             expand_valid{false};
        gui::ui::FocusBookmark focus_bk{};
        gui::ui::ListPageShell<8> shell{};
    };

    struct PopupState {
        PopupContentKind content{PopupContentKind::None};
        SettingField setting{SettingField::ListSpeed};
        std::uint32_t show_ms{0};
        gui::motion::Anim1D popup_anim_y{};
        bool popup_anim_valid{false};
        gui::ui::NodeId last_owner_id{0};
        bool last_owner_valid{false};
        PopupContentKind last_content{PopupContentKind::None};
        bool last_content_valid{false};
        std::int16_t popup_w{0};
        std::int16_t popup_h{0};
        bool popup_size_valid{false};
    };

    // -----------------------------
    // 页面栈
    // -----------------------------

    struct PageStack {
        static constexpr std::int16_t kMaxDepth = 4;

        PageId       ids[kMaxDepth]{};
        std::int16_t depth{0};

        inline void reset(PageId root) noexcept { ids[0] = root; depth  = 1; }

        [[nodiscard]] inline PageId current() const noexcept { return ids[depth - 1]; }

        inline bool push(PageId id) noexcept
        {
            if (depth >= kMaxDepth) return false;
            ids[depth++] = id;
            return true;
        }

        inline bool pop() noexcept
        {
            if (depth <= 1) return false; // 保留根页面
            --depth;
            return true;
        }

        [[nodiscard]] inline bool is_root() const noexcept { return depth <= 1; }
    };

    // -----------------------------
    // AppState：整个应用唯一状态
    // -----------------------------

    struct AppState
    {
        AppData data{};
        gui::ui::UiSettings ui{};
        std::uint32_t now_ms{0};
        gui::perf::FpsCounter fps{};
        gui::perf::FpsCounter fps_ui{};
        gui::perf::TickSource tick{};
        gui::ui::InputPolicyRegistry input_policies{};
        gui::ui::InputPolicyId input_policy_id{gui::ui::InputPolicyId::Default};
        gui::ui::InputPolicy input_policy{};

        PageStack pages{};
        PageId last_page{PageId::Main};

        MainPageState main_page{};
        BatteryPageState battery_page{};
        ChartPageState chart_page{};
        SettingsPageState settings_page{};
        IconPageState icon_page{};
        QrPageState qr_page{};
        WidgetsPageState widgets_page{};
        PopupState popup{};
        gui::ui::UiSemantics semantics{};

        bool request_quit{false};

        inline void init() noexcept
        {
            pages.reset(PageId::Main);
            last_page = PageId::Main;
            request_quit = false;
            ui = {};
            main_page.viewport.reset();
            main_page.list = {};
            battery_page.step = 1;
            battery_page.vtree.clear();
            battery_page.vtree_valid = false;
            main_page.last_activate_ms = 0;
            main_page.highlight_prev_first_y = 0;
            main_page.highlight_valid = false;
            main_page.expand_valid = false;
            main_page.saved_focus = 0;
            main_page.shell.reset();
            now_ms = 0;
            fps = {};
            fps_ui = {};
            tick = {};
            input_policies = {};
            input_policy_id = gui::ui::InputPolicyId::Default;
            input_policy = {};
            chart_page.last_activate_ms = 0;
            chart_page.last_sample_ms = 0;
            chart_page.scope_valid = false;
            chart_page.shell.reset();
            settings_page.last_activate_ms = 0;
            settings_page.viewport.reset();
            settings_page.list = {};
            settings_page.highlight_prev_first_y = 0;
            settings_page.highlight_valid = false;
            settings_page.expand_valid = false;
            settings_page.saved_focus = 0;
            settings_page.shell.reset();
            icon_page.index = 0;
            icon_page.last_ms = 0;
            icon_page.shell.reset();
            qr_page.code = {};
            widgets_page.viewport.reset();
            widgets_page.list = {};
            widgets_page.last_activate_ms = 0;
            widgets_page.focus_bk = {};
            widgets_page.focus_bk.index = 1;
            widgets_page.shell.reset();
            widgets_page.range_value = 42;
            widgets_page.stepper_value = 12;
            widgets_page.segmented_index = 0;
            widgets_page.toast_text = nullptr;
            widgets_page.toast_until_ms = 0;
            popup.content = PopupContentKind::None;
            popup.setting = SettingField::ListSpeed;
            popup.show_ms = 0;
            popup.popup_anim_valid = false;
            popup.last_owner_id = 0;
            popup.last_owner_valid = false;
            popup.last_content = PopupContentKind::None;
            popup.last_content_valid = false;
            popup.popup_w = 0;
            popup.popup_h = 0;
            popup.popup_size_valid = false;
            semantics.model_id = gui::ui::fnv1a("main_root");
            semantics.focus.domain_id = gui::ui::fnv1a("main_list");
            semantics.focus.index = 0;
            semantics.focus.count = MainPageState::item_count;
            semantics.focus.target_id = gui::ui::list_id(semantics.focus.domain_id, 1);
            semantics.nav.kind = gui::ui::NavKind::List;
            semantics.nav.wrap = gui::ui::NavWrap::Ring;
            semantics.phase = gui::ui::InteractionPhase::Idle;
        }
    };

    inline void set_tick_source(AppState& s, gui::perf::TickSource tick) noexcept
    {
        s.tick = tick;
    }
} // namespace app




