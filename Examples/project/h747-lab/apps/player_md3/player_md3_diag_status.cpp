#include "console.h"
#include "display_raster.h"
#include "memory_probe.h"
#include "player_md3_diag.hpp"
#include "player_md3_smoke_schema.hpp"

#include <cstdint>

namespace {

class StatusWriter {
public:
    explicit StatusWriter(const char* prefix) {
        h747::console::write(prefix);
    }

    void field(const h747::apps::player_md3::PlayerMd3SmokeField field) const {
        text(" ");
        text(h747::apps::player_md3::smoke_field_token(field));
        text("=");
    }

    void text(const char* value) const {
        h747::console::write(value);
    }

    void dec(const std::uint32_t value) const {
        h747::console::write_dec(value);
    }

    void hex32(const std::uint32_t value) const {
        h747::console::write_hex32(value);
    }

    void slash() const {
        text("/");
    }

    void colon() const {
        text(":");
    }

    void comma() const {
        text(",");
    }

    void dash() const {
        text("-");
    }

    void at() const {
        text("@");
    }

    void newline() const {
        text("\n");
    }
};

} // namespace

namespace h747::apps::player_md3 {

void print_status(const char* prefix) {
    const auto raster = display_raster_state();
    const auto memory = memory_probe_storage_state();
    const auto& st = state();
    const StatusWriter out{prefix};
    out.field(PlayerMd3SmokeField::RealMd3);
    out.dec(1U);
    out.field(PlayerMd3SmokeField::Mock);
    out.dec(st.smoke_mock_path);
    out.field(PlayerMd3SmokeField::Smoke);
    out.dec(st.smoke_ok);
    out.slash();
    out.dec(st.smoke_boot_ok);
    out.dec(st.smoke_render_ok);
    out.dec(st.smoke_present_ok);
    out.dec(st.smoke_content_ok);
    out.dec(st.smoke_scene_ok);
    out.field(PlayerMd3SmokeField::Delta);
    out.dec(st.smoke_frame_delta);
    out.slash();
    out.dec(st.smoke_present_delta);
    out.field(PlayerMd3SmokeField::Display);
    out.dec(st.display_ready ? 1U : 0U);
    out.field(PlayerMd3SmokeField::Sdram1);
    out.dec(memory.sdram1_ready);
    out.slash();
    out.dec(memory.sdram1_smoke_ok);
    out.field(PlayerMd3SmokeField::RuntimeStorage);
    out.dec(st.runtime_storage_ready ? 1U : 0U);
    out.field(PlayerMd3SmokeField::Boot);
    out.dec(st.runtime_bootstrapped ? 1U : 0U);
    out.field(PlayerMd3SmokeField::Render);
    out.dec(st.last_render_ok ? 1U : 0U);
    out.field(PlayerMd3SmokeField::Frames);
    out.dec(st.frames);
    out.field(PlayerMd3SmokeField::Present);
    out.dec(raster.present_count);
    out.field(PlayerMd3SmokeField::Layer);
    out.dec(raster.ltdc_layer_ready);
    out.field(PlayerMd3SmokeField::Framebuffer);
    out.hex32(raster.framebuffer_base);
    out.field(PlayerMd3SmokeField::Bytes);
    out.hex32(raster.framebuffer_bytes);
    out.field(PlayerMd3SmokeField::RenderBuffer);
    out.hex32(static_cast<std::uint32_t>(st.render_surface));
    out.field(PlayerMd3SmokeField::Platform);
    out.hex32(static_cast<std::uint32_t>(st.platform_storage));
    out.field(PlayerMd3SmokeField::Runtime);
    out.hex32(static_cast<std::uint32_t>(st.runtime_storage));
    out.field(PlayerMd3SmokeField::PoolBytes);
    out.hex32(st.external_pool_bytes);
    out.field(PlayerMd3SmokeField::RenderSamples);
    out.hex32(st.render_sample0);
    out.slash();
    out.hex32(st.render_sample_center);
    out.slash();
    out.hex32(st.render_sample_last);
    out.field(PlayerMd3SmokeField::CommandBuffer);
    out.dec(st.scene_cmd_count);
    out.slash();
    out.dec(st.scene_cmd_capacity);
    out.field(PlayerMd3SmokeField::CommandOverflow);
    out.dec(st.scene_cmd_overflowed);
    out.field(PlayerMd3SmokeField::TextBuffer);
    out.dec(st.scene_text_used);
    out.slash();
    out.dec(st.scene_text_capacity);
    out.field(PlayerMd3SmokeField::TextOverflow);
    out.dec(st.scene_text_overflowed);
    out.field(PlayerMd3SmokeField::ExecFailed);
    out.dec(st.scene_exec_failed);
    out.field(PlayerMd3SmokeField::ExecCounts);
    out.dec(st.scene_exec_cmd_rect);
    out.slash();
    out.dec(st.scene_exec_cmd_text);
    out.slash();
    out.dec(st.scene_exec_cmd_image);
    out.field(PlayerMd3SmokeField::FailTextImage);
    out.dec(st.scene_exec_fail_text);
    out.slash();
    out.dec(st.scene_exec_fail_image);
    out.field(PlayerMd3SmokeField::Input);
    out.dec(st.input_polls);
    out.slash();
    out.dec(st.input_events);
    out.field(PlayerMd3SmokeField::Touch);
    out.dec(st.input_touch_probe_ok);
    out.slash();
    out.dec(st.input_touch_ready);
    out.slash();
    out.dec(st.input_touch_down);
    out.at();
    out.dec(st.input_last_x);
    out.comma();
    out.dec(st.input_last_y);
    out.field(PlayerMd3SmokeField::Events);
    out.dec(st.input_touch_events);
    out.slash();
    out.dec(st.input_encoder_events);
    out.slash();
    out.dec(st.input_button_events);
    out.field(PlayerMd3SmokeField::Content);
    out.hex32(st.render_bg_pixel);
    out.colon();
    out.dec(st.render_non_bg_pixels);
    out.at();
    out.dec(st.render_content_min_x);
    out.comma();
    out.dec(st.render_content_min_y);
    out.dash();
    out.dec(st.render_content_max_x);
    out.comma();
    out.dec(st.render_content_max_y);
    out.field(PlayerMd3SmokeField::PresentSource);
    out.hex32(raster.present_src_sample0);
    out.slash();
    out.hex32(raster.present_src_sample_center);
    out.slash();
    out.hex32(raster.present_src_sample_last);
    out.field(PlayerMd3SmokeField::PresentDestination);
    out.hex32(raster.presented_sample0);
    out.slash();
    out.hex32(raster.presented_sample_center);
    out.slash();
    out.hex32(raster.presented_sample_last);
    out.field(PlayerMd3SmokeField::FrontBuffer);
    out.hex32(raster.front_buffer_base);
    out.colon();
    out.hex32(raster.front_sample0);
    out.slash();
    out.hex32(raster.front_sample_center);
    out.slash();
    out.hex32(raster.front_sample_last);
    out.field(PlayerMd3SmokeField::BackBuffer);
    out.hex32(raster.back_buffer_base);
    out.colon();
    out.hex32(raster.back_sample0);
    out.slash();
    out.hex32(raster.back_sample_center);
    out.slash();
    out.hex32(raster.back_sample_last);
    out.field(PlayerMd3SmokeField::LtdcFramebuffer);
    out.hex32(raster.ltdc_layer_cfb_addr);
    out.field(PlayerMd3SmokeField::LtdcControl);
    out.hex32(raster.ltdc_layer_cr);
    out.field(PlayerMd3SmokeField::LtdcPixelFormat);
    out.hex32(raster.ltdc_layer_pfcr);
    out.newline();
}

void maybe_print_loop_status() noexcept {
    auto& st = state();
    ++st.status_ticks;
    if ((st.status_ticks % 120U) == 0U) {
        print_status("player_md3.loop");
    }
}

} // namespace h747::apps::player_md3
