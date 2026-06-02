module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#ifndef CHARM_PLAYER_LYRICS
#define CHARM_PLAYER_LYRICS 1
#endif

export module player.lyrics;

#if CHARM_PLAYER_LYRICS

import fs_core;
import fs_vfs;
import player.fixed_string;
import player.product_config;
import service.fixed_vector;
import util.core;

export namespace player {
    enum class LyricsSourceKind : std::uint8_t {
        None,
        Sidecar,
        LyricsDir,
        EmbeddedFlac,
        EmbeddedMp3,
        UnsupportedEmbedded,
    };

    enum class LyricsLoadStatus : std::uint8_t {
        Missing,
        Loaded,
        Truncated,
        Unsupported,
        ParseError,
        IoError,
    };

    struct LyricsLine {
        int time_ms{-1};
        FixedString<product_config::lyrics_line_text_capacity> text{};
    };

    struct LyricsWindow {
        const char* previous{""};
        const char* current{""};
        const char* next{""};
        int current_index{-1};
        bool synced{false};
        bool available{false};
    };

    struct LyricsLoadResult {
        LyricsLoadStatus status{LyricsLoadStatus::Missing};
        LyricsSourceKind source{LyricsSourceKind::None};
        int line_count{0};
        bool synced{false};
        bool truncated{false};
    };

    struct LyricsServiceProfile {
        std::size_t service_size_bytes{0};
        std::size_t max_lines{0};
        std::size_t line_text_capacity{0};
        std::size_t raw_read_bytes{0};
        std::size_t path_text_capacity{0};
        std::size_t embedded_flac_supported{0};
        std::size_t embedded_mp3_supported{0};
        std::size_t embedded_m4a_supported{0};
    };

    namespace lyrics_detail {
        constexpr std::size_t kMaxLines = product_config::lyrics_max_lines;
        constexpr std::size_t kLineTextCapacity = product_config::lyrics_line_text_capacity;
        constexpr std::size_t kRawReadBytes = product_config::lyrics_raw_read_bytes;

        using RawBuffer = std::array<char, kRawReadBytes + 1>;

        inline bool equals_ci(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
                const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
                if (ca != cb) return false;
            }
            return true;
        }

        inline bool starts_with_ci(std::string_view s, std::string_view prefix) noexcept {
            if (s.size() < prefix.size()) return false;
            return equals_ci(s.substr(0, prefix.size()), prefix);
        }

        inline bool ends_with_ci(std::string_view s, std::string_view suffix) noexcept {
            if (s.size() < suffix.size()) return false;
            return equals_ci(s.substr(s.size() - suffix.size()), suffix);
        }

