module;
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>
#include <dr_flac.h>

export module player.cover;

import charm.gfx.image;
import fs_core;
import fs_vfs;
import util.core;

export namespace player {
    struct CoverImage {
        std::string path{};
        std::vector<std::uint32_t> argb{};
        ui::gfx::ImageId image_id{ui::gfx::invalid_image_id()};
        int width{0};
        int height{0};
    };

    namespace detail {
        bool is_flac_path(std::string_view path) noexcept {
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= path.size()) return false;
            const auto ext = path.substr(dot + 1);
            if (ext.size() != 4) return false;
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
            return a == 'f' && b == 'l' && c == 'a' && d == 'c';
        }

        bool decode_image_from_memory(const std::byte* data,
                                      std::size_t size,
                                      CoverImage& out,
                                      std::string_view path_tag) {
            int w = 0;
            int h = 0;
            int comp = 0;
            unsigned char* rgba = stbi_load_from_memory(
                reinterpret_cast<const unsigned char*>(data),
                static_cast<int>(size),
                &w, &h, &comp, 4);
            if (!rgba || w <= 0 || h <= 0) {
                if (rgba) stbi_image_free(rgba);
                return false;
            }

            out.argb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
            auto* dst = reinterpret_cast<std::uint8_t*>(out.argb.data());
            for (std::size_t i = 0; i < out.argb.size(); ++i) {
                const std::size_t base = i * 4;
                const std::uint8_t r = rgba[base + 0];
                const std::uint8_t g = rgba[base + 1];
                const std::uint8_t b = rgba[base + 2];
                const std::uint8_t a = rgba[base + 3];
                dst[base + 0] = a;
                dst[base + 1] = r;
                dst[base + 2] = g;
                dst[base + 3] = b;
            }
            stbi_image_free(rgba);

            out.width = w;
            out.height = h;
            out.path.assign(path_tag.begin(), path_tag.end());
            const auto view = make_image_view(
                PixelFormat::ARGB8888,
                w,
                h,
                w * 4,
                reinterpret_cast<const std::byte*>(out.argb.data()),
                false,
                false);
            out.image_id = ui::gfx::register_image_dedup(view);
            return ui::gfx::image_id_valid(out.image_id);
        }

        inline std::uint32_t read_be_u32(const std::byte* data) noexcept {
            const auto b0 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[0]));
            const auto b1 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[1]));
            const auto b2 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[2]));
            const auto b3 = static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]));
            return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        }

        inline bool starts_with_ci(std::string_view s, std::string_view prefix) noexcept {
            if (s.size() < prefix.size()) return false;
            for (std::size_t i = 0; i < prefix.size(); ++i) {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[i])));
                if (a != b) return false;
            }
            return true;
        }

        inline bool equals_ci(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
                const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
                if (ca != cb) return false;
            }
            return true;
        }

        std::vector<std::byte> base64_decode(std::string_view input) {
            auto decode_val = [](char c) noexcept -> int {
                if (c >= 'A' && c <= 'Z') return c - 'A';
                if (c >= 'a' && c <= 'z') return c - 'a' + 26;
                if (c >= '0' && c <= '9') return c - '0' + 52;
                if (c == '+') return 62;
                if (c == '/') return 63;
                return -1;
            };

            std::vector<std::byte> out;
            out.reserve(input.size() * 3 / 4);
            int val = 0;
            int valb = -8;
            for (char c : input) {
                if (c == '=' || std::isspace(static_cast<unsigned char>(c))) {
                    if (c == '=') break;
                    continue;
                }
                const int d = decode_val(c);
                if (d < 0) continue;
                val = (val << 6) | d;
                valb += 6;
                if (valb >= 0) {
                    const unsigned char byte = static_cast<unsigned char>((val >> valb) & 0xFF);
                    out.push_back(static_cast<std::byte>(byte));
                    valb -= 8;
                }
            }
            return out;
        }

        struct FlacCoverContext {
            std::vector<std::byte> picture{};
            int best_rank{-1};
        };

        inline int picture_rank(drflac_uint32 type) noexcept {
            if (type == DRFLAC_PICTURE_TYPE_COVER_FRONT) return 0;
            return 1;
        }

        bool parse_picture_block(const std::byte* data, std::size_t size, FlacCoverContext& ctx) {
            if (!data || size < 32) return false;
            std::size_t off = 0;
            auto read_u32 = [&](std::uint32_t& out) -> bool {
                if (off + 4 > size) return false;
                out = read_be_u32(data + off);
                off += 4;
                return true;
            };

            std::uint32_t pic_type = 0;
            std::uint32_t mime_len = 0;
            std::uint32_t desc_len = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t depth = 0;
            std::uint32_t colors = 0;
            std::uint32_t data_len = 0;
            if (!read_u32(pic_type)) return false;
            if (!read_u32(mime_len)) return false;
            if (off + mime_len > size) return false;
            off += mime_len;
            if (!read_u32(desc_len)) return false;
            if (off + desc_len > size) return false;
            off += desc_len;
            if (!read_u32(width)) return false;
            if (!read_u32(height)) return false;
            if (!read_u32(depth)) return false;
            if (!read_u32(colors)) return false;
            if (!read_u32(data_len)) return false;
            if (off + data_len > size) return false;

            const int rank = picture_rank(pic_type);
            if (ctx.best_rank >= 0 && rank >= ctx.best_rank) return false;
            ctx.picture.assign(data + off, data + off + data_len);
            ctx.best_rank = rank;
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] flac picture (vorbis) type=%u size=%u\n",
                        static_cast<unsigned int>(pic_type),
                        static_cast<unsigned int>(data_len));
