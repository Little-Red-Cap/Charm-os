module;
#include <cstdint>
#include <cstring>
#include <span>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#if defined(CHARM_ENABLE_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#endif

export module charm.font.provider_freetype;

export import charm.font;
export import charm.font.typography;
export import charm.font.provider_vfs;
export import fs_errno;
export import fs_stream;
export import fs_core;
export import fs_vfs;
export import util.core;

export namespace charm::font {
    struct FontWeightPaths {
        const char* small_path{nullptr};
        const char* normal_path{nullptr};
        const char* large_path{nullptr};
        const char* mono_path{nullptr};
    };

    struct FreetypeFontLoaderConfig {
        FontWeightPaths regular{};
        FontWeightPaths medium{};
        FontWeightPaths bold{};
        int small_px{14};
        int normal_px{16};
        int large_px{20};
        int mono_px{16};
        int embolden_medium{24}; // 26.6 fixed-point strength
        int embolden_bold{48};   // 26.6 fixed-point strength
    };

    class FreetypeFontLoader {
    public:
        FreetypeFontLoader() = default;

        void set_config(const FreetypeFontLoaderConfig& config) noexcept {
            config_ = config;
        }

        VfsFontLoaderApi vfs_api() const noexcept {
            return VfsFontLoaderApi{&load_trampoline, &reset_trampoline};
        }

        FontGlyphLoaderApi glyph_api() const noexcept {
            return FontGlyphLoaderApi{&ensure_trampoline};
        }

        void bind_glyph_loader() noexcept {
            set_font_glyph_loader(glyph_api(), this);
        }

    private:
        static bool load_host_file(std::string_view base_path,
                                   std::vector<std::uint8_t>& data) noexcept {
            if (!base_path.starts_with("/font/")) return false;
            const auto slash = base_path.find_last_of("/\\");
            if (slash == std::string_view::npos || slash + 1 >= base_path.size()) return false;
            std::string host_path{"Draft/PixelPlayer/wear/src/main/res/font/"};
            host_path.append(base_path.substr(slash + 1));
            std::FILE* fp = nullptr;
#if defined(_WIN32)
            if (fopen_s(&fp, host_path.c_str(), "rb") != 0 || !fp) return false;
#else
            fp = std::fopen(host_path.c_str(), "rb");
            if (!fp) return false;
#endif
            if (std::fseek(fp, 0, SEEK_END) != 0) {
                std::fclose(fp);
                return false;
            }
            const long size = std::ftell(fp);
            if (size <= 0) {
                std::fclose(fp);
                return false;
            }
            if (std::fseek(fp, 0, SEEK_SET) != 0) {
                std::fclose(fp);
                return false;
            }
            data.resize(static_cast<std::size_t>(size));
            const std::size_t read_bytes = std::fread(data.data(), 1, data.size(), fp);
            std::fclose(fp);
            if (read_bytes != data.size()) {
                data.clear();
                return false;
            }
            return true;
        }
        struct FaceSlot {
            std::string path{};
            int pixel_size{0};
            FontWeight weight{FontWeight::Regular};
            std::vector<std::uint8_t> data{};
#if defined(CHARM_ENABLE_FREETYPE)
            FT_Face face{nullptr};
#endif
            std::vector<Glyph> glyphs{};
            std::vector<std::vector<std::uint8_t>> bitmaps{};
            std::vector<std::uint32_t> sparse_codes{};
            std::vector<std::uint16_t> sparse_ids{};
            Font* font{nullptr};
            bool loaded{false};
        };

#if defined(CHARM_ENABLE_FREETYPE)
        bool ensure_library() noexcept {
            if (library_) return true;
            return FT_Init_FreeType(&library_) == 0;
        }
#endif

        static bool load_trampoline(void* ctx, std::string_view path, Font& out) noexcept {
            return static_cast<FreetypeFontLoader*>(ctx)->load_font(path, out);
        }

        static void reset_trampoline(void* ctx, Font& out) noexcept {
            static_cast<FreetypeFontLoader*>(ctx)->reset_font(out);
        }

        static bool ensure_trampoline(void* ctx, const Font& font, std::uint32_t code) noexcept {
            return static_cast<FreetypeFontLoader*>(ctx)->ensure_glyph(font, code);
        }