        inline std::string_view trim(std::string_view v) noexcept {
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t' ||
                                  v.front() == '\r' || v.front() == '\n')) {
                v.remove_prefix(1);
            }
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t' ||
                                  v.back() == '\r' || v.back() == '\n')) {
                v.remove_suffix(1);
            }
            return v;
        }

        inline bool is_digit(char c) noexcept {
            return c >= '0' && c <= '9';
        }

        inline int digit(char c) noexcept {
            return c - '0';
        }

        inline bool parse_lrc_timestamp(std::string_view tag, int& out_ms) noexcept {
            out_ms = 0;
            if (tag.size() < 5) return false;
            std::size_t pos = 0;
            int minutes = 0;
            int seconds = 0;
            while (pos < tag.size() && is_digit(tag[pos])) {
                minutes = minutes * 10 + digit(tag[pos++]);
            }
            if (pos >= tag.size() || tag[pos++] != ':') return false;
            if (pos + 1 >= tag.size() || !is_digit(tag[pos]) || !is_digit(tag[pos + 1])) {
                return false;
            }
            seconds = digit(tag[pos]) * 10 + digit(tag[pos + 1]);
            pos += 2;
            int millis = 0;
            if (pos < tag.size() && (tag[pos] == '.' || tag[pos] == ':')) {
                ++pos;
                int scale = 100;
                int read = 0;
                while (pos < tag.size() && is_digit(tag[pos]) && read < 3) {
                    millis += digit(tag[pos++]) * scale;
                    scale /= 10;
                    ++read;
                }
            }
            out_ms = (minutes * 60 + seconds) * 1000 + millis;
            return seconds < 60;
        }

        inline std::uint32_t read_be_u32(const std::byte* data) noexcept {
            const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]));
            const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[1]));
            const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[2]));
            const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]));
            return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        }

        inline std::uint32_t read_le_u32(const std::byte* data) noexcept {
            const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]));
            const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[1]));
            const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[2]));
            const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]));
            return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
        }

        inline std::uint32_t read_synchsafe_u32(const std::byte* data) noexcept {
            const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]) & 0x7F);
            const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[1]) & 0x7F);
            const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[2]) & 0x7F);
            const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]) & 0x7F);
            return (b0 << 21) | (b1 << 14) | (b2 << 7) | b3;
        }

        inline bool append_path_with_ext(FixedString<product_config::lyrics_path_text_capacity>& out,
                                         std::string_view base,
                                         std::string_view ext) noexcept {
            out.assign(base);
            return out.append(ext);
        }

        inline std::string_view path_stem(std::string_view path) noexcept {
            const auto slash = path.find_last_of("/\\");
            const auto start = (slash == std::string_view::npos) ? 0 : slash + 1;
            const auto dot = path.find_last_of('.');
            const auto end = (dot == std::string_view::npos || dot < start) ? path.size() : dot;
            return path.substr(start, end - start);
        }

        inline std::string_view path_without_ext(std::string_view path) noexcept {
            const auto slash = path.find_last_of("/\\");
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
                return path;
            }
            return path.substr(0, dot);
        }

        inline bool build_lyrics_dir_path(FixedString<product_config::lyrics_path_text_capacity>& out,
                                          std::string_view track_path,
                                          std::string_view ext) noexcept {
            out.assign("/Lyrics/");
            if (!out.append(path_stem(track_path))) return false;
            return out.append(ext);
        }

        inline void strip_utf8_bom(std::string_view& text) noexcept {
            if (text.size() >= 3 &&
                static_cast<unsigned char>(text[0]) == 0xEF &&
                static_cast<unsigned char>(text[1]) == 0xBB &&
                static_cast<unsigned char>(text[2]) == 0xBF) {
                text.remove_prefix(3);
            }
        }

        inline bool is_metadata_lrc_tag(std::string_view tag) noexcept {
            const auto colon = tag.find(':');
            if (colon == std::string_view::npos) return false;
            if (tag.empty() || is_digit(tag.front())) return false;
            return true;
        }
    }

    class LyricsService {
    public:
        LyricsService() = default;

        void clear() noexcept {
            lines_.clear();
            track_path_.clear();
            source_path_.clear();
            status_ = LyricsLoadStatus::Missing;
            source_ = LyricsSourceKind::None;
            synced_ = false;
            truncated_ = false;
        }

        LyricsLoadResult load_for_track(std::string_view track_path) noexcept {
            clear();
            track_path_.assign(track_path);
            if (track_path.empty()) return result();

            FixedString<product_config::lyrics_path_text_capacity> path{};
            const auto base = lyrics_detail::path_without_ext(track_path);
            if (lyrics_detail::append_path_with_ext(path, base, ".lrc") &&
                load_file(path.view(), LyricsSourceKind::Sidecar)) {
                return result();
            }
            if (lyrics_detail::append_path_with_ext(path, base, ".txt") &&
                load_file(path.view(), LyricsSourceKind::Sidecar)) {
                return result();
            }
            if (lyrics_detail::build_lyrics_dir_path(path, track_path, ".lrc") &&
                load_file(path.view(), LyricsSourceKind::LyricsDir)) {
                return result();
            }
            if (lyrics_detail::build_lyrics_dir_path(path, track_path, ".txt") &&
                load_file(path.view(), LyricsSourceKind::LyricsDir)) {
                return result();
            }
            if (load_embedded(track_path)) {
                return result();
            }

            status_ = source_ == LyricsSourceKind::UnsupportedEmbedded
                ? LyricsLoadStatus::Unsupported
                : LyricsLoadStatus::Missing;
            return result();
        }

        LyricsWindow window_for_position_ms(int position_ms) const noexcept {
            LyricsWindow out{};
            if (lines_.empty()) return out;
            out.available = true;
            out.synced = synced_;
            int index = 0;
            if (synced_) {
                index = -1;
                for (std::size_t i = 0; i < lines_.size(); ++i) {
                    if (lines_[i].time_ms <= position_ms) {
                        index = static_cast<int>(i);
                    } else {
                        break;
                    }
                }
                if (index < 0) index = 0;
            }
            out.current_index = index;
            out.current = lines_[static_cast<std::size_t>(index)].text.c_str();
            if (index > 0) {
                out.previous = lines_[static_cast<std::size_t>(index - 1)].text.c_str();
            }
            if (static_cast<std::size_t>(index + 1) < lines_.size()) {
                out.next = lines_[static_cast<std::size_t>(index + 1)].text.c_str();
            }
            return out;
        }

        LyricsLoadResult result() const noexcept {
            return LyricsLoadResult{
                .status = status_,
                .source = source_,
                .line_count = static_cast<int>(lines_.size()),
                .synced = synced_,
                .truncated = truncated_,
            };
        }

        const char* source_path() const noexcept { return source_path_.c_str(); }
        bool has_lyrics() const noexcept { return !lines_.empty(); }

        static constexpr LyricsServiceProfile profile() noexcept {
            return LyricsServiceProfile{
                .service_size_bytes = sizeof(LyricsService),
                .max_lines = lyrics_detail::kMaxLines,
                .line_text_capacity = lyrics_detail::kLineTextCapacity,
                .raw_read_bytes = lyrics_detail::kRawReadBytes,
                .path_text_capacity = product_config::lyrics_path_text_capacity,
                .embedded_flac_supported = 1,
                .embedded_mp3_supported = 1,
                .embedded_m4a_supported = 0,
            };
        }

    private:
        bool load_file(std::string_view path, LyricsSourceKind source) noexcept {
            fs::File file{};
            const auto open_st = fs::vfs_open(path, file);
            if (!open_st) return false;
            const bool ok = read_file_to_raw(file);
            (void)fs::vfs_close(file);
            if (!ok) {
                status_ = LyricsLoadStatus::IoError;
                return false;
            }
            return parse_and_commit(raw_view(), source, path);
        }

        bool read_file_to_raw(fs::File& file) noexcept {
            raw_size_ = 0;
            if (file.node.size <= 0) return false;
            const auto max_read = std::min<std::size_t>(
                static_cast<std::size_t>(file.node.size),
                raw_.size() - 1);
            if (max_read == 0) return false;
            auto st = fs::vfs_read(file, std::span<util::u8>(
                reinterpret_cast<util::u8*>(raw_.data()), max_read));
            if (!st) return false;
            const auto after = file.node.offset;
            raw_size_ = max_read;
            if (after > 0) {
                const auto actual = static_cast<std::size_t>(std::min<util::i64>(after, file.node.size));
                raw_size_ = std::min(actual, max_read);
            }
            raw_[raw_size_] = '\0';
            if (static_cast<std::size_t>(file.node.size) > max_read) {
                truncated_ = true;
            }
            return true;
        }

        std::string_view raw_view() const noexcept {
            return std::string_view(raw_.data(), raw_size_);
        }

        bool parse_and_commit(std::string_view text,
                              LyricsSourceKind source,
                              std::string_view source_path) noexcept {
            lines_.clear();
            synced_ = false;
            lyrics_detail::strip_utf8_bom(text);
            parse_text(text);
            if (lines_.empty()) {
                status_ = LyricsLoadStatus::ParseError;
                source_ = source;
                source_path_.assign(source_path);
                return false;
            }
            if (synced_) {
                sort_lines();
            }
            status_ = truncated_ ? LyricsLoadStatus::Truncated : LyricsLoadStatus::Loaded;
            source_ = source;
            source_path_.assign(source_path);
            return true;
        }

        void parse_text(std::string_view text) noexcept {
            std::size_t start = 0;
            while (start <= text.size()) {
                const auto end = text.find('\n', start);
                auto line = (end == std::string_view::npos)
                    ? text.substr(start)
                    : text.substr(start, end - start);
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
                parse_line(line);
                if (end == std::string_view::npos) break;
                start = end + 1;
            }
        }

        void parse_line(std::string_view raw_line) noexcept {
            auto line = lyrics_detail::trim(raw_line);
            if (line.empty()) return;
            if (line.front() != '[') {
                add_line(-1, line);
                return;
            }

            service::FixedVector<int, 4> times{};
            std::size_t pos = 0;
            while (pos < line.size() && line[pos] == '[') {
                const auto close = line.find(']', pos + 1);
                if (close == std::string_view::npos) break;
                const auto tag = line.substr(pos + 1, close - pos - 1);
                int ms = 0;
                if (lyrics_detail::parse_lrc_timestamp(tag, ms)) {
                    (void)times.push_back(ms);
                } else if (lyrics_detail::is_metadata_lrc_tag(tag)) {
                    return;
                } else {
                    break;
                }
                pos = close + 1;
            }

            auto text = lyrics_detail::trim(line.substr(pos));
            if (times.empty()) {
                if (!text.empty()) add_line(-1, text);
                return;
            }
            synced_ = true;
            for (std::size_t i = 0; i < times.size(); ++i) {
                add_line(times[i], text);
            }
        }

        void add_line(int time_ms, std::string_view text) noexcept {
            if (text.empty()) return;
            LyricsLine line{};
            line.time_ms = time_ms;
            line.text.assign(text);
            if (line.text.size() < text.size()) {
                truncated_ = true;
            }
            if (!lines_.push_back(line)) {
                truncated_ = true;
            }
        }

        void sort_lines() noexcept {
            std::sort(lines_.begin(), lines_.end(), [](const LyricsLine& a, const LyricsLine& b) {
                return a.time_ms < b.time_ms;
            });
        }

        bool load_embedded(std::string_view track_path) noexcept {
            if (lyrics_detail::ends_with_ci(track_path, ".flac")) {
                return load_embedded_flac(track_path);
            }
            if (lyrics_detail::ends_with_ci(track_path, ".mp3")) {
                return load_embedded_mp3(track_path);
            }
            if (lyrics_detail::ends_with_ci(track_path, ".m4a") ||
                lyrics_detail::ends_with_ci(track_path, ".mp4")) {
                source_ = LyricsSourceKind::UnsupportedEmbedded;
                return false;
            }
            return false;
        }

        bool read_exact(fs::File& file, std::span<std::byte> out) noexcept {
            const auto before = file.node.offset;
            auto st = fs::vfs_read(file, std::span<util::u8>(
                reinterpret_cast<util::u8*>(out.data()), out.size()));
            if (!st) return false;
            const auto after = file.node.offset;
            if (after > before) {
                return static_cast<std::size_t>(after - before) == out.size();
            }
            return true;
        }

        bool load_embedded_flac(std::string_view track_path) noexcept {
            fs::File file{};
            const auto open_st = fs::vfs_open(track_path, file);
            if (!open_st) return false;
            const bool ok = probe_flac_vorbis_comments(file, track_path);
            (void)fs::vfs_close(file);
            return ok;
        }

        bool probe_flac_vorbis_comments(fs::File& file, std::string_view track_path) noexcept {
            std::array<std::byte, 4> magic{};
            if (!read_exact(file, std::span<std::byte>(magic.data(), magic.size()))) return false;
            if (magic[0] != static_cast<std::byte>('f') ||
                magic[1] != static_cast<std::byte>('L') ||
                magic[2] != static_cast<std::byte>('a') ||
                magic[3] != static_cast<std::byte>('C')) {
                return false;
            }
            bool last = false;
            while (!last && file.node.offset >= 0 && file.node.offset < file.node.size) {
                std::array<std::byte, 4> header{};
                if (!read_exact(file, std::span<std::byte>(header.data(), header.size()))) return false;
                const auto flags = static_cast<unsigned char>(header[0]);
                last = (flags & 0x80u) != 0;
                const auto type = flags & 0x7Fu;
                const std::uint32_t size =
                    (static_cast<std::uint32_t>(static_cast<unsigned char>(header[1])) << 16) |
                    (static_cast<std::uint32_t>(static_cast<unsigned char>(header[2])) << 8) |
                    static_cast<std::uint32_t>(static_cast<unsigned char>(header[3]));
                if (type == 4) {
                    if (read_flac_vorbis_block(file, size, track_path)) return true;
                    return false;
                }
                const auto next = file.node.offset + static_cast<util::i64>(size);
                if (next < file.node.offset || next > file.node.size) return false;
                (void)fs::vfs_seek(file, next);
            }
            return false;
        }

        bool read_flac_vorbis_block(fs::File& file,
                                    std::uint32_t size,
                                    std::string_view track_path) noexcept {
            if (size < 8) return false;
            if (size > lyrics_detail::kRawReadBytes) {
                truncated_ = true;
                return false;
            }
            raw_size_ = size;
            if (!read_exact(file, std::span<std::byte>(
                    reinterpret_cast<std::byte*>(raw_.data()), raw_size_))) {
                return false;
            }
            const auto* data = reinterpret_cast<const std::byte*>(raw_.data());
            std::size_t off = 0;
            auto read_u32 = [&](std::uint32_t& out) -> bool {
                if (off + 4 > raw_size_) return false;
                out = lyrics_detail::read_le_u32(data + off);
                off += 4;
                return true;
            };
            std::uint32_t vendor_len = 0;
            if (!read_u32(vendor_len) || off + vendor_len > raw_size_) return false;
            off += vendor_len;
            std::uint32_t count = 0;
            if (!read_u32(count)) return false;
            for (std::uint32_t i = 0; i < count; ++i) {
                std::uint32_t len = 0;
                if (!read_u32(len) || off + len > raw_size_) return false;
                const std::string_view comment{raw_.data() + off, len};
                off += len;
                const auto eq = comment.find('=');
                if (eq == std::string_view::npos) continue;
                const auto key = comment.substr(0, eq);
                if (lyrics_detail::equals_ci(key, "LYRICS") ||
                    lyrics_detail::equals_ci(key, "UNSYNCEDLYRICS")) {
                    const auto value = comment.substr(eq + 1);
                    return parse_and_commit(value, LyricsSourceKind::EmbeddedFlac, track_path);
                }
            }
            return false;
        }

        bool load_embedded_mp3(std::string_view track_path) noexcept {
            fs::File file{};
            const auto open_st = fs::vfs_open(track_path, file);
            if (!open_st) return false;
            const bool ok = probe_mp3_id3(file, track_path);
            (void)fs::vfs_close(file);
            return ok;
        }

        bool probe_mp3_id3(fs::File& file, std::string_view track_path) noexcept {
            std::array<std::byte, 10> head{};
            if (!read_exact(file, std::span<std::byte>(head.data(), head.size()))) return false;
            if (head[0] != static_cast<std::byte>('I') ||
                head[1] != static_cast<std::byte>('D') ||
                head[2] != static_cast<std::byte>('3')) {
                return false;
            }
            const auto ver = static_cast<unsigned char>(head[3]);
            const auto flags = static_cast<unsigned char>(head[5]);
            const std::uint32_t tag_size = lyrics_detail::read_synchsafe_u32(head.data() + 6);
            if (tag_size == 0 || tag_size > lyrics_detail::kRawReadBytes) {
                truncated_ = tag_size > lyrics_detail::kRawReadBytes;
                return false;
            }
            raw_size_ = tag_size;
            if (!read_exact(file, std::span<std::byte>(
                    reinterpret_cast<std::byte*>(raw_.data()), raw_size_))) {
                return false;
            }
            if ((flags & 0x80u) != 0) {
                remove_id3_unsync();
            }
            return parse_id3_frames(ver, flags, track_path);
        }

        void remove_id3_unsync() noexcept {
            std::size_t write = 0;
            for (std::size_t read = 0; read < raw_size_; ++read) {
                const auto cur = static_cast<unsigned char>(raw_[read]);
                raw_[write++] = raw_[read];
                if (cur == 0xFF && read + 1 < raw_size_ &&
                    static_cast<unsigned char>(raw_[read + 1]) == 0x00) {
                    ++read;
                }
            }
            raw_size_ = write;
            raw_[raw_size_] = '\0';
        }

        bool parse_id3_frames(unsigned char ver,
                              unsigned char tag_flags,
                              std::string_view track_path) noexcept {
            std::size_t off = 0;
            if ((tag_flags & 0x40u) != 0 && ver == 4 && raw_size_ >= 4) {
                const std::uint32_t ext_size = lyrics_detail::read_synchsafe_u32(
                    reinterpret_cast<const std::byte*>(raw_.data()) + off);
                if (ext_size > 0 && ext_size < raw_size_) off += ext_size;
            } else if ((tag_flags & 0x40u) != 0 && ver == 3 && raw_size_ >= 4) {
                const std::uint32_t ext_size = lyrics_detail::read_be_u32(
                    reinterpret_cast<const std::byte*>(raw_.data()) + off);
                if (ext_size > 0 && ext_size + 4 < raw_size_) off += ext_size + 4;
            }

            while (off + 10 <= raw_size_) {
                const char id0 = raw_[off + 0];
                const char id1 = raw_[off + 1];
                const char id2 = raw_[off + 2];
                const char id3 = raw_[off + 3];
                if (id0 == 0 || id1 == 0 || id2 == 0 || id3 == 0) break;
                const auto* data = reinterpret_cast<const std::byte*>(raw_.data());
                const std::uint32_t frame_size = (ver == 4)
                    ? lyrics_detail::read_synchsafe_u32(data + off + 4)
                    : lyrics_detail::read_be_u32(data + off + 4);
                const std::size_t frame_start = off + 10;
                const std::size_t frame_end = frame_start + frame_size;
                if (frame_size == 0 || frame_end > raw_size_) break;
                if (id0 == 'U' && id1 == 'S' && id2 == 'L' && id3 == 'T') {
                    if (parse_uslt_frame(std::string_view(raw_.data() + frame_start, frame_size),
                                         track_path)) {
                        return true;
                    }
                }
                if (id0 == 'S' && id1 == 'Y' && id2 == 'L' && id3 == 'T') {
                    source_ = LyricsSourceKind::UnsupportedEmbedded;
                }
                off = frame_end;
            }
            return false;
        }

        bool parse_uslt_frame(std::string_view frame, std::string_view track_path) noexcept {
            if (frame.size() < 5) return false;
            const unsigned char encoding = static_cast<unsigned char>(frame[0]);
            if (encoding != 0 && encoding != 3) {
                source_ = LyricsSourceKind::UnsupportedEmbedded;
                return false;
            }
            std::size_t off = 1 + 3;
            while (off < frame.size() && frame[off] != '\0') ++off;
            if (off < frame.size()) ++off;
            if (off >= frame.size()) return false;
            const auto text = frame.substr(off);
            return parse_and_commit(text, LyricsSourceKind::EmbeddedMp3, track_path);
        }

        service::FixedVector<LyricsLine, lyrics_detail::kMaxLines> lines_{};
        FixedString<product_config::lyrics_path_text_capacity> track_path_{};
        FixedString<product_config::lyrics_path_text_capacity> source_path_{};
        lyrics_detail::RawBuffer raw_{};
        std::size_t raw_size_{0};
        LyricsLoadStatus status_{LyricsLoadStatus::Missing};
        LyricsSourceKind source_{LyricsSourceKind::None};
        bool synced_{false};
        bool truncated_{false};
    };

    inline LyricsService& lyrics_service() noexcept {
        static LyricsService service{};
        return service;
    }

    inline LyricsLoadResult load_lyrics_for_track(std::string_view track_path) noexcept {
        return lyrics_service().load_for_track(track_path);
    }

    inline LyricsWindow resolve_lyrics_window(int position_ms) noexcept {
        return lyrics_service().window_for_position_ms(position_ms);
    }

    inline LyricsLoadResult current_lyrics_result() noexcept {
        return lyrics_service().result();
    }

    inline constexpr LyricsServiceProfile lyrics_service_profile() noexcept {
        return LyricsService::profile();
    }

