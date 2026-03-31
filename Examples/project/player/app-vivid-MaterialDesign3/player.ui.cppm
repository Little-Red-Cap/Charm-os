module;
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module player.ui;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.gfx.color;
import charm.gfx.image;
import charm.font;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
import player.font_cache;
import charm.widgets.button;
import charm.widgets.chart;
import charm.widgets.cloudy_glass;
import charm.widgets.busy_wheel;
import charm.widgets.console_box;
import charm.widgets.foldable_panel;
import charm.widgets.histogram_view;
import charm.widgets.histogram;
import charm.widgets.dynamic_nebula;
import charm.widgets.crt_screen;
import charm.widgets.spectrum_view;
import charm.widgets.spinning_wheel;
import charm.widgets.image;
import charm.widgets.image_box;
import charm.widgets.label;
import charm.widgets.meter_pointer;
import charm.widgets.list_view;
import charm.widgets.progress;
import charm.widgets.battery_gasgauge;
import charm.widgets.progress_bar_drill;
import charm.widgets.progress_bar_simple;
import charm.widgets.scrollbar;
import charm.widgets.scroll_container;
import charm.widgets.segmented_control;
import charm.widgets.perf_overlay;
import charm.widgets.switcher;
import charm.widgets.slider;
import charm.widgets.dropdown;
import charm.gfx.svg;

export namespace player::ui {
    enum class PlayerStyleClass : StyleClassId {
        ControlSide = 1,
        ControlPlay = 2,
        TopBarButton = 3,
        TabBase = 4,
        ShuffleShadow = 5,
        ListCard = 6,
        InfoTag = 7,
        BottomBar = 8,
        BottomButton = 9,
        BottomPlay = 10,
    };

    inline constexpr int kUiPadding = 18;
    inline constexpr int kCoverSize = 340;
    inline constexpr int kDemoGap = 16;
    inline constexpr int kHeaderTitleOffset = 18;
    inline constexpr int kHeaderSubtitleOffset = 44;
    inline constexpr int kHeaderProgressOffset = 86;
    inline constexpr int kHeaderTimeOffset = 114;
    inline constexpr int kHeaderStatusOffset = 136;
    inline constexpr int kHeaderModeOffset = 160;
    inline constexpr int kModeWidth = 240;
    inline constexpr int kModeHeight = 28;
    inline constexpr int kModeHintHeight = 18;
    inline constexpr int kModeHintGap = 6;
    inline constexpr int kSpectrumHeight = 80;
    inline constexpr int kSpectrumGap = 12;
    inline constexpr int kOptionsHeight = 34;
    inline constexpr int kOptionsGap = 10;
    inline constexpr int kOptionLabelWidth = 92;
    inline constexpr int kEqBands = 5;
    inline constexpr int kEqPanelHeight = 248;
    inline constexpr int kEqTitleHeight = 18;
    inline constexpr int kEqRowHeight = 22;
    inline constexpr int kEqRowGap = 6;
    inline constexpr int kEqLabelWidth = 40;
    inline constexpr int kEqValueWidth = 44;
    inline constexpr int kEqRowGapX = 10;
    inline constexpr int kDspToggleWidth = 44;
    inline constexpr int kEqPresetLabelWidth = 56;
    inline constexpr int kEqPresetWidth = 140;
    inline constexpr int kListTitleGap = 26;
    inline constexpr int kListBottomReserve = 150;
    inline constexpr int kListScrollWidth = 10;
    inline constexpr int kControlsBottomMargin = 22;
    inline constexpr int kButtonWidth = 100;
    inline constexpr int kButtonHeight = 56;
    inline constexpr int kButtonGap = 12;
    inline constexpr int kModeButtonWidth = 72;
    inline constexpr int kPerfOverlayWidth = 300;
    inline constexpr int kPerfOverlayHeight = 96;

