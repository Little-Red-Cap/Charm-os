import audio.player;
import audio.result;
import player.app;
import player.controller;
import player.fs_utils;
import player.platform;
import player.storage;
import player.playback;
import player.ui_builder;
import player.ui;
import charm.core.config;
import charm.core.event;
import charm.ui.scene;
import ui.input_adapter;
import charm.gfx.color;
import charm.gfx.text_box;
import charm.gfx.image;
import charm.gfx.snapshot;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import charm.system.clock;
import charm.system.run_loop;
import util.core;
import input.raw_event;
import platform.win.time_source;

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <SDL3/SDL.h>
#if defined(_WIN32)
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace player::fs_utils;
    using namespace player::ui;

    static charm::system::ClockTick now_us(void*) noexcept {
        return platform::win::SteadyClock::now();
    }

    static player::PlayerPlatform g_platform{};
    static audio::PlayerConfig g_player_cfg{};
    static charm::system::Clock g_clock{nullptr, {.now_us = &now_us}};
    static std::optional<player::App> g_app{};

    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;

    static PlayerUiContext g_ctx{};
#include "main.overlay_fx.inc"

#include "main.font_probe.inc"

    struct PlayerLoopState {
        player::App* app{nullptr};
        player::PlayerPlatform* platform{nullptr};
        PlayerUiContext* ctx{nullptr};
        ::ui::scene::Scene* scene{nullptr};
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
        bool* running{nullptr};
        int* win_w{nullptr};
        int* win_h{nullptr};
        float t_sec{0.0f};
        std::string screenshot_path{};
        std::string screenshot_gif_path{};
        player::PlayerPage screenshot_page{player::PlayerPage::Library};
        int home_scroll_y{-1};
        bool screenshot_verbose{false};
        int screenshot_wait_frames{0};
        bool screenshot_exit{false};
    };

