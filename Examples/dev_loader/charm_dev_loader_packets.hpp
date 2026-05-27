#pragma once

#include "charm_dev_loader.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace charm::dev_loader {

inline constexpr std::uint32_t kPacketMagic = 0x504C5643U; // "CVLP", little-endian.
inline constexpr std::uint16_t kPacketVersion = 1U;
inline constexpr std::uint16_t kPacketFlagCheckCrc = 1U << 0U;

enum class PacketKind : std::uint16_t {
    status = 0,
    begin = 1,
    data = 2,
    verify = 3,
    launch_dry_run = 4,
    abort = 5,
};

enum class PacketCode : std::uint8_t {
    ok,
    invalid_argument,
    bad_magic,
    bad_version,
    bad_header,
    unsupported_kind,
    sequence_mismatch,
    payload_size_mismatch,
    receive_failed,
};

struct PacketHeader {
    std::uint32_t magic{0};
    std::uint16_t version{0};
    std::uint16_t header_size{0};
    std::uint16_t kind{0};
    std::uint16_t flags{0};
    std::uint32_t sequence{0};
    std::uint32_t offset{0};
    std::uint32_t size{0};
    std::uint32_t crc32{0};
};

inline constexpr std::uint16_t kPacketHeaderSize = sizeof(PacketHeader);

struct Packet {
    PacketHeader header{};
    std::span<const std::byte> payload{};
};

struct PacketResult {
    PacketCode packet_code{PacketCode::ok};
    PacketKind kind{PacketKind::status};
    Result receive{};
    ImageManifest manifest{};
    std::uint32_t next_sequence{0};
    std::uint32_t cursor{0};
    bool active{false};
};

[[nodiscard]] constexpr PacketHeader make_packet_header(PacketKind kind,
                                                        std::uint32_t sequence,
                                                        std::uint32_t size = 0,
                                                        std::uint32_t offset = 0,
                                                        std::uint32_t crc32 = 0,
                                                        std::uint16_t flags = 0) noexcept {
    return PacketHeader{
        .magic = kPacketMagic,
        .version = kPacketVersion,
        .header_size = kPacketHeaderSize,
        .kind = static_cast<std::uint16_t>(kind),
        .flags = flags,
        .sequence = sequence,
        .offset = offset,
        .size = size,
        .crc32 = crc32,
    };
}