#if defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU && defined(__GNUC__)
    extern "C" [[gnu::used]] void charm_player_lyrics_profile_symbols() noexcept {
        constexpr auto profile = lyrics_service_profile();
        asm volatile(
            ".global charm_player_lyrics_profile_service_size_bytes\n"
            ".set charm_player_lyrics_profile_service_size_bytes, %c0\n"
            ".global charm_player_lyrics_profile_max_lines\n"
            ".set charm_player_lyrics_profile_max_lines, %c1\n"
            ".global charm_player_lyrics_profile_line_text_capacity\n"
            ".set charm_player_lyrics_profile_line_text_capacity, %c2\n"
            ".global charm_player_lyrics_profile_raw_read_bytes\n"
            ".set charm_player_lyrics_profile_raw_read_bytes, %c3\n"
            ".global charm_player_lyrics_profile_path_text_capacity\n"
            ".set charm_player_lyrics_profile_path_text_capacity, %c4\n"
            ".global charm_player_lyrics_profile_embedded_flac_supported\n"
            ".set charm_player_lyrics_profile_embedded_flac_supported, %c5\n"
            ".global charm_player_lyrics_profile_embedded_mp3_supported\n"
            ".set charm_player_lyrics_profile_embedded_mp3_supported, %c6\n"
            ".global charm_player_lyrics_profile_embedded_m4a_supported\n"
            ".set charm_player_lyrics_profile_embedded_m4a_supported, %c7\n"
            :
            : "i"(profile.service_size_bytes),
              "i"(profile.max_lines),
              "i"(profile.line_text_capacity),
              "i"(profile.raw_read_bytes),
              "i"(profile.path_text_capacity),
              "i"(profile.embedded_flac_supported),
              "i"(profile.embedded_mp3_supported),
              "i"(profile.embedded_m4a_supported));
    }
