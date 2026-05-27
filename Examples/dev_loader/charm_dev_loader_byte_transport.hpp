#pragma once

#include "charm_dev_loader_packet_stream.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::dev_loader {

enum class ByteTransportCode : std::uint8_t {
    ok,
    invalid_argument,
    buffer_too_small,
    packet_too_large,
    packet_failed,
};

struct ByteTransportResult {
    ByteTransportCode code{ByteTransportCode::ok};
    PacketResult packet{};
    std::uint32_t bytes_consumed{0};
    std::uint32_t packets_dispatched{0};
    std::uint32_t buffered_bytes{0};
};

struct ByteTransportConfig {
    std::span<std::byte> buffer{};
    std::uint32_t max_payload_size{1024};
};

[[nodiscard]] constexpr std::string_view byte_transport_code_name(ByteTransportCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case ByteTransportCode::ok:
            return "ok"sv;
        case ByteTransportCode::invalid_argument:
            return "invalid_argument"sv;
        case ByteTransportCode::buffer_too_small:
            return "buffer_too_small"sv;
        case ByteTransportCode::packet_too_large:
            return "packet_too_large"sv;
        case ByteTransportCode::packet_failed:
            return "packet_failed"sv;
    }
    return "unknown"sv;
}

class ByteTransportRuntime {
public:
    constexpr ByteTransportRuntime(PacketRuntime& runtime, ByteTransportConfig config) noexcept
        : runtime_(&runtime), buffer_(config.buffer), max_payload_size_(config.max_payload_size) {}

    [[nodiscard]] ByteTransportResult ingest(std::span<const std::byte> bytes) noexcept {
        if (runtime_ == nullptr || buffer_.data() == nullptr || max_payload_size_ == 0U) {
            return current(ByteTransportCode::invalid_argument, 0, 0);
        }
        if (!bytes.empty() && bytes.data() == nullptr) {
            return current(ByteTransportCode::invalid_argument, 0, 0);
        }
        if (bytes.size() > (buffer_.size() - buffered_)) {
            return current(ByteTransportCode::buffer_too_small, 0, 0);
        }

        std::memcpy(buffer_.data() + buffered_, bytes.data(), bytes.size());
        buffered_ += static_cast<std::uint32_t>(bytes.size());

        std::uint32_t consumed = 0;
        std::uint32_t dispatched = 0;
        while (buffered_ >= sizeof(PacketHeader)) {
            PacketHeader header{};
            std::memcpy(&header, buffer_.data(), sizeof(header));
            const auto payload_size = packet_stream_payload_size(header);
            if (payload_size > max_payload_size_) {
                return current(ByteTransportCode::packet_too_large, consumed, dispatched);
            }
            const auto packet_size = static_cast<std::uint32_t>(sizeof(PacketHeader) + payload_size);
            if (packet_size > buffer_.size()) {
                return current(ByteTransportCode::buffer_too_small, consumed, dispatched);
            }
            if (buffered_ < packet_size) {
                break;
            }

            const std::span<const std::byte> payload{buffer_.data() + sizeof(PacketHeader), payload_size};
            last_packet_ = runtime_->handle(Packet{.header = header, .payload = payload});
            if (last_packet_.packet_code != PacketCode::ok) {
                drop_front(packet_size);
                consumed += packet_size;
                ++dispatched;
                return current(ByteTransportCode::packet_failed, consumed, dispatched);
            }

            drop_front(packet_size);
            consumed += packet_size;
            ++dispatched;
        }

        return current(ByteTransportCode::ok, consumed, dispatched);
    }

    [[nodiscard]] ByteTransportResult status() const noexcept {
        return current(ByteTransportCode::ok, 0, 0);
    }

    void reset() noexcept {
        buffered_ = 0;
        last_packet_ = runtime_ == nullptr ? PacketResult{} : runtime_->status();
    }

    void reset_session() noexcept {
        buffered_ = 0;
        if (runtime_ != nullptr) {
            runtime_->reset();
        }
        last_packet_ = runtime_ == nullptr ? PacketResult{} : runtime_->status();
    }

private:
    void drop_front(std::uint32_t count) noexcept {
        if (count >= buffered_) {
            buffered_ = 0;
            return;
        }
        const auto remaining = buffered_ - count;
        std::memmove(buffer_.data(), buffer_.data() + count, remaining);
        buffered_ = remaining;
    }

    [[nodiscard]] ByteTransportResult current(ByteTransportCode code,
                                              std::uint32_t consumed,
                                              std::uint32_t dispatched) const noexcept {
        return ByteTransportResult{
            .code = code,
            .packet = last_packet_,
            .bytes_consumed = consumed,
            .packets_dispatched = dispatched,
            .buffered_bytes = buffered_,
        };
    }

    PacketRuntime* runtime_{nullptr};
    std::span<std::byte> buffer_{};
    std::uint32_t max_payload_size_{0};
    std::uint32_t buffered_{0};
    PacketResult last_packet_{runtime_ == nullptr ? PacketResult{} : runtime_->status()};
};

} // namespace charm::dev_loader
