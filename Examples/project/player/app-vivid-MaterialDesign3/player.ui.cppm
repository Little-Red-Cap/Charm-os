module;
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <cstdio>

export module player.ui;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.gfx.color;
import charm.gfx.image;
import charm.font;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_ascii_12;
import charm.font.font_noto_sc_16;
import charm.ui.scene.pill_surface;
import charm.ui.vivid.font_package;
import charm.font.provider_freetype;
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
        LibraryListCard = 11,
    };

    inline constexpr int kUiPadding = 18;
    inline constexpr int kCoverSize = 360;
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
    inline constexpr rgba kUiTimeSoft = {140, 148, 170, 200};
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
    inline constexpr rgba kUiInfoTagBgSoft = {28, 32, 46, 180};
    inline constexpr rgba kUiInfoTagBorderSoft = {64, 74, 102, 120};
    inline constexpr rgba kUiControlSideBg = {36, 40, 58, 210};
    inline constexpr rgba kUiControlSideBorder = {64, 74, 102, 140};
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
    inline constexpr rgba kUiLibraryHeroTopStart = {102, 136, 198, 108};
    inline constexpr rgba kUiLibraryHeroTopEnd = {24, 40, 80, 18};
    inline constexpr rgba kUiLibraryHeroBottomStart = {12, 22, 48, 92};
    inline constexpr rgba kUiLibraryHeroBottomEnd = {4, 8, 22, 252};
    inline constexpr rgba kUiLibraryControlsTop = {42, 58, 98, 148};
    inline constexpr rgba kUiLibraryControlsBottom = {18, 28, 56, 208};
    inline constexpr rgba kUiLibraryControlsBorder = {118, 142, 198, 102};
    inline constexpr rgba kUiLibraryCardTop = {28, 40, 70, 150};
    inline constexpr rgba kUiLibraryCardBottom = {4, 8, 24, 248};
    inline constexpr rgba kUiLibraryCardBorder = {72, 94, 146, 112};
    inline constexpr rgba kUiLibraryHeaderPlate = {42, 58, 92, 66};
    inline constexpr rgba kUiLibraryHeaderBorder = {100, 124, 178, 54};
    inline constexpr rgba kUiLibraryBodyPlate = {8, 14, 34, 94};
    inline constexpr rgba kUiLibraryBodyBorder = {58, 76, 124, 44};
    inline constexpr rgba kUiHomeDailyMixTop = {94, 130, 220, 255};
    inline constexpr rgba kUiHomeDailyMixBottom = {172, 102, 202, 255};
    inline constexpr rgba kUiLibraryChipIdle = {24, 32, 50, 224};
    inline constexpr rgba kUiLibraryChipBorder = {82, 96, 134, 144};
    inline constexpr rgba kUiLibraryChipActive = {104, 92, 156, 236};
    inline constexpr rgba kUiLibraryChipTextMuted = {194, 202, 226, 220};
    inline constexpr rgba kUiLibraryChipText = {226, 232, 246, 244};
    inline constexpr rgba kUiLibraryChipTextActive = {248, 250, 255, 255};
    inline constexpr rgba kUiLibraryTabActive = {190, 206, 255, 248};
    inline constexpr rgba kUiLibraryTabBorderActive = {226, 234, 255, 255};
    inline constexpr rgba kUiLibraryTabTextActive = {20, 42, 92, 255};
    inline constexpr rgba kUiLibraryListAccent = {120, 150, 214, 212};
    inline constexpr rgba kUiLibraryListOnAccent = {248, 250, 255, 255};
    inline constexpr rgba kUiLibraryPathIdle = {24, 34, 52, 214};
    inline constexpr rgba kUiLibraryPathActive = {44, 62, 94, 236};
    inline constexpr rgba kUiLibraryPathBorderActive = {132, 162, 226, 246};
    inline constexpr rgba kUiLibraryPathText = {202, 214, 238, 226};
    inline constexpr rgba kUiLibraryPathTextActive = {232, 240, 255, 255};

    struct PlayerIconIds {
        ::ui::gfx::ImageId prev{};
        ::ui::gfx::ImageId play{};
        ::ui::gfx::ImageId pause{};
        ::ui::gfx::ImageId next{};
        ::ui::gfx::ImageId loop{};
        ::ui::gfx::ImageId single{};
        ::ui::gfx::ImageId shuffle{};
        ::ui::gfx::ImageId folder{};
        ::ui::gfx::ImageId home{};
        ::ui::gfx::ImageId search{};
        ::ui::gfx::ImageId settings{};
        ::ui::gfx::ImageId down{};
        ::ui::gfx::ImageId more{};
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
        constexpr std::string_view kPathHome =
            "M240,760L360,760L360,560Q360,543 371.5,531.5Q383,520 400,520L560,520Q577,520 588.5,531.5Q600,543 600,560L600,760L720,760L720,400L480,220L240,400L240,760Z"
            "M160,760L160,400Q160,381 168.5,364Q177,347 192,336L432,156Q453,140 480,140Q507,140 528,156L768,336Q783,347 791.5,364Q800,381 800,400L800,760Q800,793 776.5,816.5Q753,840 720,840L560,840Q543,840 531.5,828.5Q520,817 520,800L520,600L440,600L440,800Q440,817 428.5,828.5Q417,840 400,840L240,840Q207,840 183.5,816.5Q160,793 160,760Z";
        constexpr std::string_view kPathSearch =
            "M380,640Q271,640 195.5,564.5Q120,489 120,380Q120,271 195.5,195.5Q271,120 380,120Q489,120 564.5,195.5Q640,271 640,380Q640,424 626,463Q612,502 588,532L812,756Q823,767 823,784Q823,801 812,812Q801,823 784,823Q767,823 756,812L532,588Q502,612 463,626Q424,640 380,640Z"
            "M380,560Q455,560 507.5,507.5Q560,455 560,380Q560,305 507.5,252.5Q455,200 380,200Q305,200 252.5,252.5Q200,305 200,380Q200,455 252.5,507.5Q305,560 380,560Z";
        constexpr std::string_view kPathSettings =
            "M433,880Q406,880 386.5,862Q367,844 363,818L354,752Q341,747 329.5,740Q318,733 307,725L245,751Q220,762 195,753Q170,744 156,721L109,639Q95,616 101,590Q107,564 128,547L181,507Q180,500 180,493.5Q180,487 180,480Q180,473 180,466.5Q180,460 181,453L128,413Q107,396 101,370Q95,344 109,321L156,239Q170,216 195,207Q220,198 245,209L307,235Q318,227 330,220Q342,213 354,208L363,142Q367,116 386.5,98Q406,80 433,80L527,80Q554,80 573.5,98Q593,116 597,142L606,208Q619,213 630.5,220Q642,227 653,235L715,209Q740,198 765,207Q790,216 804,239L851,321Q865,344 859,370Q853,396 832,413L779,453Q780,460 780,466.5Q780,473 780,480Q780,487 780,493.5Q780,500 778,507L831,547Q852,564 858,590Q864,616 850,639L802,721Q788,744 763,753Q738,762 713,751L653,725Q642,733 630,740Q618,747 606,752L597,818Q593,844 573.5,862Q554,880 527,880L433,880Z"
            "M440,800L519,800L533,694Q564,686 590.5,670.5Q617,655 639,633L738,674L777,606L691,541Q696,527 698,511.5Q700,496 700,480Q700,464 698,448.5Q696,433 691,419L777,354L738,286L639,328Q617,305 590.5,289.5Q564,274 533,266L520,160L441,160L427,266Q396,274 369.5,289.5Q343,305 321,327L222,286L183,354L269,418Q264,433 262,448Q260,463 260,480Q260,496 262,511Q264,526 269,541L183,606L222,674L321,632Q343,655 369.5,670.5Q396,686 427,694L440,800Z"
            "M482,620Q540,620 581,579Q622,538 622,480Q622,422 581,381Q540,340 482,340Q423,340 382.5,381Q342,422 342,480Q342,538 382.5,579Q423,620 482,620Z";
        constexpr std::string_view kPathDown =
            "M480,608L284,412L340,356L480,496L620,356L676,412L480,608Z";
        constexpr std::string_view kPathMore =
            "M480,800Q447,800 423.5,776.5Q400,753 400,720Q400,687 423.5,663.5Q447,640 480,640Q513,640 536.5,663.5Q560,687 560,720Q560,753 536.5,776.5Q513,800 480,800Z"
            "M480,560Q447,560 423.5,536.5Q400,513 400,480Q400,447 423.5,423.5Q447,400 480,400Q513,400 536.5,423.5Q560,447 560,480Q560,513 536.5,536.5Q513,560 480,560Z"
            "M480,320Q447,320 423.5,296.5Q400,273 400,240Q400,207 423.5,183.5Q447,160 480,160Q513,160 536.5,183.5Q560,207 560,240Q560,273 536.5,296.5Q513,320 480,320Z";

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

        void build_home_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathHome, color);
        }

        void build_search_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathSearch, color);
        }

        void build_settings_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathSettings, color);
        }

        void build_down_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathDown, color);
        }

        void build_more_icon(IconBuffer& buf, const rgba& color) {
            icon_clear(buf);
            rasterize_svg_path(buf, kPathMore, color);
        }

        struct FontPackageState {
            charm::font::VfsFontPackage package{};
            bool bound{false};
        };

        FontPackageState& font_package_state() {
            static FontPackageState state{};
            return state;
        }

        struct ExactFontSlot {
            Font font{};
            std::string path{};
            int px{0};
            FontWeight weight{FontWeight::Regular};
            bool loaded{false};
        };

        struct FreetypeLoaderState {
            charm::font::FreetypeFontLoader loader{};
            std::string ttf_path{};
            std::string ttf_fallback_path{};
            std::string ttf_small{};
            std::string ttf_normal{};
            std::string ttf_large{};
            std::string ttf_mono{};
            std::string ttf_fallback{};
            std::array<ExactFontSlot, 8> exact_fonts{};
            bool ready{false};
        };

        FreetypeLoaderState& freetype_state() {
            static FreetypeLoaderState state{};
            return state;
        }

        bool& system_font_fallback_enabled_state() noexcept {
            static bool enabled = true;
            return enabled;
        }

        void reset_exact_font_cache(FreetypeLoaderState& state) noexcept {
            const auto api = state.loader.vfs_api();
            for (auto& slot : state.exact_fonts) {
                if (api.reset) {
                    api.reset(&state.loader, slot.font);
                } else {
                    slot.font = Font{};
                }
                slot.path.clear();
                slot.px = 0;
                slot.weight = FontWeight::Regular;
                slot.loaded = false;
            }
        }

        const Font& fallback_font_for_px(int px, FontWeight weight) noexcept {
            if (px >= 48) return get_font_weighted(FontId::Large, weight);
            if (px <= 14) return get_font_weighted(FontId::Small, weight);
            return get_font_weighted(FontId::Normal, weight);
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

    inline ImageView icon_home() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_home_icon(buf, kUiListFont);
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

    inline ImageView icon_search() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_search_icon(buf, kUiListFont);
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

    inline ImageView icon_settings() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_settings_icon(buf, kUiListFont);
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

    inline ImageView icon_down() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_down_icon(buf, kUiListFont);
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

    inline ImageView icon_more() noexcept {
        static detail::IconBuffer buf{};
        static bool init = false;
        if (!init) {
            detail::build_more_icon(buf, kUiListFont);
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
        out.home = reg(icon_home());
        out.search = reg(icon_search());
        out.settings = reg(icon_settings());
        out.more = reg(icon_more());
        return out;
    }

    inline const Font& player_default_font() noexcept {
        return get_font(FontId::Normal);
    }

    void set_player_system_font_fallback_enabled(bool enabled) noexcept {
        detail::system_font_fallback_enabled_state() = enabled;
    }

    bool player_system_font_fallback_enabled() noexcept {
        return detail::system_font_fallback_enabled_state();
    }

    bool font_package_bound() noexcept {
        return detail::font_package_state().bound;
    }

    void reset_player_font_package_cache() noexcept {
        auto& state = detail::font_package_state();
        state.package.reset_cache();
        state.bound = false;
        detail::reset_exact_font_cache(detail::freetype_state());
    }

    namespace detail {
        std::string make_exact_font_path(std::string_view base_path,
                                         int px,
                                         FontWeight weight,
                                         std::string_view variation_tokens = {}) {
            std::string out{};
            out.assign(base_path.begin(), base_path.end());
            out += "#px";
            out += std::to_string(px);
            switch (weight) {
            case FontWeight::Bold:
                out += "_bold";
                break;
            case FontWeight::Medium:
                out += "_medium";
                break;
            case FontWeight::Regular:
            default:
                break;
            }
            if (!variation_tokens.empty()) {
                out += "_";
                out.append(variation_tokens.begin(), variation_tokens.end());
            }
            return out;
        }

        const Font& load_player_exact_font(std::string_view resolved_path,
                                           int px,
                                           FontWeight weight) noexcept {
            auto& state = freetype_state();
            for (auto& slot : state.exact_fonts) {
                if (slot.loaded && slot.path == resolved_path) {
                    return slot.font;
                }
            }
            auto* free_slot = &state.exact_fonts[0];
            for (auto& slot : state.exact_fonts) {
                if (!slot.loaded) {
                    free_slot = &slot;
                    break;
                }
            }
            free_slot->path.assign(resolved_path.begin(), resolved_path.end());
            free_slot->px = px;
            free_slot->weight = weight;
            free_slot->loaded = false;
            free_slot->font = Font{};
            const auto api = state.loader.vfs_api();
            if (api.load && api.load(&state.loader, free_slot->path, free_slot->font)) {
                free_slot->loaded = true;
                return free_slot->font;
            }
            if (api.reset) {
                api.reset(&state.loader, free_slot->font);
            }
            free_slot->path.clear();
            free_slot->px = 0;
            free_slot->weight = FontWeight::Regular;
            const auto& fallback = fallback_font_for_px(px, weight);
            return fallback;
        }
    }

    void bind_font_package(const charm::font::FontPackageConfig& config,
                           const charm::font::VfsFontLoaderApi& loader,
                           void* ctx) noexcept {
        auto& state = detail::font_package_state();
        state.package.set_config(config);
        state.package.set_loader(loader, ctx);
        state.package.bind();
        state.bound = true;
    }

    void bind_player_freetype_font(std::string_view ttf_path,
                                   std::string_view fallback_ttf_path,
                                   int small_px,
                                   int normal_px,
                                   int large_px) noexcept {
        if (ttf_path.empty()) return;
        auto& state = detail::freetype_state();
        detail::reset_exact_font_cache(state);
        state.ttf_path.assign(ttf_path.begin(), ttf_path.end());
        state.ttf_fallback_path.assign(fallback_ttf_path.begin(), fallback_ttf_path.end());
        state.ttf_small = state.ttf_path + "#small";
        state.ttf_normal = state.ttf_path + "#normal";
        state.ttf_large = state.ttf_path + "#large";
        state.ttf_mono = state.ttf_path + "#mono";
        state.ttf_fallback.clear();
        if (!state.ttf_fallback_path.empty()) {
            state.ttf_fallback = state.ttf_fallback_path + "#normal";
        }
        const char* path_small = state.ttf_small.c_str();
        const char* path_normal = state.ttf_normal.c_str();
        const char* path_large = state.ttf_large.c_str();
        const char* path_mono = state.ttf_mono.c_str();
        const char* path_fallback = state.ttf_fallback.empty() ? nullptr : state.ttf_fallback.c_str();

        charm::font::FontPackageConfig pkg{};
        pkg.regular.small_path = path_small;
        pkg.regular.normal_path = path_normal;
        pkg.regular.large_path = path_large;
        pkg.regular.mono_path = path_mono;
        pkg.medium = pkg.regular;
        pkg.bold = pkg.regular;
        pkg.fallback_path = path_fallback;

        charm::font::FreetypeFontLoaderConfig loader_cfg{};
        loader_cfg.regular.small_path = path_small;
        loader_cfg.regular.normal_path = path_normal;
        loader_cfg.regular.large_path = path_large;
        loader_cfg.regular.mono_path = path_mono;
        loader_cfg.medium = loader_cfg.regular;
        loader_cfg.bold = loader_cfg.regular;
        loader_cfg.small_px = small_px;
        loader_cfg.normal_px = normal_px;
        loader_cfg.large_px = large_px;
        loader_cfg.mono_px = normal_px;

        state.loader.set_config(loader_cfg);
        state.loader.bind_glyph_loader();
        bind_font_package(pkg, state.loader.vfs_api(), &state.loader);
        state.ready = true;
    }

    const Font& get_player_font_px(int px, FontWeight weight) noexcept {
        auto& state = detail::freetype_state();
        if (!state.ready || state.ttf_path.empty()) {
            const auto& fallback = detail::fallback_font_for_px(px, weight);
            return fallback;
        }
        const auto path = detail::make_exact_font_path(state.ttf_path, px, weight);
        return detail::load_player_exact_font(path, px, weight);
    }

    const Font& get_player_font_px_variant(int px,
                                           FontWeight weight,
                                           std::string_view variation_tokens) noexcept {
        auto& state = detail::freetype_state();
        if (!state.ready || state.ttf_path.empty()) {
            return get_player_font_px(px, weight);
        }
        const auto path = detail::make_exact_font_path(state.ttf_path, px, weight, variation_tokens);
        return detail::load_player_exact_font(path, px, weight);
    }

    inline void apply_player_theme() {
        set_default_font(FontId::Small, &font_noto_ascii_12);
        set_default_font(FontId::Normal, &font_noto_ascii_16);
        set_default_font(FontId::Large, &font_noto_ascii_16);
        set_default_font(FontId::Mono, &font_noto_ascii_16);
        set_default_font_weight(FontId::Small, FontWeight::Regular, &font_noto_ascii_12);
        set_default_font_weight(FontId::Normal, FontWeight::Regular, &font_noto_ascii_16);
        set_default_font_weight(FontId::Large, FontWeight::Regular, &font_noto_ascii_16);
        set_default_font_weight(FontId::Mono, FontWeight::Regular, &font_noto_ascii_16);

        const bool system_fallback_ready =
            detail::system_font_fallback_enabled_state() && player::font_cache::init();
        if (!system_fallback_ready) {
            set_default_fallback_font(&font_noto_sc_16);
        }

        auto& theme = Theme::instance();
        theme.set_default_font(player_default_font());

        {
            StylePatch side{};
            side.has_corner_radius = true;
            side.corner_radius = 24;
            side.has_bg_color = true;
            side.bg_color = kUiControlSideBg;
            side.has_border_color = true;
            side.border_color = kUiControlSideBorder;
            side.has_shadow_enabled = true;
            side.shadow_enabled = true;
            side.has_shadow_color = true;
            side.shadow_color = kUiCardShadow;
            side.has_shadow_offset_x = true;
            side.shadow_offset_x = 0;
            side.has_shadow_offset_y = true;
            side.shadow_offset_y = 2;
            side.has_shadow_spread = true;
            side.shadow_spread = 0;
            side.has_shadow_radius = true;
            side.shadow_radius = 8;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ControlSide), side);

            StylePatch play{};
            play.has_corner_radius = true;
            play.corner_radius = 24;
            play.has_bg_color = true;
            play.bg_color = kUiPlayBg;
            play.has_border_color = true;
            play.border_color = kUiButtonBorder;
            play.has_shadow_enabled = true;
            play.shadow_enabled = true;
            play.has_shadow_color = true;
            play.shadow_color = kUiPlayShadow;
            play.has_shadow_offset_x = true;
            play.shadow_offset_x = 0;
            play.has_shadow_offset_y = true;
            play.shadow_offset_y = 8;
            play.has_shadow_spread = true;
            play.shadow_spread = 0;
            play.has_shadow_radius = true;
            play.shadow_radius = 22;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::ControlPlay), play);
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiButtonBg,
                .border_color = kUiButtonBorder,
                .corner_radius = 12,
            });
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::TopBarButton), patch);
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiTabBg,
                .border_color = kUiButtonBorder,
                .corner_radius = 0,
            });
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::TabBase), patch);
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .apply_bg_color = false,
                .apply_border_color = false,
                .apply_corner_radius = false,
                .shadow = {
                    .enabled = true,
                    .color = kUiCardShadow,
                    .offset_x = 0,
                    .offset_y = 3,
                    .spread = 3,
                    .radius = 16,
                },
            });
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
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = kUiLibraryCardTop;
            patch.has_gradient_end = true;
            patch.gradient_end = kUiLibraryCardBottom;
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            patch.has_border_color = true;
            patch.border_color = kUiLibraryCardBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 24;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 4;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 2;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            theme.set_style_class(static_cast<StyleClassId>(PlayerStyleClass::LibraryListCard), patch);
        }

        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiInfoTagBgSoft,
                .border_color = kUiInfoTagBorderSoft,
                .corner_radius = 10,
            });
            patch.has_font_color = true;
            patch.font_color = kUiTimeSoft;
            patch.has_font_role = true;
            patch.font_role = FontId::Small;
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
        preset.list_view.colors.accent_color = kUiLibraryListAccent;
        preset.list_view.colors.on_accent = kUiLibraryListOnAccent;
        preset.list_view.colors.border_focus = kUiLibraryPathBorderActive;
        preset.list_view.font_weight = FontWeight::Medium;
        preset.list_view.metrics.corner_radius = 18;
        preset.list_view.metrics.padding = 14;
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


