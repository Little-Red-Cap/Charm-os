module;

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
#include <cstdarg>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "main.ui_ci_shared.hpp"

export module player.win.main_host;

import audio.player;
import audio.result;
import player.app;
import player.app_config;
import player.board_port;
import player.board_runtime;
import player.cover_resource;
import player.controller;
import player.display;
import player.font_cache;
import player.font_resource;
import player.fs_utils;
import player.host_features;
import player.input;
import player.lyrics;
import player.platform;
import player.storage;
import player.playback;
import player.product_config;
import player.runtime;
import player.runtime_probe;
import player.ui_builder;
import player.ui;
import player.cover;
import charm.core.config;
import charm.core.event;
import charm.ui.scene;
import charm.ui.scene.scene_evidence;
import ui.input_adapter;
import charm.gfx.color;
import charm.gfx.text_box;
import charm.gfx.image;
import charm.gfx.draw_cmd;
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

namespace {
    using namespace player::fs_utils;
    using namespace player::ui;

    static charm::system::ClockTick now_us(void*) noexcept {
        return platform::win::SteadyClock::now();
    }

    static audio::PlayerConfig g_player_cfg{};
    static charm::system::Clock g_clock{nullptr, {.now_us = &now_us}};
    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;
    using PlayerRuntime = player::PlayerRuntime<PlayerUiContext, player::PlayerPage>;

    static player::PlayerOwnedDisplayBuffer g_display_buffer{};
    static player::PlayerPlatform g_platform{g_display_buffer.surface()};
    static PlayerUiContext g_ctx{};
    static std::optional<PlayerRuntime> g_runtime{};

#include "main.host_preview.inc"
#include "main.overlay_fx.inc"

#include "main.font_probe.inc"
#include "main.display_sdl.inc"
#include "main.host_runtime.inc"
#include "main.screenshot.inc"

#include "main.ui_ci.inc"

#include "main.input_sdl.inc"
#include "main.host_loop.inc"

    static std::uint32_t host_read_be24(const std::array<unsigned char, 4>& header) noexcept {
        return (static_cast<std::uint32_t>(header[1]) << 16) |
            (static_cast<std::uint32_t>(header[2]) << 8) |
            static_cast<std::uint32_t>(header[3]);
    }

