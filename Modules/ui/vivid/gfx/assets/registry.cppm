module;
#include <cstddef>
export module charm.gfx.assets.registry;

import charm.gfx.image;
import charm.gfx.assets.benchmark;
import charm.gfx.assets.render;

export
enum class ImageId {
    BenchmarkLogoRgb,
    BenchmarkLogoArgb,
    BenchmarkAvatar,
    RenderLogoRgb,
    RenderLogoArgb,
    RenderLogoPremul,
    RenderLogoXrgb
};

export
enum class ImageGroup {
    Benchmark,
    Render
};

export
struct ImageEntry {
    ImageId id{};
    ImageGroup group{};
    const char* name{};
};

export
inline const ImageEntry* image_registry(std::size_t& count) noexcept {
    static const ImageEntry entries[] = {
        {ImageId::BenchmarkLogoRgb, ImageGroup::Benchmark, "benchmark_logo_rgb"},
        {ImageId::BenchmarkLogoArgb, ImageGroup::Benchmark, "benchmark_logo_argb"},
        {ImageId::BenchmarkAvatar, ImageGroup::Benchmark, "benchmark_avatar_argb"},
        {ImageId::RenderLogoRgb, ImageGroup::Render, "render_logo_rgb888"},
        {ImageId::RenderLogoArgb, ImageGroup::Render, "render_logo_argb8888"},
        {ImageId::RenderLogoPremul, ImageGroup::Render, "render_logo_argb8888_premul"},
        {ImageId::RenderLogoXrgb, ImageGroup::Render, "render_logo_xrgb8888"},
    };
    count = sizeof(entries) / sizeof(entries[0]);
    return entries;
}

export
constexpr const char* image_group_name(ImageGroup group) noexcept {
    switch (group) {
    case ImageGroup::Benchmark: return "benchmark";
    case ImageGroup::Render: return "render";
    default: return "unknown";
    }
}

export
inline bool image_in_group(ImageId id, ImageGroup group) noexcept {
    switch (id) {
    case ImageId::BenchmarkLogoRgb:
    case ImageId::BenchmarkLogoArgb:
    case ImageId::BenchmarkAvatar:
        return group == ImageGroup::Benchmark;
    case ImageId::RenderLogoRgb:
    case ImageId::RenderLogoArgb:
    case ImageId::RenderLogoPremul:
    case ImageId::RenderLogoXrgb:
        return group == ImageGroup::Render;
    default:
        return false;
    }
}

export
inline const char* image_name(ImageId id) noexcept {
    switch (id) {
    case ImageId::BenchmarkLogoRgb: return "benchmark_logo_rgb";
    case ImageId::BenchmarkLogoArgb: return "benchmark_logo_argb";
    case ImageId::BenchmarkAvatar: return "benchmark_avatar_argb";
    case ImageId::RenderLogoRgb: return "render_logo_rgb888";
    case ImageId::RenderLogoArgb: return "render_logo_argb8888";
    case ImageId::RenderLogoPremul: return "render_logo_argb8888_premul";
    case ImageId::RenderLogoXrgb: return "render_logo_xrgb8888";
    default: return "unknown";
    }
}

export
inline ImageView get_image(ImageId id) noexcept {
    switch (id) {
    case ImageId::BenchmarkLogoRgb: return benchmark_logo_rgb();
    case ImageId::BenchmarkLogoArgb: return benchmark_logo_argb();
    case ImageId::BenchmarkAvatar: return benchmark_avatar_argb();
    case ImageId::RenderLogoRgb: return render_logo_rgb();
    case ImageId::RenderLogoArgb: return render_logo_argb();
    case ImageId::RenderLogoPremul: return render_logo_premul();
    case ImageId::RenderLogoXrgb: return render_logo_xrgb();
    default: return {};
    }
}
