#include "charm_dev_loader.hpp"

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
        .base_address = 0x24000000U,
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

} // namespace

int main() {
    std::array<std::byte, 12> image{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
        std::byte{0x40},
        std::byte{0x50},
        std::byte{0x60},
        std::byte{0x70},
        std::byte{0x80},
        std::byte{0x90},
        std::byte{0xA0},
        std::byte{0xB0},
        std::byte{0xC0},
    };

    const auto crc = loader::crc32_update(0, std::span<const std::byte>{image});
    loader::ImageManifest manifest{
        .magic = loader::kImageMagic,
        .version = loader::kImageVersion,
        .load_address = 0x24000020U,
        .entry_address = 0x24000021U,
        .size_bytes = static_cast<std::uint32_t>(image.size()),
        .crc32 = crc,
        .flags = 0,
    };

    bool ok = true;
    MemoryStorage memory{};
    loader::Session session{};
    auto result = session.begin(manifest, make_storage(memory));
    ok = expect(result.stage == loader::Stage::receiving && result.code == loader::Code::ok,
                "begin enters receiving") && ok;

    result = session.write_chunk(0, std::span<const std::byte>{image.data(), 5});
    ok = expect(result.stage == loader::Stage::receiving && result.received_bytes == 5,
                "partial chunk accepted") && ok;
    result = session.write_chunk(5, std::span<const std::byte>{image.data() + 5, image.size() - 5});
    ok = expect(result.stage == loader::Stage::complete && result.received_bytes == image.size(),
                "final chunk completes image") && ok;
    result = session.verify();
    ok = expect(result.stage == loader::Stage::verified && result.code == loader::Code::ok,
                "verify accepts matching crc") && ok;
    result = session.mark_launch_ready();
    ok = expect(result.stage == loader::Stage::launch_ready && result.code == loader::Code::ok,
                "launch ready after verify") && ok;
    ok = expect(memory.bytes[0x20] == image[0] && memory.bytes[0x2B] == image[11],
                "image stored at load offset") && ok;

    MemoryStorage memory2{};
    loader::Session out_of_order{};
    (void)out_of_order.begin(manifest, make_storage(memory2));
    result = out_of_order.write_chunk(4, std::span<const std::byte>{image.data(), 4});
    ok = expect(result.code == loader::Code::out_of_order,
                "out-of-order write rejected") && ok;

    MemoryStorage memory3{};
    loader::Session overflow{};
    (void)overflow.begin(manifest, make_storage(memory3));
    result = overflow.write_chunk(0, std::span<const std::byte>{image.data(), image.size()});
    ok = expect(result.stage == loader::Stage::complete, "overflow setup complete") && ok;
    result = overflow.write_chunk(static_cast<std::uint32_t>(image.size()),
                                  std::span<const std::byte>{image.data(), 1});
    ok = expect(result.code == loader::Code::invalid_argument ||
                    result.code == loader::Code::overflow,
                "extra chunk rejected") && ok;

    MemoryStorage memory4{};
    loader::Session bad_crc{};
    auto bad_manifest = manifest;
    bad_manifest.crc32 ^= 0xFFFFFFFFU;
    (void)bad_crc.begin(bad_manifest, make_storage(memory4));
    (void)bad_crc.write_chunk(0, std::span<const std::byte>{image});
    result = bad_crc.verify();
    ok = expect(result.code == loader::Code::crc_mismatch,
                "bad crc rejected") && ok;

    loader::Session invalid{};
    auto invalid_manifest = manifest;
    invalid_manifest.load_address = 0;
    result = invalid.begin(invalid_manifest, make_storage(memory4));
    ok = expect(result.code == loader::Code::invalid_argument,
                "invalid manifest rejected") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-session-smoke] ok");
    return 0;
}
