#pragma once

#include "charm_dev_loader.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace charm::dev_loader {

enum class ReceivedImageReadCode : std::uint8_t {
    ok,
    invalid_argument,
    not_launch_ready,
    empty_image,
    output_too_small,
    read_failed,
};

struct ReceivedImageReadConfig {
    Result status{};
    ImageManifest manifest{};
    Storage storage{};
    std::span<std::byte> output{};
};

struct ReceivedImageReadResult {
    ReceivedImageReadCode code{ReceivedImageReadCode::ok};
    std::span<const std::byte> image{};
    std::uint32_t bytes_read{0};
};

[[nodiscard]] constexpr std::string_view received_image_read_code_name(ReceivedImageReadCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case ReceivedImageReadCode::ok:
            return "ok"sv;
        case ReceivedImageReadCode::invalid_argument:
            return "invalid_argument"sv;
        case ReceivedImageReadCode::not_launch_ready:
            return "not_launch_ready"sv;
        case ReceivedImageReadCode::empty_image:
            return "empty_image"sv;
        case ReceivedImageReadCode::output_too_small:
            return "output_too_small"sv;
        case ReceivedImageReadCode::read_failed:
            return "read_failed"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] inline ReceivedImageReadResult received_image_read(
    const ReceivedImageReadConfig& config) noexcept {
    if (config.storage.read == nullptr || config.output.data() == nullptr ||
        config.storage.capacity_bytes == 0U || config.storage.base_address == 0U) {
        return {.code = ReceivedImageReadCode::invalid_argument};
    }
    if (config.status.stage != Stage::launch_ready || config.status.code != Code::ok) {
        return {.code = ReceivedImageReadCode::not_launch_ready};
    }
    if (!manifest_valid(config.manifest)) {
        return {.code = ReceivedImageReadCode::invalid_argument};
    }
    if (config.manifest.size_bytes == 0U) {
        return {.code = ReceivedImageReadCode::empty_image};
    }
    if (config.manifest.load_address < config.storage.base_address ||
        config.manifest.size_bytes >
            (config.storage.capacity_bytes - (config.manifest.load_address - config.storage.base_address))) {
        return {.code = ReceivedImageReadCode::invalid_argument};
    }
    if (config.manifest.size_bytes > config.output.size()) {
        return {.code = ReceivedImageReadCode::output_too_small};
    }

    const std::uint32_t storage_offset = config.manifest.load_address - config.storage.base_address;
    auto destination = config.output.first(config.manifest.size_bytes);
    if (!config.storage.read(config.storage.ctx, storage_offset, destination)) {
        return {.code = ReceivedImageReadCode::read_failed};
    }

    return ReceivedImageReadResult{
        .code = ReceivedImageReadCode::ok,
        .image = std::span<const std::byte>{destination.data(), destination.size()},
        .bytes_read = config.manifest.size_bytes,
    };
}

} // namespace charm::dev_loader
