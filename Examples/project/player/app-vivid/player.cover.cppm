module;
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

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

    void release_cover_image(CoverImage& img) {
        if (ui::gfx::image_id_valid(img.image_id)) {
            ui::gfx::unregister_image(img.image_id);
        }
        img = {};
    }

    bool load_cover_image(std::string_view path, CoverImage& out) {
        release_cover_image(out);
        if (path.empty()) return false;

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

        int w = 0;
        int h = 0;
        int comp = 0;
        unsigned char* rgba = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(buffer.data()),
            static_cast<int>(offset),
            &w, &h, &comp, 4);
        if (!rgba || w <= 0 || h <= 0) {
            if (rgba) stbi_image_free(rgba);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] decode failed: %.*s\n",
                        static_cast<int>(path.size()), path.data());
#endif
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
        out.path.assign(path.begin(), path.end());
        const auto view = make_image_view(
            PixelFormat::ARGB8888,
            w,
            h,
            w * 4,
            reinterpret_cast<const std::byte*>(out.argb.data()),
            false,
            false);
        out.image_id = ui::gfx::register_image_dedup(view);
#if defined(CHARM_PLAYER_COVER_DEBUG)
        std::printf("[cover] loaded: %.*s %dx%d id=%u/%u\n",
                    static_cast<int>(path.size()), path.data(), w, h,
                    out.image_id.slot, out.image_id.generation);
#endif
        return ui::gfx::image_id_valid(out.image_id);
    }
}
