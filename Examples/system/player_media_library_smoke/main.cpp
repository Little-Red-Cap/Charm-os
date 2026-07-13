#include <array>
#include <cstdio>
#include <string_view>

import player.media_library;

namespace {
    bool expect(bool condition, const char* message) {
        if (!condition) {
            std::printf("[player-media-library-smoke] fail: %s\n", message);
            return false;
        }
        return true;
    }

    struct FormatCase {
        std::string_view path{};
        std::string_view ext{};
        std::string_view format{};
    };

    bool run_smoke() {
        constexpr std::array<FormatCase, 11> kFormats{{
            {"/music/Artist - Wave.wav", "wav", "WAV"},
            {"/music/Artist - Lossless.flac", "flac", "FLAC"},
            {"/music/Artist - Legacy.fla", "fla", "FLA"},
            {"/music/Artist - Layer3.mp3", "mp3", "MP3"},
            {"/music/Artist - Air.aac", "aac", "AAC"},
            {"/music/Artist - Apple.m4a", "m4a", "M4A"},
            {"/music/Artist - Vorbis.ogg", "ogg", "OGG"},
            {"/music/Artist - Speech.opus", "opus", "OPUS"},
            {"/music/Artist - Monkey.ape", "ape", "APE"},
            {"/music/Artist - AppleLossless.alac", "alac", "ALAC"},
            {"/music/Artist - WavPack.wv", "wv", "WV"},
        }};

        for (const auto& item : kFormats) {
            if (!expect(player::media_is_audio_extension(item.ext),
                        "audio extension recognized")) {
                return false;
            }
            if (!expect(player::media_track_format(item.ext) == item.format,
                        "extension format label")) {
                return false;
            }
            const auto view = player::derive_media_track(item.path);
            if (!expect(view.format == item.format, "media format label")) {
                std::printf("[player-media-library-smoke] detail path=%.*s got=%.*s expected=%.*s\n",
                            static_cast<int>(item.path.size()),
                            item.path.data(),
                            static_cast<int>(view.format.size()),
                            view.format.data(),
                            static_cast<int>(item.format.size()),
                            item.format.data());
                return false;
            }
            if (!expect(view.has_artist
                        && view.artist == "Artist"
                        && !view.title.empty(),
                        "artist/title split for supported format")) {
                return false;
            }
        }

        if (!expect(player::media_is_audio_extension("FLAC")
                    && player::media_track_format("/UPPER/ARTIST - TITLE.OPUS") == "OPUS",
                    "audio extension matching is case-insensitive")) {
            return false;
        }
        if (!expect(player::media_path_has_decoder_support("/music/Artist - Track.wav")
                    && player::media_path_has_decoder_support("/music/Artist - Track.flac")
                    && player::media_path_has_decoder_support("/music/Artist - Track.mp3"),
                    "decoder-supported media paths")) {
            return false;
        }
        if (!expect(!player::media_path_has_decoder_support("/music/Artist - Track.opus")
                    && !player::media_path_has_decoder_support("/music/Artist - Track.m4a")
                    && !player::media_path_has_decoder_support("/music/Artist - Track.wv"),
                    "scan-visible but decoder-unsupported media paths")) {
            return false;
        }
        if (!expect(!player::media_is_audio_extension("txt"),
                    "non-audio extension rejected")) {
            return false;
        }
        if (!expect(player::media_track_stem("/mix/no-extension") == "no-extension",
                    "path without extension keeps full stem")) {
            return false;
        }
        const auto unknown = player::derive_media_track("/root/no-meta.unknown");
        if (!expect(unknown.format == player::kUnknownFormat
                    && unknown.artist == player::kUnknownArtist
                    && unknown.album == "root",
                    "unknown media fallback")) {
            return false;
        }

        return true;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::printf("[player-media-library-smoke] ok\n");
    return 0;
}
