module;

#include <cstddef>
#include <utility>

#ifndef CHARM_PLAYER_LAYERED_TRANSITIONS
#define CHARM_PLAYER_LAYERED_TRANSITIONS 1
#endif

export module player.render_runtime;

import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.framebuffer;
import charm.ui.scene;
import player.raster;

export namespace player {
    inline constexpr PixelFormat to_vivid_pixel_format(PlayerRasterPixelFormat format) noexcept {
        switch (format) {
        case PlayerRasterPixelFormat::RGB565:
            return PixelFormat::RGB565;
        case PlayerRasterPixelFormat::RGB888:
            return PixelFormat::RGB888;
        case PlayerRasterPixelFormat::ARGB8888:
            return PixelFormat::ARGB8888;
        }
        return PixelFormat::RGB888;
    }

    inline FrameBufferView to_framebuffer_view(const PlayerRasterSurface& surface) noexcept {
        return FrameBufferView{
            to_vivid_pixel_format(surface.pixel_format),
            surface.pixels,
            static_cast<std::size_t>(surface.width > 0 ? surface.width : 0),
            static_cast<std::size_t>(surface.height > 0 ? surface.height : 0),
            surface.stride_bytes,
        };
    }

    class PlayerRenderRuntime {
    public:
        explicit PlayerRenderRuntime(PlayerRasterSurface surface) noexcept
            : surface_(surface),
              canvas_(surface_.pixels,
                      surface_.width,
                      surface_.height,
                      to_vivid_pixel_format(surface_.pixel_format),
                      surface_.stride_bytes),
              scene_(canvas_) {}

        template <typename Fn>
        void build_scene(Fn&& fn) {
            scene_.build(std::forward<Fn>(fn));
        }

        void clear(const rgba& color) noexcept { canvas_.clear(color); }
        void begin_frame() noexcept { canvas_.begin_frame(); }
        void render() { scene_.render(); }
        void end_frame() noexcept { canvas_.end_frame(); }

        RuntimeCanvas& canvas_ref() noexcept { return canvas_; }
        const RuntimeCanvas& canvas_ref() const noexcept { return canvas_; }
        PlayerRasterSurface& surface_ref() noexcept { return surface_; }
        const PlayerRasterSurface& surface_ref() const noexcept { return surface_; }
        ::ui::scene::Scene& scene_ref() noexcept { return scene_; }
        const ::ui::scene::Scene& scene_ref() const noexcept { return scene_; }

        std::size_t stride_bytes() const noexcept { return surface_.stride_bytes; }
        int width() const noexcept { return surface_.width; }
        int height() const noexcept { return surface_.height; }

        rgba get_pixel(int x, int y) const noexcept { return canvas_.get_pixel(x, y); }
        FrameBufferView framebuffer_view() noexcept { return to_framebuffer_view(surface_); }
        FrameBufferView framebuffer_view() const noexcept { return to_framebuffer_view(surface_); }

    private:
        PlayerRasterSurface surface_{};
        RuntimeCanvas canvas_;
        ::ui::scene::Scene scene_;
    };

    struct PlayerRenderFrame {
        PlayerRenderRuntime* runtime{nullptr};
        const PlayerRasterDisplay* display{nullptr};
        rgba clear_color{0, 0, 0, 255};
    };

    template <typename Controller>
    bool render_player_frame(PlayerRenderFrame& frame,
                             Controller& controller,
                             ::ui::scene::Scene::OverlayFn overlay_fn = nullptr,
                             void* overlay_ctx = nullptr) {
        if (!frame.runtime) {
            return false;
        }
        auto& runtime = *frame.runtime;
        runtime.clear(frame.clear_color);
        runtime.begin_frame();
        runtime.scene_ref().set_overlay(overlay_fn, overlay_ctx);
#if CHARM_PLAYER_LAYERED_TRANSITIONS
        if (controller.transition_needs_destination_snapshot()) {
            if (controller.transition_destination_snapshot_ready_to_capture()) {
                controller.prepare_transition_destination_snapshot_scene();
                runtime.render();
                controller.finish_transition_destination_snapshot_capture();
                runtime.clear(frame.clear_color);
                runtime.begin_frame();
                runtime.scene_ref().set_overlay(overlay_fn, overlay_ctx);
            } else {
                controller.schedule_transition_destination_snapshot_capture();
            }
        }
        controller.compose_now_playing_transition_pixel_layer();
#else
        (void)controller;
#endif
        runtime.render();
        runtime.end_frame();
        return !frame.display
            || frame.display->present(runtime.surface_ref(),
                                      full_player_raster_region(runtime.surface_ref()));
    }
}
