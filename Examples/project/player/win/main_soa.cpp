#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <SDL3/SDL.h>
#if defined(_WIN32)
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

import charm.core.config;
import charm.core.event;
import charm.core.soa_gui;
import charm.core.soa_kernel;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.core.geometry;
import charm.core.widget_registry;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.draw_cmd;
import charm.gfx.framebuffer;
import charm.font.typography;
import out.api;

namespace {
    struct Viewport {
        int x{0};
        int y{0};
        int w{0};
        int h{0};
        float scale{1.0f};
    };

    Viewport compute_viewport(int win_w, int win_h, int canvas_w, int canvas_h) noexcept {
        const float sx = static_cast<float>(win_w) / static_cast<float>(canvas_w);
        const float sy = static_cast<float>(win_h) / static_cast<float>(canvas_h);
        const float scale = (sx < sy) ? sx : sy;
        const int w = static_cast<int>(static_cast<float>(canvas_w) * scale);
        const int h = static_cast<int>(static_cast<float>(canvas_h) * scale);
        const int x = (win_w - w) / 2;
        const int y = (win_h - h) / 2;
        return Viewport{x, y, w, h, scale};
    }

    bool map_mouse(const Viewport& vp, int wx, int wy, int& out_x, int& out_y) noexcept {
        if (wx < vp.x || wy < vp.y || wx >= vp.x + vp.w || wy >= vp.y + vp.h) return false;
        out_x = static_cast<int>((wx - vp.x) / vp.scale);
        out_y = static_cast<int>((wy - vp.y) / vp.scale);
        return true;
    }

    constexpr rgba kBackground{236, 239, 245, 255};
    constexpr rgba kPanel{252, 253, 255, 255};
    constexpr rgba kPanelBorder{206, 212, 222, 255};
    constexpr rgba kText{24, 28, 34, 255};
    constexpr rgba kTextMuted{96, 102, 112, 255};
    constexpr rgba kAccent{42, 102, 204, 255};
    constexpr rgba kOnAccent{255, 255, 255, 255};

    constexpr std::array<const char*, 10> kTracks{
        "Track 01 - Demo",
        "Track 02 - Sunrise",
        "Track 03 - Nightfall",
        "Track 04 - Horizon",
        "Track 05 - Echo",
        "Track 06 - Drift",
        "Track 07 - Signal",
        "Track 08 - Gravity",
        "Track 09 - Overture",
        "Track 10 - Outro"
    };

    constexpr int kTileWidth = screen_width;
    constexpr int kTileHeight = 96;
    constexpr std::size_t kTileStride =
        static_cast<std::size_t>(kTileWidth) * DefaultFrameBuffer::bytes_per_pixel;
    constexpr std::size_t kTileBytes = kTileStride * static_cast<std::size_t>(kTileHeight);

    static DefaultFrameBuffer g_framebuffer{};
    static std::array<std::byte, kTileBytes> g_tile_storage{};

    std::uint32_t hash_bytes(const std::byte* data, std::size_t len) noexcept {
        std::uint32_t hash = 2166136261u;
        if (!data) return hash;
        for (std::size_t i = 0; i < len; ++i) {
            hash ^= static_cast<std::uint8_t>(data[i]);
            hash *= 16777619u;
        }
        return hash;
    }

    struct SdlTileBackend {
        DefaultFrameBuffer& fb;
        bool dirty_set{false};
        int dirty_left{0};
        int dirty_top{0};
        int dirty_right{0};
        int dirty_bottom{0};

        int width() const noexcept { return screen_width; }
        int height() const noexcept { return screen_height; }

        void begin_frame() noexcept { dirty_set = false; }
        void end_frame() noexcept {}

