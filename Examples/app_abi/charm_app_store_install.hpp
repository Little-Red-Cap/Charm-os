#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::app_abi {

enum class AppStoreInstallCode : std::uint8_t {
    ok,
    invalid_argument,
    image_too_large,
    unaligned_offset,
    erase_failed,
    write_failed,
    verify_failed,
};

struct AppStoreWritableMedia {
    void* ctx{nullptr};
    std::uint32_t capacity{0};
    std::uint32_t erase_block_size{0};
    std::uint32_t write_align{0};
    bool (*erase)(void* ctx, std::uint32_t offset, std::uint32_t size) noexcept{nullptr};
    bool (*write)(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept{nullptr};
    bool (*read)(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept{nullptr};
};

struct AppStoreInstallConfig {
    AppStoreWritableMedia media{};
    std::uint32_t target_offset{0};
    std::span<const std::byte> image{};
};

struct AppStoreInstallResult {
    AppStoreInstallCode code{AppStoreInstallCode::ok};
    std::uint32_t target_offset{0};
    std::uint32_t bytes_written{0};
    std::uint32_t bytes_erased{0};
};

[[nodiscard]] constexpr std::string_view app_store_install_code_name(AppStoreInstallCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case AppStoreInstallCode::ok:
            return "ok"sv;
        case AppStoreInstallCode::invalid_argument:
            return "invalid_argument"sv;
        case AppStoreInstallCode::image_too_large:
            return "image_too_large"sv;
        case AppStoreInstallCode::unaligned_offset:
            return "unaligned_offset"sv;
        case AppStoreInstallCode::erase_failed:
            return "erase_failed"sv;
        case AppStoreInstallCode::write_failed:
            return "write_failed"sv;
        case AppStoreInstallCode::verify_failed:
            return "verify_failed"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::uint32_t app_store_install_align_up(std::uint32_t value,
                                                                  std::uint32_t alignment) noexcept {
    if (alignment <= 1U) {
        return value;
    }
    const std::uint32_t rem = value % alignment;
    return rem == 0U ? value : value + (alignment - rem);
}

[[nodiscard]] inline AppStoreInstallResult app_store_install_image(
    const AppStoreInstallConfig& config) noexcept {
    AppStoreInstallResult result{
        .target_offset = config.target_offset,
    };
    const auto& media = config.media;
    if (media.erase == nullptr || media.write == nullptr || media.read == nullptr ||
        media.capacity == 0U || media.erase_block_size == 0U || media.write_align == 0U ||
        config.image.empty() || config.image.size() > UINT32_MAX) {
        result.code = AppStoreInstallCode::invalid_argument;
        return result;
    }
    if ((config.target_offset % media.erase_block_size) != 0U ||
        (config.target_offset % media.write_align) != 0U) {
        result.code = AppStoreInstallCode::unaligned_offset;
        return result;
    }
    const auto image_size = static_cast<std::uint32_t>(config.image.size());
    if (config.target_offset > media.capacity || image_size > (media.capacity - config.target_offset)) {
        result.code = AppStoreInstallCode::image_too_large;
        return result;
    }

    const std::uint32_t erase_size = app_store_install_align_up(image_size, media.erase_block_size);
    if (erase_size > (media.capacity - config.target_offset)) {
        result.code = AppStoreInstallCode::image_too_large;
        return result;
    }
    if (!media.erase(media.ctx, config.target_offset, erase_size)) {
        result.code = AppStoreInstallCode::erase_failed;
        return result;
    }
    result.bytes_erased = erase_size;

    std::uint32_t written = 0;
    while (written < image_size) {
        std::uint32_t chunk = image_size - written;
        if (chunk > 256U) {
            chunk = 256U;
        }
        chunk = app_store_install_align_up(chunk, media.write_align);
        if (chunk > (image_size - written)) {
            chunk = image_size - written;
        }
        if (!media.write(media.ctx, config.target_offset + written, config.image.subspan(written, chunk))) {
            result.code = AppStoreInstallCode::write_failed;
            result.bytes_written = written;
            return result;
        }
        written += chunk;
    }
    result.bytes_written = written;

    std::byte verify[256]{};
    std::uint32_t verified = 0;
    while (verified < image_size) {
        std::uint32_t chunk = image_size - verified;
        if (chunk > sizeof(verify)) {
            chunk = sizeof(verify);
        }
        auto span = std::span<std::byte>{verify, chunk};
        if (!media.read(media.ctx, config.target_offset + verified, span) ||
            std::memcmp(verify, config.image.data() + verified, chunk) != 0) {
            result.code = AppStoreInstallCode::verify_failed;
            return result;
        }
        verified += chunk;
    }

    result.code = AppStoreInstallCode::ok;
    return result;
}

} // namespace charm::app_abi
