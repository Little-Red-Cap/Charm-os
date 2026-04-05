    struct PixelBounds {
        bool found{false};
        int min_x{0};
        int min_y{0};
        int max_x{0};
        int max_y{0};
        int count{0};
    };

    struct GlyphInkSpan {
        bool found{false};
        int first_row{0};
        int last_row{0};
        int rows{0};
    };

    int color_delta(const rgba& a, const rgba& b) noexcept {
        return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r))
             + std::abs(static_cast<int>(a.g) - static_cast<int>(b.g))
             + std::abs(static_cast<int>(a.b) - static_cast<int>(b.b));
    }

    rgba sample_label_bg(player::PlayerPlatform& platform, const Rect& rect) noexcept {
        const auto& fb = platform.framebuffer_ref();
        const int x = std::clamp(rect.x + rect.w - 4, 0, screen_width - 1);
        const int y = std::clamp(rect.y + rect.h / 2, 0, screen_height - 1);
        return fb.get_pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
    }

    PixelBounds scan_text_pixels(player::PlayerPlatform& platform,
                                 const Rect& rect,
                                 const rgba& bg) noexcept {
        PixelBounds out{};
        const auto& fb = platform.framebuffer_ref();
        const int x0 = std::max(0, rect.x);
        const int y0 = std::max(0, rect.y);
        const int x1 = std::min(screen_width, rect.x + rect.w);
        const int y1 = std::min(screen_height, rect.y + rect.h);
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const rgba px = fb.get_pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
                if (color_delta(px, bg) <= 18) continue;
                if (!out.found) {
                    out.found = true;
                    out.min_x = out.max_x = x;
                    out.min_y = out.max_y = y;
                } else {
                    out.min_x = std::min(out.min_x, x);
                    out.min_y = std::min(out.min_y, y);
                    out.max_x = std::max(out.max_x, x);
                    out.max_y = std::max(out.max_y, y);
                }
                ++out.count;
            }
        }
        return out;
    }

    GlyphInkSpan inspect_glyph_ink(const Glyph& glyph) noexcept {
        GlyphInkSpan out{};
        if (!glyph.bitmap || glyph.width <= 0 || glyph.height <= 0) return out;
        const std::uint8_t bpp = glyph.bpp ? glyph.bpp : 1;
        const int bytes_per_row = (glyph.width * bpp + 7) / 8;
        for (int row = 0; row < glyph.height; ++row) {
            bool row_has_ink = false;
            for (int col = 0; col < glyph.width; ++col) {
                std::uint8_t cov = 0;
                if (bpp == 1) {
                    const int byte_index = row * bytes_per_row + col / 8;
                    const int bit_offset = 7 - (col % 8);
                    cov = ((glyph.bitmap[byte_index] >> bit_offset) & 1) ? 255 : 0;
                } else if (bpp == 2) {
                    const int byte_index = row * bytes_per_row + col / 4;
                    const int shift = (3 - (col % 4)) * 2;
                    const std::uint8_t v = (glyph.bitmap[byte_index] >> shift) & 0x03;
                    cov = static_cast<std::uint8_t>((v * 255) / 3);
                } else if (bpp == 4) {
                    const int byte_index = row * bytes_per_row + col / 2;
                    const int shift = (1 - (col % 2)) * 4;
                    const std::uint8_t v = (glyph.bitmap[byte_index] >> shift) & 0x0F;
                    cov = static_cast<std::uint8_t>((v * 255) / 15);
                } else if (bpp == 8) {
                    const int byte_index = row * bytes_per_row + col;
                    cov = glyph.bitmap[byte_index];
                }
                if (cov) {
                    row_has_ink = true;
                    break;
                }
            }
            if (!row_has_ink) continue;
            if (!out.found) {
                out.found = true;
                out.first_row = out.last_row = row;
            } else {
                out.last_row = row;
            }
            ++out.rows;
        }
        return out;
    }

    void log_font_box(const char* name,
                      const char* text,
                      const Font& font,
                      const Rect& rect,
                      player::PlayerPlatform& platform) {
        if (!name || !text || rect.w <= 0 || rect.h <= 0) {
            return;
        }
        const rgba bg = sample_label_bg(platform, rect);
        const PixelBounds box = scan_text_pixels(platform, rect, bg);
        const int measured_w = measure_text_width(text, font);
        const auto first = text[0]
            ? resolve_glyph_fallback(font, static_cast<std::uint32_t>(static_cast<unsigned char>(text[0])))
            : ResolvedGlyph{};
        const Glyph empty_glyph{};
        const Glyph* glyph = first.glyph ? first.glyph : &empty_glyph;
        const GlyphInkSpan ink = inspect_glyph_ink(*glyph);
        const int bbox_w = box.found ? (box.max_x - box.min_x + 1) : 0;
        const int bbox_h = box.found ? (box.max_y - box.min_y + 1) : 0;
        std::printf(
            "[font-probe] name=%s text=%s rect=%d,%d,%d,%d bg=%u,%u,%u line=%d base=%d measure=%d bbox=%d,%d,%d,%d size=%dx%d pixels=%d glyph=%dx%d adv=%d xoff=%d yoff=%d ink=%d..%d rows=%d\n",
            name,
            text,
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            bg.r,
            bg.g,
            bg.b,
            font.line_height,
            font.baseline,
            measured_w,
            box.min_x,
            box.min_y,
            box.max_x,
            box.max_y,
            bbox_w,
            bbox_h,
            box.count,
            glyph->width,
            glyph->height,
            glyph->x_advance,
            glyph->x_offset,
            glyph->y_offset,
            ink.first_row,
            ink.last_row,
            ink.rows);
    }

    void dump_font_probe_metrics(player::PlayerPlatform& platform,
                                 const ::ui::scene::Scene& scene,
                                 const PlayerUiContext& ctx) {
        using namespace player::ui;
        const Font& hero_font = get_player_font_px(72, FontWeight::Bold);
        const Font& subtitle_font = get_player_font_px(18, FontWeight::Regular);
        const Font& page_title_font = get_font_weighted(FontId::Normal, FontWeight::Bold);
        const Font& meta_font = get_font_weighted(FontId::Small, FontWeight::Regular);
        if (ctx.current_page == player::PlayerPage::Probe) {
            log_font_box("probe.hero_top", "Your", hero_font,
                         scene.world_rect(ctx.handles.probe_hero_top), platform);
            log_font_box("probe.hero_bottom", "Mix", hero_font,
                         scene.world_rect(ctx.handles.probe_hero_bottom), platform);
            log_font_box("probe.hero_subtitle", "Today's Mix for you", subtitle_font,
                         scene.world_rect(ctx.handles.probe_hero_subtitle), platform);
            log_font_box("probe.ref_title", "Reference title", page_title_font,
                         scene.world_rect(ctx.handles.probe_ref_title), platform);
            log_font_box("probe.ref_meta", "Meta / body baseline sample", meta_font,
                         scene.world_rect(ctx.handles.probe_ref_meta), platform);
        } else if (ctx.current_page == player::PlayerPage::Home) {
            log_font_box("home.hero_top", "Your", hero_font,
                         scene.world_rect(ctx.handles.home_title_top), platform);
            log_font_box("home.hero_bottom", "Mix", hero_font,
                         scene.world_rect(ctx.handles.home_title_bottom), platform);
            log_font_box("home.hero_subtitle", "Today's Mix for you", subtitle_font,
                         scene.world_rect(ctx.handles.home_subtitle), platform);
        }
    }
