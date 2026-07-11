module;
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string_view>

#ifndef CHARM_AUDIO_ENABLE_FLAC
#define CHARM_AUDIO_ENABLE_FLAC 1
#endif
#ifndef CHARM_AUDIO_ENABLE_MP3
#define CHARM_AUDIO_ENABLE_MP3 1
#endif

export module player.media_library;

export namespace player {
    enum class MediaGroupKind {
        Albums,
        Artists,
    };

    struct MediaTrackView {
        std::string_view path{};
        std::string_view file_name{};
        std::string_view stem{};
        std::string_view title{};
        std::string_view artist{};
        std::string_view album{};
        std::string_view format{};
        bool has_artist{false};
    };

    struct MediaLibraryStats {
        std::size_t track_count{0};
        std::size_t album_count{0};
        std::size_t artist_count{0};
        bool track_truncated{false};
        bool dir_truncated{false};
    };

    inline constexpr std::string_view kUnknownArtist = "Unknown Artist";
    inline constexpr std::string_view kUnknownAlbum = "Unknown Album";
    inline constexpr std::string_view kUnknownFormat = "UNKNOWN";
    inline constexpr std::string_view kFormatMp3 = "MP3";
    inline constexpr std::string_view kFormatWav = "WAV";
    inline constexpr std::string_view kFormatFla = "FLA";
    inline constexpr std::string_view kFormatFlac = "FLAC";
    inline constexpr std::string_view kFormatAac = "AAC";
    inline constexpr std::string_view kFormatM4a = "M4A";
    inline constexpr std::string_view kFormatOgg = "OGG";
    inline constexpr std::string_view kFormatOpus = "OPUS";
    inline constexpr std::string_view kFormatApe = "APE";
    inline constexpr std::string_view kFormatAlac = "ALAC";
    inline constexpr std::string_view kFormatWv = "WV";

    std::string_view media_extension_format(std::string_view ext) noexcept {
        if (ext.size() == 3) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            if (a == 'm' && b == 'p' && c == '3') return kFormatMp3;
            if (a == 'w' && b == 'a' && c == 'v') return kFormatWav;
            if (a == 'f' && b == 'l' && c == 'a') return kFormatFla;
            if (a == 'a' && b == 'a' && c == 'c') return kFormatAac;
            if (a == 'm' && b == '4' && c == 'a') return kFormatM4a;
            if (a == 'o' && b == 'g' && c == 'g') return kFormatOgg;
            if (a == 'a' && b == 'p' && c == 'e') return kFormatApe;
        }
        if (ext.size() == 4) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
            if (a == 'f' && b == 'l' && c == 'a' && d == 'c') return kFormatFlac;
            if (a == 'o' && b == 'p' && c == 'u' && d == 's') return kFormatOpus;
            if (a == 'a' && b == 'l' && c == 'a' && d == 'c') return kFormatAlac;
        }
        if (ext.size() == 2) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            if (a == 'w' && b == 'v') return kFormatWv;
        }
        return {};
    }

    bool media_is_audio_extension(std::string_view ext) noexcept {
        return !media_extension_format(ext).empty();
    }

    bool media_format_has_decoder_support(std::string_view format) noexcept {
        return format == kFormatWav
            || (CHARM_AUDIO_ENABLE_FLAC && format == kFormatFlac)
            || (CHARM_AUDIO_ENABLE_MP3 && format == kFormatMp3);
    }

    int compare_media_text_ci(std::string_view a, std::string_view b) noexcept {
        const std::size_t n = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < n; ++i) {
            const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
            const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
            if (ca < cb) return -1;
            if (ca > cb) return 1;
        }
        if (a.size() < b.size()) return -1;
        if (a.size() > b.size()) return 1;
        return 0;
    }

    std::string_view media_file_name(std::string_view path) noexcept {
        const auto slash = path.find_last_of("/\\");
        return (slash == std::string_view::npos) ? path : path.substr(slash + 1);
    }

    std::string_view media_directory_path(std::string_view path) noexcept {
        const auto slash = path.find_last_of("/\\");
        return (slash == std::string_view::npos) ? std::string_view{} : path.substr(0, slash);
    }

    std::string_view media_parent_folder(std::string_view path) noexcept {
        const auto slash = path.find_last_of("/\\");
        if (slash == std::string_view::npos) return {};
        std::string_view parent = path.substr(0, slash);
        const auto prev = parent.find_last_of("/\\");
        return (prev == std::string_view::npos) ? parent : parent.substr(prev + 1);
    }

    std::string_view media_track_format(std::string_view path) noexcept {
        const auto name = media_file_name(path);
        const auto dot = name.find_last_of('.');
        const auto ext = (dot != std::string_view::npos && dot + 1 < name.size())
            ? name.substr(dot + 1)
            : path;
        return media_extension_format(ext);
    }

    bool media_path_has_decoder_support(std::string_view path) noexcept {
        return media_format_has_decoder_support(media_track_format(path));
    }

    std::string_view media_track_stem(std::string_view path) noexcept {
        std::string_view name = media_file_name(path);
        const auto dot = name.find_last_of('.');
        if (dot == std::string_view::npos) return name;
        const auto ext = name.substr(dot + 1);
        return media_is_audio_extension(ext) ? name.substr(0, dot) : name;
    }

    std::string_view trim_media_text(std::string_view value) noexcept {
        while (!value.empty() && static_cast<unsigned char>(value.front()) <= 0x20u) {
            value.remove_prefix(1);
        }
        while (!value.empty() && static_cast<unsigned char>(value.back()) <= 0x20u) {
            value.remove_suffix(1);
        }
        return value;
    }

    bool split_media_stem_artist_title(std::string_view stem,
                                       std::string_view& artist_out,
                                       std::string_view& title_out) noexcept {
        artist_out = {};
        title_out = {};
        const auto assign_split = [&](std::string_view artist,
                                      std::string_view title) noexcept -> bool {
            artist = trim_media_text(artist);
            title = trim_media_text(title);
            if (artist.empty() || title.empty()) return false;
            artist_out = artist;
            title_out = title;
            return true;
        };

        const auto spaced_sep = stem.find(" - ");
        if (spaced_sep != std::string_view::npos) {
            if (assign_split(stem.substr(0, spaced_sep), stem.substr(spaced_sep + 3))) {
                return true;
            }
        }
        return false;
    }

    MediaTrackView derive_media_track(std::string_view path) noexcept {
        MediaTrackView out{};
        out.path = path;
        out.file_name = media_file_name(path);
        out.stem = media_track_stem(path);
        out.album = media_parent_folder(path);
        out.format = media_track_format(path);
        std::string_view artist{};
        std::string_view title{};
        if (split_media_stem_artist_title(out.stem, artist, title)) {
            out.artist = artist;
            out.title = title;
            out.has_artist = true;
        } else {
            out.artist = kUnknownArtist;
            out.title = out.stem;
            out.has_artist = false;
        }
        if (out.album.empty()) {
            out.album = kUnknownAlbum;
        }
        if (out.format.empty()) {
            out.format = kUnknownFormat;
        }
        return out;
    }

    std::string_view media_group_key(const MediaTrackView& view,
                                     MediaGroupKind kind) noexcept {
        switch (kind) {
        case MediaGroupKind::Albums:
            return view.album.empty() ? kUnknownAlbum : view.album;
        case MediaGroupKind::Artists:
            return view.artist.empty() ? kUnknownArtist : view.artist;
        }
        return {};
    }
}
