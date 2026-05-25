#include "console.h"
#include "display_raster.h"
#include "memory_probe.h"
#include "player_md3_diag.hpp"
#include "player_md3_smoke_schema.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

import charm.core.handle;

namespace {

std::uint32_t sample_argb8888(const std::uintptr_t pixels, const std::uint32_t byte_offset) noexcept {
    if (pixels == 0U) {
        return 0U;
    }
    std::uint32_t value{};
    std::memcpy(&value, reinterpret_cast<const std::byte*>(pixels + byte_offset), sizeof(value));
    return value;
}

std::uint32_t clamp_size_to_u32(const std::size_t value) noexcept {
    constexpr std::size_t max_u32 = 0xFFFFFFFFULL;
    return static_cast<std::uint32_t>(value > max_u32 ? max_u32 : value);
}

void write_field(const h747::apps::player_md3::PlayerMd3SmokeField field) {
    h747::console::write(" ");
    h747::console::write(h747::apps::player_md3::smoke_field_token(field));
    h747::console::write("=");
}

} // namespace

namespace h747::apps::player_md3 {

void sample_render_surface() noexcept {
    auto& st = state();
    const auto raster = display_raster_state();
    if ((st.render_surface == 0U) || (raster.framebuffer_bytes < sizeof(std::uint32_t))) {
        st.render_sample0 = 0U;
        st.render_sample_center = 0U;
        st.render_sample_last = 0U;
        return;
    }

    const auto mode = st.panel.mode();
    const std::uint32_t center_offset =
        ((mode.extent.height / 2U) * mode.stride_bytes) +
        ((mode.extent.width / 2U) * 4U);
    st.render_sample0 = sample_argb8888(st.render_surface, 0U);
    st.render_sample_center =
        (center_offset + sizeof(std::uint32_t) <= raster.framebuffer_bytes)
            ? sample_argb8888(st.render_surface, center_offset)
            : 0U;
    st.render_sample_last =
        sample_argb8888(st.render_surface, raster.framebuffer_bytes - sizeof(std::uint32_t));
}

void sample_render_content_bounds() noexcept {
    auto& st = state();
    const auto mode = st.panel.mode();
    if ((st.render_surface == 0U) || mode.extent.width == 0U || mode.extent.height == 0U) {
        st.render_bg_pixel = 0U;
        st.render_non_bg_pixels = 0U;
        st.render_content_min_x = 0U;
        st.render_content_min_y = 0U;
        st.render_content_max_x = 0U;
        st.render_content_max_y = 0U;
        return;
    }

    const auto* pixels = reinterpret_cast<const std::byte*>(st.render_surface);
    const std::uint32_t bg = sample_argb8888(st.render_surface, 0U);
    std::uint32_t count = 0U;
    std::uint32_t min_x = mode.extent.width;
    std::uint32_t min_y = mode.extent.height;
    std::uint32_t max_x = 0U;
    std::uint32_t max_y = 0U;

    for (std::uint32_t y = 0U; y < mode.extent.height; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(pixels + (y * mode.stride_bytes));
        for (std::uint32_t x = 0U; x < mode.extent.width; ++x) {
            if (row[x] == bg) {
                continue;
            }
            ++count;
            if (x < min_x) {
                min_x = x;
            }
            if (y < min_y) {
                min_y = y;
            }
            if (x > max_x) {
                max_x = x;
            }
            if (y > max_y) {
                max_y = y;
            }
        }
    }

    st.render_bg_pixel = bg;
    st.render_non_bg_pixels = count;
    st.render_content_min_x = (count != 0U) ? min_x : 0U;
    st.render_content_min_y = (count != 0U) ? min_y : 0U;
    st.render_content_max_x = (count != 0U) ? max_x : 0U;
    st.render_content_max_y = (count != 0U) ? max_y : 0U;
}

void sample_scene_stats() noexcept {
    auto* shell = shell_ref();
    auto& st = state();
    if (shell == nullptr || shell->scene() == nullptr) {
        st.scene_cmd_count = 0U;
        st.scene_cmd_capacity = 0U;
        st.scene_cmd_overflowed = 0U;
        st.scene_text_used = 0U;
        st.scene_text_capacity = 0U;
        st.scene_text_overflowed = 0U;
        st.scene_exec_failed = 0U;
        st.scene_exec_cmd_text = 0U;
        st.scene_exec_cmd_rect = 0U;
        st.scene_exec_cmd_image = 0U;
        st.scene_exec_fail_text = 0U;
        st.scene_exec_fail_image = 0U;
        return;
    }

    const auto& scene = *shell->scene();
    const auto cmd = scene.last_cmd_stats();
    const auto exec = scene.last_exec_stats();
    st.scene_cmd_count = clamp_size_to_u32(cmd.cmd_count);
    st.scene_cmd_capacity = clamp_size_to_u32(cmd.cmd_capacity);
    st.scene_cmd_overflowed = cmd.cmd_overflowed ? 1U : 0U;
    st.scene_text_used = clamp_size_to_u32(cmd.text_used);
    st.scene_text_capacity = clamp_size_to_u32(cmd.text_capacity);
    st.scene_text_overflowed = cmd.text_overflowed ? 1U : 0U;
    st.scene_exec_failed = clamp_size_to_u32(exec.failed_cmds);
    st.scene_exec_cmd_text = clamp_size_to_u32(exec.cmd_text);
    st.scene_exec_cmd_rect = clamp_size_to_u32(exec.cmd_rect);
    st.scene_exec_cmd_image = clamp_size_to_u32(exec.cmd_image);
    st.scene_exec_fail_text = clamp_size_to_u32(exec.fail_text);
    st.scene_exec_fail_image = clamp_size_to_u32(exec.fail_image);
}

void update_smoke_verdict() noexcept {
    const auto raster = display_raster_state();
    auto& st = state();
    const std::uint32_t frame_delta = st.frames - st.smoke_last_frames;
    const std::uint32_t present_delta = raster.present_count - st.smoke_last_present;
    st.smoke_last_frames = st.frames;
    st.smoke_last_present = raster.present_count;
    st.smoke_frame_delta = frame_delta;
    st.smoke_present_delta = present_delta;
    st.smoke_mock_path = 0U;
    st.smoke_boot_ok = (st.display_ready && st.runtime_storage_ready && st.runtime_bootstrapped) ? 1U : 0U;
    st.smoke_render_ok = (st.last_render_ok && frame_delta > 0U) ? 1U : 0U;
    st.smoke_present_ok = (raster.present_ok != 0U && raster.present_count > 0U && present_delta > 0U) ? 1U : 0U;
    st.smoke_content_ok = (st.render_non_bg_pixels > 0U && st.render_content_max_x > st.render_content_min_x
                           && st.render_content_max_y > st.render_content_min_y)
        ? 1U
        : 0U;
    st.smoke_scene_ok = (st.scene_cmd_count > 0U && st.scene_exec_failed == 0U
                         && st.scene_text_overflowed == 0U && st.scene_cmd_overflowed == 0U)
        ? 1U
        : 0U;
    st.smoke_ok = (st.smoke_boot_ok && st.smoke_render_ok && st.smoke_present_ok
                   && st.smoke_content_ok && st.smoke_scene_ok && st.smoke_mock_path == 0U)
        ? 1U
        : 0U;
}

void print_status(const char* prefix) {
    const auto raster = display_raster_state();
    const auto memory = memory_probe_storage_state();
    const auto& st = state();
    h747::console::write(prefix);
    write_field(PlayerMd3SmokeField::RealMd3);
    h747::console::write_dec(1U);
    write_field(PlayerMd3SmokeField::Mock);
    h747::console::write_dec(st.smoke_mock_path);
    write_field(PlayerMd3SmokeField::Smoke);
    h747::console::write_dec(st.smoke_ok);
    h747::console::write("/");
    h747::console::write_dec(st.smoke_boot_ok);
    h747::console::write_dec(st.smoke_render_ok);
    h747::console::write_dec(st.smoke_present_ok);
    h747::console::write_dec(st.smoke_content_ok);
    h747::console::write_dec(st.smoke_scene_ok);
    write_field(PlayerMd3SmokeField::Delta);
    h747::console::write_dec(st.smoke_frame_delta);
    h747::console::write("/");
    h747::console::write_dec(st.smoke_present_delta);
    write_field(PlayerMd3SmokeField::Display);
    h747::console::write_dec(st.display_ready ? 1U : 0U);
    write_field(PlayerMd3SmokeField::Sdram1);
    h747::console::write_dec(memory.sdram1_ready);
    h747::console::write("/");
    h747::console::write_dec(memory.sdram1_smoke_ok);
    write_field(PlayerMd3SmokeField::RuntimeStorage);
    h747::console::write_dec(st.runtime_storage_ready ? 1U : 0U);
    write_field(PlayerMd3SmokeField::Boot);
    h747::console::write_dec(st.runtime_bootstrapped ? 1U : 0U);
    write_field(PlayerMd3SmokeField::Render);
    h747::console::write_dec(st.last_render_ok ? 1U : 0U);
    write_field(PlayerMd3SmokeField::Frames);
    h747::console::write_dec(st.frames);
    write_field(PlayerMd3SmokeField::Present);
    h747::console::write_dec(raster.present_count);
    write_field(PlayerMd3SmokeField::Layer);
    h747::console::write_dec(raster.ltdc_layer_ready);
    write_field(PlayerMd3SmokeField::Framebuffer);
    h747::console::write_hex32(raster.framebuffer_base);
    write_field(PlayerMd3SmokeField::Bytes);
    h747::console::write_hex32(raster.framebuffer_bytes);
    write_field(PlayerMd3SmokeField::RenderBuffer);
    h747::console::write_hex32(static_cast<std::uint32_t>(st.render_surface));
    write_field(PlayerMd3SmokeField::Platform);
    h747::console::write_hex32(static_cast<std::uint32_t>(st.platform_storage));
    write_field(PlayerMd3SmokeField::Runtime);
    h747::console::write_hex32(static_cast<std::uint32_t>(st.runtime_storage));
    write_field(PlayerMd3SmokeField::PoolBytes);
    h747::console::write_hex32(st.external_pool_bytes);
    write_field(PlayerMd3SmokeField::RenderSamples);
    h747::console::write_hex32(st.render_sample0);
    h747::console::write("/");
    h747::console::write_hex32(st.render_sample_center);
    h747::console::write("/");
    h747::console::write_hex32(st.render_sample_last);
    write_field(PlayerMd3SmokeField::CommandBuffer);
    h747::console::write_dec(st.scene_cmd_count);
    h747::console::write("/");
    h747::console::write_dec(st.scene_cmd_capacity);
    write_field(PlayerMd3SmokeField::CommandOverflow);
    h747::console::write_dec(st.scene_cmd_overflowed);
    write_field(PlayerMd3SmokeField::TextBuffer);
    h747::console::write_dec(st.scene_text_used);
    h747::console::write("/");
    h747::console::write_dec(st.scene_text_capacity);
    write_field(PlayerMd3SmokeField::TextOverflow);
    h747::console::write_dec(st.scene_text_overflowed);
    write_field(PlayerMd3SmokeField::ExecFailed);
    h747::console::write_dec(st.scene_exec_failed);
    write_field(PlayerMd3SmokeField::ExecCounts);
    h747::console::write_dec(st.scene_exec_cmd_rect);
    h747::console::write("/");
    h747::console::write_dec(st.scene_exec_cmd_text);
    h747::console::write("/");
    h747::console::write_dec(st.scene_exec_cmd_image);
    write_field(PlayerMd3SmokeField::FailTextImage);
    h747::console::write_dec(st.scene_exec_fail_text);
    h747::console::write("/");
    h747::console::write_dec(st.scene_exec_fail_image);
    write_field(PlayerMd3SmokeField::Input);
    h747::console::write_dec(st.input_polls);
    h747::console::write("/");
    h747::console::write_dec(st.input_events);
    write_field(PlayerMd3SmokeField::Touch);
    h747::console::write_dec(st.input_touch_probe_ok);
    h747::console::write("/");
    h747::console::write_dec(st.input_touch_ready);
    h747::console::write("/");
    h747::console::write_dec(st.input_touch_down);
    h747::console::write("@");
    h747::console::write_dec(st.input_last_x);
    h747::console::write(",");
    h747::console::write_dec(st.input_last_y);
    write_field(PlayerMd3SmokeField::Events);
    h747::console::write_dec(st.input_touch_events);
    h747::console::write("/");
    h747::console::write_dec(st.input_encoder_events);
    h747::console::write("/");
    h747::console::write_dec(st.input_button_events);
    write_field(PlayerMd3SmokeField::Content);
    h747::console::write_hex32(st.render_bg_pixel);
    h747::console::write(":");
    h747::console::write_dec(st.render_non_bg_pixels);
    h747::console::write("@");
    h747::console::write_dec(st.render_content_min_x);
    h747::console::write(",");
    h747::console::write_dec(st.render_content_min_y);
    h747::console::write("-");
    h747::console::write_dec(st.render_content_max_x);
    h747::console::write(",");
    h747::console::write_dec(st.render_content_max_y);
    write_field(PlayerMd3SmokeField::PresentSource);
    h747::console::write_hex32(raster.present_src_sample0);
    h747::console::write("/");
    h747::console::write_hex32(raster.present_src_sample_center);
    h747::console::write("/");
    h747::console::write_hex32(raster.present_src_sample_last);
    write_field(PlayerMd3SmokeField::PresentDestination);
    h747::console::write_hex32(raster.presented_sample0);
    h747::console::write("/");
    h747::console::write_hex32(raster.presented_sample_center);
    h747::console::write("/");
    h747::console::write_hex32(raster.presented_sample_last);
    write_field(PlayerMd3SmokeField::FrontBuffer);
    h747::console::write_hex32(raster.front_buffer_base);
    h747::console::write(":");
    h747::console::write_hex32(raster.front_sample0);
    h747::console::write("/");
    h747::console::write_hex32(raster.front_sample_center);
    h747::console::write("/");
    h747::console::write_hex32(raster.front_sample_last);
    write_field(PlayerMd3SmokeField::BackBuffer);
    h747::console::write_hex32(raster.back_buffer_base);
    h747::console::write(":");
    h747::console::write_hex32(raster.back_sample0);
    h747::console::write("/");
    h747::console::write_hex32(raster.back_sample_center);
    h747::console::write("/");
    h747::console::write_hex32(raster.back_sample_last);
    write_field(PlayerMd3SmokeField::LtdcFramebuffer);
    h747::console::write_hex32(raster.ltdc_layer_cfb_addr);
    write_field(PlayerMd3SmokeField::LtdcControl);
    h747::console::write_hex32(raster.ltdc_layer_cr);
    write_field(PlayerMd3SmokeField::LtdcPixelFormat);
    h747::console::write_hex32(raster.ltdc_layer_pfcr);
    h747::console::write("\n");
}

void maybe_print_loop_status() noexcept {
    auto& st = state();
    ++st.status_ticks;
    if ((st.status_ticks % 120U) == 0U) {
        print_status("player_md3.loop");
    }
}

} // namespace h747::apps::player_md3

extern "C" void charm_vivid_soa_unsupported_widget_kind(unsigned kind, unsigned caller) noexcept {
    h747::console::write("player_md3: unsupported widget kind=");
    h747::console::write_dec(kind);
    h747::console::write(" name=");
    h747::console::write(widget_kind_name(static_cast<WidgetKind>(kind)));
    h747::console::write(" caller=");
    h747::console::write_hex32(caller);
    h747::console::write("\n");
}