        void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
            if (!src || bytes == 0) return;
            if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) return;
            const std::size_t bpp = DefaultFrameBuffer::bytes_per_pixel;
            const std::size_t stride = DefaultFrameBuffer::stride_bytes;
            const std::size_t max_bytes = static_cast<std::size_t>(screen_width - x) * bpp;
            const std::size_t copy_bytes = bytes < max_bytes ? bytes : max_bytes;
            std::byte* dst = fb.data()
                + static_cast<std::size_t>(y) * stride
                + static_cast<std::size_t>(x) * bpp;
            std::memcpy(dst, src, copy_bytes);
        }

        void mark_dirty(int x, int y, int w, int h) noexcept {
            if (w <= 0 || h <= 0) return;
            const int left = x;
            const int top = y;
            const int right = x + w;
            const int bottom = y + h;
            if (!dirty_set) {
                dirty_left = left;
                dirty_top = top;
                dirty_right = right;
                dirty_bottom = bottom;
                dirty_set = true;
                return;
            }
            if (left < dirty_left) dirty_left = left;
            if (top < dirty_top) dirty_top = top;
            if (right > dirty_right) dirty_right = right;
            if (bottom > dirty_bottom) dirty_bottom = bottom;
        }

        bool dirty_rect(Rect& out) const noexcept {
            if (!dirty_set) return false;
            out = Rect{
                dirty_left,
                dirty_top,
                dirty_right - dirty_left,
                dirty_bottom - dirty_top
            };
            return out.w > 0 && out.h > 0;
        }
    };

    void apply_player_theme() noexcept {
        ThemeTokens tokens{};
        tokens.surface = kBackground;
        tokens.surface_variant = kPanel;
        tokens.on_surface = kText;
        tokens.on_surface_muted = kTextMuted;
        tokens.outline = kPanelBorder;
        tokens.accent = kAccent;
        tokens.on_accent = kOnAccent;
        tokens.focus_ring = kAccent;
        apply_theme_tokens(tokens);

        auto& sheet = StyleSheet::instance();
        sheet.clear();

        StyleRolePatch panel{};
        panel.has_bg_color = true;
        panel.bg_color = StyleRole::SurfaceVariant;
        panel.has_border_color = true;
        panel.border_color = StyleRole::Outline;
        panel.has_border_focus = true;
        panel.border_focus = StyleRole::FocusRing;
        sheet.add_role_rule(StyleSelector{WidgetKind::ScrollContainer, 0, kStyleVariantAny}, panel);
        sheet.add_role_rule(StyleSelector{WidgetKind::List, 0, kStyleVariantAny}, panel);

        StyleRolePatch list_item{};
        list_item.has_bg_color = true;
        list_item.bg_color = StyleRole::SurfaceVariant;
        list_item.has_border_color = true;
        list_item.border_color = StyleRole::SurfaceVariant;
        list_item.has_font_color = true;
        list_item.font_color = StyleRole::OnSurface;
        list_item.has_accent_color = true;
        list_item.accent_color = StyleRole::Accent;
        list_item.has_on_accent = true;
        list_item.on_accent = StyleRole::OnAccent;
        sheet.add_role_rule(StyleSelector{WidgetKind::ListItem, 0, kStyleVariantAny}, list_item);

        StyleRolePatch progress{};
        progress.has_bg_color = true;
        progress.bg_color = StyleRole::SurfaceVariant;
        progress.has_border_color = true;
        progress.border_color = StyleRole::Outline;
        progress.has_accent_color = true;
        progress.accent_color = StyleRole::Accent;
        sheet.add_role_rule(StyleSelector{WidgetKind::Progress, 0, kStyleVariantAny}, progress);

        StyleRolePatch title_btn{};
        title_btn.has_bg_color = true;
        title_btn.bg_color = StyleRole::Accent;
        title_btn.has_bg_hover = true;
        title_btn.bg_hover = StyleRole::AccentHover;
        title_btn.has_bg_pressed = true;
        title_btn.bg_pressed = StyleRole::AccentPressed;
        title_btn.has_border_color = true;
        title_btn.border_color = StyleRole::Accent;
        title_btn.has_border_hover = true;
        title_btn.border_hover = StyleRole::AccentHover;
        title_btn.has_border_pressed = true;
        title_btn.border_pressed = StyleRole::AccentPressed;
        title_btn.has_font_color = true;
        title_btn.font_color = StyleRole::OnAccent;
        title_btn.has_on_accent = true;
        title_btn.on_accent = StyleRole::OnAccent;
        sheet.add_role_rule(StyleSelector{WidgetKind::Button, 0, kStyleVariantAny}, title_btn);
    }
}