    inline constexpr rgba kUiBackground = {16, 18, 24, 255};
    inline constexpr rgba kUiCover = {40, 42, 56, 255};
    inline constexpr rgba kUiTitle = {240, 242, 250, 255};
    inline constexpr rgba kUiSubtitle = {170, 176, 200, 255};
    inline constexpr rgba kUiTime = {122, 132, 156, 255};
    inline constexpr rgba kUiStatus = {160, 166, 190, 255};
    inline constexpr rgba kUiListTitle = {210, 214, 230, 255};
    inline constexpr rgba kUiHint = {130, 138, 160, 255};
    inline constexpr rgba kUiError = {232, 130, 140, 255};
    inline constexpr rgba kUiOk = {156, 210, 196, 255};
    inline constexpr rgba kUiPaused = {230, 190, 120, 255};
    inline constexpr rgba kUiOption = {190, 198, 220, 255};
    inline constexpr rgba kUiSwitchOn = {140, 176, 255, 255};
    inline constexpr rgba kUiEqTitle = {210, 216, 235, 255};

    inline constexpr rgba kUiButtonBg = {36, 40, 58, 255};
    inline constexpr rgba kUiButtonBorder = {64, 74, 102, 255};
    inline constexpr rgba kUiButtonHover = {48, 56, 82, 255};
    inline constexpr rgba kUiTabBg = {32, 36, 52, 255};
    inline constexpr rgba kUiTabActive = {72, 92, 132, 255};
    inline constexpr rgba kUiListBg = {34, 38, 56, 255};
    inline constexpr rgba kUiInfoTagBg = {28, 32, 46, 255};
    inline constexpr rgba kUiListBorder = {62, 72, 104, 255};
    inline constexpr rgba kUiListFont = {220, 228, 246, 255};
    inline constexpr rgba kUiProgressBg = {22, 26, 40, 255};
    inline constexpr rgba kUiProgressBorder = {64, 74, 102, 255};
    inline constexpr rgba kUiBackdropBase = {18, 20, 30, 255};
    inline constexpr rgba kUiScrollBg = {36, 40, 58, 255};
    inline constexpr rgba kUiScrollBorder = {72, 82, 110, 255};
    inline constexpr rgba kUiPerfBg = {26, 28, 40, 220};
    inline constexpr rgba kUiPerfBorder = {70, 80, 110, 255};
    inline constexpr rgba kUiPerfFont = {230, 236, 248, 255};
    inline constexpr rgba kUiPlayBg = {14, 18, 30, 255};
    inline constexpr rgba kUiPlayShadow = {0, 0, 0, 160};
    inline constexpr rgba kUiCardShadow = {0, 0, 0, 70};

    struct PlayerIconIds {
        ::ui::gfx::ImageId prev{};
        ::ui::gfx::ImageId play{};
        ::ui::gfx::ImageId pause{};
        ::ui::gfx::ImageId next{};
        ::ui::gfx::ImageId loop{};
        ::ui::gfx::ImageId single{};
        ::ui::gfx::ImageId shuffle{};
        ::ui::gfx::ImageId folder{};
    };

    namespace detail {
        constexpr int kIconSize = 32;
        constexpr int kIconStride = kIconSize * 4;
        using IconBuffer = std::array<std::byte, kIconSize * kIconSize * 4>;
        constexpr ::ui::gfx::svg::ViewBox kIconView{960.0f, 960.0f};

        void icon_clear(IconBuffer& buf) {
            buf.fill(std::byte{0});
        }

        void icon_set_pixel(IconBuffer& buf, int x, int y, const rgba& color) {
            if (x < 0 || y < 0 || x >= kIconSize || y >= kIconSize) return;
            const std::size_t idx = static_cast<std::size_t>(y * kIconSize + x) * 4;
            buf[idx + 0] = std::byte{color.a};
            buf[idx + 1] = std::byte{color.r};
            buf[idx + 2] = std::byte{color.g};
            buf[idx + 3] = std::byte{color.b};
        }

        bool rasterize_svg_path(IconBuffer& buf, std::string_view path, const rgba& color) {
            const ::ui::gfx::svg::RasterConfig cfg{.width = kIconSize, .height = kIconSize, .view = kIconView};
            return ::ui::gfx::svg::rasterize_path(path, cfg,
                                                std::span<std::byte>(buf.data(), buf.size()),
                                                color, true);
        }