#endif
}

#else

export namespace player {
    enum class LyricsSourceKind : std::uint8_t {
        None,
        Sidecar,
        LyricsDir,
        EmbeddedFlac,
        EmbeddedMp3,
        UnsupportedEmbedded,
    };

    enum class LyricsLoadStatus : std::uint8_t {
        Missing,
        Loaded,
        Truncated,
        Unsupported,
        ParseError,
        IoError,
    };

    struct LyricsWindow {
        const char* previous{""};
        const char* current{""};
        const char* next{""};
        int current_index{-1};
        bool synced{false};
        bool available{false};
    };

    struct LyricsLoadResult {
        LyricsLoadStatus status{LyricsLoadStatus::Missing};
        LyricsSourceKind source{LyricsSourceKind::None};
        int line_count{0};
        bool synced{false};
        bool truncated{false};
    };

    struct LyricsServiceProfile {
        std::size_t service_size_bytes{0};
        std::size_t max_lines{0};
        std::size_t line_text_capacity{0};
        std::size_t raw_read_bytes{0};
        std::size_t path_text_capacity{0};
        std::size_t embedded_flac_supported{0};
        std::size_t embedded_mp3_supported{0};
        std::size_t embedded_m4a_supported{0};
    };

    inline LyricsLoadResult load_lyrics_for_track(std::string_view) noexcept {
        return {};
    }

    inline LyricsWindow resolve_lyrics_window(int) noexcept {
        return {};
    }

    inline LyricsLoadResult current_lyrics_result() noexcept {
        return {};
    }

    inline constexpr LyricsServiceProfile lyrics_service_profile() noexcept {
        return {};
    }
}

#endif
