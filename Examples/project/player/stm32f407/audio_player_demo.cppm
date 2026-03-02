module;

#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

#include "usart.h"

export module player.stm32.audio_player_demo;

import audio.player;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import util.core;
import util.expected;

namespace {
    constexpr util::u32 kTimeoutMs = 1000;

    void uart_write(const char* text) noexcept {
        if (!text) return;
        HAL_UART_Transmit(&huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(text)),
            static_cast<uint16_t>(std::strlen(text)), kTimeoutMs);
    }

    std::size_t name_len(std::string_view name) noexcept {
        if (!name.data()) return 0;
        if (name.size() > 0) return name.size();
        return std::strlen(name.data());
    }

    char ascii_lower(char c) noexcept {
        if (c >= 'A' && c <= 'Z') return static_cast<char>(c + ('a' - 'A'));
        return c;
    }

    bool is_mp3_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'm' && c2 == 'p' && c3 == '3';
    }

    bool is_flac_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 5) return false;
        const char c0 = ascii_lower(name.data()[len - 5]);
        const char c1 = ascii_lower(name.data()[len - 4]);
        const char c2 = ascii_lower(name.data()[len - 3]);
        const char c3 = ascii_lower(name.data()[len - 2]);
        const char c4 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'f' && c2 == 'l' && c3 == 'a' && c4 == 'c';
    }

    bool is_wav_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'w' && c2 == 'a' && c3 == 'v';
    }

    struct FindAudioCtx {
        char path[128]{};
        bool found{false};
    };

    fs::Status find_first_audio(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<FindAudioCtx*>(ctx);
        if (!out) return fs::Status{fs::Errc::inval};
        if (out->found || entry.type != fs::NodeType::file) return fs::Status{fs::Errc::ok};
        if (!is_mp3_name(entry.name) && !is_flac_name(entry.name) && !is_wav_name(entry.name)) {
            return fs::Status{fs::Errc::ok};
        }
        const auto len = name_len(entry.name);
        const char prefix[] = "/MUSIC/";
        std::size_t pos = 0;
        for (std::size_t i = 0; i < sizeof(prefix) - 1 && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = prefix[i];
        }
        for (std::size_t i = 0; i < len && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = entry.name.data()[i];
        }
        out->path[pos] = '\0';
        out->found = true;
        return fs::Status{fs::Errc::ok};
    }

    audio::AudioPlayer* g_player = nullptr;
} // namespace

export void audio_player_demo_start() noexcept {
    static audio::AudioPlayer player{audio::PlayerConfig{}};
    g_player = &player;

    FindAudioCtx ctx{};
    auto st = fs::vfs_list("/MUSIC", &ctx, &find_first_audio);
    if (!st || !ctx.found) {
        uart_write("player: no audio found\r\n");
        return;
    }

    uart_write("player: play ");
    uart_write(ctx.path);
    uart_write("\r\n");

    (void)player.play(ctx.path);
}

export void audio_player_demo_tick() noexcept {
    if (g_player) g_player->tick();
}