        constexpr std::string_view kPathPrev =
            "M220,680L220,280Q220,263 231.5,251.5Q243,240 260,240Q277,240 288.5,251.5Q300,263 300,280L300,680Q300,697 288.5,708.5Q277,720 260,720Q243,720 231.5,708.5Q220,697 220,680Z"
            "M678,679L430,513Q421,507 416.5,498.5Q412,490 412,480Q412,470 416.5,461.5Q421,453 430,447L678,281Q683,277 689,276Q695,275 700,275Q716,275 728,286Q740,297 740,315L740,645Q740,663 728,674Q716,685 700,685Q695,685 689,684Q683,683 678,679Z"
            "M660,480L660,480L660,480Z"
            "M660,570L660,390L524,480L660,570Z";
        constexpr std::string_view kPathPlay =
            "M320,687L320,273Q320,256 332,244.5Q344,233 360,233Q365,233 370.5,234.5Q376,236 381,239L707,446Q716,452 720.5,461Q725,470 725,480Q725,490 720.5,499Q716,508 707,514L381,721Q376,724 370.5,725.5Q365,727 360,727Q344,727 332,715.5Q320,704 320,687Z"
            "M400,480L400,480L400,480Z"
            "M400,614L610,480L400,346L400,614Z";
        constexpr std::string_view kPathPause =
            "M600,760Q567,760 543.5,736.5Q520,713 520,680L520,280Q520,247 543.5,223.5Q567,200 600,200L680,200Q713,200 736.5,223.5Q760,247 760,280L760,680Q760,713 736.5,736.5Q713,760 680,760L600,760Z"
            "M280,760Q247,760 223.5,736.5Q200,713 200,680L200,280Q200,247 223.5,223.5Q247,200 280,200L360,200Q393,200 416.5,223.5Q440,247 440,280L440,680Q440,713 416.5,736.5Q393,760 360,760L280,760Z"
            "M600,680L680,680L680,280L600,280L600,680Z"
            "M280,680L360,680L360,280L280,280L280,680Z";
        constexpr std::string_view kPathNext =
            "M660,680L660,280Q660,263 671.5,251.5Q683,240 700,240Q717,240 728.5,251.5Q740,263 740,280L740,680Q740,697 728.5,708.5Q717,720 700,720Q683,720 671.5,708.5Q660,697 660,680Z"
            "M220,645L220,315Q220,297 232,286Q244,275 260,275Q265,275 271,276Q277,277 282,281L530,447Q539,453 543.5,461.5Q548,470 548,480Q548,490 543.5,498.5Q539,507 530,513L282,679Q277,683 271,684Q265,685 260,685Q244,685 232,674Q220,663 220,645Z"
            "M300,480L300,480L300,480Z"
            "M300,570L436,480L300,390L300,570Z";
        constexpr std::string_view kPathFolder =
            "M120,260H360L440,340H840V760H120Z";

