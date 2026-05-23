module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

export module player.runtime_probe;

import charm.system.clock;
import player.display;
import player.platform;
import player.runtime;

export namespace player {
    struct PlayerRuntimeMemoryProbeResult {
        bool bootstrap_ok{false};
        bool render_ok{false};
        int present_count{0};
        PlayerDirtyRegion dirty{};
        bool surface_valid{false};
        bool has_nonzero_pixel{false};
        bool root_bound{false};
        bool ok{false};
    };

    inline bool runtime_probe_has_nonzero_pixel(const PlayerDisplaySurface& surface) noexcept {
        if (!surface.valid()) {
            return false;
        }
        const std::size_t bpp = surface.bytes_per_pixel();
        if (bpp == 0) {
            return false;
        }
        for (int y = 0; y < surface.height; y += 32) {
            const auto* row = surface.pixels + static_cast<std::size_t>(y) * surface.stride_bytes;
            for (int x = 0; x < surface.width; x += 32) {
                const auto* px = row + static_cast<std::size_t>(x) * bpp;
                for (std::size_t i = 0; i < bpp; ++i) {
                    if (px[i] != std::byte{}) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    template <typename Controller>
    bool runtime_probe_root_bound(PlayerPlatform& platform, Controller& controller) noexcept {
        if constexpr (requires { controller.handles.root; }) {
            return controller.handles.root && platform.scene_ref().root() == controller.handles.root;
        } else {
            return false;
        }
    }

    template <typename Controller, typename Page, typename SinkState>
    PlayerRuntimeMemoryProbeResult run_player_runtime_memory_probe(charm::system::Clock& clock,
                                                                   PlayerPlatform& platform,
                                                                   Controller& controller,
                                                                   PlayerRuntimeConfig<Page> config,
                                                                   std::optional<PlayerRuntime<Controller, Page>>& runtime_storage,
                                                                   SinkState& sink_state,
                                                                   PlayerDisplaySink& display_sink,
                                                                   charm::system::ClockTick tick_us) {
        runtime_storage.emplace(clock, platform, controller, std::move(config));
        auto& runtime = *runtime_storage;
        PlayerRuntimeMemoryProbeResult out{};
        (void)runtime.bootstrap();
        out.bootstrap_ok = runtime.app() != nullptr;
        runtime.tick(tick_us);
        out.render_ok = runtime.render(&display_sink);

        const auto& presented = sink_state.last_surface;
        out.present_count = sink_state.present_count;
        out.dirty = sink_state.last_dirty;
        out.surface_valid = presented.valid();
        out.has_nonzero_pixel = runtime_probe_has_nonzero_pixel(presented);
        out.root_bound = runtime_probe_root_bound(platform, controller);
        out.ok = out.bootstrap_ok
            && out.render_ok
            && out.present_count == 1
            && out.surface_valid
            && out.has_nonzero_pixel
            && out.root_bound;

        runtime.shutdown();
        runtime_storage.reset();
        return out;
    }
}
