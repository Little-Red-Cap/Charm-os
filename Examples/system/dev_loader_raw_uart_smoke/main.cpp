#include "charm_dev_loader_byte_transport.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::array<std::byte, 512> bytes{};
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
        .base_address = 0x24040000U,
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

struct RawFrontend {
    loader::ByteTransportRuntime& transport;
    bool active{false};
    std::uint32_t raw_bytes{0};
    loader::ByteTransportResult last{};

    void begin() noexcept {
        transport.reset_session();
        active = true;
        raw_bytes = 0;
        last = transport.status();
    }

    void abort_command() noexcept {
        active = false;
        transport.reset_session();
        last = transport.status();
    }

    loader::ByteTransportResult ingest(std::span<const std::byte> bytes) noexcept {
        if (!active) {
            last = transport.status();
            return last;
        }
        raw_bytes += static_cast<std::uint32_t>(bytes.size());
        last = transport.ingest(bytes);
        if (last.code != loader::ByteTransportCode::ok ||
            last.packet.kind == loader::PacketKind::abort ||
            last.packet.receive.stage == loader::Stage::launch_ready) {
            active = false;
        }
        return last;
    }
};

template <std::size_t N>
bool feed_chunks(RawFrontend& raw,
                 std::span<const std::byte> stream,
                 const std::array<std::uint32_t, N>& chunks) {
    std::uint32_t offset = 0;
    std::uint32_t index = 0;
    while (offset < stream.size()) {
        const auto requested = chunks[index++ % chunks.size()];
        const auto remaining = static_cast<std::uint32_t>(stream.size() - offset);
        const auto count = requested < remaining ? requested : remaining;
        const auto result = raw.ingest(std::span<const std::byte>{stream.data() + offset, count});
        if (result.code != loader::ByteTransportCode::ok) {
            return false;
        }
        offset += count;
    }
    return true;
}

bool build_abort_stream(std::span<std::byte> output, std::uint32_t& bytes_written) {
    bytes_written = 0;
    const auto begin = loader::make_packet_header(loader::PacketKind::begin, 0, 4);
    const auto abort = loader::make_packet_header(loader::PacketKind::abort, 1);
    return loader::packet_stream_write_packet(begin, {}, output, bytes_written) &&
           loader::packet_stream_write_packet(abort, {}, output, bytes_written);
}

} // namespace

