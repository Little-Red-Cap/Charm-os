module;
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

export module player.track_probe;

import audio.decode_pipe;
import audio.player;
import audio.result;
#if defined(CHARM_AUDIO_USE_VFS)
import audio.source.fs;
#else
import audio.source.file;
#endif
import media.stream.source;

#ifndef CHARM_AUDIO_ENABLE_FLAC
#define CHARM_AUDIO_ENABLE_FLAC 1
#endif
#ifndef CHARM_AUDIO_ENABLE_MP3
#define CHARM_AUDIO_ENABLE_MP3 1
#endif

namespace {
    bool ends_with_icase(const char* text, const char* suffix) {
        if (!text || !suffix) return false;
        const std::size_t value_len = std::strlen(text);
        const std::size_t suf_len = std::strlen(suffix);
        if (value_len < suf_len) return false;
        const std::size_t start = value_len - suf_len;
        for (std::size_t i = 0; i < suf_len; ++i) {
            const char a = static_cast<char>(std::tolower(text[start + i]));
            const char b = static_cast<char>(std::tolower(suffix[i]));
            if (a != b) return false;
        }
        return true;
    }
}

export namespace player {
    bool probe_duration_seconds(audio::AudioSourceBinding source_binding,
                                const char* path,
                                int& out_seconds) {
        if (!source_binding.valid() || !path || !*path) return false;
        const auto source = source_binding.open(path);
        if (!source) return false;
        auto ref = *source;
        const bool probed = [&]() {
            audio::SourceKind kind = audio::SourceKind::wav;
            if (CHARM_AUDIO_ENABLE_FLAC && ends_with_icase(path, ".flac")) {
                kind = audio::SourceKind::flac;
            } else if (CHARM_AUDIO_ENABLE_MP3 && ends_with_icase(path, ".mp3")) {
                kind = audio::SourceKind::mp3;
            } else if (ends_with_icase(path, ".wav")) {
                kind = audio::SourceKind::wav;
            } else {
                std::array<std::byte, 12> header{};
                auto pos = ref.tell();
                auto read = ref.read(std::span<std::byte>(header.data(), header.size()));
                if (read && *read >= 4) {
                    const auto b0 = static_cast<unsigned char>(header[0]);
                    const auto b1 = static_cast<unsigned char>(header[1]);
                    const auto b2 = static_cast<unsigned char>(header[2]);
                    const auto b3 = static_cast<unsigned char>(header[3]);
                    if (CHARM_AUDIO_ENABLE_FLAC && b0 == 'f' && b1 == 'L' && b2 == 'a' && b3 == 'C') {
                        kind = audio::SourceKind::flac;
                    } else if (CHARM_AUDIO_ENABLE_MP3 && b0 == 'I' && b1 == 'D' && b2 == '3') {
                        kind = audio::SourceKind::mp3;
                    } else if (CHARM_AUDIO_ENABLE_MP3 && b0 == 0xFF && (b1 & 0xE0) == 0xE0) {
                        kind = audio::SourceKind::mp3;
                    } else if (read && *read >= 12) {
                        const auto b8 = static_cast<unsigned char>(header[8]);
                        const auto b9 = static_cast<unsigned char>(header[9]);
                        const auto b10 = static_cast<unsigned char>(header[10]);
                        const auto b11 = static_cast<unsigned char>(header[11]);
                        if (b0 == 'R' && b1 == 'I' && b2 == 'F' && b3 == 'F' &&
                            b8 == 'W' && b9 == 'A' && b10 == 'V' && b11 == 'E') {
                            kind = audio::SourceKind::wav;
                        }
                    }
                }
                if (pos) {
                    (void)ref.seek(*pos, media::SeekWhence::set);
                } else {
                    (void)ref.seek(0, media::SeekWhence::set);
                }
            }

            audio::AudioDecodePipe probe{};
            auto opened = probe.open(ref, kind);
            if (!opened) return false;
            const auto total = probe.total_frames();
            const auto fmt = probe.input_format();
            if (total == 0 || fmt.rate == 0) return false;
            const auto secs = static_cast<int>(total / fmt.rate);
            out_seconds = (secs > 0) ? secs : 1;
            return true;
        }();
        source_binding.close();
        return probed;
    }

    bool probe_duration_seconds(const char* path, int& out_seconds) {
#if defined(CHARM_AUDIO_USE_VFS)
        audio::FsDataSource src{};
#else
        audio::FileDataSource src{};
#endif
        return probe_duration_seconds(audio::make_audio_source_binding(src), path, out_seconds);
    }
}
