#include "charm_dev_loader_byte_transport.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::array<std::byte, 256> bytes{};
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
        .base_address = 0x24070000U,
        .capacity_bytes = static_cast<std::uint32_t>(memory.bytes.size()),
        .write = storage_write,
        .read = storage_read,
    };
}

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

template <std::size_t N>
bool feed_pattern(loader::ByteTransportRuntime& transport,
                  std::span<const std::byte> stream,
                  const std::array<std::uint32_t, N>& chunks) {
    std::uint32_t offset = 0;
    std::size_t chunk_index = 0;
    loader::ByteTransportResult result{};
    while (offset < stream.size()) {
        const auto requested = chunks[chunk_index++ % chunks.size()];
        const auto remaining = static_cast<std::uint32_t>(stream.size() - offset);
        const auto count = requested < remaining ? requested : remaining;
        result = transport.ingest(std::span<const std::byte>{stream.data() + offset, count});
        if (result.code != loader::ByteTransportCode::ok) {
            return false;
        }
        offset += count;
    }
    result = transport.status();
    return result.packet.receive.stage == loader::Stage::launch_ready && result.buffered_bytes == 0;
}

} // namespace

int main() {
    bool ok = true;

    const std::array<std::byte, 19> payload{
        std::byte{0x7f},
        std::byte{'E'},
        std::byte{'L'},
        std::byte{'F'},
        std::byte{0x10},
        std::byte{0x11},
        std::byte{0x12},
        std::byte{0x13},
        std::byte{0x14},
        std::byte{0x15},
        std::byte{0x16},
        std::byte{0x17},
        std::byte{0x18},
        std::byte{0x19},
        std::byte{0x1a},
        std::byte{0x1b},
        std::byte{0x1c},
        std::byte{0x1d},
        std::byte{0x1e},
    };

    std::array<std::byte, 512> stream{};
    auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                 .payload = payload,
                                                 .chunk_size = 6,
                                                 .check_crc = true,
                                                 .append_launch_dry_run = true,
                                             },
                                             stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::ok,
                "packet stream builds for byte transport") && ok;
    const std::span<const std::byte> stream_view{stream.data(), built.bytes_written};

    {
        MemoryStorage memory{};
        std::array<std::byte, 128> buffer{};
        loader::PacketRuntime packet_runtime{make_storage(memory)};
        loader::ByteTransportRuntime transport{
            packet_runtime,
            loader::ByteTransportConfig{.buffer = buffer, .max_payload_size = 64},
        };

        const std::array<std::uint32_t, 1> one_byte{1};
        ok = expect(feed_pattern(transport, stream_view, one_byte),
                    "one-byte chunks reach launch-ready") && ok;
        ok = expect(std::memcmp(memory.bytes.data(), payload.data(), payload.size()) == 0,
                    "one-byte chunks write payload") && ok;
    }

    {
        MemoryStorage memory{};
        std::array<std::byte, 128> buffer{};
        loader::PacketRuntime packet_runtime{make_storage(memory)};
        loader::ByteTransportRuntime transport{
            packet_runtime,
            loader::ByteTransportConfig{.buffer = buffer, .max_payload_size = 64},
        };

        const std::array<std::uint32_t, 4> uneven{3, 11, 2, 17};
        ok = expect(feed_pattern(transport, stream_view, uneven),
                    "uneven chunks reach launch-ready") && ok;
        ok = expect(std::memcmp(memory.bytes.data(), payload.data(), payload.size()) == 0,
                    "uneven chunks write payload") && ok;
    }

    {
        MemoryStorage memory{};
        std::array<std::byte, 4> tiny_buffer{};
        loader::PacketRuntime packet_runtime{make_storage(memory)};
        loader::ByteTransportRuntime transport{
            packet_runtime,
            loader::ByteTransportConfig{.buffer = tiny_buffer, .max_payload_size = 64},
        };
        const auto result = transport.ingest(std::span<const std::byte>{stream.data(), 5});
        ok = expect(result.code == loader::ByteTransportCode::buffer_too_small,
                    "tiny buffer rejects incoming bytes") && ok;
    }

    {
        MemoryStorage memory{};
        std::array<std::byte, 128> buffer{};
        loader::PacketRuntime packet_runtime{make_storage(memory)};
        loader::ByteTransportRuntime transport{
            packet_runtime,
            loader::ByteTransportConfig{.buffer = buffer, .max_payload_size = 2},
        };
        const auto first_data_offset = sizeof(loader::PacketHeader);
        const auto first_data_size = sizeof(loader::PacketHeader) + 6U;
        const auto result = transport.ingest(
            std::span<const std::byte>{stream.data() + first_data_offset, first_data_size});
        ok = expect(result.code == loader::ByteTransportCode::packet_too_large,
                    "max payload size enforced") && ok;
    }

    {
        auto bad_stream = stream;
        loader::PacketHeader header{};
        std::memcpy(&header, bad_stream.data(), sizeof(header));
        header.magic = 0;
        std::memcpy(bad_stream.data(), &header, sizeof(header));

        MemoryStorage memory{};
        std::array<std::byte, 128> buffer{};
        loader::PacketRuntime packet_runtime{make_storage(memory)};
        loader::ByteTransportRuntime transport{
            packet_runtime,
            loader::ByteTransportConfig{.buffer = buffer, .max_payload_size = 64},
        };
        const auto result = transport.ingest(std::span<const std::byte>{bad_stream.data(), sizeof(loader::PacketHeader)});
        ok = expect(result.code == loader::ByteTransportCode::packet_failed &&
                        result.packet.packet_code == loader::PacketCode::bad_magic,
                    "packet failure propagated") && ok;
    }

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-byte-transport-smoke] ok");
    return 0;
}