        static std::string_view strip_variant(std::string_view path) noexcept {
            const std::size_t pos = path.find('#');
            if (pos == std::string_view::npos) {
                return path;
            }
            return path.substr(0, pos);
        }

#if defined(CHARM_ENABLE_FREETYPE)
        static void select_unicode_charmap(FT_Face face) noexcept {
            if (!face) return;
            if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) == 0) return;
            for (int i = 0; i < face->num_charmaps; ++i) {
                FT_CharMap cmap = face->charmaps[i];
                if (!cmap) continue;
                if (cmap->encoding == FT_ENCODING_UNICODE) {
                    if (FT_Set_Charmap(face, cmap) == 0) return;
                }
            }
        }

        static void select_best_charmap(FT_Face face) noexcept {
            if (!face) return;
            select_unicode_charmap(face);
            if (FT_Get_Char_Index(face, static_cast<FT_ULong>('A')) != 0) return;
            for (int i = 0; i < face->num_charmaps; ++i) {
                FT_CharMap cmap = face->charmaps[i];
                if (!cmap) continue;
                if (FT_Set_Charmap(face, cmap) != 0) continue;
                if (FT_Get_Char_Index(face, static_cast<FT_ULong>('A')) != 0) return;
            }
        }
#endif

        static void debug_list_dir(std::string_view dir) noexcept {
            struct Ctx {
                std::string_view dir{};
            } ctx{dir};
            auto list_cb = [](void* user, const fs::MountOps::ListEntry& entry) noexcept -> fs::Status {
                auto* c = static_cast<Ctx*>(user);
                if (!c) return fs::Status{fs::Errc::inval};
                std::printf("[font] vfs list %.*s entry=%.*s type=%d\n",
                            static_cast<int>(c->dir.size()), c->dir.data(),
                            static_cast<int>(entry.name.size()), entry.name.data(),
                            static_cast<int>(entry.type));
                return fs::Status{fs::Errc::ok};
            };
            auto st = fs::vfs_list(dir, &ctx, list_cb);
            if (!st) {
                std::printf("[font] vfs list failed dir=%.*s err=%d\n",
                            static_cast<int>(dir.size()), dir.data(),
                            static_cast<int>(st.err));
            }
        }

        bool load_font(std::string_view path, Font& out) noexcept {
#if !defined(CHARM_ENABLE_FREETYPE)
            (void)path;
            out = Font{};
            return false;
#else
            if (!ensure_library()) {
                out = Font{};
                return false;
            }
            const std::string_view base_path = strip_variant(path);
            fs::File f{};
            auto open_st = fs::vfs_open(base_path, f);
            if (!open_st) {
                std::vector<std::uint8_t> data;
                if (!load_host_file(base_path, data)) {
                    std::fprintf(stdout, "[font] freetype open failed path=%.*s base=%.*s\n",
                                 static_cast<int>(path.size()), path.data(),
                                 static_cast<int>(base_path.size()), base_path.data());
                    std::printf("[font] freetype open err=%d\n", static_cast<int>(open_st.err));
                    std::string_view list_dir = "/";
                    const auto slash = base_path.find_last_of("/\\");
                    if (slash != std::string_view::npos) {
                        if (slash == 0) {
                            list_dir = "/";
                        } else {
                            list_dir = base_path.substr(0, slash);
                        }
                    }
                    debug_list_dir(list_dir);
                    out = Font{};
                    return false;
                }
                const auto size_px = resolve_pixel_size(path);
                const auto weight = resolve_weight(path);
                FaceSlot slot{};
                slot.path.assign(path.begin(), path.end());
                slot.pixel_size = size_px;
                slot.weight = weight;
                slot.data = std::move(data);
                slot.font = &out;

                if (FT_New_Memory_Face(library_,
                                       reinterpret_cast<const FT_Byte*>(slot.data.data()),
                                       static_cast<FT_Long>(slot.data.size()),
                                       0,
                                       &slot.face) != 0) {
                    out = Font{};
                    return false;
                }
                select_best_charmap(slot.face);
                (void)FT_Set_Pixel_Sizes(slot.face, 0, static_cast<FT_UInt>(size_px));
                out.line_height = static_cast<int>(slot.face->size->metrics.height >> 6);
                out.baseline = static_cast<int>(slot.face->size->metrics.ascender >> 6);
                out.fallback_glyph = nullptr;
                out.fallback_font = nullptr;
                out.ranges = {};

                slots_.push_back(std::move(slot));
                FaceSlot& stored = slots_.back();
                if (!ensure_glyph_for_slot(stored, static_cast<std::uint32_t>('?'))) {
                    stored.font->fallback_glyph = nullptr;
                } else if (!stored.glyphs.empty()) {
                    stored.font->fallback_glyph = &stored.glyphs[0];
                }
                stored.loaded = true;
                return true;
            }
            const auto size = f.node.size;
            if (size <= 0) {
                (void)fs::vfs_close(f);
                out = Font{};
                return false;
            }
            std::vector<std::uint8_t> data;
            data.resize(static_cast<std::size_t>(size));
            auto read_st = fs::vfs_read(f, std::span<util::u8>{reinterpret_cast<util::u8*>(data.data()), data.size()});
            (void)fs::vfs_close(f);
            if (!read_st) {
                std::fprintf(stdout, "[font] freetype read failed path=%.*s size=%lld\n",
                             static_cast<int>(path.size()), path.data(),
                             static_cast<long long>(size));
                out = Font{};
                return false;
            }

            const auto size_px = resolve_pixel_size(path);
            const auto weight = resolve_weight(path);
            FaceSlot slot{};
            slot.path.assign(path.begin(), path.end());
            slot.pixel_size = size_px;
            slot.weight = weight;
            slot.data = std::move(data);
            slot.font = &out;

            if (FT_New_Memory_Face(library_,
                                   reinterpret_cast<const FT_Byte*>(slot.data.data()),
                                   static_cast<FT_Long>(slot.data.size()),
                                   0,
                                   &slot.face) != 0) {
                out = Font{};
                return false;
            }
            select_best_charmap(slot.face);
            (void)FT_Set_Pixel_Sizes(slot.face, 0, static_cast<FT_UInt>(size_px));
            out.line_height = static_cast<int>(slot.face->size->metrics.height >> 6);
            out.baseline = static_cast<int>(slot.face->size->metrics.ascender >> 6);
            out.fallback_glyph = nullptr;
            out.fallback_font = nullptr;
            out.ranges = {};

            slots_.push_back(std::move(slot));
            FaceSlot& stored = slots_.back();
            if (!ensure_glyph_for_slot(stored, static_cast<std::uint32_t>('?'))) {
                stored.font->fallback_glyph = nullptr;
            } else if (!stored.glyphs.empty()) {
                stored.font->fallback_glyph = &stored.glyphs[0];
            }
            stored.loaded = true;
            return true;
#endif
        }

        void reset_font(Font& out) noexcept {
            for (auto& slot : slots_) {
                if (slot.font == &out) {
#if defined(CHARM_ENABLE_FREETYPE)
                    if (slot.face) {
                        FT_Done_Face(slot.face);
                    }
#endif
                    slot = FaceSlot{};
                    break;
                }
            }
            out = Font{};
        }

        bool ensure_glyph(const Font& font, std::uint32_t code) noexcept {
            for (auto& slot : slots_) {
                if (slot.font == &font) {
                    return ensure_glyph_for_slot(slot, code);
                }
            }
            return false;
        }

        bool ensure_glyph_for_slot(FaceSlot& slot, std::uint32_t code) noexcept {
#if !defined(CHARM_ENABLE_FREETYPE)
            (void)slot;
            (void)code;
            return false;
#else
            for (std::size_t i = 0; i < slot.sparse_codes.size(); ++i) {
                if (slot.sparse_codes[i] == code) return true;
            }
            if (!slot.face) return false;
            const auto glyph_index = FT_Get_Char_Index(slot.face, static_cast<FT_ULong>(code));
            if (glyph_index == 0) {
                return false;
            }
            const FT_Error load_err = FT_Load_Glyph(slot.face, glyph_index, FT_LOAD_DEFAULT);
            if (load_err != 0) {
                return false;
            }
            apply_weight(slot);
            const FT_Error render_err = FT_Render_Glyph(slot.face->glyph, FT_RENDER_MODE_NORMAL);
            if (render_err != 0) {
                return false;
            }

            const FT_GlyphSlot gslot = slot.face->glyph;
            const int w = static_cast<int>(gslot->bitmap.width);
            const int h = static_cast<int>(gslot->bitmap.rows);

            std::vector<std::uint8_t> bitmap;
            if (w > 0 && h > 0 && gslot->bitmap.buffer) {
                bitmap.resize(static_cast<std::size_t>(w * h));
                for (int y = 0; y < h; ++y) {
                    const auto* src = gslot->bitmap.buffer + y * gslot->bitmap.pitch;
                    auto* dst = bitmap.data() + y * w;
                    std::memcpy(dst, src, static_cast<std::size_t>(w));
                }
            }

            slot.bitmaps.push_back(std::move(bitmap));
            const auto& stored = slot.bitmaps.back();

            Glyph g{};
            g.bitmap = stored.empty() ? nullptr : stored.data();
            g.width = w;
            g.height = h;
            g.x_advance = static_cast<int>(gslot->advance.x >> 6);
            g.x_offset = static_cast<int>(gslot->bitmap_left);
            g.y_offset = static_cast<int>(gslot->bitmap_top);
            g.bpp = stored.empty() ? 0 : 8;

            const std::uint16_t gid = static_cast<std::uint16_t>(slot.glyphs.size());
            slot.glyphs.push_back(g);
            slot.sparse_codes.push_back(code);
            slot.sparse_ids.push_back(gid);

            slot.font->table = std::span<const Glyph>(slot.glyphs.data(), slot.glyphs.size());
            slot.font->sparse_codes = std::span<const std::uint32_t>(slot.sparse_codes.data(), slot.sparse_codes.size());
            slot.font->sparse_glyph_ids = std::span<const std::uint16_t>(slot.sparse_ids.data(), slot.sparse_ids.size());
            if (!slot.glyphs.empty()) {
                slot.font->fallback_glyph = &slot.glyphs[0];
            }
            return true;
#endif
        }

