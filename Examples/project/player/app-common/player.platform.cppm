module;
#include <cstddef>
#include <optional>

export module player.platform;

import charm.core.soa_gui;
import charm.core.soa_kernel;
import charm.gfx.canvas;
import charm.gfx.draw_cmd;
import charm.gfx.framebuffer;

export namespace player {
    struct PlayerPlatform {
        DefaultFrameBuffer framebuffer{};
        DefaultCanvas canvas{framebuffer};
        ui::draw_cmd::DefaultDrawCmdBuffer cmd_buf{};
        ui::draw_cmd::DrawCmdExecutor cmd_exec{};
        std::optional<SoaGui> gui{};

        void bind_gui(SoaKernel& kernel, WidgetHandle root) {
            gui.emplace(canvas, kernel, root);
        }

        void begin_frame() { canvas.begin_frame(); }
        void record() {
            if (gui) gui->record_commands(cmd_buf);
        }
        void execute() { cmd_exec.execute(canvas, cmd_buf); }
        void end_frame() { canvas.end_frame(); }

        DefaultCanvas& canvas_ref() { return canvas; }
        DefaultFrameBuffer& framebuffer_ref() { return framebuffer; }
        ui::draw_cmd::DefaultDrawCmdBuffer& commands() { return cmd_buf; }
        std::size_t stride_bytes() const { return DefaultFrameBuffer::stride_bytes; }
    };
}
