module;
#include <cstdint>

export module player.ui_builder;

import charm.core.config;
import charm.core.container;
import charm.core.event;
import charm.core.factory;
import charm.core.layout;
import charm.gfx.assets.render;
import charm.widgets.battery_gauge;
import charm.widgets.button;
import charm.widgets.chart;
import charm.widgets.cloudy_glass;
import charm.widgets.code_block;
import charm.widgets.dropdown;
import charm.widgets.foldable_panel;
import charm.widgets.histogram_view;
import charm.widgets.image;
import charm.widgets.label;
import charm.widgets.list_view;
import charm.widgets.menu_tree;
import charm.widgets.perf_overlay;
import charm.widgets.progress;
import charm.widgets.progress_flowing;
import charm.widgets.progress_wheel;
import charm.widgets.rich_text;
import charm.widgets.scrollbar;
import charm.widgets.segmented_control;
import charm.widgets.stepper;
import charm.widgets.table_view;
import charm.widgets.text;
import charm.widgets.timeline;
import charm.widgets.tree_view;
import charm.widgets.waveform_view;
import charm.widgets.switcher;
import charm.widgets.spinning_wheel;
import charm.widgets.image_box;
import charm.widgets.meter_pointer;
import charm.widgets.progress_bar_drill;
import player.controller;
import player.ui;
#if CHARM_PLAYER_DEBUG_UI
import player.ui_debug;
#endif

export namespace player {
    using namespace player::ui;
#if CHARM_PLAYER_DEBUG_UI
    using namespace player::ui_debug;
#endif

    struct UiCallbacks {
        ListView::DrawRowFn list_draw{nullptr};
        ListView::SelectFn list_select{nullptr};
        ListView::PoolCreateFn list_pool_create{nullptr};
        ListView::PoolBindFn list_pool_bind{nullptr};
        ListView::PoolRecycleFn list_pool_recycle{nullptr};
        void* list_ctx{nullptr};
        Callback list_scroll_change{};
        Callback play_mode_change{};
        Callback spectrum_toggle{};
        Callback low_load_toggle{};
        Callback eq_toggle{};
        Callback eq_slider_change{};
        Callback eq_preset_change{};
        Callback prev_click{};
        Callback next_click{};
        Callback pause_click{};
    };

