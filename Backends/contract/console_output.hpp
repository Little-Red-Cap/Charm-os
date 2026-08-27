#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace charm::backend::contract::console {
    enum class StatusCode : std::uint8_t {
        ok,
        busy,
        invalid_argument,
    };

    struct Status {
        StatusCode code{StatusCode::ok};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == StatusCode::ok;
        }
    };

    struct Transfer {
        Status status{};
        std::size_t bytes{0};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return status.is_ok();
        }
    };

    struct ByteSink {
        static constexpr std::string_view label{"ByteSink"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& sink, std::span<const std::byte> bytes) {
            { sink.write(bytes) } noexcept -> std::same_as<Transfer>;
            { sink.flush() } noexcept -> std::same_as<Status>;
        };
    };

    struct TextSink {
        static constexpr std::string_view label{"TextSink"};

        template <typename T>
        static constexpr bool satisfied_by =
            ByteSink::template satisfied_by<T> &&
            requires(T& sink, std::string_view text) {
                { sink.write(text) } noexcept -> std::same_as<Transfer>;
            };
    };

    struct LineSource {
        static constexpr std::string_view label{"LineSource"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& source) {
            { source.poll_line() } noexcept -> std::same_as<std::optional<std::string_view>>;
        };
    };

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
        std::string_view backend{};
        std::string_view runtime_domain{};
        std::string_view adapter{};
        std::string_view transport{};
        std::string_view tx_mode{};
        std::string_view rx_mode{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::size_t bytes_accepted{0};
        std::size_t fallback_count{0};
        std::size_t dropped_bytes{0};
        std::size_t busy_count{0};
        std::size_t lines_polled{0};
    };

    struct EvidenceView {
        std::string_view capability_name{};
        std::string_view requirement_role{};
        std::string_view provider_instance{};
        std::string_view runtime_domain{};
        std::string_view transport{};
        std::string_view tx_mode{};
        std::string_view rx_mode{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::size_t bytes_accepted{0};
        std::size_t fallback_count{0};
        std::size_t dropped_bytes{0};
        std::size_t busy_count{0};
    };

    template <std::size_t Capacity>
    struct EvidenceCollector {
        std::array<EvidenceFrame, Capacity> frames{};
        std::size_t count{0};

        [[nodiscard]] constexpr bool append(const EvidenceFrame& frame) noexcept {
            if (count >= frames.size()) {
                return false;
            }
            frames[count++] = frame;
            return true;
        }
    };

    [[nodiscard]] constexpr EvidenceStatus status_from_degradation(const std::size_t fallback_count,
                                                                   const std::size_t dropped_bytes,
                                                                   const std::size_t busy_count) noexcept {
        return (fallback_count == 0U && dropped_bytes == 0U && busy_count == 0U)
                   ? EvidenceStatus::ok
                   : EvidenceStatus::degraded;
    }

    [[nodiscard]] constexpr EvidenceView project_view(const EvidenceFrame& frame) noexcept {
        return EvidenceView{
            .capability_name = frame.capability_name,
            .requirement_role = frame.requirement_role,
            .provider_instance = frame.provider_instance,
            .runtime_domain = frame.runtime_domain,
            .transport = frame.transport,
            .tx_mode = frame.tx_mode,
            .rx_mode = frame.rx_mode,
            .status = frame.status,
            .bytes_accepted = frame.bytes_accepted,
            .fallback_count = frame.fallback_count,
            .dropped_bytes = frame.dropped_bytes,
            .busy_count = frame.busy_count,
        };
    }
}
