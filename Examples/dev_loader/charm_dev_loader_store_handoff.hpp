#pragma once

#include "charm_app_store.hpp"
#include "charm_app_store_install.hpp"
#include "charm_dev_loader_received_image.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::dev_loader {

struct StoreInstallReceivedConfig {
    ReceivedImageReadConfig received{};
    app_abi::AppStoreWritableMedia media{};
    std::uint32_t target_offset{0};
    bool validate_store_header{true};
};

struct StoreInstallReceivedResult {
    ReceivedImageReadResult received{};
    app_abi::AppStoreReadCode store_code{app_abi::AppStoreReadCode::invalid_argument};
    app_abi::AppStoreHeader header{};
    app_abi::AppStoreInstallResult install{
        .code = app_abi::AppStoreInstallCode::invalid_argument,
    };
};

struct StoreStageNamedConfig {
    app_abi::AppStoreReader reader{};
    std::string_view name{};
    std::span<std::byte> cache{};
    app_abi::AppImageFormat format{app_abi::AppImageFormat::elf};
};

struct SpanStoreReaderContext {
    std::span<const std::byte> bytes{};
};

[[nodiscard]] inline app_abi::AppStoreReader make_span_store_reader(
    SpanStoreReaderContext& context) noexcept {
    return app_abi::AppStoreReader{
        .ctx = &context,
        .read = [](void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            auto* local = static_cast<SpanStoreReaderContext*>(ctx);
            if (local == nullptr || local->bytes.data() == nullptr ||
                offset > local->bytes.size() || bytes.size() > (local->bytes.size() - offset)) {
                return false;
            }
            std::memcpy(bytes.data(), local->bytes.data() + offset, bytes.size());
            return true;
        },
    };
}

[[nodiscard]] inline StoreInstallReceivedResult store_install_received_image(
    const StoreInstallReceivedConfig& config) noexcept {
    StoreInstallReceivedResult result{};
    result.received = received_image_read(config.received);
    if (result.received.code != ReceivedImageReadCode::ok) {
        return result;
    }

    if (config.validate_store_header) {
        SpanStoreReaderContext context{.bytes = result.received.image};
        const auto reader = make_span_store_reader(context);
        result.store_code = app_abi::app_store_read_header(reader, result.header);
        if (result.store_code != app_abi::AppStoreReadCode::ok) {
            return result;
        }
    } else {
        result.store_code = app_abi::AppStoreReadCode::ok;
    }

    result.install = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = config.media,
        .target_offset = config.target_offset,
        .image = result.received.image,
    });
    return result;
}

[[nodiscard]] inline app_abi::AppStoreStageResult store_stage_named_app_image(
    const StoreStageNamedConfig& config) noexcept {
    return app_abi::app_store_stage_named_image(config.reader,
                                                config.name,
                                                config.cache,
                                                config.format);
}

} // namespace charm::dev_loader