#endif
            return true;
        }

        struct FlacFileReader {
            fs::File* file{nullptr};
            std::size_t size{0};
            std::size_t base_offset{0};
        };

        struct FlacMetaContext {
            FlacFileReader reader{};
            FlacCoverContext cover{};
        };

        inline void on_flac_meta(void* user, drflac_metadata* meta) {
            if (!user || !meta) return;
            auto* ctx = static_cast<FlacMetaContext*>(user);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] flac meta type=%u\n", static_cast<unsigned int>(meta->type));
#endif
            if (meta->type == DRFLAC_METADATA_BLOCK_TYPE_PICTURE) {
                const auto& pic = meta->data.picture;
                if (pic.pictureDataSize == 0 || !pic.pPictureData) return;
                const int rank = picture_rank(pic.type);
                if (ctx->cover.best_rank >= 0 && rank >= ctx->cover.best_rank) return;
                if (pic.pictureDataSize > (8u * 1024u * 1024u)) return;
                ctx->cover.picture.assign(reinterpret_cast<const std::byte*>(pic.pPictureData),
                                          reinterpret_cast<const std::byte*>(pic.pPictureData + pic.pictureDataSize));
                ctx->cover.best_rank = rank;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac picture type=%u size=%u\n",
                            static_cast<unsigned int>(pic.type),
                            static_cast<unsigned int>(pic.pictureDataSize));
#endif
                return;
            }

            if (meta->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac vorbis comments=%u\n",
                            static_cast<unsigned int>(meta->data.vorbis_comment.commentCount));
#endif
                drflac_vorbis_comment_iterator it{};
                drflac_init_vorbis_comment_iterator(&it,
                                                    meta->data.vorbis_comment.commentCount,
                                                    meta->data.vorbis_comment.pComments);
                drflac_uint32 len = 0;
                const char* comment = nullptr;
                while ((comment = drflac_next_vorbis_comment(&it, &len)) != nullptr) {
                    if (len == 0) continue;
                    std::string_view entry{comment, len};
                    const auto sep = entry.find('=');
                    if (sep == std::string_view::npos || sep + 1 >= entry.size()) continue;
                    const auto key = entry.substr(0, sep);
                    const auto value = entry.substr(sep + 1);
                    if (value.empty()) continue;

                    if (equals_ci(key, "METADATA_BLOCK_PICTURE")) {
                        const auto b64 = value;
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] flac picture comment len=%u b64=%zu\n",
                                    static_cast<unsigned int>(len),
                                    static_cast<std::size_t>(b64.size()));
#endif
                        auto decoded = base64_decode(b64);
                        if (decoded.empty()) continue;
                    if (parse_picture_block(decoded.data(), decoded.size(), ctx->cover)) {
                        return;
                    }
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] flac picture parse failed\n");
#endif
                        continue;
                    }

                    if (equals_ci(key, "COVERART")) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                        std::printf("[cover] flac coverart len=%u b64=%zu\n",
                                    static_cast<unsigned int>(len),
                                    static_cast<std::size_t>(value.size()));