#if defined(CHARM_ENABLE_FREETYPE)
        void apply_weight(FaceSlot& slot) noexcept {
            if (!slot.face || slot.weight == FontWeight::Regular) return;
            FT_GlyphSlot gslot = slot.face->glyph;
            if (slot.weight == FontWeight::Bold) {
                FT_GlyphSlot_Embolden(gslot);
                FT_GlyphSlot_Embolden(gslot);
                if (config_.embolden_bold > 0) {
                    FT_GlyphSlot_Embolden(gslot);
                }
                return;
            }
            if (slot.weight == FontWeight::Medium) {
                FT_GlyphSlot_Embolden(gslot);
                if (config_.embolden_medium > 0) {
                    FT_GlyphSlot_Embolden(gslot);
                }
            }
        }
#endif

        int resolve_pixel_size(std::string_view path) const noexcept {
            const std::size_t pos = path.find('#');
            if (pos != std::string_view::npos) {
                const std::string_view suffix = path.substr(pos + 1);
                if (suffix == "small") return config_.small_px;
                if (suffix == "normal") return config_.normal_px;
                if (suffix == "large") return config_.large_px;
                if (suffix == "mono") return config_.mono_px;
                if (suffix.starts_with("px")) {
                    int value = 0;
                    for (std::size_t i = 2; i < suffix.size(); ++i) {
                        const char c = suffix[i];
                        if (c < '0' || c > '9') break;
                        value = value * 10 + (c - '0');
                    }
                    if (value > 0) return value;
                }
            }
            if (path_matches(config_.regular.small_path, path)
                || path_matches(config_.medium.small_path, path)
                || path_matches(config_.bold.small_path, path)) {
                return config_.small_px;
            }
            if (path_matches(config_.regular.large_path, path)
                || path_matches(config_.medium.large_path, path)
                || path_matches(config_.bold.large_path, path)) {
                return config_.large_px;
            }
            if (path_matches(config_.regular.mono_path, path)
                || path_matches(config_.medium.mono_path, path)
                || path_matches(config_.bold.mono_path, path)) {
                return config_.mono_px;
            }
            return config_.normal_px;
        }

        FontWeight resolve_weight(std::string_view path) const noexcept {
            const std::size_t pos = path.find('#');
            if (pos != std::string_view::npos) {
                const std::string_view suffix = path.substr(pos + 1);
                if (suffix.find("bold") != std::string_view::npos) {
                    return FontWeight::Bold;
                }
                if (suffix.find("medium") != std::string_view::npos) {
                    return FontWeight::Medium;
                }
            }
            if (path_matches(config_.bold.small_path, path)
                || path_matches(config_.bold.normal_path, path)
                || path_matches(config_.bold.large_path, path)
                || path_matches(config_.bold.mono_path, path)) {
                return FontWeight::Bold;
            }
            if (path_matches(config_.medium.small_path, path)
                || path_matches(config_.medium.normal_path, path)
                || path_matches(config_.medium.large_path, path)
                || path_matches(config_.medium.mono_path, path)) {
                return FontWeight::Medium;
            }
            return FontWeight::Regular;
        }

        static bool path_matches(const char* config_path, std::string_view path) noexcept {
            return config_path && path == std::string_view{config_path};
        }

        FreetypeFontLoaderConfig config_{};
        std::vector<FaceSlot> slots_{};
#if defined(CHARM_ENABLE_FREETYPE)
        FT_Library library_{nullptr};
#endif
    };
}