int main() {
    bool ok = true;

    const std::array<std::byte, 73> payload{
        std::byte{0x7f}, std::byte{'E'},  std::byte{'L'},  std::byte{'F'},
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
        std::byte{0x05}, std::byte{0x06}, std::byte{0x07}, std::byte{0x08},
        std::byte{0x09}, std::byte{0x0a}, std::byte{0x0b}, std::byte{0x0c},
        std::byte{0x0d}, std::byte{0x0e}, std::byte{0x0f}, std::byte{0x10},
        std::byte{0x11}, std::byte{0x12}, std::byte{0x13}, std::byte{0x14},
        std::byte{0x15}, std::byte{0x16}, std::byte{0x17}, std::byte{0x18},
        std::byte{0x19}, std::byte{0x1a}, std::byte{0x1b}, std::byte{0x1c},
        std::byte{0x1d}, std::byte{0x1e}, std::byte{0x1f}, std::byte{0x20},
        std::byte{0x21}, std::byte{0x22}, std::byte{0x23}, std::byte{0x24},
        std::byte{0x25}, std::byte{0x26}, std::byte{0x27}, std::byte{0x28},
        std::byte{0x29}, std::byte{0x2a}, std::byte{0x2b}, std::byte{0x2c},
        std::byte{0x2d}, std::byte{0x2e}, std::byte{0x2f}, std::byte{0x30},
        std::byte{0x31}, std::byte{0x32}, std::byte{0x33}, std::byte{0x34},
        std::byte{0x35}, std::byte{0x36}, std::byte{0x37}, std::byte{0x38},
        std::byte{0x39}, std::byte{0x3a}, std::byte{0x3b}, std::byte{0x3c},
        std::byte{0x3d}, std::byte{0x3e}, std::byte{0x3f}, std::byte{0x40},
        std::byte{0x41}, std::byte{0x42}, std::byte{0x43}, std::byte{0x44},
        std::byte{0x45},
    };

    std::array<std::byte, 1024> stream{};
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 13,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   stream);
    ok = expect(built.code == loader::PacketStreamBuildCode::ok,
                "raw packetstream builds") && ok;
    const std::span<const std::byte> stream_view{stream.data(), built.bytes_written};

    MemoryStorage memory{};
    std::array<std::byte, 256> transport_buffer{};
    loader::PacketRuntime packet_runtime{make_storage(memory)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{.buffer = transport_buffer, .max_payload_size = 64},
    };
    RawFrontend raw{transport};

    raw.begin();
    ok = expect(raw.active && raw.raw_bytes == 0, "raw begin enters active mode") && ok;
    ok = expect(feed_chunks(raw, stream_view, std::array<std::uint32_t, 5>{1, 7, 3, 29, 64}),
                "raw chunks feed through byte transport") && ok;
    ok = expect(!raw.active &&
                    raw.last.packet.receive.stage == loader::Stage::launch_ready &&
                    raw.last.packet.next_sequence == built.next_sequence,
                "raw mode exits at launch-ready") && ok;
    ok = expect(raw.raw_bytes == built.bytes_written,
                "raw byte counter tracks packetstream bytes") && ok;
    ok = expect(std::memcmp(memory.bytes.data(), payload.data(), payload.size()) == 0,
                "raw receive stores payload") && ok;

    raw.begin();
    ok = expect(feed_chunks(raw, stream_view, std::array<std::uint32_t, 3>{5, 11, 2}),
                "raw begin resets packet sequence for second transfer") && ok;
    ok = expect(!raw.active && raw.last.packet.receive.stage == loader::Stage::launch_ready,
                "second raw transfer reaches launch-ready") && ok;

    raw.begin();
    const auto partial = raw.ingest(std::span<const std::byte>{stream.data(), 3});
    ok = expect(raw.active &&
                    partial.code == loader::ByteTransportCode::ok &&
                    partial.buffered_bytes == 3,
                "partial packet remains buffered and active") && ok;
    raw.abort_command();
    ok = expect(!raw.active &&
                    raw.last.packet.receive.stage == loader::Stage::idle &&
                    raw.last.packet.next_sequence == 0,
                "raw abort command resets transport") && ok;

    raw.begin();
    auto bad_stream = stream;
    loader::PacketHeader bad_header{};
    std::memcpy(&bad_header, bad_stream.data(), sizeof(bad_header));
    bad_header.magic = 0;
    std::memcpy(bad_stream.data(), &bad_header, sizeof(bad_header));
    const auto bad = raw.ingest(std::span<const std::byte>{bad_stream.data(), sizeof(loader::PacketHeader)});
    ok = expect(!raw.active &&
                    bad.code == loader::ByteTransportCode::packet_failed &&
                    bad.packet.packet_code == loader::PacketCode::bad_magic,
                "bad raw packet exits raw mode with diagnostic") && ok;

    raw.begin();
    std::array<std::byte, 128> abort_stream{};
    std::uint32_t abort_stream_size = 0;
    ok = expect(build_abort_stream(abort_stream, abort_stream_size),
                "abort packetstream builds") && ok;
    const auto aborted = raw.ingest(std::span<const std::byte>{abort_stream.data(), abort_stream_size});
    ok = expect(!raw.active &&
                    aborted.code == loader::ByteTransportCode::ok &&
                    aborted.packet.kind == loader::PacketKind::abort &&
                    aborted.packet.receive.stage == loader::Stage::idle,
                "packet abort exits raw mode") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-raw-uart-smoke] ok");
    return 0;
}
