#include "player_md3.h"

#include "console.h"
#include "display_raster.h"

#include <cstddef>
#include <cstdint>

import charm.font.defaults_noto;
import charm.font.typography;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.draw_cmd;
import charm.gfx.pixel_format;
import charm.gfx.text_box;

namespace {

constexpr int kWidth = 720;
constexpr int kHeight = 1280;
constexpr std::size_t kStrideBytes = 720U * 4U;
constexpr std::size_t kFrameBytes = 720U * 1280U * 4U;

struct PlayerMd3State {
    bool draw_ready{false};
    bool frame_rendered{false};
    std::uint32_t frame_counter{0};
    std::uint32_t failed_cmds{0};
};

PlayerMd3State& state() noexcept {
    static PlayerMd3State s{};
    return s;
}

ui::draw_cmd::DefaultDrawCmdBuffer& draw_buffer() noexcept {
    static ui::draw_cmd::DefaultDrawCmdBuffer buffer{};
    return buffer;
}

ui::draw_cmd::DrawCmdExecutor& draw_executor() noexcept {
    static ui::draw_cmd::DrawCmdExecutor executor{};
    return executor;
}

const Font& normal_font() noexcept {
    return get_font(FontId::Normal);
}

const Font& small_font() noexcept {
    return get_font(FontId::Small);
}

void text_box(ui::draw_cmd::DefaultDrawCmdBuffer& out,
              const Rect& rect,
              const char* text,
              const rgba color,
              const Font& font,
              const TextAlignH align = TextAlignH::Left) noexcept {
    out.draw_text_box(rect, text, color, font, align, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
}

void fill_panel(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                const Rect& rect,
                const int radius,
                const rgba fill,
                const rgba stroke = rgba{0, 0, 0, 0}) noexcept {
    out.fill_round_rect(rect, radius, fill);
    if (stroke.a != 0) {
        out.stroke_round_rect(rect, radius, stroke);
    }
}

void draw_button(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                 const Rect& rect,
                 const char* label,
                 const rgba fill,
                 const rgba text) noexcept {
    const int radius = rect.h / 2;
    fill_panel(out, rect, radius, fill, rgba{255, 255, 255, 32});
    text_box(out, rect, label, text, normal_font(), TextAlignH::Center);
}

void record_player_frame(ui::draw_cmd::DefaultDrawCmdBuffer& out) noexcept {
    out.clear();

    out.fill_rect(Rect{0, 0, kWidth, kHeight}, rgba{9, 14, 16, 255});
    out.fill_round_rect(Rect{-126, -96, 438, 438}, 219, rgba{40, 82, 72, 180});
    out.fill_round_rect(Rect{348, 136, 430, 528}, 215, rgba{72, 48, 82, 118});
    out.fill_round_rect(Rect{-84, 818, 388, 388}, 194, rgba{124, 82, 44, 96});

    text_box(out, Rect{42, 46, 260, 42}, "Charm Player", rgba{232, 242, 234, 255}, normal_font());
    text_box(out, Rect{420, 46, 248, 42}, "H747 Vivid", rgba{166, 190, 178, 255}, normal_font(),
             TextAlignH::Right);

    fill_panel(out, Rect{58, 142, 580, 580}, 64, rgba{230, 160, 86, 255});
    out.fill_round_rect(Rect{58, 142, 580, 188}, 64, rgba{250, 198, 106, 255});
    out.fill_round_rect(Rect{58, 398, 580, 324}, 64, rgba{60, 118, 108, 255});
    out.fill_circle(348, 432, 215, rgba{20, 34, 36, 96});
    out.stroke_circle(348, 432, 216, rgba{255, 245, 218, 44});
    text_box(out, Rect{142, 378, 412, 76}, "MD3", rgba{255, 245, 218, 255}, normal_font(), TextAlignH::Center);
    text_box(out, Rect{142, 448, 412, 52}, "portable draw pipeline", rgba{255, 236, 196, 220}, small_font(),
             TextAlignH::Center);

    text_box(out, Rect{58, 770, 604, 58}, "Default Cover Preview", rgba{246, 249, 242, 255}, normal_font(),
             TextAlignH::Center);
    text_box(out, Rect{58, 828, 604, 42}, "Vivid DrawCmd -> RuntimeCanvas -> SDRAM framebuffer",
             rgba{164, 186, 176, 255}, small_font(), TextAlignH::Center);

    const Rect track{76, 922, 568, 22};
    fill_panel(out, track, 11, rgba{22, 34, 34, 255});
    fill_panel(out, Rect{76, 922, 238, 22}, 11, rgba{244, 184, 98, 255});
    out.fill_circle(314, 933, 18, rgba{255, 224, 154, 255});
    text_box(out, Rect{76, 956, 160, 30}, "01:48", rgba{144, 164, 156, 255}, small_font());
    text_box(out, Rect{484, 956, 160, 30}, "04:16", rgba{144, 164, 156, 255}, small_font(), TextAlignH::Right);

    draw_button(out, Rect{112, 1044, 118, 72}, "Prev", rgba{30, 44, 44, 235}, rgba{230, 238, 232, 255});
    draw_button(out, Rect{266, 1022, 188, 116}, "Play", rgba{244, 184, 98, 255}, rgba{24, 28, 22, 255});
    draw_button(out, Rect{490, 1044, 118, 72}, "Next", rgba{30, 44, 44, 235}, rgba{230, 238, 232, 255});

    text_box(out, Rect{68, 1184, 584, 36}, "portable target: no SDL / no FreeType / no host cover decode",
             rgba{132, 156, 148, 255}, small_font(), TextAlignH::Center);
}

void print_status(const char* prefix) {
    const auto raster = display_raster_state();
    const auto& st = state();
    h747::console::write(prefix);
    h747::console::write(" draw=");
    h747::console::write_dec(st.draw_ready ? 1U : 0U);
    h747::console::write(" rendered=");
    h747::console::write_dec(st.frame_rendered ? 1U : 0U);
    h747::console::write(" frames=");
    h747::console::write_dec(st.frame_counter);
    h747::console::write(" failed_cmds=");
    h747::console::write_dec(st.failed_cmds);
    h747::console::write(" present=");
    h747::console::write_dec(raster.present_count);
    h747::console::write(" fb=");
    h747::console::write_hex32(raster.framebuffer_base);
    h747::console::write(" bytes=");
    h747::console::write_hex32(raster.framebuffer_bytes);
    h747::console::write("\n");
}

void render_once() noexcept {
    auto& st = state();
    auto& buffer = draw_buffer();
    RuntimeCanvas frame_canvas{
        static_cast<std::byte*>(display_raster_framebuffer()),
        kWidth,
        kHeight,
        PixelFormat::ARGB8888,
        kStrideBytes,
    };
    record_player_frame(buffer);
    reset_alpha_blend_count();
    frame_canvas.begin_frame();
    const auto stats = draw_executor().execute(frame_canvas, buffer);
    frame_canvas.end_frame();
    st.failed_cmds = static_cast<std::uint32_t>(stats.failed_cmds);
    st.frame_rendered = stats.failed_cmds == 0
        && display_raster_present(display_raster_framebuffer(), static_cast<std::uint32_t>(kFrameBytes)) != 0U;
    ++st.frame_counter;
}

} // namespace

namespace h747::apps::player_md3 {

void init() {
    h747::console::write_line("player_md3: vivid portable draw pipeline");
    const auto raster = display_raster_state();
    if (raster.init_ok == 0U || raster.framebuffer_ready == 0U || raster.ltdc_layer_ready == 0U) {
        h747::console::write_line("player_md3: display_raster service is not ready");
        print_status("player_md3");
        return;
    }

    state().draw_ready = true;
    render_once();
    print_status("player_md3");
}

void loop_once() noexcept {
    if (!state().draw_ready) {
        return;
    }
}

} // namespace h747::apps::player_md3