#endif
                        auto decoded = base64_decode(value);
                        if (decoded.empty()) continue;
                        if (decoded.size() > (8u * 1024u * 1024u)) continue;
                        if (!ctx->cover.picture.empty()) continue;
                        ctx->cover.picture = std::move(decoded);
                        ctx->cover.best_rank = picture_rank(DRFLAC_PICTURE_TYPE_COVER_FRONT);
                        return;
                    }
                }
            }
        }

        inline std::size_t flac_read(void* user, void* buffer_out, std::size_t bytes_to_read) {
            auto* ctx = static_cast<FlacMetaContext*>(user);
            auto* reader = ctx ? &ctx->reader : nullptr;
            if (!reader || !reader->file) return 0;
            std::size_t to_read = bytes_to_read;
            if (reader->file->node.size > 0 && reader->file->node.offset >= 0) {
                const auto remaining = static_cast<std::int64_t>(reader->size - reader->file->node.offset);
                if (remaining <= 0) return 0;
                if (static_cast<std::uint64_t>(remaining) < to_read) {
                    to_read = static_cast<std::size_t>(remaining);
                }
            }
            const auto before = reader->file->node.offset;
            auto st = fs::read(*reader->file,
                               std::span<util::u8>(reinterpret_cast<util::u8*>(buffer_out), to_read));
            if (!st) return 0;
            const auto after = reader->file->node.offset;
            if (after >= before) {
                const auto delta = static_cast<std::size_t>(after - before);
                return (delta <= to_read) ? delta : to_read;
            }
            return to_read;
        }

        inline drflac_bool32 flac_seek(void* user, int offset, drflac_seek_origin origin) {
            auto* ctx = static_cast<FlacMetaContext*>(user);
            auto* reader = ctx ? &ctx->reader : nullptr;
            if (!reader || !reader->file) return DRFLAC_FALSE;
            const util::i64 base = static_cast<util::i64>(reader->base_offset);
            util::i64 rel = 0;
            if (origin == DRFLAC_SEEK_SET) {
                rel = offset;
            } else if (origin == DRFLAC_SEEK_CUR) {
                rel = (reader->file->node.offset - base) + offset;
            } else if (origin == DRFLAC_SEEK_END) {
                rel = static_cast<util::i64>(reader->size - reader->base_offset) + offset;
            }
            if (rel < 0) rel = 0;
            const util::i64 target = base + rel;
            if (target < base) return DRFLAC_FALSE;
            auto st = fs::vfs_seek(*reader->file, target);
            return st ? DRFLAC_TRUE : DRFLAC_FALSE;
        }

        inline drflac_bool32 flac_tell(void* user, drflac_int64* cursor) {
            auto* ctx = static_cast<FlacMetaContext*>(user);
            auto* reader = ctx ? &ctx->reader : nullptr;
            if (!reader || !reader->file || !cursor) return DRFLAC_FALSE;
            const auto base = static_cast<drflac_int64>(reader->base_offset);
            *cursor = static_cast<drflac_int64>(reader->file->node.offset) - base;
            return DRFLAC_TRUE;
        }

        bool load_flac_cover(std::string_view path, CoverImage& out) {
            fs::File f{};
            auto st = fs::vfs_open(path, f);
            if (!st) return false;
            if (f.node.size <= 0) {
                (void)fs::vfs_close(f);
                return false;
            }
            (void)fs::vfs_seek(f, 0);
            std::array<std::byte, 10> head{};
            if (fs::read(f, std::span<util::u8>(
                    reinterpret_cast<util::u8*>(head.data()), head.size()))) {
                const bool is_flac = head[0] == std::byte{'f'} && head[1] == std::byte{'L'}
                    && head[2] == std::byte{'a'} && head[3] == std::byte{'C'};
                const bool is_id3 = head[0] == std::byte{'I'} && head[1] == std::byte{'D'}
                    && head[2] == std::byte{'3'};
                if (is_id3) {
                    const auto b6 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[6]));
                    const auto b7 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[7]));
                    const auto b8 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[8]));
                    const auto b9 = static_cast<std::uint32_t>(static_cast<unsigned char>(head[9]));
                    const std::uint32_t tag_size = (b6 << 21) | (b7 << 14) | (b8 << 7) | b9;
                    if (tag_size > 0 && tag_size < f.node.size) {
                        const auto base = static_cast<util::i64>(10u + tag_size);
                        (void)fs::vfs_seek(f, base);
                        FlacMetaContext meta{};
                        meta.reader = FlacFileReader{&f, static_cast<std::size_t>(f.node.size), static_cast<std::size_t>(base)};
                        drflac* flac = drflac_open_with_metadata(
                            flac_read,
                            flac_seek,
                            flac_tell,
                            on_flac_meta,
                            &meta,
                            nullptr);
                        if (!flac) {
                            flac = drflac_open_with_metadata_relaxed(
                                flac_read,
                                flac_seek,
                                flac_tell,
                                on_flac_meta,
                                drflac_container_native,
                                &meta,
                                nullptr);
                        }
                        if (!flac) {
                            flac = drflac_open_with_metadata_relaxed(
                                flac_read,
                                flac_seek,
                                flac_tell,
                                on_flac_meta,
                                drflac_container_ogg,
                                &meta,
                                nullptr);
                        }
                        if (flac) {
                            drflac_close(flac);
                        } else {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                            std::printf("[cover] flac open failed (id3): %.*s\n",
                                        static_cast<int>(path.size()), path.data());
#endif
                        }
                        (void)fs::vfs_close(f);
                        if (!meta.cover.picture.empty()) {
                            return decode_image_from_memory(meta.cover.picture.data(), meta.cover.picture.size(), out, path);
                        }
                        return false;
                    }
                }
                (void)fs::vfs_seek(f, 0);
            } else {
                (void)fs::vfs_seek(f, 0);
            }

            FlacMetaContext meta{};
            meta.reader = FlacFileReader{&f, static_cast<std::size_t>(f.node.size), 0};
            drflac* flac = drflac_open_with_metadata(
                flac_read,
                flac_seek,
                flac_tell,
                on_flac_meta,
                &meta,
                nullptr);
            if (!flac) {
                flac = drflac_open_with_metadata_relaxed(
                    flac_read,
                    flac_seek,
                    flac_tell,
                    on_flac_meta,
                    drflac_container_native,
                    &meta,
                    nullptr);
            }
            if (!flac) {
                flac = drflac_open_with_metadata_relaxed(
                    flac_read,
                    flac_seek,
                    flac_tell,
                    on_flac_meta,
                    drflac_container_ogg,
                    &meta,
                    nullptr);
            }
            if (flac) {
                drflac_close(flac);
            } else {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac open failed: %.*s\n",
                            static_cast<int>(path.size()), path.data());
