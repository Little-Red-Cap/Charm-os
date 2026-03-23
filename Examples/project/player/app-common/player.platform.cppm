module;
#include <cstddef>
#include <optional>
#include <utility>

export module player.platform;

import charm.ui.scene;
import charm.gfx.canvas;
import charm.gfx.framebuffer;

export namespace player {
    struct PlayerPlatform {
        DefaultFrameBuffer framebuffer{};
        DefaultCanvas canvas{framebuffer};
        ::ui::scene::Scene scene{canvas};

        template <typename Fn>
        void build_scene(Fn&& fn) {
            scene.build(std::forward<Fn>(fn));
        }

        void begin_frame() { canvas.begin_frame(); }
        void render() { scene.render(); }
        void end_frame() { canvas.end_frame(); }

        DefaultCanvas& canvas_ref() { return canvas; }
        DefaultFrameBuffer& framebuffer_ref() { return framebuffer; }
        ::ui::scene::Scene& scene_ref() { return scene; }
        std::size_t stride_bytes() const { return DefaultFrameBuffer::stride_bytes; }
    };
}
