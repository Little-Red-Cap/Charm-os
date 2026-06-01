#include <cstdint>

#include "console.h"
#include "display_raster.h"
#include "memory_probe.h"
#include "player_md3_diag.hpp"
#include "player_md3_smoke_schema.hpp"

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

    void sdec(const std::int32_t value) const {
        if (value < 0) {
            text("-");
            dec(static_cast<std::uint32_t>(-value));
            return;
        }
        dec(static_cast<std::uint32_t>(value));
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
    out.field(PlayerMd3SmokeField::Style);
    out.dec(st.style_rule_count);
    out.slash();
    out.dec(st.style_rule_capacity);
    out.slash();
    out.dec(st.style_metrics_used);
    out.slash();
    out.dec(st.style_metrics_capacity);
    out.slash();
    out.dec(st.style_metrics_overflowed);
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
    out.field(PlayerMd3SmokeField::CmdBatch);
    out.dec(st.scene_batch_shrink);
    out.slash();
    out.dec(st.scene_batch_shrink_rect);
    out.slash();
    out.dec(st.scene_batch_shrink_text_path);
    out.field(PlayerMd3SmokeField::ExecBatch);
    out.dec(st.scene_exec_dispatch_groups);
    out.slash();
    out.dec(st.scene_exec_batch_flushes);
    out.slash();
    out.dec(st.scene_exec_clip_failures);
    out.slash();
    out.dec(st.scene_exec_overflowed);
    out.field(PlayerMd3SmokeField::ExecGroups);
    out.dec(st.scene_exec_group_rect);
    out.slash();
    out.dec(st.scene_exec_group_text);
    out.slash();
    out.dec(st.scene_exec_group_image);
    out.slash();
    out.dec(st.scene_exec_group_other);
    out.field(PlayerMd3SmokeField::ExecCmdKinds);
    out.dec(st.scene_exec_cmd_rect);
    out.slash();
    out.dec(st.scene_exec_cmd_text);
    out.slash();
    out.dec(st.scene_exec_cmd_image);
    out.slash();
    out.dec(st.scene_exec_cmd_line);
    out.slash();
    out.dec(st.scene_exec_cmd_path);
    out.slash();
    out.dec(st.scene_exec_cmd_other);
    out.field(PlayerMd3SmokeField::ExecFailDetail);
    out.dec(st.scene_exec_fail_text);
    out.slash();
    out.dec(st.scene_exec_fail_image);
    out.slash();
    out.dec(st.scene_exec_fail_other);
    out.field(PlayerMd3SmokeField::PerfTime);
    out.dec(st.perf_time_available);
    out.slash();
    out.dec(st.perf_time_frame_us);
    out.slash();
    out.dec(st.perf_time_tick_us);
    out.slash();
    out.dec(st.perf_time_render_us);
    out.slash();
    out.dec(st.perf_time_record_us);
    out.slash();
    out.dec(st.perf_time_execute_us);
    out.slash();
    out.dec(st.perf_time_present_us);
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
    out.field(PlayerMd3SmokeField::InputRoute);
    out.dec(st.input_route_console_commands);
    out.slash();
    out.dec(st.input_route_touch_pointers);
    out.slash();
    out.dec(st.input_route_encoder_commands);
    out.slash();
    out.dec(st.input_route_button_commands);
    out.at();
    out.dec(st.input_route_last_source);
    out.slash();
    out.dec(st.input_route_last_kind);
    out.slash();
    out.dec(st.input_route_last_code);
    out.field(PlayerMd3SmokeField::InputSmoke);
    out.dec(st.input_smoke_ok);
    out.slash();
    out.dec(st.input_smoke_cmds);
    out.slash();
    out.dec(st.input_smoke_before_events);
    out.dash();
    out.dec(st.input_smoke_after_events);
    out.slash();
    out.dec(st.input_smoke_frames);
    out.slash();
    out.dec(st.input_smoke_exec_fail);
    out.field(PlayerMd3SmokeField::Storage);
    out.dec(st.storage_ready);
    out.slash();
    out.dec(st.storage_fat_probe_ok);
    out.slash();
    out.dec(st.storage_reads);
    out.slash();
    out.dec(st.storage_read_fails);
    out.at();
    out.dec(st.storage_part_lba);
    out.colon();
    out.dec(st.storage_blocks);
    out.field(PlayerMd3SmokeField::StorageDetail);
    out.dec(st.storage_attempted);
    out.slash();
    out.dec(st.storage_initialized);
    out.slash();
    out.dec(st.storage_block_device_ready);
    out.slash();
    out.dec(st.storage_partition_auto);
    out.at();
    out.dec(st.storage_block_size);
    out.colon();
    out.dec(st.storage_init_status);
    out.slash();
    out.dec(st.storage_last_hal_status);
    out.slash();
    out.hex32(st.storage_last_error);
    out.slash();
    out.dec(st.storage_card_state);
    out.slash();
    out.dec(st.storage_wait_timeouts);
    out.slash();
    out.dec(st.storage_last_lba);
    out.slash();
    out.dec(st.storage_last_count);
    out.slash();
    out.hex32(st.storage_sta);
    out.field(PlayerMd3SmokeField::StorageBus);
    out.dec(st.storage_selected_bus_width);
    out.slash();
    out.dec(st.storage_wide_status_8);
    out.slash();
    out.dec(st.storage_wide_status_4);
    out.slash();
    out.dec(st.storage_wide_status_1);
    out.field(PlayerMd3SmokeField::Audio);
    out.dec(st.audio_ready);
    out.slash();
    out.dec(st.audio_dma_ready);
    out.slash();
    out.dec(st.audio_i2s_status);
    out.slash();
    out.dec(st.audio_dma_status);
    out.field(PlayerMd3SmokeField::Fs);
    out.dec(st.fs_mount_ok);
    out.slash();
    out.dec(st.fs_track_count);
    out.slash();
    out.dec(st.fs_has_tracks);
    out.slash();
    out.sdec(st.fs_mount_err);
    out.field(PlayerMd3SmokeField::Font);
    out.dec(st.font_primary_open);
    out.slash();
    out.dec(st.font_fallback_open);
    out.slash();
    out.dec(st.font_cache_ready);
    out.slash();
    out.sdec(st.font_err);
    out.field(PlayerMd3SmokeField::Cover);
    out.dec(st.cover_folder_found);
    out.slash();
    out.dec(st.cover_decode_ok);
    out.slash();
    out.dec(st.cover_width);
    out.text("x");
    out.dec(st.cover_height);
    out.slash();
    out.sdec(st.cover_err);
    out.field(PlayerMd3SmokeField::Media);
    out.dec(st.media_first_open);
    out.slash();
    out.dec(st.media_duration_ok);
    out.slash();
    out.dec(st.media_track_ready);
    out.slash();
    out.sdec(st.media_err);
    out.field(PlayerMd3SmokeField::Playback);
    out.dec(st.playback_player_state);
    out.slash();
    out.dec(st.playback_running);
    out.slash();
    out.dec(st.playback_track_ready);
    out.slash();
    out.dec(st.playback_dma_callbacks);
    out.slash();
    out.dec(st.playback_underruns);
    out.slash();
    out.sdec(st.playback_last_error_stage);
    out.slash();
    out.sdec(st.playback_last_error);
    out.field(PlayerMd3SmokeField::PlaybackSmoke);
    out.dec(st.playback_smoke_ok);
    out.slash();
    out.dec(st.playback_smoke_before_callbacks);
    out.dash();
    out.dec(st.playback_smoke_after_callbacks);
    out.slash();
    out.dec(st.playback_smoke_frames);
    out.slash();
    out.dec(st.playback_smoke_saw_playing);
    out.slash();
    out.sdec(st.playback_smoke_error_stage);
    out.slash();
    out.sdec(st.playback_smoke_error);
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