    static std::uint32_t host_read_le32(const std::vector<char>& data, std::size_t off) noexcept {
        if (off + 4 > data.size()) return 0;
        return static_cast<std::uint32_t>(static_cast<unsigned char>(data[off])) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(data[off + 1])) << 8) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(data[off + 2])) << 16) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(data[off + 3])) << 24);
    }

    static bool host_read_exact(std::ifstream& file, char* data, std::size_t size) {
        file.read(data, static_cast<std::streamsize>(size));
        return file.good() || file.gcount() == static_cast<std::streamsize>(size);
    }

    static int run_host_lyrics_probe(std::string_view path) {
        std::filesystem::path fs_path{std::string(path)};
        std::printf("[lyrics.probe] path=%s\n", fs_path.string().c_str());

        const auto load_result = player::load_lyrics_for_track(path);
        const auto window = player::resolve_lyrics_window(0);
        std::printf("[lyrics.probe] loader status=%u source=%u lines=%d synced=%d truncated=%d current=%s\n",
                    static_cast<unsigned>(load_result.status),
                    static_cast<unsigned>(load_result.source),
                    load_result.line_count,
                    load_result.synced ? 1 : 0,
                    load_result.truncated ? 1 : 0,
                    window.current ? window.current : "");

        std::ifstream file(fs_path, std::ios::binary);
        if (!file) {
            std::printf("[lyrics.probe] host_open=0\n");
            return 2;
        }

        std::array<char, 4> magic{};
        if (!host_read_exact(file, magic.data(), magic.size()) ||
            magic[0] != 'f' || magic[1] != 'L' || magic[2] != 'a' || magic[3] != 'C') {
            std::printf("[lyrics.probe] flac_magic=0\n");
            return 3;
        }
        std::printf("[lyrics.probe] flac_magic=1\n");

        bool last = false;
        int block_index = 0;
        while (!last && file) {
            std::array<unsigned char, 4> header{};
            if (!host_read_exact(file, reinterpret_cast<char*>(header.data()), header.size())) {
                std::printf("[lyrics.probe] block_read=0 index=%d\n", block_index);
                return 4;
            }
            last = (header[0] & 0x80u) != 0;
            const unsigned type = header[0] & 0x7Fu;
            const std::uint32_t size = host_read_be24(header);
            std::printf("[lyrics.probe] block index=%d type=%u size=%u last=%d\n",
                        block_index,
                        type,
                        static_cast<unsigned>(size),
                        last ? 1 : 0);
            if (type != 4) {
                file.seekg(static_cast<std::streamoff>(size), std::ios::cur);
                ++block_index;
                continue;
            }

            std::vector<char> block(size);
            if (!block.empty() && !host_read_exact(file, block.data(), block.size())) {
                std::printf("[lyrics.probe] vorbis_read=0 size=%u\n", static_cast<unsigned>(size));
                return 5;
            }
            if (block.size() < 8) {
                std::printf("[lyrics.probe] vorbis_short=1\n");
                return 6;
            }

            std::size_t off = 0;
            const std::uint32_t vendor_len = host_read_le32(block, off);
            off += 4;
            if (off + vendor_len > block.size()) {
                std::printf("[lyrics.probe] vendor_oob=1 len=%u\n", static_cast<unsigned>(vendor_len));
                return 7;
            }
            off += vendor_len;
            if (off + 4 > block.size()) {
                std::printf("[lyrics.probe] comment_count_oob=1\n");
                return 8;
            }
            const std::uint32_t comment_count = host_read_le32(block, off);
            off += 4;
            std::printf("[lyrics.probe] vorbis_comments=%u vendor_len=%u\n",
                        static_cast<unsigned>(comment_count),
                        static_cast<unsigned>(vendor_len));

            bool found_lyrics_key = false;
            for (std::uint32_t i = 0; i < comment_count; ++i) {
                if (off + 4 > block.size()) {
                    std::printf("[lyrics.probe] comment_len_oob index=%u\n", static_cast<unsigned>(i));
                    break;
                }
                const std::uint32_t len = host_read_le32(block, off);
                off += 4;
                if (off + len > block.size()) {
                    std::printf("[lyrics.probe] comment_oob index=%u len=%u\n",
                                static_cast<unsigned>(i),
                                static_cast<unsigned>(len));
                    break;
                }
                std::string_view comment(block.data() + off, len);
                off += len;
                const auto eq = comment.find('=');
                const std::string_view key = (eq == std::string_view::npos) ? comment : comment.substr(0, eq);
                const std::size_t value_len = (eq == std::string_view::npos) ? 0 : comment.size() - eq - 1;
                const bool likely_lyrics =
                    key.find("LYRIC") != std::string_view::npos ||
                    key.find("lyric") != std::string_view::npos ||
                    key.find("Lyric") != std::string_view::npos;
                found_lyrics_key = found_lyrics_key || likely_lyrics;
                std::printf("[lyrics.probe] comment index=%u key=%.*s value_len=%zu likely_lyrics=%d\n",
                            static_cast<unsigned>(i),
                            static_cast<int>(key.size()),
                            key.data(),
                            value_len,
                            likely_lyrics ? 1 : 0);
            }
            std::printf("[lyrics.probe] found_likely_lyrics=%d\n", found_lyrics_key ? 1 : 0);
            return found_lyrics_key ? 0 : 9;
        }
        std::printf("[lyrics.probe] vorbis_comment_block=0\n");
        return 10;
    }
}

export int run_player_win_main(int argc, char** argv) {
    PreviewOptions options = parse_preview_options(argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] ? argv[i] : "";
        if (arg.rfind("--lyrics-probe=", 0) == 0) {
            return run_host_lyrics_probe(arg.substr(15));
        }
    }
    print_host_feature_summary();

    if (options.runtime_memory_smoke) {
        return run_runtime_memory_smoke(options);
    }

    SdlHostRuntime runtime{};
    if (!init_sdl_host_runtime(runtime)) {
        return 1;
    }
    bootstrap_player_preview(options);

    if (options.ui_ci) {
        return run_ui_ci_preview(runtime);
    }
    if (options.ui_ci_perf_only) {
        return run_ui_ci_perf_only_preview(runtime);
    }

    run_interactive_preview_loop(runtime, options);
    shutdown_player_preview(runtime);
    return 0;
}
