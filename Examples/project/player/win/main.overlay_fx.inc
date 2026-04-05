    static std::array<float, 24> g_spectrum{};

    void update_spectrum(float t_sec, bool active) {
        const float base_speed = 2.2f;
        for (std::size_t i = 0; i < g_spectrum.size(); ++i) {
            const float phase = t_sec * base_speed + static_cast<float>(i) * 0.35f;
            const float wave = 0.5f + 0.5f * std::sin(phase);
            const float target = active ? (0.12f + wave * 0.88f) : 0.05f;
            g_spectrum[i] = g_spectrum[i] * 0.82f + target * 0.18f;
        }
    }

    void draw_library_fx(::ui::scene::SceneOverlay& out,
                         const PlayerUiContext& ctx,
                         const ::ui::scene::Scene& scene) {
        (void)out;
        if (ctx.current_page != player::PlayerPage::Library) {
            return;
        }
        if (!ctx.handles.page_library || scene.world_rect(ctx.handles.page_library).w <= 0) {
            return;
        }
        (void)scene;
    }

    void draw_now_playing_fx(::ui::scene::SceneOverlay& out,
                             const PlayerUiContext& ctx,
                             const ::ui::scene::Scene& scene,
                             float t_sec) {
        if (ctx.current_page != player::PlayerPage::NowPlaying) {
            return;
        }
        if (!ctx.handles.page_now_playing || scene.world_rect(ctx.handles.page_now_playing).w <= 0) {
            return;
        }
        const Rect cover = scene.world_rect(ctx.handles.cover);
        const int cover_radius = 20;
        const auto cover_image = ctx.cover_image.image_id;
        if (!ui::gfx::image_id_valid(cover_image)) {
            out.fill_round_rect(cover, cover_radius, kUiCover);
        }
        rgba cover_ring = kUiOk;
        if (ctx.is_playing()) {
            const float pulse = 0.4f + 0.6f * std::sin(t_sec * 2.0f);
            cover_ring.a = static_cast<std::uint8_t>(80 + pulse * 120.0f);
        } else {
            cover_ring.a = 70;
        }
        out.stroke_round_rect(cover, cover_radius, cover_ring);
        (void)scene;

        const Rect spec = scene.world_rect(ctx.handles.spectrum);
        if (spec.w > 0 && spec.h > 0) {
            out.fill_round_rect(spec, 10, kUiListBg);
            out.stroke_round_rect(spec, 10, kUiListBorder);
            const int bar_count = static_cast<int>(g_spectrum.size());
            const int gap = 2;
            const int bar_w = std::max(2, (spec.w - gap * (bar_count - 1)) / bar_count);
            int x = spec.x;
            for (int i = 0; i < bar_count; ++i) {
                const float v = g_spectrum[static_cast<std::size_t>(i)];
                const int h = static_cast<int>(v * static_cast<float>(spec.h - 8));
                const int y = spec.y + spec.h - 4 - h;
                const Rect bar{ x, y, bar_w, h };
                out.fill_round_rect(bar, 3, kUiSwitchOn);
                x += bar_w + gap;
            }
        }

        const Rect progress = scene.world_rect(ctx.handles.progress);
        if (progress.w > 0 && progress.h > 0) {
            const int wave_y = progress.y + progress.h / 2;
            const int dot_r = 2;
            const int dot_gap = 6;
            const int dot_count = std::max(8, progress.w / (dot_r * 2 + dot_gap));
            const int phase = static_cast<int>(t_sec * 8.0f) % (dot_r * 2 + dot_gap);
            int x = progress.x + 8 + phase;
            for (int i = 0; i < dot_count; ++i) {
                const int y = wave_y + ((i % 4 == 0) ? -1 : 0);
                out.fill_circle(Rect{x - dot_r, y - dot_r, dot_r * 2, dot_r * 2}, kUiTimeSoft);
                x += dot_r * 2 + dot_gap;
                if (x > progress.x + progress.w - 6) break;
            }
        }
    }
