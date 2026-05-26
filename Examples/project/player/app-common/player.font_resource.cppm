module;

#include <cstddef>
#include <string_view>

export module player.font_resource;

import player.app_config;
import player.product_config;

export namespace player {
    struct PlayerFontPackageResourceView {
        const std::byte* data{nullptr};
        std::size_t size_bytes{0};
        std::string_view key{};
        int small_px{product_config::default_font_small_px};
        int normal_px{product_config::default_font_normal_px};
        int large_px{product_config::default_font_large_px};

        [[nodiscard]] bool valid() const noexcept {
            return data != nullptr && size_bytes > 0 && !key.empty();
        }
    };

    inline FontResourceConfig make_builtin_font_resource_config(int small_px = product_config::default_font_small_px,
                                                                int normal_px = product_config::default_font_normal_px,
                                                                int large_px = product_config::default_font_large_px) noexcept {
        FontResourceConfig out{};
        out.kind = FontResourceKind::Builtin;
        out.small_px = small_px;
        out.normal_px = normal_px;
        out.large_px = large_px;
        return out;
    }

    inline FontResourceConfig make_file_font_resource_config(std::string_view primary_path,
                                                             std::string_view fallback_path = {},
                                                             int small_px = product_config::default_font_small_px,
                                                             int normal_px = product_config::default_font_normal_px,
                                                             int large_px = product_config::default_font_large_px) noexcept {
        FontResourceConfig out = make_builtin_font_resource_config(small_px, normal_px, large_px);
        if (!primary_path.empty()) {
            out.kind = FontResourceKind::FilePath;
            out.primary_path.assign(primary_path);
            out.fallback_path.assign(fallback_path);
        }
        return out;
    }

    inline FontResourceConfig make_package_font_resource_config(
        const PlayerFontPackageResourceView& package) noexcept {
        if (!package.valid()) {
            return make_builtin_font_resource_config();
        }
        FontResourceConfig out = make_builtin_font_resource_config(package.small_px,
                                                                   package.normal_px,
                                                                   package.large_px);
        out.kind = FontResourceKind::Package;
        out.package_data = package.data;
        out.package_size_bytes = package.size_bytes;
        out.package_key.assign(package.key);
        return out;
    }

    inline void apply_font_resource_overrides(FontResourceConfig& font,
                                              int small_px,
                                              int normal_px,
                                              int large_px) noexcept {
        if (small_px > 0) {
            font.small_px = small_px;
        }
        if (normal_px > 0) {
            font.normal_px = normal_px;
        }
        if (large_px > 0) {
            font.large_px = large_px;
        }
    }
}