#endif
            }
            (void)fs::vfs_close(f);
            if (meta.cover.picture.empty()) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] flac picture none: %.*s\n",
                            static_cast<int>(path.size()), path.data());
#endif
                return false;
            }
            return decode_image_from_memory(meta.cover.picture.data(), meta.cover.picture.size(), out, path);
        }
    } // namespace detail

    void release_cover_image(CoverImage& img) {
        if (ui::gfx::image_id_valid(img.image_id)) {
            ui::gfx::unregister_image(img.image_id);
        }
        img = {};
    }

    bool load_cover_image(std::string_view path, CoverImage& out) {
        release_cover_image(out);
        if (path.empty()) return false;
        if (detail::is_flac_path(path)) {
            return detail::load_flac_cover(path, out);
        }

        fs::File f{};
        const auto st = fs::vfs_open(path, f);
        if (!st) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] open failed: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }
        const auto size = static_cast<std::size_t>(f.node.size);
        if (size == 0 || size > (8u * 1024u * 1024u)) {
            (void)fs::vfs_close(f);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] invalid size: %.*s size=%zu\n",
                        static_cast<int>(path.size()), path.data(), size);
#endif
            return false;
        }

        std::vector<std::byte> buffer;
        buffer.resize(size);
        std::size_t offset = 0;
        while (offset < size) {
            const std::size_t chunk = std::min<std::size_t>(size - offset, 64 * 1024);
            const auto before = f.node.offset;
            const auto st_read = fs::read(f, std::span<util::u8>(
                reinterpret_cast<util::u8*>(buffer.data() + offset), chunk));
        if (!st_read) {
            (void)fs::vfs_close(f);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] read failed: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }
            const auto after = f.node.offset;
            if (after <= before) break;
            const std::size_t read = static_cast<std::size_t>(after - before);
            offset += read;
        }
        (void)fs::vfs_close(f);
        if (offset == 0) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] empty read: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }

        if (!detail::decode_image_from_memory(buffer.data(), offset, out, path)) {
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] decode failed: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
            return false;
        }
#if defined(CHARM_PLAYER_COVER_DEBUG)
        std::printf("[cover] loaded: %.*s %dx%d id=%u/%u\n",
                    static_cast<int>(path.size()), path.data(), out.width, out.height,
                    out.image_id.slot, out.image_id.generation);
#endif
        return true;
    }
}
