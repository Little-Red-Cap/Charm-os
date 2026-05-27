#pragma once

#include "charm_app_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::app_abi {

inline constexpr std::uint32_t kAppReceivedImageMaxName = 48U;

enum class AppReceivedImageStageCode : std::uint8_t {
    ok,
    invalid_argument,
    not_verified,
    empty_image,
    buffer_too_small,
    format_unsupported,
    name_too_long,
};

struct AppReceivedImageStageConfig {
    std::string_view name{};
    AppImageFormat format{AppImageFormat::elf};
    std::span<const std::byte> image{};
    bool verified{false};
    std::span<std::byte> cache{};
};

struct AppReceivedImageStageResult {
    AppReceivedImageStageCode code{AppReceivedImageStageCode::ok};
    AppImage image{};
    std::uint32_t bytes_copied{0};
};

[[nodiscard]] constexpr std::string_view app_received_image_stage_code_name(
    AppReceivedImageStageCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case AppReceivedImageStageCode::ok:
            return "ok"sv;
        case AppReceivedImageStageCode::invalid_argument:
            return "invalid_argument"sv;
        case AppReceivedImageStageCode::not_verified:
            return "not_verified"sv;
        case AppReceivedImageStageCode::empty_image:
            return "empty_image"sv;
        case AppReceivedImageStageCode::buffer_too_small:
            return "buffer_too_small"sv;
        case AppReceivedImageStageCode::format_unsupported:
            return "format_unsupported"sv;
        case AppReceivedImageStageCode::name_too_long:
            return "name_too_long"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr bool app_received_image_format_supported(AppImageFormat format) noexcept {
    switch (format) {
        case AppImageFormat::function:
        case AppImageFormat::elf:
        case AppImageFormat::modulex:
            return true;
    }
    return false;
}

[[nodiscard]] inline AppReceivedImageStageResult app_received_image_stage(
    const AppReceivedImageStageConfig& config) noexcept {
    if (config.name.empty() || config.cache.data() == nullptr) {
        return {.code = AppReceivedImageStageCode::invalid_argument};
    }
    if (!config.verified) {
        return {.code = AppReceivedImageStageCode::not_verified};
    }
    if (config.image.empty()) {
        return {.code = AppReceivedImageStageCode::empty_image};
    }
    if (config.image.data() == nullptr) {
        return {.code = AppReceivedImageStageCode::invalid_argument};
    }
    if (!app_received_image_format_supported(config.format)) {
        return {.code = AppReceivedImageStageCode::format_unsupported};
    }
    if (config.name.size() + 1U > kAppReceivedImageMaxName) {
        return {.code = AppReceivedImageStageCode::name_too_long};
    }
    if (config.image.size() > config.cache.size() || config.image.size() > UINT32_MAX) {
        return {.code = AppReceivedImageStageCode::buffer_too_small};
    }

    std::memcpy(config.cache.data(), config.image.data(), config.image.size());
    return AppReceivedImageStageResult{
        .code = AppReceivedImageStageCode::ok,
        .image = AppImage{
            .name = config.name,
            .format = config.format,
            .image_base = config.cache.data(),
            .image_size = config.image.size(),
        },
        .bytes_copied = static_cast<std::uint32_t>(config.image.size()),
    };
}

} // namespace charm::app_abi
