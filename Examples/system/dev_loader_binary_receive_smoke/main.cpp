#include "charm_dev_loader.hpp"

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
        .base_address = 0x24040020U,
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
    bool ok = true;
    const std::array<std::byte, 10> image{
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
    };
    const auto crc = loader::crc32_update(0, std::span<const std::byte>{image});

    MemoryStorage memory{};
    loader::BinaryReceiveRuntime runtime{make_storage(memory)};
    auto result = runtime.begin(static_cast<std::uint32_t>(image.size()), crc, true);
    ok = expect(result.stage == loader::Stage::receiving && result.code == loader::Code::ok,
                "binary begin enters receiving") && ok;
    result = runtime.write(std::span<const std::byte>{image.data(), 3});
    ok = expect(result.stage == loader::Stage::receiving && runtime.cursor() == 3,
                "first binary chunk advances cursor") && ok;
    result = runtime.write(std::span<const std::byte>{image.data() + 3, 4});
    ok = expect(result.stage == loader::Stage::receiving && runtime.cursor() == 7,
                "second binary chunk advances cursor") && ok;
    result = runtime.write(std::span<const std::byte>{image.data() + 7, 3});
    ok = expect(result.stage == loader::Stage::complete && runtime.cursor() == image.size(),
                "final binary chunk completes image") && ok;
    result = runtime.verify();
    ok = expect(result.stage == loader::Stage::verified && result.code == loader::Code::ok,
                "binary receive verifies crc") && ok;
    result = runtime.mark_launch_ready();
    ok = expect(result.stage == loader::Stage::launch_ready,
                "binary receive reaches dry-run launch-ready") && ok;
    ok = expect(memory.bytes[0] == image[0] && memory.bytes[9] == image[9],
                "binary receive writes storage") && ok;

    MemoryStorage overflow_memory{};
    loader::BinaryReceiveRuntime overflow{make_storage(overflow_memory)};
    result = overflow.begin(4, 0, false);
    ok = expect(result.stage == loader::Stage::receiving, "overflow setup starts") && ok;
    result = overflow.write(std::span<const std::byte>{image.data(), 5});
    ok = expect(result.code == loader::Code::overflow,
                "oversized binary chunk rejected") && ok;

    MemoryStorage order_memory{};
    loader::BinaryReceiveRuntime out_of_order{make_storage(order_memory)};
    result = out_of_order.begin(4, 0, false);
    ok = expect(result.stage == loader::Stage::receiving, "out-of-order setup starts") && ok;
    result = out_of_order.write_at(1, std::span<const std::byte>{image.data(), 2});
    ok = expect(result.code == loader::Code::out_of_order,
                "out-of-order binary write rejected") && ok;

    MemoryStorage crc_memory{};
    loader::BinaryReceiveRuntime bad_crc{make_storage(crc_memory)};
    result = bad_crc.begin(static_cast<std::uint32_t>(image.size()), crc ^ 0xFFFFFFFFU, true);
    ok = expect(result.stage == loader::Stage::receiving, "bad crc setup starts") && ok;
    result = bad_crc.write(std::span<const std::byte>{image});
    ok = expect(result.stage == loader::Stage::complete, "bad crc image completes") && ok;
    result = bad_crc.verify();
    ok = expect(result.code == loader::Code::crc_mismatch,
                "bad binary crc rejected") && ok;

    MemoryStorage invalid_memory{};
    loader::BinaryReceiveRuntime invalid{make_storage(invalid_memory)};
    result = invalid.begin(0, 0, false);
    ok = expect(result.code == loader::Code::invalid_argument,
                "zero-size binary begin rejected") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-binary-receive-smoke] ok");
    return 0;
}
