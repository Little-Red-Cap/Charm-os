    static void build_probe(::ui::scene::SceneBuilder& builder, UiHandles& h) {
        const WidgetHandle probe = builder.create_container();
        anchor_rect(builder, probe, {0, 0, screen_width, screen_height});
        h.page_probe = probe;

        const WidgetHandle probe_red = builder.create_container();
        anchor_rect(builder, probe_red, {36, 72, 180, 120});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {255, 32, 32, 255};
            patch.has_corner_radius = true;
            patch.corner_radius = 20;
            patch.has_border_color = true;
            patch.border_color = {255, 255, 255, 24};
            builder.set_style_patch(probe_red, patch);
        }

        const WidgetHandle probe_green = builder.create_container();
        anchor_rect(builder, probe_green, {264, 72, 180, 120});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {32, 255, 32, 255};
            patch.has_corner_radius = true;
            patch.corner_radius = 20;
            patch.has_border_color = true;
            patch.border_color = {255, 255, 255, 24};
            builder.set_style_patch(probe_green, patch);
        }

        h.probe_panel = builder.create_container();
        anchor_rect(builder, h.probe_panel, {36, 230, screen_width - 72, 280});
        {
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {255, 32, 32, 255};
            patch.has_gradient_end = true;
            patch.gradient_end = {32, 255, 32, 255};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 28;
            patch.has_border_color = true;
            patch.border_color = {255, 255, 255, 24};
            builder.set_style_patch(h.probe_panel, patch);
        }

        const WidgetHandle probe_band = builder.create_container();
        anchor_rect(builder, probe_band, {36, 548, screen_width - 72, 160});
        {
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {32, 96, 255, 255};
            patch.has_gradient_end = true;
            patch.gradient_end = {255, 224, 64, 255};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 1;
            patch.has_corner_radius = true;
            patch.corner_radius = 20;
            patch.has_border_color = true;
            patch.border_color = {255, 255, 255, 24};
            builder.set_style_patch(probe_band, patch);
        }

        h.probe_title = builder.create_label_static("Probe Page");
        apply_player_text_style(builder, h.probe_title, PlayerTextRole::PageTitle, kUiTitle);
        builder.set_label_align(h.probe_title, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_title, {48, 748, screen_width - 96, 44});

        h.probe_hint = builder.create_label_static("点击任意位置或按任意键进入 Player");
        apply_player_text_style(builder, h.probe_hint, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(h.probe_hint, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_hint, {48, 804, screen_width - 96, 28});

        builder.link(probe, probe_red);
        builder.link(probe, probe_green);
        builder.link(probe, h.probe_panel);
        builder.link(probe, probe_band);
        builder.link(probe, h.probe_title);
        builder.link(probe, h.probe_hint);
    }