        void build_prev_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathPrev, color);
        }

        void build_play_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathPlay, color);
        }

        void build_pause_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathPause, color);
        }

        void build_loop_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf,
                               "M274,760L308,794Q320,806 319.5,822Q319,838 308,850Q296,862 279.5,862.5Q263,863 251,851L148,748Q142,742 139.5,735Q137,728 137,720Q137,712 139.5,705Q142,698 148,692L251,589Q263,577 279.5,577.5Q296,578 308,590Q319,602 319.5,618Q320,634 308,646L274,680L680,680Q680,680 680,680Q680,680 680,680L680,560Q680,543 691.5,531.5Q703,520 720,520Q737,520 748.5,531.5Q760,543 760,560L760,680Q760,713 736.5,736.5Q713,760 680,760L274,760Z"
                               "M686,280L280,280Q280,280 280,280Q280,280 280,280L280,400Q280,417 268.5,428.5Q257,440 240,440Q223,440 211.5,428.5Q200,417 200,400L200,280Q200,247 223.5,223.5Q247,200 280,200L686,200L652,166Q640,154 640.5,138Q641,122 652,110Q664,98 680.5,97.5Q697,97 709,109L812,212Q818,218 820.5,225Q823,232 823,240Q823,248 820.5,255Q818,262 812,268L709,371Q697,383 680.5,382.5Q664,382 652,370Q641,358 640.5,342Q640,326 652,314L686,280Z",
                               color);
        }

        void build_single_icon(IconBuffer& buf, const rgba& color) {
            build_loop_icon(buf, color);
            rasterize_svg_path(buf,
                               "M460,420L430,420Q417,420 408.5,411.5Q400,403 400,390Q400,377 408.5,368.5Q417,360 430,360L480,360Q497,360 508.5,371.5Q520,383 520,400L520,570Q520,583 511.5,591.5Q503,600 490,600Q477,600 468.5,591.5Q460,583 460,570L460,420Z"
                               "M274,760L308,794Q320,806 319.5,822Q319,838 308,850Q296,862 279.5,862.5Q263,863 251,851L148,748Q142,742 139.5,735Q137,728 137,720Q137,712 139.5,705Q142,698 148,692L251,589Q263,577 279.5,577.5Q296,578 308,590Q319,602 319.5,618Q320,634 308,646L274,680L680,680Q680,680 680,680Q680,680 680,680L680,560Q680,543 691.5,531.5Q703,520 720,520Q737,520 748.5,531.5Q760,543 760,560L760,680Q760,713 736.5,736.5Q713,760 680,760L274,760Z"
                               "M686,280L280,280Q280,280 280,280Q280,280 280,280L280,400Q280,417 268.5,428.5Q257,440 240,440Q223,440 211.5,428.5Q200,417 200,400L200,280Q200,247 223.5,223.5Q247,200 280,200L686,200L652,166Q640,154 640.5,138Q641,122 652,110Q664,98 680.5,97.5Q697,97 709,109L812,212Q818,218 820.5,225Q823,232 823,240Q823,248 820.5,255Q818,262 812,268L709,371Q697,383 680.5,382.5Q664,382 652,370Q641,358 640.5,342Q640,326 652,314L686,280Z",
                               color);
        }

        void build_shuffle_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf,
                               "M600,800Q583,800 571.5,788.5Q560,777 560,760Q560,743 571.5,731.5Q583,720 600,720L664,720L565,621Q553,609 553.5,592.5Q554,576 566,564Q578,552 594.5,552Q611,552 623,564L720,662L720,600Q720,583 731.5,571.5Q743,560 760,560Q777,560 788.5,571.5Q800,583 800,600L800,760Q800,777 788.5,788.5Q777,800 760,800L600,800Z"
                               "M172,788Q161,777 161,760Q161,743 172,732L664,240L600,240Q583,240 571.5,228.5Q560,217 560,200Q560,183 571.5,171.5Q583,160 600,160L760,160Q777,160 788.5,171.5Q800,183 800,200L800,360Q800,377 788.5,388.5Q777,400 760,400Q743,400 731.5,388.5Q720,377 720,360L720,296L228,788Q217,799 200,799Q183,799 172,788Z"
                               "M171,228Q160,217 160,200Q160,183 171,172Q182,161 198.5,161Q215,161 227,172L395,339Q406,350 406.5,366.5Q407,383 395,395Q384,406 367,406Q350,406 339,395L171,228Z",
                               color);
        }

        void build_next_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathNext, color);
        }

        void build_folder_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathFolder, color);
        }
    }

    inline ImageView icon_prev() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_prev_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_play() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_play_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_pause() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_pause_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_loop() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_loop_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_single() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_single_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_shuffle() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_shuffle_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_next() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_next_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline ImageView icon_folder() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_folder_icon(buf, kUiListFont);
            init = true;
        }
        return make_image_view(PixelFormat::ARGB8888,
                               detail::kIconSize,
                               detail::kIconSize,
                               detail::kIconStride,
                               buf.data(),
                               false,
                               false);
    }

    inline PlayerIconIds register_player_icons() noexcept {
        PlayerIconIds out{};
        auto reg = [](const ImageView& view) noexcept {
            const auto res = ::ui::gfx::register_image_dedup(view);
            return res.ok() ? res.id : ::ui::gfx::invalid_image_id();
        };
        out.prev = reg(icon_prev());
        out.play = reg(icon_play());
        out.pause = reg(icon_pause());
        out.next = reg(icon_next());
        out.loop = reg(icon_loop());
        out.single = reg(icon_single());
        out.shuffle = reg(icon_shuffle());
        out.folder = reg(icon_folder());
        return out;
    }

    inline const Font& player_default_font() noexcept {
        return get_font(FontId::Normal);
    }

    inline void apply_player_theme() {
        set_default_font(FontId::Small, &font_noto_ascii_16);
        set_default_font(FontId::Normal, &font_noto_ascii_16);
        set_default_font(FontId::Large, &font_noto_ascii_16);
        set_default_font(FontId::Mono, &font_noto_ascii_16);
        if (!player::font_cache::init()) {
            set_default_fallback_font(&font_noto_sc_16);
        }

        auto& theme = Theme::instance();
        theme.set_default_font(player_default_font());

        {
            StylePatch side{};
            side.has_corner_radius = true;
            side.corner_radius = 24;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ControlSide), side);

            StylePatch play{};
            play.has_corner_radius = true;
            play.corner_radius = 24;
            play.has_shadow_enabled = true;
            play.shadow_enabled = true;
            play.has_shadow_color = true;
            play.shadow_color = kUiPlayShadow;
            play.has_shadow_offset_x = true;
            play.shadow_offset_x = 0;
            play.has_shadow_offset_y = true;
            play.shadow_offset_y = 6;
            play.has_shadow_spread = true;
            play.shadow_spread = 0;
            play.has_shadow_radius = true;
            play.shadow_radius = 18;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ControlPlay), play);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiButtonBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 12;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::TopBarButton), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiTabBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::TabBase), patch);
        }

        {
            StylePatch patch{};
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 3;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 3;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ShuffleShadow), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiListBg;
            patch.has_border_color = true;
            patch.border_color = kUiListBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 16;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 4;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 4;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 18;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ListCard), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiInfoTagBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 10;
            patch.has_font_color = true;
            patch.font_color = kUiTime;
            patch.has_font = true;
            patch.font = &get_font(FontId::Small);
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::InfoTag), patch);
        }

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiListBg;
            patch.has_border_color = true;
            patch.border_color = kUiListBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 18;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 4;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 6;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 18;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::BottomBar), patch);
        }
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = 16;
            patch.has_bg_color = true;
            patch.bg_color = kUiButtonBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 3;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 10;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::BottomButton), patch);

            StylePatch play = patch;
            play.corner_radius = 18;
            play.has_shadow_offset_y = true;
            play.shadow_offset_y = 4;
            play.has_shadow_radius = true;
            play.shadow_radius = 14;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::BottomPlay), play);
        }
        Style baseline = theme.get<Button>();
        baseline.font = &player_default_font();
        baseline.colors.border_color = kUiButtonBorder;
        baseline.colors.border_focus = kUiOk;
        baseline.metrics.padding = 8;
        baseline.metrics.corner_radius = 14;
        apply_baseline_theme_preset(baseline);

        ThemePreset preset{};
        preset.has_label = true;
        preset.label = theme.get<Label>();
        preset.label.colors.font_color = kUiTitle;
        preset.has_button = true;
        preset.button = theme.get<Button>();
        preset.button.colors.bg_color = kUiButtonBg;
        preset.button.colors.border_color = kUiButtonBorder;
        preset.button.metrics.padding = 8;
        preset.button.metrics.corner_radius = 14;
        preset.button.colors.font_color = kUiListFont;
        preset.has_list_view = true;
        preset.list_view = theme.get<ListView>();
        preset.list_view.colors.bg_color = kUiListBg;
        preset.list_view.colors.border_color = kUiListBorder;
        preset.list_view.colors.on_accent = kUiListFont;
        preset.list_view.metrics.corner_radius = 12;
        preset.list_view.metrics.padding = 12;
        preset.list_view.colors.font_color = kUiListFont;
        preset.has_progress = true;
        preset.progress = theme.get<Progress>();
        preset.progress.colors.bg_color = kUiProgressBg;
        preset.progress.colors.border_color = kUiProgressBorder;
        preset.has_scroll_bar = true;
        preset.scroll_bar = theme.get<ScrollBar>();
        preset.scroll_bar.colors.bg_color = kUiScrollBg;
        preset.scroll_bar.colors.border_color = kUiScrollBorder;
        preset.scroll_bar.metrics.corner_radius = 10;
        apply_theme_preset(preset);

        auto& sheet = StyleSheet::instance();
        auto set_base = [&](WidgetKind kind, Style s) {
            if (!s.font) {
                s.font = &player_default_font();
            }
            sheet.set_base_style(kind, s);
        };

        Style scroll_container = theme.get<ScrollContainer>();
        scroll_container.colors.bg_color = kUiBackground;
        scroll_container.colors.border_color = kUiBackground;
        set_base(WidgetKind::ScrollContainer, scroll_container);

        Style progress_bar_simple = theme.get<ProgressBarSimple>();
        progress_bar_simple.colors.bg_color = kUiProgressBg;
        progress_bar_simple.colors.border_color = kUiProgressBorder;
        set_base(WidgetKind::ProgressBarSimple, progress_bar_simple);

        Style segmented_control = theme.get<SegmentedControl>();
        segmented_control.colors.bg_color = kUiButtonBg;
        segmented_control.colors.border_color = kUiButtonBorder;
        segmented_control.colors.bg_pressed = kUiButtonHover;
        segmented_control.colors.border_pressed = kUiButtonBorder;
        segmented_control.colors.font_color = kUiListFont;
        segmented_control.metrics.corner_radius = 14;
        segmented_control.metrics.padding = 6;
        set_base(WidgetKind::SegmentedControl, segmented_control);

        Style perf_overlay = theme.get<PerfOverlay>();
        perf_overlay.colors.bg_color = kUiPerfBg;
        perf_overlay.colors.border_color = kUiPerfBorder;
        perf_overlay.colors.font_color = kUiPerfFont;
        perf_overlay.metrics.padding = 6;
        set_base(WidgetKind::PerfOverlay, perf_overlay);

        Style foldable_panel = theme.get<FoldablePanel>();
        foldable_panel.metrics.header_padding = 10;
        foldable_panel.metrics.content_padding = 10;
        set_base(WidgetKind::FoldablePanel, foldable_panel);

        Style cloudy_glass = theme.get<CloudyGlass>();
        cloudy_glass.metrics.glass_highlight_pos = 12;
        cloudy_glass.metrics.glass_highlight_alpha = 90;
        cloudy_glass.metrics.glass_shadow_alpha = 50;
        cloudy_glass.metrics.glass_opacity_min = 60;
        cloudy_glass.metrics.glass_opacity_max = 200;
        set_base(WidgetKind::CloudyGlass, cloudy_glass);

        Style spectrum_view = theme.get<SpectrumView>();
        spectrum_view.colors.bg_color = kUiListBg;
        spectrum_view.colors.border_color = kUiListBorder;
        spectrum_view.colors.bg_pressed = kUiSwitchOn;
        spectrum_view.colors.border_focus = kUiOk;
        set_base(WidgetKind::SpectrumView, spectrum_view);

        Style battery_gasgauge = theme.get<BatteryGasGauge>();
        battery_gasgauge.colors.bg_color = kUiListBg;
        battery_gasgauge.colors.border_color = kUiListBorder;
        battery_gasgauge.metrics.corner_radius = 12;
        battery_gasgauge.metrics.padding = 6;
        set_base(WidgetKind::BatteryGasGauge, battery_gasgauge);

        Style histogram = theme.get<Histogram>();
        histogram.colors.bg_color = kUiListBg;
        histogram.colors.border_color = kUiListBorder;
        histogram.metrics.corner_radius = 12;
        histogram.metrics.padding = 6;
        histogram.colors.font_color = kUiListFont;
        set_base(WidgetKind::Histogram, histogram);

        Style busy_wheel = theme.get<BusyWheel>();
        busy_wheel.colors.font_color = kUiListFont;
        set_base(WidgetKind::BusyWheel, busy_wheel);

        Style console_box = theme.get<ConsoleBox>();
        console_box.colors.bg_color = kUiListBg;
        console_box.colors.border_color = kUiListBorder;
        console_box.metrics.padding = 6;
        set_base(WidgetKind::ConsoleBox, console_box);

        sheet.clear();
        StylePatch btn_base{};
        btn_base.has_bg_color = true;
        btn_base.bg_color = kUiButtonBg;
        btn_base.has_border_color = true;
        btn_base.border_color = kUiButtonBorder;
        sheet.add_rule({WidgetKind::Button, 0}, btn_base);

        StylePatch btn_hover{};
        btn_hover.has_bg_color = true;
        btn_hover.bg_color = kUiButtonHover;
        sheet.add_rule({WidgetKind::Button, static_cast<std::uint8_t>(StyleStateFlag::Hovered)}, btn_hover);

        StylePatch btn_pressed{};
        btn_pressed.has_bg_color = true;
        btn_pressed.bg_color = kUiSwitchOn;
        btn_pressed.has_border_color = true;
        btn_pressed.border_color = kUiSwitchOn;
        sheet.add_rule({WidgetKind::Button, static_cast<std::uint8_t>(StyleStateFlag::Pressed)}, btn_pressed);

        StylePatch chart_patch{};
        chart_patch.has_bg_color = true;
        chart_patch.bg_color = kUiListBg;
        chart_patch.has_border_color = true;
        chart_patch.border_color = kUiListBorder;
        chart_patch.has_font_color = true;
        chart_patch.font_color = kUiListFont;
        theme.patch<Chart>(chart_patch);

        StylePatch hist_patch = chart_patch;
        theme.patch<HistogramView>(hist_patch);
        theme.patch<Histogram>(hist_patch);

        StylePatch switch_patch{};
        switch_patch.has_bg_color = true;
        switch_patch.bg_color = kUiButtonBg;
        switch_patch.has_border_color = true;
        switch_patch.border_color = kUiButtonBorder;
        switch_patch.has_bg_pressed = true;
        switch_patch.bg_pressed = kUiSwitchOn;
        switch_patch.has_border_pressed = true;
        switch_patch.border_pressed = kUiSwitchOn;
        theme.patch<Switch>(switch_patch);

        StylePatch slider_patch{};
        slider_patch.has_bg_color = true;
        slider_patch.bg_color = kUiButtonBg;
        slider_patch.has_border_color = true;
        slider_patch.border_color = kUiButtonBorder;
        theme.patch<Slider>(slider_patch);

        StylePatch dropdown_patch{};
        dropdown_patch.has_bg_color = true;
        dropdown_patch.bg_color = kUiButtonBg;
        dropdown_patch.has_border_color = true;
        dropdown_patch.border_color = kUiButtonBorder;
        dropdown_patch.has_font_color = true;
        dropdown_patch.font_color = kUiListFont;
        theme.patch<Dropdown>(dropdown_patch);

        StylePatch image_patch{};
        image_patch.has_corner_radius = true;
        image_patch.corner_radius = 24;
        theme.patch<Image>(image_patch);

        StylePatch image_box_patch = chart_patch;
        theme.patch<ImageBox>(image_box_patch);

        StylePatch meter_patch = chart_patch;
        meter_patch.has_border_focus = true;
        meter_patch.border_focus = kUiOk;
        theme.patch<MeterPointer>(meter_patch);

        StylePatch drill_patch = chart_patch;
        drill_patch.has_bg_pressed = true;
        drill_patch.bg_pressed = kUiSwitchOn;
        theme.patch<ProgressBarDrill>(drill_patch);

        StylePatch wheel_patch{};
        wheel_patch.has_font_color = true;
        wheel_patch.font_color = kUiListFont;
        theme.patch<SpinningWheel>(wheel_patch);
        theme.patch<BusyWheel>(wheel_patch);
        StylePatch nebula_patch{};
        nebula_patch.has_font_color = true;
        nebula_patch.font_color = kUiListFont;
        theme.patch<DynamicNebula>(nebula_patch);

        StylePatch crt_patch = chart_patch;
        theme.patch<CrtScreen>(crt_patch);

        StylePatch spectrum_patch = chart_patch;
        spectrum_patch.has_bg_pressed = true;
        spectrum_patch.bg_pressed = kUiSwitchOn;
        spectrum_patch.has_border_focus = true;
        spectrum_patch.border_focus = kUiOk;
        theme.patch<SpectrumView>(spectrum_patch);

        sheet.rebuild_if_needed();
    }
}
