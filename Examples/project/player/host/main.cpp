#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

import charm.core.config;
import charm.backend.host.sdl3;
import charm.gfx.color;
import player.host_sdl3_adapter;
import player.md3_port;
import player.raster;

namespace {
    struct Options {
        bool hidden{false};
        std::size_t frame_limit{0};
    };

    bool parse_options(int argc, char** argv, Options& options) noexcept {
        for (int i = 1; i < argc; ++i) {
            const std::string_view argument{argv[i]};
            if (argument == "--hidden") {
                options.hidden = true;
                continue;
            }
            constexpr std::string_view prefix{"--frames="};
            if (argument.starts_with(prefix)) {
                const auto value = argument.substr(prefix.size());
                const auto result = std::from_chars(value.data(),
                                                    value.data() + value.size(),
                                                    options.frame_limit);
                if (result.ec == std::errc{} && result.ptr == value.data() + value.size()
                    && options.frame_limit != 0) {
                    continue;
                }
            }
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv) {
    Options options{};
    if (!parse_options(argc, argv, options)) {
        std::puts("player-md3-host: invalid arguments");
        return 2;
    }

    constexpr auto bytes_per_pixel = std::size_t{4};
    const auto stride = static_cast<std::size_t>(screen_width) * bytes_per_pixel;
    std::vector<std::byte> pixels(stride * static_cast<std::size_t>(screen_height));
    const player::PlayerRasterSurface surface{
        .pixels = pixels,
        .width = screen_width,
        .height = screen_height,
        .stride_bytes = stride,
        .pixel_format = player::PlayerRasterPixelFormat::ARGB8888,
    };
    player::PlayerMd3RuntimeConfig<player::PlayerPage> config{
        .start_page = player::PlayerPage::Home,
        .initial_track_index = 0,
        .auto_start = false,
        .clear_color = rgba{9, 12, 16, 255},
    };
    static player::PlayerMd3PortApplication app{config};
    player::host_sdl3::Runtime runtime{};
    const auto opened = runtime.open(
        charm::backend::host::sdl3::Config{
            .title = "Charm Player MD3",
            .window_width = 426,
            .window_height = 908,
            .hidden = options.hidden,
            .resizable = true,
        },
        surface,
        app.endpoint());
    if (opened != player::host_sdl3::OpenCode::ok) {
        std::printf("player-md3-host: open failed code=%u error=%s\n",
                    static_cast<unsigned int>(opened),
                    runtime.last_error());
        return 1;
    }

    std::size_t frames = 0;
    while ((options.frame_limit == 0 || frames < options.frame_limit)
           && runtime.run_once()) {
        ++frames;
        if (!options.hidden) {
            std::this_thread::sleep_for(std::chrono::milliseconds{16});
        }
    }
    const bool ok = !runtime.failed()
        && (runtime.quit_requested() || frames == options.frame_limit);
    const auto presented = runtime.presented_frames();
    runtime.close();
    if (!ok) {
        std::printf("player-md3-host: failed frames=%llu presented=%llu\n",
                    static_cast<unsigned long long>(frames),
                    static_cast<unsigned long long>(presented));
        return 1;
    }
    std::printf("player-md3-host: ok frames=%llu presented=%llu\n",
                static_cast<unsigned long long>(frames),
                static_cast<unsigned long long>(presented));
    return 0;
}
