#pragma once

#include "charm_app_runtime.hpp"

#include <string_view>

namespace charm::app_abi {

struct StagedAppImageSource {
    AppImage image{};
    void* load_ctx{nullptr};
    AppLoadResult (*load)(void* ctx,
                          const AppImage& image,
                          const AppLoadBuffer& buffer) noexcept{nullptr};
};

[[nodiscard]] inline const AppImage* find_staged_app_image(void* ctx,
                                                           std::string_view name) noexcept {
    auto* source = static_cast<StagedAppImageSource*>(ctx);
    if (source == nullptr || source->image.name != name) {
        return nullptr;
    }
    return &source->image;
}

[[nodiscard]] inline AppLoadResult load_staged_app_image(void* ctx,
                                                         const AppImage& image,
                                                         const AppLoadBuffer& buffer) noexcept {
    auto* source = static_cast<StagedAppImageSource*>(ctx);
    if (source == nullptr || source->load == nullptr || &source->image != &image) {
        return {.code = AppRunCode::image_not_found};
    }
    return source->load(source->load_ctx, image, buffer);
}

[[nodiscard]] inline AppImageSource make_staged_app_image_source(
    StagedAppImageSource& staged) noexcept {
    return AppImageSource{
        .ctx = &staged,
        .find = find_staged_app_image,
        .load = load_staged_app_image,
    };
}

} // namespace charm::app_abi
