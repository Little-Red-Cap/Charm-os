#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_hex.hpp"

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

char hex_nibble(std::uint8_t value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    return kHex[value & 0x0FU];
}

template <std::size_t Capacity>
std::uint32_t encode_hex(std::span<const std::byte> bytes, std::array<char, Capacity>& out) {
    std::uint32_t cursor = 0;
    for (const auto byte : bytes) {
        const auto value = static_cast<std::uint8_t>(byte);
        out[cursor++] = hex_nibble(value >> 4U);
        out[cursor++] = hex_nibble(value);
    }
    return cursor;
}

} // namespace

int main() {
    bool ok = true;

    std::array<std::byte, 8> decoded{};
    auto hex = loader::hex_decode_bytes("7f454c46", decoded);
    ok = expect(hex.code == loader::HexDecodeCode::ok &&
                    hex.bytes_written == 4 &&
                    decoded[0] == std::byte{0x7f} &&
                    decoded[3] == std::byte{0x46},
                "continuous hex decodes") && ok;

    hex = loader::hex_decode_bytes("7F 45 4c 46", decoded);
    ok = expect(hex.code == loader::HexDecodeCode::ok &&
                    hex.bytes_written == 4 &&
                    decoded[1] == std::byte{0x45} &&
                    decoded[2] == std::byte{0x4c},
                "spaced mixed-case hex decodes") && ok;

    hex = loader::hex_decode_bytes("   ", decoded);
    ok = expect(hex.code == loader::HexDecodeCode::empty,
                "empty hex rejected") && ok;
    hex = loader::hex_decode_bytes("123", decoded);
    ok = expect(hex.code == loader::HexDecodeCode::odd_digits,
                "odd hex digits rejected") && ok;
    hex = loader::hex_decode_bytes("12zz", decoded);
    ok = expect(hex.code == loader::HexDecodeCode::invalid_char,
                "invalid hex rejected") && ok;

    std::array<std::byte, 1> tiny{};
    hex = loader::hex_decode_bytes("0102", tiny);
    ok = expect(hex.code == loader::HexDecodeCode::output_too_small,
                "small decode buffer rejected") && ok;

    const std::array<std::byte, 21> payload{
        std::byte{0x7f},
        std::byte{'E'},
        std::byte{'L'},
        std::byte{'F'},
        std::byte{0x21},
        std::byte{0x22},
        std::byte{0x23},
        std::byte{0x24},
        std::byte{0x25},
        std::byte{0x26},
        std::byte{0x27},
        std::byte{0x28},
        std::byte{0x29},
        std::byte{0x2a},
        std::byte{0x2b},
        std::byte{0x2c},
        std::byte{0x2d},
        std::byte{0x2e},
        std::byte{0x2f},
        std::byte{0x30},
        std::byte{0x31},
    };

    std::array<std::byte, 512> stream{};
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 7,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::ok,
                "packet stream builds") && ok;

    MemoryStorage memory{};
    std::array<std::byte, 128> transport_buffer{};
    loader::PacketRuntime packet_runtime{make_storage(memory)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{.buffer = transport_buffer, .max_payload_size = 64},
    };

    constexpr std::uint32_t kConsoleChunkBytes = 17;
    std::array<char, kConsoleChunkBytes * 2U> hex_line{};
    std::array<std::byte, kConsoleChunkBytes> ingest_bytes{};
    std::uint32_t offset = 0;
    while (offset < built.bytes_written) {
        const auto remaining = built.bytes_written - offset;
        const auto n = remaining < kConsoleChunkBytes ? remaining : kConsoleChunkBytes;
        const auto hex_len = encode_hex(std::span<const std::byte>{stream.data() + offset, n}, hex_line);
        const auto decoded_line = loader::hex_decode_bytes(std::string_view{hex_line.data(), hex_len}, ingest_bytes);
        ok = expect(decoded_line.code == loader::HexDecodeCode::ok &&
                        decoded_line.bytes_written == n,
                    "hex packetstream chunk decodes") && ok;
        const auto result = transport.ingest(std::span<const std::byte>{ingest_bytes.data(), decoded_line.bytes_written});
        ok = expect(result.code == loader::ByteTransportCode::ok,
                    "hex packetstream chunk ingests") && ok;
        offset += n;
    }

    const auto status = transport.status();
    ok = expect(status.packet.receive.stage == loader::Stage::launch_ready &&
                    status.buffered_bytes == 0,
                "hex ingest reaches launch-ready") && ok;
    ok = expect(std::memcmp(memory.bytes.data(), payload.data(), payload.size()) == 0,
                "hex ingest writes payload bytes") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-hex-ingest-smoke] ok");
    return 0;
}
