#include "charm_dev_loader_packets.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::array<std::byte, 128> bytes{};
};

bool storage_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(storage->bytes.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool storage_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), storage->bytes.data() + offset, bytes.size());
    return true;
}

loader::Storage make_storage(MemoryStorage& memory) {
    return loader::Storage{
        .ctx = &memory,
        .base_address = 0x24050000U,
        .capacity_bytes = static_cast<std::uint32_t>(memory.bytes.size()),
        .write = storage_write,
        .read = storage_read,
    };
}

loader::Packet make_packet(loader::PacketKind kind,
                           std::uint32_t sequence,
                           std::span<const std::byte> payload = {},
                           std::uint32_t offset = 0,
                           std::uint32_t size = 0,
                           std::uint32_t crc = 0,
                           std::uint16_t flags = 0) {
    return loader::Packet{
        .header = loader::make_packet_header(
            kind,
            sequence,
            size == 0U ? static_cast<std::uint32_t>(payload.size()) : size,
            offset,
            crc,
            flags),
        .payload = payload,
    };
}

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

} // namespace

int main() {
    bool ok = true;
    const std::array<std::byte, 8> image{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
        std::byte{0x40},
        std::byte{0x50},
        std::byte{0x60},
        std::byte{0x70},
        std::byte{0x80},
    };
    const auto crc = loader::crc32_update(0, std::span<const std::byte>{image});

    MemoryStorage memory{};
    loader::PacketRuntime runtime{make_storage(memory)};

    auto result = runtime.handle(make_packet(loader::PacketKind::status, 99));
    ok = expect(result.packet_code == loader::PacketCode::ok &&
                    result.next_sequence == 0 &&
                    result.receive.stage == loader::Stage::idle,
                "status ignores sequence and reports idle") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::begin,
                                        0,
                                        {},
                                        0,
                                        static_cast<std::uint32_t>(image.size()),
                                        crc,
                                        loader::kPacketFlagCheckCrc));
    ok = expect(result.packet_code == loader::PacketCode::ok &&
                    result.receive.stage == loader::Stage::receiving &&
                    result.next_sequence == 1,
                "begin packet starts receive") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::data,
                                        1,
                                        std::span<const std::byte>{image.data(), 3},
                                        0));
    ok = expect(result.packet_code == loader::PacketCode::ok &&
                    result.receive.stage == loader::Stage::receiving &&
                    result.cursor == 3 &&
                    result.next_sequence == 2,
                "first data packet accepted") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::data,
                                        2,
                                        std::span<const std::byte>{image.data() + 3, image.size() - 3},
                                        3));
    ok = expect(result.packet_code == loader::PacketCode::ok &&
                    result.receive.stage == loader::Stage::complete &&
                    result.cursor == image.size() &&
                    result.next_sequence == 3,
                "final data packet completes receive") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::verify, 3));
    ok = expect(result.packet_code == loader::PacketCode::ok &&
                    result.receive.stage == loader::Stage::verified &&
                    result.next_sequence == 4,
                "verify packet succeeds") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::launch_dry_run, 4));
    ok = expect(result.packet_code == loader::PacketCode::ok &&
                    result.receive.stage == loader::Stage::launch_ready &&
                    result.next_sequence == 5,
                "launch dry-run packet succeeds") && ok;
    ok = expect(std::memcmp(memory.bytes.data(), image.data(), image.size()) == 0,
                "packet receive writes payload bytes") && ok;

    auto bad_magic = make_packet(loader::PacketKind::status, 0);
    bad_magic.header.magic = 0;
    result = runtime.handle(bad_magic);
    ok = expect(result.packet_code == loader::PacketCode::bad_magic,
                "bad magic rejected") && ok;

    auto bad_version = make_packet(loader::PacketKind::status, 0);
    bad_version.header.version = 0;
    result = runtime.handle(bad_version);
    ok = expect(result.packet_code == loader::PacketCode::bad_version,
                "bad version rejected") && ok;

    auto bad_header = make_packet(loader::PacketKind::status, 0);
    bad_header.header.header_size = 0;
    result = runtime.handle(bad_header);
    ok = expect(result.packet_code == loader::PacketCode::bad_header,
                "bad header size rejected") && ok;

    auto bad_kind = make_packet(loader::PacketKind::status, 0);
    bad_kind.header.kind = 999;
    result = runtime.handle(bad_kind);
    ok = expect(result.packet_code == loader::PacketCode::unsupported_kind,
                "unsupported kind rejected") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::abort, 99));
    ok = expect(result.packet_code == loader::PacketCode::sequence_mismatch,
                "sequence mismatch rejected") && ok;

    auto mismatch = make_packet(loader::PacketKind::data,
                                5,
                                std::span<const std::byte>{image.data(), 1},
                                0,
                                2);
    result = runtime.handle(mismatch);
    ok = expect(result.packet_code == loader::PacketCode::payload_size_mismatch,
                "payload size mismatch rejected") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::abort, 5));
    ok = expect(result.packet_code == loader::PacketCode::ok &&
                    result.receive.stage == loader::Stage::idle &&
                    result.next_sequence == 6,
                "abort packet resets receive") && ok;

    result = runtime.handle(make_packet(loader::PacketKind::data,
                                        6,
                                        std::span<const std::byte>{image.data(), 1},
                                        0));
    ok = expect(result.packet_code == loader::PacketCode::receive_failed &&
                    result.receive.code == loader::Code::no_session,
                "data without session maps receive failure") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-packet-smoke] ok");
    return 0;
}
