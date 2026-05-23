    bool has_pending_screenshot(const PlayerLoopState& state) noexcept {
        return !state.screenshot_path.empty() || !state.screenshot_gif_path.empty();
    }

    void prepare_screenshot_page(PlayerLoopState& state) noexcept {
        auto* ctx = state.runtime ? state.runtime->controller() : nullptr;
        if (!has_pending_screenshot(state) || !ctx) {
            return;
        }
        if (ctx->current_page != state.screenshot_page) {
            ctx->set_page(state.screenshot_page);
            if (state.screenshot_wait_frames < 1) state.screenshot_wait_frames = 1;
        }
        if (state.screenshot_page == player::PlayerPage::Home
            && state.home_scroll_y >= 0
            && ctx->handles.home_scroll) {
            if (ctx->access.scroll_y(ctx->handles.home_scroll) != state.home_scroll_y) {
                ctx->access.set_scroll_y(ctx->handles.home_scroll, state.home_scroll_y);
                if (state.screenshot_wait_frames < 1) state.screenshot_wait_frames = 1;
            }
        }
    }

    bool flush_screenshot(PlayerLoopState& state) noexcept {
        auto* platform = state.runtime ? state.runtime->platform() : nullptr;
        auto* ctx = state.runtime ? state.runtime->controller() : nullptr;
        if (!has_pending_screenshot(state) || !platform || !ctx) {
            return false;
        }
        if (state.screenshot_wait_frames > 0) {
            state.screenshot_wait_frames--;
            return true;
        }

        ::FrameBufferView view = platform->framebuffer_view();

        if (state.screenshot_verbose) {
            dump_font_probe_metrics(*platform, state.runtime->scene_ref(), *ctx);
        }

        if (!state.screenshot_path.empty()) {
            const bool ok = ::charm::gfx::snapshot::write_ppm(state.screenshot_path.c_str(), view);
            if (state.screenshot_verbose) {
                std::printf("[ui] screenshot ppm=%s ok=%d\n", state.screenshot_path.c_str(), ok ? 1 : 0);
            }
            state.screenshot_path.clear();
        }
        if (!state.screenshot_gif_path.empty()) {
            std::vector<std::vector<std::uint8_t>> frames{};
            frames.push_back(::charm::gfx::snapshot::capture_indexed_332(view));
            const bool ok = ::charm::gfx::snapshot::write_gif(state.screenshot_gif_path.c_str(),
                                                              static_cast<int>(view.width),
                                                              static_cast<int>(view.height),
                                                              frames,
                                                              8);
            if (state.screenshot_verbose) {
                std::printf("[ui] screenshot gif=%s ok=%d\n", state.screenshot_gif_path.c_str(), ok ? 1 : 0);
            }
            state.screenshot_gif_path.clear();
        }

        if (state.screenshot_exit
            && state.screenshot_path.empty()
            && state.screenshot_gif_path.empty()
            && state.running) {
            *state.running = false;
        }
        return false;
    }
