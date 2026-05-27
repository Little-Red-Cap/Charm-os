#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_hex.hpp"
#include "charm_dev_loader_packet_console.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace {

namespace loader = charm::dev_loader;
using namespace std::literals::string_view_literals;

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

bool replay_console_lines(std::string_view script,
                          std::span<const std::byte> expected_payload,
                          std::uint32_t expected_lines) {
    MemoryStorage memory{};
    std::array<std::byte, 128> transport_buffer{};
    std::array<std::byte, 64> decoded_line{};
    loader::PacketRuntime packet_runtime{make_storage(memory)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{.buffer = transport_buffer, .max_payload_size = 64},
    };

    std::uint32_t line_count = 0;
    while (!script.empty()) {
        const auto end = script.find('\n');
        if (end == std::string_view::npos) {
            std::printf("[ERR] generated script line missing trailing newline\n");
            return false;
        }

        const auto line = script.substr(0, end);
        script.remove_prefix(end + 1U);
        ++line_count;

        constexpr auto kPrefix = "dev packet ingest "sv;
        if (!line.starts_with(kPrefix)) {
            std::printf("[ERR] generated script line has wrong prefix\n");
            return false;
        }
        const auto hex = loader::hex_decode_bytes(line.substr(kPrefix.size()), decoded_line);
        if (hex.code != loader::HexDecodeCode::ok) {
            std::printf("[ERR] generated script hex decode failed: %.*s\n",
                        static_cast<int>(loader::hex_decode_code_name(hex.code).size()),
                        loader::hex_decode_code_name(hex.code).data());
            return false;
        }

        const auto result = transport.ingest(
            std::span<const std::byte>{decoded_line.data(), hex.bytes_written});
        if (result.code != loader::ByteTransportCode::ok) {
            std::printf("[ERR] generated script ingest failed: %.*s\n",
                        static_cast<int>(loader::byte_transport_code_name(result.code).size()),
                        loader::byte_transport_code_name(result.code).data());
            return false;
        }
    }

    const auto status = transport.status();
    return line_count == expected_lines &&
           status.buffered_bytes == 0U &&
           status.packet.receive.stage == loader::Stage::launch_ready &&
           std::memcmp(memory.bytes.data(), expected_payload.data(), expected_payload.size()) == 0;
}

} // namespace

int main() {
    bool ok = true;

    const std::array<std::byte, 23> payload{
        std::byte{0x7f},
        std::byte{'E'},
        std::byte{'L'},
        std::byte{'F'},
        std::byte{0x31},
        std::byte{0x32},
        std::byte{0x33},
        std::byte{0x34},
        std::byte{0x35},
        std::byte{0x36},
        std::byte{0x37},
        std::byte{0x38},
        std::byte{0x39},
        std::byte{0x3a},
        std::byte{0x3b},
        std::byte{0x3c},
        std::byte{0x3d},
        std::byte{0x3e},
        std::byte{0x3f},
        std::byte{0x40},
        std::byte{0x41},
        std::byte{0x42},
        std::byte{0x43},
    };

    std::array<std::byte, 512> stream{};
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 6,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::ok,
                "packet stream builds") && ok;

    const loader::PacketConsoleBuildConfig config{
        .stream = std::span<const std::byte>{stream.data(), built.bytes_written},
        .bytes_per_line = 17,
    };
    std::array<char, 2048> script{};
    const auto console = loader::packet_console_build(config, script);
    ok = expect(console.code == loader::PacketConsoleBuildCode::ok &&
                    console.line_count == loader::packet_console_line_count(config) &&
                    console.bytes_written == loader::packet_console_required_size(config),
                "console script builds") && ok;
    ok = expect(replay_console_lines(std::string_view{script.data(), console.bytes_written},
                                     payload,
                                     console.line_count),
                "console script replays through byte transport") && ok;

    std::array<char, 8> tiny_script{};
    auto failed = loader::packet_console_build(config, tiny_script);
    ok = expect(failed.code == loader::PacketConsoleBuildCode::output_too_small,
                "small script output rejected") && ok;

    failed = loader::packet_console_build(loader::PacketConsoleBuildConfig{
                                              .stream = {},
                                              .bytes_per_line = 17,
                                          },
                                          script);
    ok = expect(failed.code == loader::PacketConsoleBuildCode::input_empty,
                "empty stream rejected") && ok;

    failed = loader::packet_console_build(loader::PacketConsoleBuildConfig{
                                              .stream = std::span<const std::byte>{stream.data(), built.bytes_written},
                                              .bytes_per_line = 0,
                                          },
                                          script);
    ok = expect(failed.code == loader::PacketConsoleBuildCode::bytes_per_line_zero,
                "zero bytes-per-line rejected") && ok;

    failed = loader::packet_console_build(loader::PacketConsoleBuildConfig{
                                              .stream = std::span<const std::byte>{stream.data(), built.bytes_written},
                                              .bytes_per_line = 17,
                                              .command_prefix = {},
                                          },
                                          script);
    ok = expect(failed.code == loader::PacketConsoleBuildCode::invalid_argument,
                "empty command prefix rejected") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-packet-console-smoke] ok");
    return 0;
}