int main(int argc, char** argv) {
    bool use_tiles = false;
    bool run_compare = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--soa-tile") {
            use_tiles = true;
        } else if (arg == "--soa-compare") {
            run_compare = true;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        (void)out::error<"SDL_Init failed: {}">(SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Player (SoA)", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    auto& theme = Theme::instance();
    theme.set_default_font(get_font(FontId::Normal));
    apply_player_theme();

    DefaultCanvas canvas(g_framebuffer);
    FrameBufferView tile_view{
        screen_pixel_format,
        g_tile_storage.data(),
        kTileWidth,
        kTileHeight,
        kTileStride
    };
    ui::draw_cmd::DrawCmdTileConfig tile_config{};
    tile_config.tile_width = kTileWidth;
    tile_config.tile_height = kTileHeight;
    tile_config.clear_color = kBackground;
    tile_config.clear_tile = true;
    SdlTileBackend tile_backend{g_framebuffer};
    SoaKernel kernel{};
    SoaFactory factory{kernel};

    auto root = factory.create_container();
    kernel.set_rect(root, {0, 0, screen_width, screen_height});
    kernel.set_clip_children(root, true);

    auto header = factory.create_scroll_container();
    auto title = factory.create_button("Charm Player (SoA)");
    auto status = factory.create_label("Status: Ready");
    auto progress = factory.create_progress();
    auto list = factory.create_list();

    factory.link(root, header);
    factory.link(root, list);
    factory.link(header, title);
    factory.link(header, status);
    factory.link(header, progress);

    kernel.set_hit_testable(header, false);
    kernel.set_focusable(header, false);
    kernel.set_hit_testable(title, false);
    kernel.set_focusable(title, false);

    constexpr int outer_pad = 16;
    constexpr int header_pad = 12;
    constexpr int header_h = 108;
    const int content_w = screen_width - outer_pad * 2;
    const int list_y = outer_pad + header_h + 12;
    const int list_h = screen_height - list_y - outer_pad;

    kernel.set_rect(header, {outer_pad, outer_pad, content_w, header_h});
    kernel.set_rect(title, {header_pad, header_pad, content_w - header_pad * 2, 30});
    kernel.set_rect(status, {header_pad, 50, content_w - header_pad * 2, 20});
    kernel.set_rect(progress, {header_pad, 74, content_w - header_pad * 2, 18});
    kernel.set_rect(list, {outer_pad, list_y, content_w, list_h});
    kernel.set_list_row_height(list, 28);
    kernel.set_range(progress, 0, 100);

    for (const char* item_text : kTracks) {
        auto item = factory.create_list_item(item_text);
        factory.link(list, item);
    }

    SoaGui gui(canvas, kernel, root);

#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_compare) {
        ui::draw_cmd::DefaultDrawCmdBuffer buf{};
        ui::draw_cmd::DrawCmdExecutor exec{};
        gui.record_commands(buf);
        const auto cmd_stats = buf.stats();

        g_framebuffer.clear(kBackground);
        canvas.begin_frame();
        const auto exec_stats = exec.execute(canvas, buf);
        canvas.end_frame();
        const std::uint32_t hash_full =
            hash_bytes(g_framebuffer.data(), DefaultFrameBuffer::buffer_bytes);

        g_framebuffer.clear(kBackground);
        const auto tile_stats = exec.execute_tiles(tile_backend, tile_view, buf, tile_config);
        const std::uint32_t hash_tile =
            hash_bytes(g_framebuffer.data(), DefaultFrameBuffer::buffer_bytes);

        (void)out::println<"[player] cmd count={} bytes={} text={}/{} overflow={} text_overflow={}">(
            static_cast<std::uint32_t>(cmd_stats.cmd_count),
            static_cast<std::uint32_t>(cmd_stats.cmd_bytes),
            static_cast<std::uint32_t>(cmd_stats.text_used),
            static_cast<std::uint32_t>(cmd_stats.text_capacity),
            cmd_stats.cmd_overflowed ? 1 : 0,
            cmd_stats.text_overflowed ? 1 : 0);
        (void)out::println<"[player] exec count={} bytes={} clip_push={} clip_pop={} overflow={}">(
            static_cast<std::uint32_t>(exec_stats.cmd_count),
            static_cast<std::uint32_t>(exec_stats.cmd_bytes),
            static_cast<std::uint32_t>(exec_stats.clip_pushes),
            static_cast<std::uint32_t>(exec_stats.clip_pops),
            exec_stats.overflowed ? 1 : 0);
        (void)out::println<"[player] tile total={} drawn={} flushes={} cmd={} bytes={}">(
            tile_stats.tiles_total,
            tile_stats.tiles_drawn,
            tile_stats.tile_flush_count,
            static_cast<std::uint32_t>(tile_stats.cmd_count),
            static_cast<std::uint32_t>(tile_stats.cmd_bytes));
        (void)out::println<"[player] compare full=0x{:08X} tile=0x{:08X}">(
            static_cast<unsigned>(hash_full),
            static_cast<unsigned>(hash_tile));
        if (hash_full != hash_tile) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }
#else
    if (run_compare) {
        (void)out::println<"[player] --soa-compare requires VIVID_SOA_TRACE_INPUT">();
    }
#endif

    int win_w = screen_width;
    int win_h = screen_height;
    int mouse_x = 0;
    int mouse_y = 0;
    bool running = true;
    bool force_redraw = true;
    int progress_value = 0;
    std::uint32_t last_anim_tick = SDL_GetTicks();
    std::uint32_t last_paint_version = kernel.paint_dirty_version();
    std::uint32_t last_layout_version = kernel.layout_dirty_version();

    while (running) {
        SDL_Event evt{};
        SDL_GetWindowSize(window, &win_w, &win_h);
        const Viewport vp = compute_viewport(win_w, win_h, screen_width, screen_height);

        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                force_redraw = true;
            }
            if (evt.type == SDL_EVENT_MOUSE_MOTION) {
                if (map_mouse(vp, evt.motion.x, evt.motion.y, mouse_x, mouse_y)) {
                    gui.dispatch_event(Event::mouse(Event::Type::MouseMove, mouse_x, mouse_y, 0));
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, mouse_x, mouse_y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, mouse_x, mouse_y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                gui.dispatch_event(Event::wheel(mouse_x, mouse_y, static_cast<int>(evt.wheel.y)));
            }
        }

        const std::uint32_t now_ms = SDL_GetTicks();
        if (now_ms - last_anim_tick >= 50) {
            last_anim_tick = now_ms;
            progress_value = (progress_value + 1) % 101;
            kernel.set_value(progress, progress_value);
        }

        const std::uint32_t paint_version = kernel.paint_dirty_version();
        const std::uint32_t layout_version = kernel.layout_dirty_version();
        const bool needs_redraw = force_redraw
            || paint_version != last_paint_version
            || layout_version != last_layout_version;

        if (needs_redraw) {
            if (use_tiles) {
                gui.render_tiles(tile_backend, tile_view, tile_config);
                Rect dirty{};
                if (tile_backend.dirty_rect(dirty)) {
                    SDL_Rect rect{dirty.x, dirty.y, dirty.w, dirty.h};
                    const std::size_t bpp = DefaultFrameBuffer::bytes_per_pixel;
                    const std::size_t stride = DefaultFrameBuffer::stride_bytes;
                    const std::byte* src = g_framebuffer.data()
                        + static_cast<std::size_t>(dirty.y) * stride
                        + static_cast<std::size_t>(dirty.x) * bpp;
                    SDL_UpdateTexture(texture, &rect, src, static_cast<int>(stride));
                }
            } else {
                canvas.clear(kBackground);
                gui.render();
                SDL_UpdateTexture(texture, nullptr, canvas.data(),
                                  static_cast<int>(DefaultFrameBuffer::stride_bytes));
            }

            SDL_SetRenderDrawColor(renderer, kBackground.r, kBackground.g, kBackground.b, 255);
            SDL_RenderClear(renderer);
            SDL_FRect dst{
                static_cast<float>(vp.x),
                static_cast<float>(vp.y),
                static_cast<float>(vp.w),
                static_cast<float>(vp.h)
            };
            SDL_RenderTexture(renderer, texture, nullptr, &dst);
            SDL_RenderPresent(renderer);

            last_paint_version = paint_version;
            last_layout_version = layout_version;
            force_redraw = false;
        }

        SDL_Delay(8);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
