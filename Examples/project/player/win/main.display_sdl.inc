    struct SdlDisplaySinkState {
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
    };

    SDL_PixelFormat sdl_pixel_format(player::PlayerDisplayPixelFormat format) noexcept {
        switch (format) {
        case player::PlayerDisplayPixelFormat::RGB565:
            return SDL_PIXELFORMAT_RGB565;
        case player::PlayerDisplayPixelFormat::RGB888:
            return SDL_PIXELFORMAT_RGB24;
        case player::PlayerDisplayPixelFormat::ARGB8888:
            return SDL_PIXELFORMAT_ARGB8888;
        }
        return SDL_PIXELFORMAT_RGB24;
    }

    bool sdl_display_present(void* ctx,
                             const player::PlayerDisplaySurface& surface,
                             player::PlayerDirtyRegion) noexcept {
        auto* state = static_cast<SdlDisplaySinkState*>(ctx);
        if (!state || !state->renderer || !state->texture || !surface.valid()) {
            return false;
        }
        SDL_UpdateTexture(state->texture,
                          nullptr,
                          surface.pixels,
                          static_cast<int>(surface.stride_bytes));
        SDL_RenderClear(state->renderer);
        SDL_RenderTexture(state->renderer, state->texture, nullptr, nullptr);
        SDL_RenderPresent(state->renderer);
        return true;
    }

    player::PlayerDisplaySink make_sdl_display_sink(SdlDisplaySinkState& state) noexcept {
        return player::PlayerDisplaySink{&state, &sdl_display_present};
    }
