    static void build_probe(::ui::scene::SceneBuilder& builder, UiHandles& h) {
        const WidgetHandle probe = builder.create_container();
        anchor_rect(builder, probe, {0, 0, screen_width, screen_height});
        h.page_probe = probe;

        h.probe_backdrop = builder.create_container();
        anchor_rect(builder, h.probe_backdrop, {0, 0, screen_width, screen_height});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiBackground;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            builder.set_style_override(h.probe_backdrop, patch);
        }

        h.probe_panel = builder.create_container();
        anchor_rect(builder, h.probe_panel, {28, 104, screen_width - 56, 560});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {20, 24, 32, 255};
            patch.has_border_color = true;
            patch.border_color = {255, 255, 255, 18};
            patch.has_border_width = true;
            patch.border_width = 1;
            patch.has_corner_radius = true;
            patch.corner_radius = 28;
            builder.set_style_override(h.probe_panel, patch);
        }

        h.probe_title = builder.create_label_static("Font Probe");
        apply_player_text_style(builder, h.probe_title, PlayerTextRole::PageTitle, kUiTitle);
        builder.set_label_align(h.probe_title, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_title, {48, 32, screen_width - 96, 40});

        h.probe_hint = builder.create_label_static("使用截图模式打印理论宽度与像素包围盒，点击或按键进入 Player");
        apply_player_text_style(builder, h.probe_hint, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(h.probe_hint, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_hint, {40, 72, screen_width - 80, 24});

        h.probe_hero_top = builder.create_label_static("Your");
        apply_player_text_style(builder, h.probe_hero_top, PlayerTextRole::HeroTitle, kUiTitle);
        builder.set_label_align(h.probe_hero_top, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_hero_top, {48, 146, 360, 102});

        h.probe_hero_bottom = builder.create_label_static("Mix");
        apply_player_text_style(builder, h.probe_hero_bottom, PlayerTextRole::HeroTitle, kUiTitle);
        builder.set_label_align(h.probe_hero_bottom, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_hero_bottom, {48, 232, 360, 102});

        h.probe_hero_subtitle = builder.create_label_static("Today's Mix for you");
        apply_player_text_style(builder, h.probe_hero_subtitle, PlayerTextRole::HeroSubtitle, kUiSubtitle);
        builder.set_label_align(h.probe_hero_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_hero_subtitle, {48, 340, 360, 30});

        h.probe_ref_title = builder.create_label_static("Reference title");
        apply_player_text_style(builder, h.probe_ref_title, PlayerTextRole::PageTitle, kUiTitle);
        builder.set_label_align(h.probe_ref_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_ref_title, {48, 430, 320, 38});

        h.probe_ref_meta = builder.create_label_static("Meta / body baseline sample");
        apply_player_text_style(builder, h.probe_ref_meta, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(h.probe_ref_meta, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.probe_ref_meta, {48, 478, 320, 24});

        builder.link(probe, h.probe_backdrop);
        builder.link(probe, h.probe_panel);
        builder.link(probe, h.probe_title);
        builder.link(probe, h.probe_hint);
        builder.link(probe, h.probe_hero_top);
        builder.link(probe, h.probe_hero_bottom);
        builder.link(probe, h.probe_hero_subtitle);
        builder.link(probe, h.probe_ref_title);
        builder.link(probe, h.probe_ref_meta);
    }
