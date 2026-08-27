#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace charm::backend::contract::block {
    enum class StatusCode : std::uint8_t {
        ok,
        no_entry,
        invalid_argument,
        io_error,
    };

    struct Status {
        StatusCode code{StatusCode::ok};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == StatusCode::ok;
        }
    };

    struct BlockDevice {
        static constexpr std::string_view label{"BlockDevice"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& dev,
                                                      std::uint64_t lba,
                                                      std::span<std::byte> out,
                                                      std::span<const std::byte> in) {
            { dev.read(lba, out) } noexcept -> std::same_as<Status>;
            { dev.write(lba, in) } noexcept -> std::same_as<Status>;
            { dev.flush() } noexcept -> std::same_as<Status>;
            { dev.block_size() } noexcept -> std::same_as<std::uint64_t>;
            { dev.block_count() } noexcept -> std::same_as<std::uint64_t>;
        };
    };

    using ReadFn = Status (*)(void*, std::uint64_t, std::span<std::byte>) noexcept;
    using WriteFn = Status (*)(void*, std::uint64_t, std::span<const std::byte>) noexcept;

    struct BlockEndpoint {
        std::string_view name{};
        std::uint64_t block_size{0};
        std::uint64_t block_count{0};
        void* ctx{nullptr};
        ReadFn read_fn{nullptr};
        WriteFn write_fn{nullptr};
    };

    [[nodiscard]] inline Status read(BlockEndpoint& endpoint,
                                     const std::uint64_t lba,
                                     const std::span<std::byte> out) noexcept {
        if (!endpoint.ctx || !endpoint.read_fn) {
            return {StatusCode::no_entry};
        }
        return endpoint.read_fn(endpoint.ctx, lba, out);
    }

    [[nodiscard]] inline Status write(BlockEndpoint& endpoint,
                                      const std::uint64_t lba,
                                      const std::span<const std::byte> in) noexcept {
        if (!endpoint.ctx || !endpoint.write_fn) {
            return {StatusCode::no_entry};
        }
        return endpoint.write_fn(endpoint.ctx, lba, in);
    }

    enum class EvidenceStatus : std::uint8_t {
        ok,
        degraded,
        error,
    };

    struct EvidenceFrame {
        std::string_view capability_name{};
        std::string_view requirement_role{};
        std::string_view provider_instance{};
        std::string_view provider_type{};
        std::string_view block_endpoint{};
        std::string_view runtime_domain{};
        std::string_view media_kind{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::uint64_t block_size{0};
        std::uint64_t block_count{0};
        std::uint64_t capacity{0};
        std::size_t read_count{0};
        std::size_t write_count{0};
        StatusCode last_error{StatusCode::ok};
    };

    struct EvidenceView {
        std::string_view capability_name{};
        std::string_view requirement_role{};
        std::string_view provider_instance{};
        std::string_view block_endpoint{};
        std::string_view runtime_domain{};
        std::string_view media_kind{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::uint64_t block_size{0};
        std::uint64_t block_count{0};
        std::uint64_t capacity{0};
        std::size_t read_count{0};
        std::size_t write_count{0};
        StatusCode last_error{StatusCode::ok};
    };

    [[nodiscard]] constexpr EvidenceStatus status_from_error(const StatusCode code) noexcept {
        return code == StatusCode::ok ? EvidenceStatus::ok : EvidenceStatus::degraded;
    }

    [[nodiscard]] constexpr EvidenceView project_view(const EvidenceFrame& frame) noexcept {
        return EvidenceView{
            .capability_name = frame.capability_name,
            .requirement_role = frame.requirement_role,
            .provider_instance = frame.provider_instance,
            .block_endpoint = frame.block_endpoint,
            .runtime_domain = frame.runtime_domain,
            .media_kind = frame.media_kind,
            .status = frame.status,
            .block_size = frame.block_size,
            .block_count = frame.block_count,
            .capacity = frame.capacity,
            .read_count = frame.read_count,
            .write_count = frame.write_count,
            .last_error = frame.last_error,
        };
    }
}
