#pragma once

#include "charm_dev_loader_packets.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::dev_loader {

enum class PacketStreamBuildCode : std::uint8_t {
    ok,
    invalid_argument,
    chunk_size_zero,
    payload_empty,
    output_too_small,
};

enum class PacketStreamReplayCode : std::uint8_t {
    ok,
    invalid_argument,
    truncated_header,
    truncated_payload,
    packet_failed,
};

struct PacketStreamBuildConfig {
    std::span<const std::byte> payload{};
    std::uint32_t chunk_size{256};
    bool check_crc{true};
    bool append_launch_dry_run{true};
};

struct PacketStreamBuildResult {
    PacketStreamBuildCode code{PacketStreamBuildCode::ok};
    std::uint32_t bytes_written{0};
    std::uint32_t packet_count{0};
    std::uint32_t payload_crc32{0};
    std::uint32_t next_sequence{0};
};

struct PacketStreamReplayResult {
    PacketStreamReplayCode code{PacketStreamReplayCode::ok};
    PacketResult packet{};
    std::uint32_t bytes_consumed{0};
    std::uint32_t packet_count{0};
};

[[nodiscard]] constexpr std::string_view packet_stream_build_code_name(PacketStreamBuildCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case PacketStreamBuildCode::ok:
            return "ok"sv;
        case PacketStreamBuildCode::invalid_argument:
            return "invalid_argument"sv;
        case PacketStreamBuildCode::chunk_size_zero:
            return "chunk_size_zero"sv;
        case PacketStreamBuildCode::payload_empty:
            return "payload_empty"sv;
        case PacketStreamBuildCode::output_too_small:
            return "output_too_small"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::string_view packet_stream_replay_code_name(PacketStreamReplayCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case PacketStreamReplayCode::ok:
            return "ok"sv;
        case PacketStreamReplayCode::invalid_argument:
            return "invalid_argument"sv;
        case PacketStreamReplayCode::truncated_header:
            return "truncated_header"sv;
        case PacketStreamReplayCode::truncated_payload:
            return "truncated_payload"sv;
        case PacketStreamReplayCode::packet_failed:
            return "packet_failed"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::uint32_t packet_stream_required_size(const PacketStreamBuildConfig& config) noexcept {
    if (config.payload.empty() || config.chunk_size == 0U) {
        return 0;
    }
    const auto payload_size = static_cast<std::uint32_t>(config.payload.size());
    const auto data_packets = (payload_size + config.chunk_size - 1U) / config.chunk_size;
    const auto packet_count = 1U + data_packets + 1U + (config.append_launch_dry_run ? 1U : 0U);
    return (packet_count * kPacketHeaderSize) + payload_size;
}

[[nodiscard]] inline bool packet_stream_write_packet(PacketHeader header,
                                                     std::span<const std::byte> payload,
                                                     std::span<std::byte> output,
                                                     std::uint32_t& cursor) noexcept {
    const auto needed = static_cast<std::uint32_t>(sizeof(PacketHeader) + payload.size());
    if (cursor > output.size() || needed > (output.size() - cursor)) {
        return false;
    }
    std::memcpy(output.data() + cursor, &header, sizeof(PacketHeader));
    cursor += static_cast<std::uint32_t>(sizeof(PacketHeader));
    if (!payload.empty()) {
        std::memcpy(output.data() + cursor, payload.data(), payload.size());
        cursor += static_cast<std::uint32_t>(payload.size());
    }
    return true;
}

[[nodiscard]] inline PacketStreamBuildResult packet_stream_build(const PacketStreamBuildConfig& config,
                                                                 std::span<std::byte> output) noexcept {
    if (output.data() == nullptr) {
        return {.code = PacketStreamBuildCode::invalid_argument};
    }
    if (config.chunk_size == 0U) {
        return {.code = PacketStreamBuildCode::chunk_size_zero};
    }
    if (config.payload.empty()) {
        return {.code = PacketStreamBuildCode::payload_empty};
    }
    if (config.payload.data() == nullptr) {
        return {.code = PacketStreamBuildCode::invalid_argument};
    }
    const auto required = packet_stream_required_size(config);
    if (required == 0U || required > output.size()) {
        return {.code = PacketStreamBuildCode::output_too_small};
    }

    const auto payload_size = static_cast<std::uint32_t>(config.payload.size());
    const auto crc = crc32_update(0, config.payload);
    const auto flags = config.check_crc ? kPacketFlagCheckCrc : std::uint16_t{0};
    std::uint32_t cursor = 0;
    std::uint32_t sequence = 0;
    std::uint32_t packet_count = 0;

    if (!packet_stream_write_packet(make_packet_header(PacketKind::begin, sequence++, payload_size, 0, crc, flags),
                                    {},
                                    output,
                                    cursor)) {
        return {.code = PacketStreamBuildCode::output_too_small};
    }
    ++packet_count;

    std::uint32_t offset = 0;
    while (offset < payload_size) {
        const auto remaining = payload_size - offset;
        const auto chunk = remaining < config.chunk_size ? remaining : config.chunk_size;
        const auto* data = config.payload.data() + offset;
        const std::span<const std::byte> payload{data, chunk};
        if (!packet_stream_write_packet(make_packet_header(PacketKind::data, sequence++, chunk, offset),
                                        payload,
                                        output,
                                        cursor)) {
            return {.code = PacketStreamBuildCode::output_too_small};
        }
        ++packet_count;
        offset += chunk;
    }

    if (!packet_stream_write_packet(make_packet_header(PacketKind::verify, sequence++), {}, output, cursor)) {
        return {.code = PacketStreamBuildCode::output_too_small};
    }
    ++packet_count;

    if (config.append_launch_dry_run) {
        if (!packet_stream_write_packet(make_packet_header(PacketKind::launch_dry_run, sequence++), {}, output, cursor)) {
            return {.code = PacketStreamBuildCode::output_too_small};
        }
        ++packet_count;
    }

    return PacketStreamBuildResult{
        .code = PacketStreamBuildCode::ok,
        .bytes_written = cursor,
        .packet_count = packet_count,
        .payload_crc32 = crc,
        .next_sequence = sequence,
    };
}

[[nodiscard]] constexpr std::uint32_t packet_stream_payload_size(const PacketHeader& header) noexcept {
    if (header.kind == static_cast<std::uint16_t>(PacketKind::data)) {
        return header.size;
    }
    return 0;
}

[[nodiscard]] inline PacketStreamReplayResult packet_stream_replay(std::span<const std::byte> stream,
                                                                   PacketRuntime& runtime) noexcept {
    if (stream.data() == nullptr) {
        return {.code = PacketStreamReplayCode::invalid_argument};
    }

    std::uint32_t cursor = 0;
    std::uint32_t packet_count = 0;
    PacketResult last = runtime.status();
    while (cursor < stream.size()) {
        if (sizeof(PacketHeader) > (stream.size() - cursor)) {
            return PacketStreamReplayResult{
                .code = PacketStreamReplayCode::truncated_header,
                .packet = last,
                .bytes_consumed = cursor,
                .packet_count = packet_count,
            };
        }

        PacketHeader header{};
        std::memcpy(&header, stream.data() + cursor, sizeof(PacketHeader));
        cursor += static_cast<std::uint32_t>(sizeof(PacketHeader));

        const auto payload_size = packet_stream_payload_size(header);
        if (payload_size > (stream.size() - cursor)) {
            return PacketStreamReplayResult{
                .code = PacketStreamReplayCode::truncated_payload,
                .packet = last,
                .bytes_consumed = cursor,
                .packet_count = packet_count,
            };
        }

        const std::span<const std::byte> payload{stream.data() + cursor, payload_size};
        cursor += payload_size;
        last = runtime.handle(Packet{.header = header, .payload = payload});
        ++packet_count;
        if (last.packet_code != PacketCode::ok) {
            return PacketStreamReplayResult{
                .code = PacketStreamReplayCode::packet_failed,
                .packet = last,
                .bytes_consumed = cursor,
                .packet_count = packet_count,
            };
        }
    }

    return PacketStreamReplayResult{
        .code = PacketStreamReplayCode::ok,
        .packet = last,
        .bytes_consumed = cursor,
        .packet_count = packet_count,
    };
}

} // namespace charm::dev_loader
