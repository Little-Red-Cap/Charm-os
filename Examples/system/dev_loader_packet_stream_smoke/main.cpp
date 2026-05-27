#include "charm_dev_loader_packet_stream.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace {

namespace loader = charm::dev_loader;
namespace fs = std::filesystem;

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
        .base_address = 0x24060000U,
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

bool read_file(const fs::path& path, std::vector<std::byte>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const auto size = file.tellg();
    if (size <= 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(out.data()), size);
    return static_cast<bool>(file);
}

bool replay_stream(std::span<const std::byte> stream,
                   std::span<const std::byte> expected_payload,
                   loader::PacketStreamReplayResult& replay) {
    MemoryStorage memory{};
    loader::PacketRuntime runtime{make_storage(memory)};
    replay = loader::packet_stream_replay(stream, runtime);
    return replay.code == loader::PacketStreamReplayCode::ok &&
           replay.packet.receive.stage == loader::Stage::launch_ready &&
           std::memcmp(memory.bytes.data(), expected_payload.data(), expected_payload.size()) == 0;
}

bool replay_file(const fs::path& path) {
    std::vector<std::byte> stream{};
    if (!read_file(path, stream)) {
        std::printf("[ERR] failed to read stream file: %s\n", path.string().c_str());
        return false;
    }
    if (stream.size() < sizeof(loader::PacketHeader)) {
        std::printf("[ERR] stream file has no begin header\n");
        return false;
    }

    loader::PacketHeader begin{};
    std::memcpy(&begin, stream.data(), sizeof(begin));
    if (begin.kind != static_cast<std::uint16_t>(loader::PacketKind::begin) ||
        begin.size == 0U ||
        begin.size > (16U * 1024U * 1024U)) {
        std::printf("[ERR] stream file has invalid begin packet\n");
        return false;
    }

    struct DynamicStorage {
        std::vector<std::byte> bytes{};
    };
    auto storage_write = [](void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept -> bool {
        auto* storage = static_cast<DynamicStorage*>(ctx);
        if (storage == nullptr || offset > storage->bytes.size() ||
            bytes.size() > (storage->bytes.size() - offset)) {
            return false;
        }
        std::memcpy(storage->bytes.data() + offset, bytes.data(), bytes.size());
        return true;
    };
    auto storage_read = [](void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
        auto* storage = static_cast<DynamicStorage*>(ctx);
        if (storage == nullptr || offset > storage->bytes.size() ||
            bytes.size() > (storage->bytes.size() - offset)) {
            return false;
        }
        std::memcpy(bytes.data(), storage->bytes.data() + offset, bytes.size());
        return true;
    };

    DynamicStorage storage{};
    storage.bytes.resize(begin.size);
    loader::PacketRuntime runtime{loader::Storage{
        .ctx = &storage,
        .base_address = 0x24060000U,
        .capacity_bytes = static_cast<std::uint32_t>(storage.bytes.size()),
        .write = storage_write,
        .read = storage_read,
    }};
    const auto replay = loader::packet_stream_replay(stream, runtime);
    if (replay.code != loader::PacketStreamReplayCode::ok ||
        replay.packet.receive.stage != loader::Stage::launch_ready) {
        std::printf("[ERR] stream file replay failed: replay=%.*s packet=%.*s stage=%.*s code=%.*s\n",
                    static_cast<int>(loader::packet_stream_replay_code_name(replay.code).size()),
                    loader::packet_stream_replay_code_name(replay.code).data(),
                    static_cast<int>(loader::packet_code_name(replay.packet.packet_code).size()),
                    loader::packet_code_name(replay.packet.packet_code).data(),
                    static_cast<int>(loader::stage_name(replay.packet.receive.stage).size()),
                    loader::stage_name(replay.packet.receive.stage).data(),
                    static_cast<int>(loader::code_name(replay.packet.receive.code).size()),
                    loader::code_name(replay.packet.receive.code).data());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        if (!replay_file(argv[1])) {
            return 1;
        }
        std::puts("[dev-loader-packet-stream-smoke] ok");
        return 0;
    }

    bool ok = true;

    const std::array<std::byte, 17> payload{
        std::byte{0x7f},
        std::byte{'E'},
        std::byte{'L'},
        std::byte{'F'},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
        std::byte{0x07},
        std::byte{0x08},
        std::byte{0x09},
        std::byte{0x0a},
        std::byte{0x0b},
        std::byte{0x0c},
        std::byte{0x0d},
    };

    std::array<std::byte, 512> stream{};
    auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                 .payload = payload,
                                                 .chunk_size = 5,
                                                 .check_crc = true,
                                                 .append_launch_dry_run = true,
                                             },
                                             stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::ok &&
                    built.packet_count == 7 &&
                    built.next_sequence == 7,
                "multi-chunk packet stream builds") && ok;

    loader::PacketStreamReplayResult replay{};
    ok = expect(replay_stream(std::span<const std::byte>{stream.data(), built.bytes_written}, payload, replay),
                "multi-chunk packet stream replays to launch-ready") && ok;
    ok = expect(replay.packet.packet_code == loader::PacketCode::ok &&
                    replay.packet.next_sequence == built.next_sequence &&
                    replay.bytes_consumed == built.bytes_written,
                "multi-chunk replay reports final status") && ok;

    std::array<std::byte, 512> single_stream{};
    built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                            .payload = payload,
                                            .chunk_size = 64,
                                            .check_crc = false,
                                            .append_launch_dry_run = true,
                                        },
                                        single_stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::ok &&
                    built.packet_count == 4,
                "single data packet stream builds") && ok;
    ok = expect(replay_stream(std::span<const std::byte>{single_stream.data(), built.bytes_written}, payload, replay),
                "single data packet stream replays") && ok;

    std::array<std::byte, 32> small_output{};
    built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                            .payload = payload,
                                            .chunk_size = 5,
                                        },
                                        small_output);
    ok = expect(built.code == loader::PacketStreamBuildCode::output_too_small,
                "output too small rejected") && ok;

    built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                            .payload = payload,
                                            .chunk_size = 0,
                                        },
                                        stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::chunk_size_zero,
                "zero chunk size rejected") && ok;

    built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                            .payload = {},
                                            .chunk_size = 8,
                                        },
                                        stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::payload_empty,
                "empty payload rejected") && ok;

    std::array<std::byte, 512> error_stream{};
    built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                            .payload = payload,
                                            .chunk_size = 6,
                                            .check_crc = true,
                                        },
                                        error_stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::ok,
                "error-path base stream builds") && ok;

    MemoryStorage memory{};
    loader::PacketRuntime runtime{make_storage(memory)};
    replay = loader::packet_stream_replay(
        std::span<const std::byte>{error_stream.data(), sizeof(loader::PacketHeader) - 1U},
        runtime);
    ok = expect(replay.code == loader::PacketStreamReplayCode::truncated_header,
                "truncated header rejected") && ok;

    runtime = loader::PacketRuntime{make_storage(memory)};
    replay = loader::packet_stream_replay(
        std::span<const std::byte>{error_stream.data(), (sizeof(loader::PacketHeader) * 2U) + 5U},
        runtime);
    ok = expect(replay.code == loader::PacketStreamReplayCode::truncated_payload,
                "truncated payload rejected") && ok;

    auto bad_magic_stream = error_stream;
    loader::PacketHeader bad_header{};
    std::memcpy(&bad_header, bad_magic_stream.data(), sizeof(bad_header));
    bad_header.magic = 0;
    std::memcpy(bad_magic_stream.data(), &bad_header, sizeof(bad_header));
    runtime = loader::PacketRuntime{make_storage(memory)};
    replay = loader::packet_stream_replay(
        std::span<const std::byte>{bad_magic_stream.data(), built.bytes_written},
        runtime);
    ok = expect(replay.code == loader::PacketStreamReplayCode::packet_failed &&
                    replay.packet.packet_code == loader::PacketCode::bad_magic,
                "bad magic maps to packet failure") && ok;

    auto bad_sequence_stream = error_stream;
    std::memcpy(&bad_header, bad_sequence_stream.data(), sizeof(bad_header));
    bad_header.sequence = 42;
    std::memcpy(bad_sequence_stream.data(), &bad_header, sizeof(bad_header));
    runtime = loader::PacketRuntime{make_storage(memory)};
    replay = loader::packet_stream_replay(
        std::span<const std::byte>{bad_sequence_stream.data(), built.bytes_written},
        runtime);
    ok = expect(replay.code == loader::PacketStreamReplayCode::packet_failed &&
                    replay.packet.packet_code == loader::PacketCode::sequence_mismatch,
                "bad sequence maps to packet failure") && ok;

    auto bad_crc_stream = error_stream;
    std::size_t first_data_header_offset = sizeof(loader::PacketHeader);
    bad_crc_stream[first_data_header_offset + sizeof(loader::PacketHeader)] ^= std::byte{0xff};
    runtime = loader::PacketRuntime{make_storage(memory)};
    replay = loader::packet_stream_replay(
        std::span<const std::byte>{bad_crc_stream.data(), built.bytes_written},
        runtime);
    ok = expect(replay.code == loader::PacketStreamReplayCode::packet_failed &&
                    replay.packet.kind == loader::PacketKind::verify &&
                    replay.packet.receive.code == loader::Code::crc_mismatch,
                "crc mismatch maps to verify packet failure") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-packet-stream-smoke] ok");
    return 0;
}