[[nodiscard]] constexpr std::string_view packet_kind_name(PacketKind kind) noexcept {
    using namespace std::literals::string_view_literals;
    switch (kind) {
        case PacketKind::status:
            return "status"sv;
        case PacketKind::begin:
            return "begin"sv;
        case PacketKind::data:
            return "data"sv;
        case PacketKind::verify:
            return "verify"sv;
        case PacketKind::launch_dry_run:
            return "launch_dry_run"sv;
        case PacketKind::abort:
            return "abort"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::string_view packet_code_name(PacketCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case PacketCode::ok:
            return "ok"sv;
        case PacketCode::invalid_argument:
            return "invalid_argument"sv;
        case PacketCode::bad_magic:
            return "bad_magic"sv;
        case PacketCode::bad_version:
            return "bad_version"sv;
        case PacketCode::bad_header:
            return "bad_header"sv;
        case PacketCode::unsupported_kind:
            return "unsupported_kind"sv;
        case PacketCode::sequence_mismatch:
            return "sequence_mismatch"sv;
        case PacketCode::payload_size_mismatch:
            return "payload_size_mismatch"sv;
        case PacketCode::receive_failed:
            return "receive_failed"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr bool packet_kind_supported(std::uint16_t kind) noexcept {
    switch (static_cast<PacketKind>(kind)) {
        case PacketKind::status:
        case PacketKind::begin:
        case PacketKind::data:
        case PacketKind::verify:
        case PacketKind::launch_dry_run:
        case PacketKind::abort:
            return true;
    }
    return false;
}

class PacketRuntime {
public:
    explicit constexpr PacketRuntime(Storage storage) noexcept : receiver_(storage) {}

    [[nodiscard]] PacketResult handle(const Packet& packet) noexcept {
        auto result = status_result();
        if (packet.header.magic != kPacketMagic) {
            result.packet_code = PacketCode::bad_magic;
            return result;
        }
        if (packet.header.version != kPacketVersion) {
            result.packet_code = PacketCode::bad_version;
            return result;
        }
        if (packet.header.header_size != kPacketHeaderSize) {
            result.packet_code = PacketCode::bad_header;
            return result;
        }
        if (!packet_kind_supported(packet.header.kind)) {
            result.packet_code = PacketCode::unsupported_kind;
            return result;
        }

        const auto kind = static_cast<PacketKind>(packet.header.kind);
        result.kind = kind;
        if (kind == PacketKind::status) {
            return result;
        }
        if (packet.header.sequence != next_sequence_) {
            result.packet_code = PacketCode::sequence_mismatch;
            return result;
        }

        switch (kind) {
            case PacketKind::begin:
                result = begin(packet);
                break;
            case PacketKind::data:
                result = data(packet);
                break;
            case PacketKind::verify:
                result = no_payload(packet, receiver_.verify());
                break;
            case PacketKind::launch_dry_run:
                result = no_payload(packet, receiver_.mark_launch_ready());
                break;
            case PacketKind::abort:
                result = abort(packet);
                break;
            case PacketKind::status:
                break;
        }

        if (result.packet_code == PacketCode::ok) {
            ++next_sequence_;
            result.next_sequence = next_sequence_;
        }
        return result;
    }

    [[nodiscard]] PacketResult status() const noexcept {
        return status_result();
    }

    void reset() noexcept {
        receiver_.abort();
        next_sequence_ = 0;
    }

private:
    [[nodiscard]] PacketResult status_result() const noexcept {
        return PacketResult{
            .packet_code = PacketCode::ok,
            .kind = PacketKind::status,
            .receive = receiver_.current(),
            .manifest = receiver_.manifest(),
            .next_sequence = next_sequence_,
            .cursor = receiver_.cursor(),
            .active = receiver_.active(),
        };
    }

    [[nodiscard]] PacketResult with_receive(PacketKind kind,
                                            Result receive,
                                            PacketCode packet_code = PacketCode::ok) const noexcept {
        return PacketResult{
            .packet_code = packet_code,
            .kind = kind,
            .receive = receive,
            .manifest = receiver_.manifest(),
            .next_sequence = next_sequence_,
            .cursor = receiver_.cursor(),
            .active = receiver_.active(),
        };
    }

    [[nodiscard]] PacketResult begin(const Packet& packet) noexcept {
        if (!packet.payload.empty() || packet.header.size == 0U) {
            return with_receive(PacketKind::begin, receiver_.current(), PacketCode::invalid_argument);
        }
        const bool check_crc = (packet.header.flags & kPacketFlagCheckCrc) != 0U;
        const auto receive = receiver_.begin(packet.header.size, packet.header.crc32, check_crc);
        return with_receive(PacketKind::begin,
                            receive,
                            receive.code == Code::ok ? PacketCode::ok : PacketCode::receive_failed);
    }

    [[nodiscard]] PacketResult data(const Packet& packet) noexcept {
        if (packet.header.size != packet.payload.size()) {
            return with_receive(PacketKind::data, receiver_.current(), PacketCode::payload_size_mismatch);
        }
        const auto receive = receiver_.write_at(packet.header.offset, packet.payload);
        return with_receive(PacketKind::data,
                            receive,
                            receive.code == Code::ok ? PacketCode::ok : PacketCode::receive_failed);
    }

    [[nodiscard]] PacketResult no_payload(const Packet& packet, Result receive) const noexcept {
        if (!packet.payload.empty() || packet.header.size != 0U) {
            return with_receive(static_cast<PacketKind>(packet.header.kind),
                                receiver_.current(),
                                PacketCode::payload_size_mismatch);
        }
        return with_receive(static_cast<PacketKind>(packet.header.kind),
                            receive,
                            receive.code == Code::ok ? PacketCode::ok : PacketCode::receive_failed);
    }

    [[nodiscard]] PacketResult abort(const Packet& packet) noexcept {
        if (!packet.payload.empty() || packet.header.size != 0U) {
            return with_receive(PacketKind::abort, receiver_.current(), PacketCode::payload_size_mismatch);
        }
        receiver_.abort();
        return with_receive(PacketKind::abort, receiver_.current());
    }

    BinaryReceiveRuntime receiver_;
    std::uint32_t next_sequence_{0};
};

} // namespace charm::dev_loader
