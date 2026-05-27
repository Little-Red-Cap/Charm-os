module;

#include <array>
#include <cstddef>
#include <utility>

#ifndef CHARM_PLAYER_LAYERED_TRANSITIONS
#define CHARM_PLAYER_LAYERED_TRANSITIONS 1
#endif

export module player.platform;

import charm.core.config;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.draw_cmd;
import charm.gfx.framebuffer;
import charm.ui.scene;
import player.display;

export namespace player {
    class PlayerOwnedDisplayBuffer {
    public:
        static constexpr PlayerDisplayPixelFormat pixel_format = default_player_display_pixel_format;
        static constexpr std::size_t bytes_per_pixel = player_display_bytes_per_pixel(pixel_format);
        static constexpr std::size_t stride_bytes =
            static_cast<std::size_t>(screen_width) * bytes_per_pixel;
        static constexpr std::size_t buffer_bytes =
            stride_bytes * static_cast<std::size_t>(screen_height);

        PlayerDisplaySurface surface() noexcept {
            return PlayerDisplaySurface{
                storage_.data(),
                screen_width,
                screen_height,
                stride_bytes,
                pixel_format,
                PlayerDisplaySurfaceOwnership::Owned,
            };
        }

    private:
        alignas(4) std::array<std::byte, buffer_bytes> storage_{};
    };

    struct PlayerPlatform {
        explicit PlayerPlatform(PlayerDisplaySurface surface) noexcept
            : surface_storage(surface),
              canvas(surface_storage.pixels,
                     surface_storage.width,
                     surface_storage.height,
                     to_vivid_pixel_format(surface_storage.pixel_format),
                     surface_storage.stride_bytes),
              scene(canvas) {}

        template <typename Fn>
        void build_scene(Fn&& fn) {
            scene.build(std::forward<Fn>(fn));
        }

        void clear(const rgba& color) noexcept { canvas.clear(color); }
        void begin_frame() noexcept { canvas.begin_frame(); }
        void render() { scene.render(); }
        void end_frame() noexcept { canvas.end_frame(); }

        RuntimeCanvas& canvas_ref() noexcept { return canvas; }
        const RuntimeCanvas& canvas_ref() const noexcept { return canvas; }
        PlayerDisplaySurface& surface_ref() noexcept { return surface_storage; }
        const PlayerDisplaySurface& surface_ref() const noexcept { return surface_storage; }
        ::ui::scene::Scene& scene_ref() noexcept { return scene; }
        const ::ui::scene::Scene& scene_ref() const noexcept { return scene; }

        std::size_t stride_bytes() const noexcept { return surface_storage.stride_bytes; }
        int width() const noexcept { return surface_storage.width; }
        int height() const noexcept { return surface_storage.height; }

        rgba get_pixel(int x, int y) const noexcept { return canvas.get_pixel(x, y); }
        FrameBufferView framebuffer_view() noexcept { return to_framebuffer_view(surface_storage); }
        FrameBufferView framebuffer_view() const noexcept { return to_framebuffer_view(surface_storage); }

    private:
        PlayerDisplaySurface surface_storage{};
        RuntimeCanvas canvas;
        ::ui::scene::Scene scene;
    };

    struct PlayerFrameContext {
        PlayerPlatform* platform{nullptr};
        PlayerDisplaySink* display_sink{nullptr};
        rgba clear_color{0, 0, 0, 255};
    };

    template <typename Controller>
    bool render_player_frame(PlayerFrameContext& frame,
                             Controller& controller,
                             ::ui::scene::Scene::OverlayFn overlay_fn = nullptr,
                             void* overlay_ctx = nullptr) {
        if (!frame.platform) {
            return false;
        }
        auto& platform = *frame.platform;
        platform.clear(frame.clear_color);
        platform.begin_frame();
        platform.scene_ref().set_overlay(overlay_fn, overlay_ctx);
#if CHARM_PLAYER_LAYERED_TRANSITIONS
        if (controller.transition_needs_destination_snapshot()) {
            controller.prepare_transition_destination_snapshot_scene();
            platform.render();
            controller.finish_transition_destination_snapshot_capture();
            platform.clear(frame.clear_color);
            platform.begin_frame();
            platform.scene_ref().set_overlay(overlay_fn, overlay_ctx);
        }
        controller.compose_now_playing_transition_pixel_layer();
#else
        (void)controller;
#endif
        platform.render();
        platform.end_frame();
        if (frame.display_sink) {
            return frame.display_sink->present(
                platform.surface_ref(),
                full_player_dirty_region(platform.surface_ref()));
        }
        return true;
    }
}