    UiHandles build_ui(UiFactory& factory, PlayerController& ctx, const UiCallbacks& cb) {
        auto anchor_pos = [](auto* obj, int x, int y) {
            if (!obj) return;
            obj->set_pos(x, y);
            obj->set_anchor(x, y, -1, -1);
        };
        auto anchor_rect = [](auto* obj, const Rect& r) {
            if (!obj) return;
            obj->set_rect(r);
            obj->set_anchor(r.x, r.y, -1, -1);
        };
        UiHandles h{};
        h.root = factory.create_container();
        auto* root = factory.get_container(h.root);
        root->set_rect({0, 0, screen_width, screen_height});
        root->set_background(kUiBackground);

        const int cover_top = kUiPadding * 2;
        const int header_top = cover_top + kCoverSize;
        const int mode_y = header_top + kHeaderModeOffset;
        const int mode_hint_y = mode_y + kModeHeight + kModeHintGap;
        const int spectrum_y = mode_hint_y + kModeHintHeight + kSpectrumGap;
        const int options_y = spectrum_y + kSpectrumHeight + kOptionsGap;
        const int eq_y = options_y + kOptionsHeight + kOptionsGap;
        const int list_y = eq_y + kEqPanelHeight + kSpectrumGap;
        const int list_h = screen_height - list_y - kListBottomReserve;

        h.cover = factory.create_container();
        if (auto* cover = factory.get_container(h.cover)) {
            anchor_rect(cover, {(screen_width - kCoverSize) / 2, cover_top, kCoverSize, kCoverSize});
            cover->set_background(kUiCover);
        }

        h.title = factory.create_label("Beautiful Trick");
        if (auto* title = factory.get_label(h.title)) {
            title->set_color(kUiTitle);
            anchor_rect(title, {kUiPadding, header_top + kHeaderTitleOffset,
                                screen_width - kUiPadding * 2, 24});
            title->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.subtitle = factory.create_label("FELT 路 FLAC");
        if (auto* sub = factory.get_label(h.subtitle)) {
            sub->set_color(kUiSubtitle);
            anchor_rect(sub, {kUiPadding, header_top + kHeaderSubtitleOffset,
                              screen_width - kUiPadding * 2, 20});
            sub->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.progress = factory.create_progress();
        if (auto* bar = factory.get_progress(h.progress)) {
            anchor_rect(bar, {kUiPadding, header_top + kHeaderProgressOffset,
                              screen_width - kUiPadding * 2, 16});
            bar->set_range(0, 100);
            bar->set_value(0);
        }

        h.time = factory.create_label("0:00 / 3:00");
        if (auto* time = factory.get_label(h.time)) {
            time->set_color(kUiTime);
            anchor_rect(time, {kUiPadding, header_top + kHeaderTimeOffset,
                               screen_width - kUiPadding * 2, 18});
            time->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.status = factory.create_label("Stopped");
        if (auto* status = factory.get_label(h.status)) {
            status->set_color(kUiStatus);
            anchor_rect(status, {kUiPadding, header_top + kHeaderStatusOffset,
                                 screen_width - kUiPadding * 2, 18});
            status->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.play_mode = factory.create_segmented_control();
        if (auto* mode = factory.get_segmented_control(h.play_mode)) {
            static const char* kModeItems[] = {"Order", "Single", "Shuffle"};
            mode->set_items(kModeItems, 3);
            mode->set_selected(0);
            mode->set_on_change(cb.play_mode_change);
            anchor_rect(mode, {(screen_width - kModeWidth) / 2, mode_y, kModeWidth, kModeHeight});
        }

        h.mode_hint = factory.create_label("Mode: Order");
        if (auto* hint = factory.get_label(h.mode_hint)) {
            hint->set_color(kUiOption);
            anchor_rect(hint, {kUiPadding, mode_hint_y, screen_width - kUiPadding * 2, kModeHintHeight});
            hint->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.spectrum_hist = factory.create_histogram_view();
        if (auto* hist = factory.get_histogram_view(h.spectrum_hist)) {
            anchor_rect(hist, {kUiPadding, spectrum_y, screen_width - kUiPadding * 2, kSpectrumHeight});
            hist->set_range(0, 100);
        }

        h.spectrum_peak = factory.create_chart();
        if (auto* chart = factory.get_chart(h.spectrum_peak)) {
            anchor_rect(chart, {kUiPadding, spectrum_y, screen_width - kUiPadding * 2, kSpectrumHeight});
        }

        h.options_row = factory.create_container();
        if (auto* row = factory.get_container(h.options_row)) {
            anchor_rect(row, {kUiPadding, options_y, screen_width - kUiPadding * 2, kOptionsHeight});
            row->set_flow_layout(12, 0, 0);
            row->set_align(static_cast<int>(AlignH::Start), static_cast<int>(AlignV::Center));
        }

        h.opt_spectrum_label = factory.create_label("Spectrum");
        if (auto* label = factory.get_label(h.opt_spectrum_label)) {
            label->set_color(kUiOption);
            label->set_align(TextAlignH::Left, TextAlignV::Center);
            label->set_size(kOptionLabelWidth, kOptionsHeight);
        }

        h.opt_spectrum_switch = factory.create_switch();
        if (auto* sw = factory.get_switch(h.opt_spectrum_switch)) {
            sw->set_on(ctx.spectrum_enabled);
            sw->set_on_change(cb.spectrum_toggle);
            sw->set_size(44, 24);
        }

        h.opt_low_label = factory.create_label("Low load");
        if (auto* label = factory.get_label(h.opt_low_label)) {
            label->set_color(kUiOption);
            label->set_align(TextAlignH::Left, TextAlignV::Center);
            label->set_size(kOptionLabelWidth, kOptionsHeight);
        }

        h.opt_low_switch = factory.create_switch();
        if (auto* sw = factory.get_switch(h.opt_low_switch)) {
            sw->set_on(ctx.spectrum_low_load);
            sw->set_on_change(cb.low_load_toggle);
            sw->set_size(44, 24);
        }

        h.opt_eq_label = factory.create_label("EQ Off");
        if (auto* label = factory.get_label(h.opt_eq_label)) {
            label->set_color(kUiOption);
            label->set_align(TextAlignH::Left, TextAlignV::Center);
            label->set_size(70, kOptionsHeight);
        }

        h.opt_eq_switch = factory.create_switch();
        if (auto* sw = factory.get_switch(h.opt_eq_switch)) {
            sw->set_on(ctx.eq_enabled);
            sw->set_on_change(cb.eq_toggle);
            sw->set_size(44, 24);
        }

        h.eq_panel = factory.create_container();
        if (auto* panel = factory.get_container(h.eq_panel)) {
            anchor_rect(panel, {kUiPadding, eq_y, screen_width - kUiPadding * 2, kEqPanelHeight});
            panel->set_background(kUiListBg);
        }

        h.eq_title = factory.create_label("EQ");
        const int eq_inner_x = kUiPadding + 10;
        const int eq_inner_w = screen_width - kUiPadding * 2 - 20;
        const int eq_title_y = eq_y + 6;
        const int eq_preset_x = eq_inner_x + eq_inner_w -
            (kEqPresetLabelWidth + kEqRowGapX + kEqPresetWidth);
        if (auto* title = factory.get_label(h.eq_title)) {
            title->set_color(kUiEqTitle);
            anchor_rect(title, {eq_inner_x, eq_title_y, screen_width - kUiPadding * 2 - 20, kEqTitleHeight});
            title->set_align(TextAlignH::Left, TextAlignV::Center);
        }

        h.eq_preset_label = factory.create_label("Preset");
        if (auto* label = factory.get_label(h.eq_preset_label)) {
            label->set_color(kUiOption);
            anchor_rect(label, {eq_preset_x, eq_title_y, kEqPresetLabelWidth, kEqTitleHeight});
            label->set_align(TextAlignH::Left, TextAlignV::Center);
        }

        h.eq_preset = factory.create_dropdown();
        if (auto* drop = factory.get_dropdown(h.eq_preset)) {
            drop->set_size(kEqPresetWidth, kEqRowHeight);
            drop->add_option("Flat");
            drop->add_option("Bass");
            drop->add_option("Vocal");
            drop->add_option("Treble");
            drop->add_option("Custom");
            drop->set_selected(0);
            drop->set_on_change(cb.eq_preset_change);
            anchor_rect(drop, {eq_preset_x + kEqPresetLabelWidth + kEqRowGapX, eq_title_y,
                               kEqPresetWidth, kEqRowHeight});
        }

        static const char* kEqLabels[kEqBands] = {"60", "250", "1k", "4k", "12k"};
        const int slider_w = eq_inner_w - kEqLabelWidth - kEqValueWidth - kEqRowGapX * 2;
        for (int i = 0; i < kEqBands; ++i) {
            const int row_y = eq_y + kEqTitleHeight + 12 + i * (kEqRowHeight + kEqRowGap);
            h.eq_band_labels[i] = factory.create_label(kEqLabels[i]);
            if (auto* label = factory.get_label(h.eq_band_labels[i])) {
                label->set_color(kUiOption);
                anchor_rect(label, {eq_inner_x, row_y, kEqLabelWidth, kEqRowHeight});
                label->set_align(TextAlignH::Left, TextAlignV::Center);
            }

            h.eq_sliders[i] = factory.create_slider();
            if (auto* slider = factory.get_slider(h.eq_sliders[i])) {
                anchor_rect(slider, {eq_inner_x + kEqLabelWidth + kEqRowGapX, row_y, slider_w, kEqRowHeight});
                slider->set_range(-12, 12);
                slider->set_value(0);
                slider->set_on_change(cb.eq_slider_change);
            }

            h.eq_value_labels[i] = factory.create_label("0 dB");
            if (auto* value = factory.get_label(h.eq_value_labels[i])) {
                value->set_color(kUiOption);
                anchor_rect(value, {eq_inner_x + kEqLabelWidth + kEqRowGapX + slider_w + kEqRowGapX,
                                    row_y, kEqValueWidth, kEqRowHeight});
                value->set_align(TextAlignH::Right, TextAlignV::Center);
            }
        }

        h.perf_overlay = factory.create_perf_overlay();
        if (auto* perf = factory.get_perf_overlay(h.perf_overlay)) {
            anchor_rect(perf, {screen_width - (kPerfOverlayWidth + kUiPadding),
                               kUiPadding, kPerfOverlayWidth, kPerfOverlayHeight});
        }

#if CHARM_PLAYER_DEBUG_UI
        h.debug_grid = factory.create_container();
        if (auto* grid = factory.get_container(h.debug_grid)) {
            const int half_w = (screen_width - kUiPadding * 2 - kDemoGap) / 2;
            const int cell_h = (list_h - kDemoGap) / 2;
            anchor_rect(grid, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            grid->set_grid_layout(2, half_w, cell_h, kDemoGap, 0);
            grid->set_align(static_cast<int>(AlignH::Center), static_cast<int>(AlignV::Center));
            grid->set_visible(false);
        }
#endif

        h.list_title = factory.create_label("Tracks");
        if (auto* title = factory.get_label(h.list_title)) {
            title->set_color(kUiListTitle);
            anchor_rect(title, {kUiPadding, list_y - kListTitleGap, screen_width - kUiPadding * 2, 18});
            title->set_align(TextAlignH::Left, TextAlignV::Center);
        }

        h.list = factory.create_list_view();
        if (auto* list = factory.get_list_view(h.list)) {
            anchor_rect(list, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            list->set_on_draw(cb.list_draw, cb.list_ctx);
            list->set_on_select(cb.list_select, cb.list_ctx);
            list->set_item_pool(cb.list_pool_create, cb.list_pool_bind, cb.list_pool_recycle, cb.list_ctx);
            list->set_prefetch_rows(2);
            list->set_row_height(32);
            list->set_wheel_step(32);
        }

        h.list_scroll = factory.create_scroll_bar();
        if (auto* bar = factory.get_scroll_bar(h.list_scroll)) {
            anchor_rect(bar, {screen_width - kUiPadding - kListScrollWidth, list_y, kListScrollWidth, list_h});
            bar->set_orientation(ScrollBar::Orientation::Vertical);
            bar->set_on_change(cb.list_scroll_change);
        }

        h.list_hint = factory.create_label("No tracks in /music or /");
        if (auto* hint = factory.get_label(h.list_hint)) {
            hint->set_color(kUiHint);
            anchor_rect(hint, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            hint->set_align(TextAlignH::Center, TextAlignV::Center);
            hint->set_visible(false);
        }

#if CHARM_PLAYER_DEBUG_UI
        h.tree = factory.create_tree_view();
        if (auto* tree = factory.get_tree_view(h.tree)) {
            tree_rebuild_visible(g_tree_demo);
            tree->set_data_source(&tree_row_count, &tree_node_info, &on_tree_draw, &g_tree_demo);
            tree->set_on_toggle(&on_tree_toggle);
            tree->set_item_pool(&on_tree_pool_create, &on_tree_pool_bind, &on_tree_pool_recycle, &g_tree_demo);
            tree->set_prefetch_rows(2);
            tree->set_row_height(28);
        }

        h.table = factory.create_table_view();
        if (auto* table = factory.get_table_view(h.table)) {
            table_rebuild_order(g_table_demo);
            table->set_data_source(&table_row_count, &table_col_count, &on_table_draw, &g_table_demo);
            table->set_column_width_fn(&table_col_width);
            table->set_on_select(&on_table_select, &g_table_demo);
            table->set_row_height(28);
        }

        h.chart = factory.create_chart();
        if (auto* chart = factory.get_chart(h.chart)) {
            static int points[] = {3, 8, 5, 12, 6, 14, 7, 10, 4};
            chart->set_points(points, static_cast<int>(sizeof(points) / sizeof(points[0])));
        }

        h.debug_side = factory.create_container();
        if (auto* side = factory.get_container(h.debug_side)) {
            side->set_flex_layout(1, 0, 0, 8, 8);
            side->set_flex_grow(1);
        }

        ctx.menu_tree.init(factory, h.debug_side);
        ctx.menu_tree.set_rect({0, 0, 200, 120});
        ctx.menu_tree.set_item_height(22);
        ctx.menu_tree.set_indent(12);
        const int menu_file = ctx.menu_tree.add_item(-1, "File");
        ctx.menu_tree.add_item(menu_file, "New");
        ctx.menu_tree.add_item(menu_file, "Open");
        ctx.menu_tree.add_item(menu_file, "Save");
        const int menu_edit = ctx.menu_tree.add_item(-1, "Edit");
        ctx.menu_tree.add_item(menu_edit, "Undo");
        ctx.menu_tree.add_item(menu_edit, "Redo");
        ctx.menu_tree.add_item(-1, "View");
        ctx.menu_tree.set_expanded(menu_file, true);
        ctx.menu_tree.rebuild();

        h.logo = factory.create_image();
        if (auto* logo = factory.get_image(h.logo)) {
            logo->set_image(render_logo_argb());
            logo->set_scale_mode(Image::ScaleMode::Fit);
            logo->set_alignment(Image::AlignH::Center, Image::AlignV::Center);
            logo->set_rotation(Image::Rotation::Rotate90);
            logo->set_sampling(Image::Sampling::Bilinear);
            logo->set_crop_mode(Image::CropMode::Transparent);
            logo->set_edge_mode(Image::EdgeMode::AllowOutside);
            logo->set_anchor(0.5f, 0.5f);
            logo->set_crop({4, 4, 22, 22});
            logo->set_size(200, 90);
        }

        h.stepper = factory.create_stepper();
        if (auto* stepper = factory.get_stepper(h.stepper)) {
            stepper->set_steps(4);
            stepper->set_current(1);
            stepper->set_label(0, "Init");
            stepper->set_label(1, "Load");
            stepper->set_label(2, "Play");
            stepper->set_label(3, "Done");
            stepper->set_size(200, 48);
        }

        h.timeline = factory.create_timeline();
        if (auto* timeline = factory.get_timeline(h.timeline)) {
            timeline->set_item_count(4);
            timeline->set_item_text(0, "Boot");
            timeline->set_item_text(1, "Scan");
            timeline->set_item_text(2, "Decode");
            timeline->set_item_text(3, "Ready");
            timeline->set_current(2);
            timeline->set_row_height(26);
            timeline->set_size(200, 140);
            timeline->set_flex_grow(1);
        }

        h.rich_text = factory.create_rich_text();
        if (auto* rich = factory.get_rich_text(h.rich_text)) {
            rich->set_text("[b]Rich[/b] [color=#7ED321]text[/color] demo\n"
                           "[mono]mono[/mono] and [code]code[/code] sample");
            rich->set_size(200, 70);
        }

        h.code_block = factory.create_code_block();
        if (auto* block = factory.get_code_block(h.code_block)) {
            block->set_text("int main() {\n  return 0;\n}");
            block->set_wrap(TextWrap::None);
            block->set_size(200, 80);
        }

        h.progress_wheel = factory.create_progress_wheel();
        if (auto* wheel = factory.get_progress_wheel(h.progress_wheel)) {
            wheel->set_value(72);
            wheel->set_thickness(8);
            wheel->set_size(90, 90);
        }

        h.waveform = factory.create_waveform_view();
        if (auto* wave = factory.get_waveform_view(h.waveform)) {
            static int samples[] = {3, 5, 8, 6, 2, -2, -5, -3, 1, 4, 7, 4, 1, -3, -6, -4};
            wave->set_samples(samples, static_cast<int>(sizeof(samples) / sizeof(samples[0])));
            wave->set_size(200, 80);
        }

        h.battery_gauge = factory.create_battery_gauge();
        if (auto* battery = factory.get_battery_gauge(h.battery_gauge)) {
            battery->set_value(65);
            battery->set_size(200, 48);
        }

        h.histogram = factory.create_histogram_view();
        if (auto* hist = factory.get_histogram_view(h.histogram)) {
            static int bins[] = {2, 5, 8, 3, 6, 9, 4, 7, 5, 2, 6, 8};
            hist->set_values(bins, static_cast<int>(sizeof(bins) / sizeof(bins[0])));
            hist->set_size(200, 80);
        }

        h.fold_panel = factory.create_foldable_panel("Foldable Panel");
        if (auto* panel = factory.get_foldable_panel(h.fold_panel)) {
            panel->set_body("Tap header to expand or collapse.");
            panel->set_expanded(true);
            panel->set_size(200, 120);
        }
        h.progress_flow = factory.create_progress_flowing();
        if (auto* flow = factory.get_progress_flowing(h.progress_flow)) {
            flow->set_range(0, 100);
            flow->set_indeterminate(true);
            flow->set_flow_span(12);
            flow->set_flow_speed(2);
            flow->set_size(200, 16);
        }
        h.cloudy_glass = factory.create_cloudy_glass();
        if (auto* glass = factory.get_cloudy_glass(h.cloudy_glass)) {
            glass->set_size(200, 70);
            glass->set_opacity(140);
        }
        h.spinning_wheel = factory.create_spinning_wheel();
        if (auto* wheel = factory.get_spinning_wheel(h.spinning_wheel)) {
            wheel->set_size(90, 90);
            wheel->set_thickness(8);
            wheel->set_speed(5.0f);
        }
        h.image_box = factory.create_image_box();
        if (auto* box = factory.get_image_box(h.image_box)) {
            box->set_image(render_logo_argb());
            box->set_scale_mode(ImageBox::ScaleMode::Fit);
            box->set_alignment(ImageBox::AlignH::Center, ImageBox::AlignV::Center);
            box->set_size(200, 120);
        }
        h.meter_pointer = factory.create_meter_pointer();
        if (auto* meter = factory.get_meter_pointer(h.meter_pointer)) {
            meter->set_range(0, 100);
            meter->set_value(35);
            meter->set_size(160, 160);
        }
        h.progress_drill = factory.create_progress_bar_drill();
        if (auto* bar = factory.get_progress_bar_drill(h.progress_drill)) {
            bar->set_range(0, 100);
            bar->set_value(35);
            bar->set_size(200, 18);
        }
        auto fold_btn_primary = factory.create_button("Apply");
        if (auto* btn = factory.get_button(fold_btn_primary)) {
            btn->set_size(90, 32);
        }
        auto fold_btn_secondary = factory.create_button("Reset");
        if (auto* btn = factory.get_button(fold_btn_secondary)) {
            btn->set_size(90, 32);
        }
        factory.link(h.fold_panel, fold_btn_primary);
        factory.link(h.fold_panel, fold_btn_secondary);
#endif

        const int controls_w = kButtonWidth * 3 + kButtonGap * 2;
        const int controls_h = kButtonHeight;
        const int controls_x = (screen_width - controls_w) / 2;
        const int controls_y = screen_height - controls_h - kControlsBottomMargin;
        h.controls = factory.create_container();
        if (auto* controls = factory.get_container(h.controls)) {
            anchor_rect(controls, {controls_x, controls_y, controls_w, controls_h});
            controls->set_flow_layout(kButtonGap, kButtonGap, 0);
            controls->set_align(static_cast<int>(AlignH::Center), static_cast<int>(AlignV::Start));
        }

        h.btn_prev = factory.create_button("Prev");
        if (auto* prev = factory.get_button(h.btn_prev)) {
            prev->set_size(kButtonWidth, kButtonHeight);
            prev->set_on_click(cb.prev_click);
            prev->set_text("");
            prev->set_icon(icon_prev(), 20, 20);
        }

        h.btn_next = factory.create_button("Next");
        if (auto* next = factory.get_button(h.btn_next)) {
            next->set_size(kButtonWidth, kButtonHeight);
            next->set_on_click(cb.next_click);
            next->set_text("");
            next->set_icon(icon_next(), 20, 20);
        }

        h.btn_pause = factory.create_button("Pause");
        if (auto* pause = factory.get_button(h.btn_pause)) {
            pause->set_size(kButtonWidth, kButtonHeight);
            pause->set_on_click(cb.pause_click);
            pause->set_text("");
            pause->set_icon(icon_play(), 20, 20);
        }

        factory.link(h.root, h.cover);
        factory.link(h.root, h.title);
        factory.link(h.root, h.subtitle);
        factory.link(h.root, h.progress);
        factory.link(h.root, h.time);
        factory.link(h.root, h.status);
        factory.link(h.root, h.play_mode);
        factory.link(h.root, h.mode_hint);
        factory.link(h.root, h.spectrum_hist);
        factory.link(h.root, h.spectrum_peak);
        factory.link(h.root, h.options_row);
        factory.link(h.root, h.eq_panel);
        factory.link(h.eq_panel, h.eq_title);
        factory.link(h.root, h.list_title);
        factory.link(h.root, h.list);
        factory.link(h.root, h.list_scroll);
        factory.link(h.root, h.list_hint);
#if CHARM_PLAYER_DEBUG_UI
        factory.link(h.root, h.debug_grid);
        factory.link(h.debug_grid, h.tree);
        factory.link(h.debug_grid, h.table);
        factory.link(h.debug_grid, h.chart);
        factory.link(h.debug_grid, h.debug_side);
        factory.link(h.debug_side, h.logo);
        factory.link(h.debug_side, h.stepper);
        factory.link(h.debug_side, h.timeline);
        factory.link(h.debug_side, h.rich_text);
        factory.link(h.debug_side, h.code_block);
        factory.link(h.debug_side, h.progress_wheel);
        factory.link(h.debug_side, h.waveform);
        factory.link(h.debug_side, h.battery_gauge);
        factory.link(h.debug_side, h.histogram);
        factory.link(h.debug_side, h.fold_panel);
        factory.link(h.debug_side, h.progress_flow);
        factory.link(h.debug_side, h.cloudy_glass);
        factory.link(h.debug_side, h.spinning_wheel);
        factory.link(h.debug_side, h.image_box);
        factory.link(h.debug_side, h.meter_pointer);
        factory.link(h.debug_side, h.progress_drill);
#endif
        factory.link(h.root, h.controls);
        factory.link(h.controls, h.btn_prev);
        factory.link(h.controls, h.btn_pause);
        factory.link(h.controls, h.btn_next);
        factory.bring_to_front(h.root, h.perf_overlay);

        factory.link(h.options_row, h.opt_spectrum_label);
        factory.link(h.options_row, h.opt_spectrum_switch);
        factory.link(h.options_row, h.opt_low_label);
        factory.link(h.options_row, h.opt_low_switch);
        factory.link(h.options_row, h.opt_eq_label);
        factory.link(h.options_row, h.opt_eq_switch);

        factory.link(h.eq_panel, h.eq_preset_label);
        factory.link(h.eq_panel, h.eq_preset);
        for (int i = 0; i < kEqBands; ++i) {
            factory.link(h.eq_panel, h.eq_band_labels[i]);
            factory.link(h.eq_panel, h.eq_sliders[i]);
            factory.link(h.eq_panel, h.eq_value_labels[i]);
        }

        return h;
    }
}