#include "main.screenshot.inc"

    struct UiCiResult {
        bool ok{true};
        int failed{0};
    };

    constexpr std::size_t kUiCmdBudget = 1200;
    constexpr std::uint64_t kUiAlphaBlendBudget = 1000000;
    constexpr std::uint32_t kUiLayerBytesBudget = 0;

    void ui_ci_emit(const char* name, bool ok, const char* reason) {
        if (ok) {
            std::printf("[ui-ci] case=%s ok=1\n", name);
        } else {
            std::printf("[ui-ci] case=%s ok=0 reason=%s\n", name, reason ? reason : "unknown");
        }
    }

    void ui_ci_click(player::App& app, PlayerUiContext& ctx, ::ui::scene::Scene& scene, int x, int y) {
        input::RawInputEvent down{};
        down.type = input::RawInputEventType::Pointer;
        down.ms = 0;
        down.pointer = input::PointerRaw{true, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0};
        down.pointer_action = input::PointerAction::Down;
        app.dispatch_raw_input(scene, ctx, down);

        input::RawInputEvent up{};
        up.type = input::RawInputEventType::Pointer;
        up.ms = 0;
        up.pointer = input::PointerRaw{false, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0};
        up.pointer_action = input::PointerAction::Up;
        app.dispatch_raw_input(scene, ctx, up);
    }

    UiCiResult run_ui_ci(player::App& app, PlayerUiContext& ctx, player::PlayerPlatform& platform) {
        UiCiResult res{};
        auto& scene = platform.scene_ref();

        auto pump_frame = [&]() {
            app.tick();
            ctx.tick_player(app.player());
            platform.framebuffer_ref().clear(kUiBackground);
            platform.begin_frame();
            if (ctx.transition_needs_destination_snapshot()) {
                ctx.prepare_transition_destination_snapshot_scene();
                platform.render();
                ctx.finish_transition_destination_snapshot_capture();
                platform.framebuffer_ref().clear(kUiBackground);
                platform.begin_frame();
            }
            ctx.compose_now_playing_transition_pixel_layer();
            platform.render();
            platform.end_frame();
        };
        auto wait_for_page = [&](player::PlayerPage expected_page, int max_frames = 36) {
            if (ctx.current_page == expected_page) return true;
            for (int i = 0; i < max_frames; ++i) {
                SDL_Delay(16);
                pump_frame();
                if (ctx.current_page == expected_page) {
                    return true;
                }
            }
            return ctx.current_page == expected_page;
        };
        auto settle_now_playing_transition = [&](player::PlayerPage expected_page) {
            if (!ctx.now_playing_transition.active) {
                return ctx.current_page == expected_page;
            }
            ctx.finish_now_playing_transition();
            pump_frame();
            return ctx.current_page == expected_page;
        };

        pump_frame();

        {
            const auto cmd_stats = scene.last_cmd_stats();
            const auto exec_stats = scene.last_exec_stats();
            const auto layer_stats = scene.layer_stats();
            if (cmd_stats.cmd_count > kUiCmdBudget) {
                ui_ci_emit("frame_budget_cmd", false, "cmd_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_cmd", true, nullptr);
            }
            if (exec_stats.alpha_blend_count > kUiAlphaBlendBudget) {
                ui_ci_emit("frame_budget_alpha", false, "alpha_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_alpha", true, nullptr);
            }
            if (layer_stats.layer_bytes > kUiLayerBytesBudget) {
                ui_ci_emit("frame_budget_layer", false, "layer_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_layer", true, nullptr);
            }
        }
        {
            const auto epoch = scene.current_layer_epoch();
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 16, 16},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.reserve_snapshot(spec);
            const auto* record = scene.snapshot_record(handle);
            const bool reserved = static_cast<bool>(handle)
                && record
                && record->epoch == epoch
                && scene.update_command_snapshot(handle)
                && scene.mark_snapshot_stale(handle)
                && scene.refresh_snapshot_epoch(handle)
                && scene.release_snapshot(handle)
                && !scene.snapshot_record(handle);
            if (reserved && scene.layer_stats().snapshot_count == 0) {
                ui_ci_emit("layer_snapshot_lifecycle", true, nullptr);
            } else {
                ui_ci_emit("layer_snapshot_lifecycle", false, "snapshot_lifecycle");
                res.ok = false;
                res.failed++;
            }
        }
        {
            auto& layer = ctx.page_home_layer;
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 24, 24},
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto freeze = layer.freeze(scene, scene.access(), spec, true);
            const bool frozen = freeze.ok()
                && freeze.handle
                && layer.snapshot() == freeze.handle
                && layer.state() == ::ui::scene::PageLayerState::Frozen
                && !layer.visible();
            layer.mark_transitioning();
            const bool transitioning = frozen
                && layer.state() == ::ui::scene::PageLayerState::Transitioning;
            layer.thaw(scene, scene.access(), true);
            const bool thawed = transitioning
                && layer.live()
                && layer.visible()
                && !layer.snapshot()
                && scene.layer_stats().snapshot_count == 0;
            if (thawed) {
                ui_ci_emit("page_layer_freeze_thaw", true, nullptr);
            } else {
                ui_ci_emit("page_layer_freeze_thaw", false, "page_layer_freeze_thaw");
                if (layer.snapshot()) {
                    (void)layer.release_snapshot(scene);
                }
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto capture = scene.capture_command_snapshot_result(spec);
            const auto handle = capture.handle;
            const auto* record = scene.snapshot_record(handle);
            const bool captured = capture.ok()
                && static_cast<bool>(handle)
                && record
                && record->kind == ::ui::scene::SnapshotKind::CommandBuffer
                && record->command_count > 0
                && record->bytes > 0
                && scene.layer_stats().snapshot_count == 1
                && scene.release_snapshot(handle)
                && scene.layer_stats().snapshot_count == 0;
            if (captured) {
                ui_ci_emit("layer_command_snapshot_capture", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_capture", false, "command_snapshot_capture");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            std::array<::ui::scene::SnapshotHandle, static_cast<std::size_t>(layer_cache_slots)> handles{};
            bool filled = true;
            for (auto& handle : handles) {
                const auto capture = scene.capture_command_snapshot_result(spec);
                if (!capture.ok() || !capture.handle) {
                    filled = false;
                    break;
                }
                handle = capture.handle;
            }
            const auto extra = scene.capture_command_snapshot_result(spec);
            bool released_all = true;
            for (auto handle : handles) {
                if (handle && !scene.release_snapshot(handle)) {
                    released_all = false;
                }
            }
            const bool exhausted = filled
                && extra.status == ::ui::scene::LayerCaptureStatus::NoSnapshotSlot
                && !extra.handle
                && released_all;
            if (exhausted) {
                ui_ci_emit("layer_command_snapshot_capture_full", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_capture_full", false, "command_snapshot_capture_full");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect root_rect = scene.world_rect(ctx.handles.root);
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 16, 16},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            bool stale_checked = static_cast<bool>(handle) && scene.snapshot_current(handle);
            if (ctx.handles.root) {
                scene.access().set_rect(ctx.handles.root, root_rect);
            }
            stale_checked = stale_checked
                && !scene.validate_snapshot(handle)
                && scene.snapshot_record(handle)
                && scene.snapshot_record(handle)->stale
                && scene.release_snapshot(handle);
            if (stale_checked) {
                ui_ci_emit("layer_snapshot_stale_epoch", true, nullptr);
            } else {
                ui_ci_emit("layer_snapshot_stale_epoch", false, "stale_epoch");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{4, 8, 20, 10},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto result = scene.compose_snapshot_dry_run({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{.x = 6, .y = 2, .opacity = 255},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto stats = scene.layer_stats();
            const bool composed = result.ok
                && result.kind == ::ui::scene::SnapshotKind::CommandBuffer
                && result.target_bounds.x == 10
                && result.target_bounds.y == 10
                && result.composite_pixels == 200
                && stats.composite_pixels >= result.composite_pixels
                && scene.release_snapshot(handle);
            if (composed) {
                ui_ci_emit("layer_compose_dry_run", true, nullptr);
            } else {
                ui_ci_emit("layer_compose_dry_run", false, "compose_dry_run");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{4, 8, 20, 10},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{.x = -10, .y = -3, .opacity = 192},
                .clip = Rect{0, 0, 12, 8},
                .has_clip = true,
            });
            const bool planned = plan.valid
                && plan.source_visible.x == 10
                && plan.source_visible.y == 8
                && plan.source_visible.w == 12
                && plan.source_visible.h == 3
                && plan.target_bounds.x == 0
                && plan.target_bounds.y == 5
                && plan.target_bounds.w == 12
                && plan.target_bounds.h == 3
                && plan.composite_pixels == 36;
            const bool released = scene.release_snapshot(handle);
            const bool ok = planned && released;
            if (ok) {
                ui_ci_emit("layer_compose_plan_clip", true, nullptr);
            } else {
                ui_ci_emit("layer_compose_plan_clip", false, "compose_plan_clip");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, 40, 20},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{},
                .clip = Rect{0, 0, 40, 20},
                .has_clip = true,
            });
            const auto generous = scene.check_layer_budget(plan, {
                .max_layer_bytes = 2000000,
                .max_composite_pixels = 1000,
                .max_command_count = 2000,
            });
            const auto strict = scene.check_layer_budget(plan, {
                .max_layer_bytes = 1,
                .max_composite_pixels = 1,
                .max_command_count = 1,
            });
            const bool budget_ok = plan.valid
                && generous.ok
                && !generous.layer_bytes_over
                && !generous.composite_pixels_over
                && !generous.command_count_over
                && !strict.ok
                && strict.layer_bytes_over
                && strict.composite_pixels_over
                && strict.command_count_over;
            const bool released = scene.release_snapshot(handle);
            if (budget_ok && released) {
                ui_ci_emit("layer_compose_budget", true, nullptr);
            } else {
                ui_ci_emit("layer_compose_budget", false, "compose_budget");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto replay = scene.replay_command_snapshot(plan);
            const bool replayed = plan.valid
                && replay.ok()
                && replay.stats.cmd_count > 0
                && replay.stats.cmd_bytes > 0
                && replay.stats.failed_cmds == 0;
            const bool released = scene.release_snapshot(handle);
            if (replayed && released) {
                ui_ci_emit("layer_command_snapshot_replay", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_replay", false, "command_snapshot_replay");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect root_rect = scene.world_rect(ctx.handles.root);
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto handle = scene.capture_command_snapshot(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = handle,
                .transform = ::ui::scene::LayerTransform{},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            if (ctx.handles.root) {
                scene.access().set_rect(ctx.handles.root, root_rect);
            }
            const auto replay = scene.replay_command_snapshot(plan);
            const auto* record = scene.snapshot_record(handle);
            const bool stale_rejected = plan.valid
                && replay.status == ::ui::scene::LayerReplayStatus::StaleSnapshot
                && record
                && record->stale;
            const bool released = scene.release_snapshot(handle);
            if (stale_rejected && released) {
                ui_ci_emit("layer_command_snapshot_replay_stale", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_replay_stale", false, "command_snapshot_replay_stale");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto first = scene.capture_command_snapshot(spec);
            const auto* first_record = scene.snapshot_record(first);
            const auto first_payload = first_record
                ? first_record->payload_slot
                : ::ui::scene::kInvalidSnapshotPayloadSlot;
            const bool first_released = scene.release_snapshot(first);
            const auto second = scene.capture_command_snapshot(spec);
            const auto* second_record = scene.snapshot_record(second);
            const auto second_payload = second_record
                ? second_record->payload_slot
                : ::ui::scene::kInvalidSnapshotPayloadSlot;
            const bool reused = first_released
                && first_payload != ::ui::scene::kInvalidSnapshotPayloadSlot
                && second_payload == first_payload
                && scene.release_snapshot(second);
            if (reused) {
                ui_ci_emit("layer_command_snapshot_payload_reuse", true, nullptr);
            } else {
                ui_ci_emit("layer_command_snapshot_payload_reuse", false, "command_snapshot_payload_reuse");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect pixel_bounds{0, 0, 48, 32};
            const ::ui::scene::SnapshotSpec spec{
                .bounds = pixel_bounds,
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto capture = scene.capture_pixel_snapshot_result(spec);
            const auto* record = scene.snapshot_record(capture.handle);
            const bool captured = capture.ok()
                && record
                && record->kind == ::ui::scene::SnapshotKind::PixelSurface
                && record->bytes == ::ui::scene::snapshot_pixel_bytes(
                    ::screen_pixel_format, pixel_bounds.w, pixel_bounds.h);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = capture.handle,
                .transform = ::ui::scene::LayerTransform{.x = 8, .y = 6, .opacity = 255},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto before = scene.layer_stats();
            const auto replay = scene.compose_pixel_snapshot(plan);
            const auto after = scene.layer_stats();
            const bool composed = plan.valid
                && replay.ok()
                && replay.kind == ::ui::scene::SnapshotKind::PixelSurface
                && after.pixel_blit_count > before.pixel_blit_count
                && after.pixel_blit_pixels >= before.pixel_blit_pixels + plan.composite_pixels;
            const bool released = scene.release_snapshot(capture.handle);
            if (captured && composed && released) {
                ui_ci_emit("layer_pixel_snapshot_capture_compose", true, nullptr);
            } else {
                ui_ci_emit("layer_pixel_snapshot_capture_compose", false, "pixel_snapshot_capture_compose");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const Rect pixel_bounds{0, 0, 24, 16};
            const ::ui::scene::SnapshotSpec spec{
                .bounds = pixel_bounds,
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto capture = scene.capture_pixel_snapshot_result(spec);
            const auto plan = scene.make_snapshot_compose_plan({
                .source = capture.handle,
                .transform = ::ui::scene::LayerTransform{.x = 3, .y = 4, .opacity = 128},
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            const auto before = scene.layer_stats();
            const auto replay = scene.compose_pixel_snapshot(plan);
            const auto after = scene.layer_stats();
            const bool composed = capture.ok()
                && plan.valid
                && replay.ok()
                && replay.stats.alpha_blend_count == plan.composite_pixels
                && after.pixel_blit_count > before.pixel_blit_count
                && after.pixel_blit_pixels >= before.pixel_blit_pixels + plan.composite_pixels;
            const bool released = scene.release_snapshot(capture.handle);
            if (composed && released) {
                ui_ci_emit("layer_pixel_snapshot_opacity_compose", true, nullptr);
            } else {
                ui_ci_emit("layer_pixel_snapshot_opacity_compose", false, "pixel_snapshot_opacity_compose");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::LayerBudgetResult budget_over{
                .ok = false,
                .layer_bytes_over = true,
            };
            const auto decision = ::ui::scene::decide_layer_profile(
                ::ui::scene::LayerProfile::Rich, budget_over);
            const ::ui::scene::LayerBudgetResult composite_over{
                .ok = false,
                .composite_pixels_over = true,
            };
            const auto composite_decision = ::ui::scene::decide_layer_profile(
                ::ui::scene::LayerProfile::Cheap, composite_over);
            const auto pixel_bytes = static_cast<std::uint32_t>(
                ::ui::scene::snapshot_pixel_bytes(screen_pixel_format, screen_width, screen_height));
            const auto rich_admission = ::ui::scene::decide_layer_admission({
                .profile = ::ui::scene::LayerProfile::Rich,
                .budget = ::ui::scene::LayerBudget{
                    .max_layer_bytes = pixel_bytes * 2u,
                    .max_composite_pixels = 0,
                    .max_command_count = 0,
                },
                .pixel_snapshot_bytes = pixel_bytes,
                .cache_slots = layer_cache_slots,
                .need_double_snapshot = true,
            });
            const auto low_budget_admission = ::ui::scene::decide_layer_admission({
                .profile = ::ui::scene::LayerProfile::Cheap,
                .budget = ::ui::scene::LayerBudget{
                    .max_layer_bytes = 1u,
                    .max_composite_pixels = 0,
                    .max_command_count = 0,
                },
                .pixel_snapshot_bytes = pixel_bytes,
                .cache_slots = layer_cache_slots,
                .need_double_snapshot = true,
            });
            const auto static_admission = ::ui::scene::decide_layer_admission({
                .profile = ::ui::scene::LayerProfile::Static,
                .budget = {},
                .pixel_snapshot_bytes = pixel_bytes,
                .cache_slots = layer_cache_slots,
                .need_double_snapshot = true,
            });
            const bool ok = ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Rich, 128) == 128
                && ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Cheap, 128) == 170
                && ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Static, 127) == 0
                && ::ui::scene::resolve_layer_opacity(
                    ::ui::scene::LayerProfile::Static, 128) == 255
                && decision.requested == ::ui::scene::LayerProfile::Rich
                && decision.effective == ::ui::scene::LayerProfile::Cheap
                && decision.reason == ::ui::scene::LayerFallbackReason::LayerBytesOver
                && composite_decision.requested == ::ui::scene::LayerProfile::Cheap
                && composite_decision.effective == ::ui::scene::LayerProfile::Static
                && composite_decision.reason == ::ui::scene::LayerFallbackReason::CompositePixelsOver
                && rich_admission == ::ui::scene::LayerAdmission::PixelDouble
                && low_budget_admission == ::ui::scene::LayerAdmission::CommandSnapshot
                && static_admission == ::ui::scene::LayerAdmission::StaticCut;
            if (ok) {
                ui_ci_emit("layer_profile_decision", true, nullptr);
            } else {
                ui_ci_emit("layer_profile_decision", false, "layer_profile_decision");
                res.ok = false;
                res.failed++;
            }
        }
        {
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
            const auto capture = scene.capture_pixel_snapshot_result(spec);
            const bool cache_can_fit_fullscreen =
                layer_cache_width >= screen_width && layer_cache_height >= screen_height;
            const bool ok = cache_can_fit_fullscreen
                ? (capture.ok() && capture.handle && scene.release_snapshot(capture.handle))
                : (capture.status == ::ui::scene::LayerCaptureStatus::StoreFailed
                    && !capture.handle);
            if (ok) {
                ui_ci_emit("layer_pixel_snapshot_fullscreen_profile", true, nullptr);
            } else {
                ui_ci_emit("layer_pixel_snapshot_fullscreen_profile", false, "pixel_snapshot_fullscreen_profile");
                if (capture.handle) {
                    (void)scene.release_snapshot(capture.handle);
                }
                res.ok = false;
                res.failed++;
            }
        }

        auto click_handle = [&](WidgetHandle h, const char* case_name) -> bool {
            if (!h) {
                ui_ci_emit(case_name, false, "invalid_handle");
                res.ok = false;
                res.failed++;
                return false;
            }
            const Rect r = scene.world_rect(h);
            if (r.w <= 0 || r.h <= 0) {
                ui_ci_emit(case_name, false, "zero_rect");
                res.ok = false;
                res.failed++;
                return false;
            }
            const int cx = r.x + r.w / 2;
            const int cy = r.y + r.h / 2;
            ui_ci_click(app, ctx, scene, cx, cy);
            return true;
        };

        ctx.set_page(player::PlayerPage::Library);
        pump_frame();
        if (click_handle(ctx.handles.nav_home, "library_to_home")) {
            if (wait_for_page(player::PlayerPage::Home)) {
                ui_ci_emit("library_to_home", true, nullptr);
            } else {
                ui_ci_emit("library_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
        const std::uint32_t captures_before_now = ctx.layer_transition_capture_count;
        const std::uint32_t releases_before_now = ctx.layer_transition_release_count;
        const std::uint32_t capture_fails_before_now = ctx.layer_transition_capture_fail_count;
        const std::uint32_t composes_before_now = ctx.layer_transition_compose_count;
        const std::uint32_t compose_pixels_before_now = ctx.layer_transition_composite_pixels;
        const std::uint32_t pixel_composes_before_now = ctx.layer_transition_pixel_compose_count;
        const std::uint32_t pixel_compose_pixels_before_now = ctx.layer_transition_pixel_compose_pixels;
        const std::uint32_t destination_captures_before_now = ctx.layer_transition_destination_capture_count;
        const std::uint32_t destination_composes_before_now = ctx.layer_transition_destination_compose_count;
        const std::uint32_t destination_compose_pixels_before_now =
            ctx.layer_transition_destination_compose_pixels;
        if (click_handle(ctx.handles.bottom_hit, "home_to_now")) {
            if (wait_for_page(player::PlayerPage::NowPlaying)
                || settle_now_playing_transition(player::PlayerPage::NowPlaying)) {
                ui_ci_emit("home_to_now", true, nullptr);
            } else {
                ui_ci_emit("home_to_now", false, "page_not_now");
                res.ok = false;
                res.failed++;
            }
        }
        if (ctx.layer_transition_capture_count > captures_before_now
            && ctx.layer_transition_release_count > releases_before_now
            && ctx.layer_transition_capture_fail_count == capture_fails_before_now
            && ctx.last_layer_transition_capture_status == ::ui::scene::LayerCaptureStatus::Ok) {
            ui_ci_emit("layer_transition_home_to_now_snapshot", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_home_to_now_snapshot", false, "transition_snapshot");
            res.ok = false;
            res.failed++;
        }
        if (ctx.layer_transition_compose_count > composes_before_now
            && ctx.layer_transition_composite_pixels > compose_pixels_before_now
            && ctx.layer_transition_pixel_compose_count > pixel_composes_before_now
            && ctx.layer_transition_pixel_compose_pixels > pixel_compose_pixels_before_now
            && ctx.layer_transition_destination_capture_count > destination_captures_before_now
            && ctx.layer_transition_destination_compose_count > destination_composes_before_now
            && ctx.layer_transition_destination_compose_pixels > destination_compose_pixels_before_now
            && ctx.effective_layer_profile == ::ui::scene::LayerProfile::Rich
            && ctx.layer_transition_fallback_reason == ::ui::scene::LayerFallbackReason::None
            && ctx.layer_transition_last_budget_ok
            && ctx.layer_transition_last_layer_bytes > 0
            && ctx.layer_transition_last_layer_bytes_budget > 0
            && ctx.layer_transition_last_composite_pixels > 0
            && ctx.layer_transition_last_composite_pixels_budget > 0) {
            ui_ci_emit("layer_transition_home_to_now_compose", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_home_to_now_compose", false, "transition_compose");
            res.ok = false;
            res.failed++;
        }

        pump_frame();
        const std::uint32_t captures_before_back = ctx.layer_transition_capture_count;
        const std::uint32_t releases_before_back = ctx.layer_transition_release_count;
        const std::uint32_t capture_fails_before_back = ctx.layer_transition_capture_fail_count;
        const std::uint32_t composes_before_back = ctx.layer_transition_compose_count;
        const std::uint32_t compose_pixels_before_back = ctx.layer_transition_composite_pixels;
        const std::uint32_t pixel_composes_before_back = ctx.layer_transition_pixel_compose_count;
        const std::uint32_t pixel_compose_pixels_before_back = ctx.layer_transition_pixel_compose_pixels;
        const std::uint32_t destination_captures_before_back = ctx.layer_transition_destination_capture_count;
        const std::uint32_t destination_composes_before_back = ctx.layer_transition_destination_compose_count;
        const std::uint32_t destination_compose_pixels_before_back =
            ctx.layer_transition_destination_compose_pixels;
        if (click_handle(ctx.handles.now_back, "now_back_to_home")) {
            if (wait_for_page(player::PlayerPage::Home)
                || settle_now_playing_transition(player::PlayerPage::Home)) {
                ui_ci_emit("now_back_to_home", true, nullptr);
            } else {
                ui_ci_emit("now_back_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }
        if (ctx.layer_transition_capture_count > captures_before_back
            && ctx.layer_transition_release_count > releases_before_back
            && ctx.layer_transition_capture_fail_count == capture_fails_before_back
            && ctx.last_layer_transition_capture_status == ::ui::scene::LayerCaptureStatus::Ok) {
            ui_ci_emit("layer_transition_now_to_home_snapshot", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_now_to_home_snapshot", false, "transition_snapshot");
            res.ok = false;
            res.failed++;
        }
        if (ctx.layer_transition_compose_count > composes_before_back
            && ctx.layer_transition_composite_pixels > compose_pixels_before_back
            && ctx.layer_transition_pixel_compose_count > pixel_composes_before_back
            && ctx.layer_transition_pixel_compose_pixels > pixel_compose_pixels_before_back
            && ctx.layer_transition_destination_capture_count > destination_captures_before_back
            && ctx.layer_transition_destination_compose_count > destination_composes_before_back
            && ctx.layer_transition_destination_compose_pixels > destination_compose_pixels_before_back) {
            ui_ci_emit("layer_transition_now_to_home_compose", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_now_to_home_compose", false, "transition_compose");
            res.ok = false;
            res.failed++;
        }

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
        const auto profile_before_drill = ctx.requested_layer_profile;
        const bool drill_before = ctx.layer_profile_budget_drill_enabled;
        const std::uint32_t static_cuts_before = ctx.layer_static_cut_count;
        const std::uint32_t admission_static_cuts_before = ctx.layer_admission_static_cut_count;
        const std::uint32_t budget_fails_before = ctx.layer_transition_budget_fail_count;
        const std::uint32_t captures_before_admission_drill = ctx.layer_transition_capture_count;
        const std::uint32_t destination_captures_before_admission_drill =
            ctx.layer_transition_destination_capture_count;
        ctx.requested_layer_profile = ::ui::scene::LayerProfile::Cheap;
        ctx.layer_profile_budget_drill_enabled = true;
        if (click_handle(ctx.handles.bottom_hit, "home_to_now_static_cut")) {
            if (wait_for_page(player::PlayerPage::NowPlaying)
                || settle_now_playing_transition(player::PlayerPage::NowPlaying)) {
                ui_ci_emit("home_to_now_static_cut", true, nullptr);
            } else {
                ui_ci_emit("home_to_now_static_cut", false, "page_not_now");
                res.ok = false;
                res.failed++;
            }
        }
        const bool static_cut = ctx.current_page == player::PlayerPage::NowPlaying
            && !ctx.now_playing_transition.active
            && ctx.page_transition_state == player::PlayerController::PageTransitionState::Idle
            && ctx.effective_layer_profile == ::ui::scene::LayerProfile::Static
            && ctx.layer_transition_fallback_reason != ::ui::scene::LayerFallbackReason::None
            && !ctx.layer_transition_last_budget_ok
            && ctx.last_layer_admission == ::ui::scene::LayerAdmission::CommandSnapshot
            && ctx.layer_static_cut_count > static_cuts_before
            && ctx.layer_admission_static_cut_count > admission_static_cuts_before
            && ctx.layer_transition_budget_fail_count > budget_fails_before
            && ctx.layer_transition_capture_count == captures_before_admission_drill
            && ctx.layer_transition_destination_capture_count == destination_captures_before_admission_drill
            && scene.layer_stats().snapshot_count == 0;
        if (static_cut) {
            ui_ci_emit("layer_transition_static_cut_drill", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_static_cut_drill", false, "static_cut_drill");
            res.ok = false;
            res.failed++;
        }
        ctx.requested_layer_profile = profile_before_drill;
        ctx.layer_profile_budget_drill_enabled = drill_before;
        ctx.set_page(player::PlayerPage::Home);
        pump_frame();

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
        const std::uint32_t aborts_before_interrupt = ctx.layer_transition_abort_count;
        const std::uint32_t releases_before_interrupt = ctx.layer_transition_release_count;
        const auto layer_count_before_interrupt = scene.layer_stats().snapshot_count;
        bool interrupt_started = false;
        if (ctx.begin_now_playing_expand_transition()) {
            interrupt_started = ctx.now_playing_transition.active
                && ctx.now_playing_transition.source_snapshot
                && scene.layer_stats().snapshot_count > layer_count_before_interrupt;
            ctx.set_page(player::PlayerPage::Library);
            pump_frame();
        }
        const bool interrupted = interrupt_started
            && !ctx.now_playing_transition.active
            && ctx.page_transition_state == player::PlayerController::PageTransitionState::Idle
            && ctx.layer_transition_abort_count > aborts_before_interrupt
            && ctx.layer_transition_release_count > releases_before_interrupt
            && scene.layer_stats().snapshot_count == 0
            && ctx.current_page == player::PlayerPage::Library
            && ctx.page_library_layer.visible()
            && !ctx.page_home_layer.snapshot()
            && !ctx.page_now_layer.snapshot()
            && !ctx.page_library_layer.snapshot();
        if (interrupted) {
            ui_ci_emit("layer_transition_interrupt_abort", true, nullptr);
        } else {
            ui_ci_emit("layer_transition_interrupt_abort", false, "transition_interrupt_abort");
            res.ok = false;
            res.failed++;
        }

        const auto* tracks = ctx.storage.tracks;
        if (tracks && tracks->size() > 0) {
            ctx.set_page(player::PlayerPage::Library);
            pump_frame();
            const Rect list = scene.world_rect(ctx.handles.list);
            if (list.w > 0 && list.h > 0) {
                const int before = ctx.last_list_selected;
                ui_ci_click(app, ctx, scene, list.x + 12, list.y + 12);
                pump_frame();
                const int after = ctx.last_list_selected;
                if (after >= 0) {
                    ui_ci_emit("list_select", true, nullptr);
                } else {
                    const char* reason = (before == after) ? "no_change" : "no_select";
                    ui_ci_emit("list_select", false, reason);
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("list_select", false, "list_rect_zero");
                res.ok = false;
                res.failed++;
            }
        } else {
            ui_ci_emit("list_select", true, "skipped_no_tracks");
        }

        std::printf("[ui-ci] done ok=%d failed=%d\n", res.ok ? 1 : 0, res.failed);
        return res;
    }

    bool dispatch_sdl_event(::ui::scene::Scene& scene,
                            player::App& app,
                            PlayerUiContext& ctx,
                            const SDL_Event& evt);

    void loop_poll_events(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->running || !state->app || !state->ctx || !state->platform) {
            return;
        }
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                *state->running = false;
                break;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                if (state->win_w) {
                    *state->win_w = static_cast<int>(evt.window.data1);
                }
                if (state->win_h) {
                    *state->win_h = static_cast<int>(evt.window.data2);
                }
            }
            dispatch_sdl_event(state->platform->scene_ref(), *state->app, *state->ctx, evt);
        }
    }

    void loop_update(void* ctx, charm::system::ClockTick now_us, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->app || !state->ctx) {
            return;
        }
        state->t_sec = static_cast<float>(now_us) * 0.000001f;
        state->app->tick();
        state->ctx->tick_player(state->app->player());
        update_spectrum(state->t_sec, state->ctx->is_playing());
    }

    void loop_render(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->platform || !state->ctx || !state->scene || !state->renderer || !state->texture) {
            return;
        }
        prepare_screenshot_page(*state);
        state->platform->framebuffer_ref().clear(kUiBackground);
        state->platform->begin_frame();
        state->platform->scene_ref().set_overlay(
            [](::ui::scene::SceneOverlay& overlay, void* ctx) noexcept {
                auto* state = static_cast<PlayerLoopState*>(ctx);
                if (!state || !state->ctx || !state->scene) return;
                draw_library_fx(overlay, *state->ctx, *state->scene);
                draw_now_playing_fx(overlay, *state->ctx, *state->scene, state->t_sec);
            },
            state);
        if (state->ctx->transition_needs_destination_snapshot()) {
            state->ctx->prepare_transition_destination_snapshot_scene();
            state->platform->render();
            state->ctx->finish_transition_destination_snapshot_capture();
            state->platform->framebuffer_ref().clear(kUiBackground);
            state->platform->begin_frame();
        }
        state->ctx->compose_now_playing_transition_pixel_layer();
        state->platform->render();
        state->platform->end_frame();

        SDL_UpdateTexture(state->texture,
                          nullptr,
                          state->platform->canvas_ref().data(),
                          static_cast<int>(state->platform->stride_bytes()));
        SDL_RenderClear(state->renderer);
        SDL_RenderTexture(state->renderer, state->texture, nullptr, nullptr);
        SDL_RenderPresent(state->renderer);
        if (flush_screenshot(*state)) {
            return;
        }
    }

    std::optional<input::Button> map_nav_button(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return input::Button::Up;
        case SDLK_DOWN: return input::Button::Down;
        case SDLK_RETURN: return input::Button::Enter;
        case SDLK_ESCAPE: return input::Button::Back;
        case SDLK_BACKSPACE: return input::Button::Back;
        default:
            break;
        }
        return std::nullopt;
    }

    std::optional<player::UiKey> map_ui_key(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return player::UiKey::Up;
        case SDLK_DOWN: return player::UiKey::Down;
        case SDLK_RETURN: return player::UiKey::Enter;
        case SDLK_SPACE: return player::UiKey::PlayToggle;
        case SDLK_N: return player::UiKey::Next;
        case SDLK_P: return player::UiKey::Prev;
        case SDLK_M: return player::UiKey::Mode;
        default:
            break;
        }
        return std::nullopt;
    }

    bool dispatch_sdl_event(::ui::scene::Scene& scene, player::App& app, PlayerUiContext& ctx, const SDL_Event& evt) {
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{false,
                                            static_cast<std::int16_t>(evt.motion.x),
                                            static_cast<std::int16_t>(evt.motion.y),
                                            0};
            raw.pointer_action = input::PointerAction::Move;
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{true,
                                            static_cast<std::int16_t>(evt.button.x),
                                            static_cast<std::int16_t>(evt.button.y),
                                            0};
            raw.pointer_action = input::PointerAction::Down;
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{false,
                                            static_cast<std::int16_t>(evt.button.x),
                                            static_cast<std::int16_t>(evt.button.y),
                                            0};
            raw.pointer_action = input::PointerAction::Up;
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            {
                float mx = 0.0f;
                float my = 0.0f;
                SDL_GetMouseState(&mx, &my);
                scene.dispatch_event(Event::wheel(static_cast<int>(mx),
                                                  static_cast<int>(my),
                                                  evt.wheel.y));
            }
            ctx.process_input_events();
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (auto k = map_ui_key(evt.key.key)) {
                ctx.handle_key_action(*k);
            }
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = true;
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        case SDL_EVENT_KEY_UP:
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = false;
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        default:
            return false;
        }
    }
}

int main(int argc, char** argv) {
    std::string screenshot_path{};
    std::string screenshot_gif_path{};
    bool screenshot_verbose = false;
    int screenshot_wait_frames = 0;
    bool screenshot_exit = false;
    player::PlayerPage start_page = player::PlayerPage::Home;
    bool start_page_set = false;
    std::optional<player::LibraryTab> library_tab_override{};
    std::string library_context_override{};
    bool library_open_first_group = false;
    int library_select_index = -1;
    bool library_open_info = false;
    int library_open_info_index = -1;
    bool library_open_action_menu = false;
    int library_open_action_menu_index = -1;
    int track_index_override = 0;
    bool ui_ci = false;
    std::string font_ttf_path{};
    std::string font_fallback_ttf_path{};
    bool disable_system_font_fallback = false;
    int font_small_px = 0;
    int font_normal_px = 0;
    int font_large_px = 0;
    int home_scroll_y = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] ? argv[i] : "";
        if (arg.rfind("--screenshot=", 0) == 0) {
            screenshot_path.assign(arg.substr(13));
        } else if (arg.rfind("--screenshot-gif=", 0) == 0) {
            screenshot_gif_path.assign(arg.substr(17));
        } else if (arg.rfind("--screenshot-page=", 0) == 0) {
            const std::string_view page = arg.substr(18);
            if (page == "probe") {
                start_page = player::PlayerPage::Probe;
                start_page_set = true;
            } else if (page == "home") {
                start_page = player::PlayerPage::Home;
                start_page_set = true;
            } else if (page == "now") {
                start_page = player::PlayerPage::NowPlaying;
                start_page_set = true;
            } else if (page == "library") {
                start_page = player::PlayerPage::Library;
                start_page_set = true;
            }
        } else if (arg.rfind("--page=", 0) == 0) {
            const std::string_view page = arg.substr(7);
            if (page == "probe") {
                start_page = player::PlayerPage::Probe;
                start_page_set = true;
            } else if (page == "home") {
                start_page = player::PlayerPage::Home;
                start_page_set = true;
            } else if (page == "now") {
                start_page = player::PlayerPage::NowPlaying;
                start_page_set = true;
            } else if (page == "library") {
                start_page = player::PlayerPage::Library;
                start_page_set = true;
            }
        } else if (arg.rfind("--library-tab=", 0) == 0) {
            const std::string_view tab = arg.substr(14);
            if (tab == "songs") {
                library_tab_override = player::LibraryTab::Songs;
            } else if (tab == "albums") {
                library_tab_override = player::LibraryTab::Albums;
            } else if (tab == "artists") {
                library_tab_override = player::LibraryTab::Artists;
            }
        } else if (arg.rfind("--library-context=", 0) == 0) {
            library_context_override.assign(arg.substr(18));
        } else if (arg == "--library-open-first-group") {
            library_open_first_group = true;
        } else if (arg.rfind("--library-select-index=", 0) == 0) {
            library_select_index = std::atoi(std::string(arg.substr(23)).c_str());
        } else if (arg == "--library-open-info") {
            library_open_info = true;
        } else if (arg.rfind("--library-open-info-index=", 0) == 0) {
            library_open_info = true;
            library_open_info_index = std::atoi(std::string(arg.substr(26)).c_str());
        } else if (arg == "--library-open-action-menu") {
            library_open_action_menu = true;
        } else if (arg.rfind("--library-open-action-menu-index=", 0) == 0) {
            library_open_action_menu = true;
            library_open_action_menu_index = std::atoi(std::string(arg.substr(33)).c_str());
        } else if (arg.rfind("--track-index=", 0) == 0) {
            track_index_override = std::max(0, std::atoi(std::string(arg.substr(14)).c_str()));
        } else if (arg == "--screenshot-verbose") {
            screenshot_verbose = true;
        } else if (arg == "--screenshot-exit") {
            screenshot_exit = true;
        } else if (arg.rfind("--screenshot-frame=", 0) == 0) {
            const std::string_view value = arg.substr(19);
            screenshot_wait_frames = std::max(0, std::atoi(std::string(value).c_str()));
        } else if (arg == "--ui-ci") {
            ui_ci = true;
        } else if (arg.rfind("--font-ttf=", 0) == 0) {
            font_ttf_path.assign(arg.substr(11));
        } else if (arg.rfind("--font-fallback-ttf=", 0) == 0) {
            font_fallback_ttf_path.assign(arg.substr(20));
        } else if (arg == "--font-disable-system-fallback") {
            disable_system_font_fallback = true;
        } else if (arg.rfind("--font-small=", 0) == 0) {
            font_small_px = std::max(0, std::atoi(std::string(arg.substr(13)).c_str()));
        } else if (arg.rfind("--font-normal=", 0) == 0) {
            font_normal_px = std::max(0, std::atoi(std::string(arg.substr(14)).c_str()));
        } else if (arg.rfind("--font-large=", 0) == 0) {
            font_large_px = std::max(0, std::atoi(std::string(arg.substr(13)).c_str()));
        } else if (arg.rfind("--home-scroll=", 0) == 0) {
            home_scroll_y = std::max(0, std::atoi(std::string(arg.substr(14)).c_str()));
        }
    }
    if (!start_page_set && (!screenshot_path.empty() || !screenshot_gif_path.empty())) {
        start_page = player::PlayerPage::NowPlaying;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Player", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
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

    g_player_cfg.output_mode = audio::OutputMode::fixed_rate;
    g_player_cfg.fixed_rate = 48000;
    charm::system::ClockCaps::TimeSource::bind(g_clock);
    player::ui::set_player_system_font_fallback_enabled(!disable_system_font_fallback);
    player::AppConfig app_cfg{g_player_cfg};
    if (!font_ttf_path.empty()) {
        app_cfg.ttf_path = font_ttf_path;
    } else {
        app_cfg.ttf_path = "/font/gflex_variable.ttf";
    }
    if (!font_fallback_ttf_path.empty()) {
        app_cfg.ttf_fallback_path = font_fallback_ttf_path;
    }
    if (font_small_px > 0) {
        app_cfg.ttf_small_px = font_small_px;
    }
    if (font_normal_px > 0) {
        app_cfg.ttf_normal_px = font_normal_px;
    }
    if (font_large_px > 0) {
        app_cfg.ttf_large_px = font_large_px;
    }
    g_app.emplace(std::move(app_cfg), g_clock);

    player::init_storage(player::default_storage_config());
    g_app->bind_player(g_ctx);
    g_ctx.bind_scene(g_platform.scene_ref());
    g_ctx.set_start_page(start_page);
    (void)g_app->scan_storage();
    g_ctx.apply_storage_view(g_app->storage_view(), false);
    g_platform.build_scene([&](::ui::scene::SceneBuilder& builder) {
        g_app->bind_ui(builder, g_ctx);
    });
    g_ctx.set_page(start_page);

    const bool has_track = g_app->bootstrap_player(g_ctx, track_index_override, false);
    if (library_tab_override.has_value()) {
        g_ctx.set_library_tab(*library_tab_override);
    }
    if (!library_context_override.empty()) {
        (void)g_ctx.set_library_context_for_preview(library_context_override);
    } else if (library_open_first_group) {
        (void)g_ctx.open_first_library_group_for_preview();
    }
    if (library_select_index >= 0) {
        (void)g_ctx.set_library_selected_index_for_preview(library_select_index);
    }
        if (library_open_info) {
            const bool opened = g_ctx.open_library_info_popup_for_preview(library_open_info_index);
            if (screenshot_verbose) {
                const char* info_title = g_platform.scene_ref().text(g_ctx.handles.list_info_title);
                const char* info_subtitle = g_platform.scene_ref().text(g_ctx.handles.list_info_subtitle);
                const char* info_meta = g_platform.scene_ref().text(g_ctx.handles.list_info_meta);
                const char* info_path_title =
                    g_platform.scene_ref().text(g_ctx.handles.list_info_path_title);
                const char* info_path = g_platform.scene_ref().text(g_ctx.handles.list_info_path);
                const char* info_path_detail =
                    g_platform.scene_ref().text(g_ctx.handles.list_info_path_detail);
                const char* info_hint = g_platform.scene_ref().text(g_ctx.handles.list_info_hint);
                const Rect scrim_rect = g_platform.scene_ref().world_rect(g_ctx.handles.list_info_scrim);
                const Rect card_rect = g_platform.scene_ref().world_rect(g_ctx.handles.list_info_card);
                std::fprintf(stderr,
                             "[preview] library_open_info flag=1 opened=%d selected=%d request=%d title=%s subtitle=%s meta=%s path_title=%s path=%s detail=%s hint=%s scrim=%d,%d,%d,%d card=%d,%d,%d,%d\n",
                         opened ? 1 : 0,
                         g_ctx.last_list_selected,
                         library_open_info_index,
                         info_title ? info_title : "",
                         info_subtitle ? info_subtitle : "",
                         info_meta ? info_meta : "",
                         info_path_title ? info_path_title : "",
                         info_path ? info_path : "",
                         info_path_detail ? info_path_detail : "",
                         info_hint ? info_hint : "",
                         scrim_rect.x, scrim_rect.y, scrim_rect.w, scrim_rect.h,
                         card_rect.x, card_rect.y, card_rect.w, card_rect.h);
        }
    } else if (screenshot_verbose) {
        std::fprintf(stderr,
                     "[preview] library_open_info flag=0 selected=%d request=%d\n",
                     g_ctx.last_list_selected,
                     library_open_info_index);
    }
    if (library_open_action_menu) {
        const bool opened =
            g_ctx.open_library_action_menu_for_preview(library_open_action_menu_index);
        if (screenshot_verbose) {
            const char* menu_title = g_platform.scene_ref().text(g_ctx.handles.list_action_title);
            const Rect card_rect = g_platform.scene_ref().world_rect(g_ctx.handles.list_action_card);
            const char* item0 = g_platform.scene_ref().text(g_ctx.handles.list_action_items[0]);
            const char* item1 = g_platform.scene_ref().text(g_ctx.handles.list_action_items[1]);
            const char* item2 = g_platform.scene_ref().text(g_ctx.handles.list_action_items[2]);
            std::fprintf(stderr,
                         "[preview] library_open_action_menu flag=1 opened=%d selected=%d request=%d title=%s items=[%s|%s|%s] card=%d,%d,%d,%d\n",
                         opened ? 1 : 0,
                         g_ctx.last_list_selected,
                         library_open_action_menu_index,
                         menu_title ? menu_title : "",
                         item0 ? item0 : "",
                         item1 ? item1 : "",
                         item2 ? item2 : "",
                         card_rect.x, card_rect.y, card_rect.w, card_rect.h);
        }
    } else if (screenshot_verbose) {
        std::fprintf(stderr,
                     "[preview] library_open_action_menu flag=0 selected=%d request=%d\n",
                     g_ctx.last_list_selected,
                     library_open_action_menu_index);
    }
    if (has_track && !fs_seek_selftest(g_ctx.track_path())) {
        g_ctx.set_status("Fs seek selftest failed");
    }

    if (ui_ci) {
        const UiCiResult result = run_ui_ci(*g_app, g_ctx, g_platform);
        g_app->shutdown(g_ctx);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result.ok ? 0 : 2;
    }

    int win_w = screen_width;
    int win_h = screen_height;
    bool running = true;
    PlayerLoopState loop_state{
        .app = &(*g_app),
        .platform = &g_platform,
        .ctx = &g_ctx,
        .scene = &g_platform.scene_ref(),
        .renderer = renderer,
        .texture = texture,
        .running = &running,
        .win_w = &win_w,
        .win_h = &win_h,
        .screenshot_path = std::move(screenshot_path),
        .screenshot_gif_path = std::move(screenshot_gif_path),
        .screenshot_page = start_page,
        .home_scroll_y = home_scroll_y,
        .screenshot_verbose = screenshot_verbose,
        .screenshot_wait_frames = screenshot_wait_frames,
        .screenshot_exit = screenshot_exit
    };
    charm::system::RunLoop<4> loop{};
    loop.bind_clock(g_clock);
    (void)loop.add_step(charm::system::LoopPhase::io, charm::system::SubmitProjection::event, &loop_poll_events, &loop_state, "player_io");
    (void)loop.add_step(charm::system::LoopPhase::update, charm::system::SubmitProjection::event, &loop_update, &loop_state, "player_update");
    (void)loop.add_step(charm::system::LoopPhase::render, charm::system::SubmitProjection::event, &loop_render, &loop_state, "player_render");
    std::array<char, 384> run_loop_audit{};
    (void)loop.format_audit_json(run_loop_audit.data(), run_loop_audit.size());
    std::printf("[runloop.audit] %s\n", run_loop_audit.data());
    while (running) {
        loop.run_once();
        (void)win_w;
        (void)win_h;
    }

    g_app->shutdown(g_ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

