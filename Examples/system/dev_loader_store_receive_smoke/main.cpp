#include "charm_app_store.hpp"
#include "charm_app_store_install.hpp"
#include "charm_dev_loader.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;

struct MemoryNor {
    std::array<std::byte, 512> bytes{};

    MemoryNor() {
        bytes.fill(std::byte{0xff});
    }
};

struct RamStorage {
    std::array<std::byte, 256> bytes{};
};

bool nor_erase(void* ctx, std::uint32_t offset, std::uint32_t size) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || offset > nor->bytes.size() ||
        size > (nor->bytes.size() - offset) || (offset % 64U) != 0U || (size % 64U) != 0U) {
        return false;
    }
    for (std::uint32_t i = 0; i < size; ++i) {
        nor->bytes[offset + i] = std::byte{0xff};
    }
    return true;
}

bool nor_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || offset > nor->bytes.size() ||
        bytes.size() > (nor->bytes.size() - offset) || (offset % 4U) != 0U) {
        return false;
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto current = static_cast<unsigned>(nor->bytes[offset + i]);
        const auto next = static_cast<unsigned>(bytes[i]);
        if ((~current & next) != 0U) {
            return false;
        }
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        nor->bytes[offset + i] &= bytes[i];
    }
    return true;
}

bool nor_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || offset > nor->bytes.size() ||
        bytes.size() > (nor->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), nor->bytes.data() + offset, bytes.size());
    return true;
}

bool ram_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* storage = static_cast<RamStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(storage->bytes.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool ram_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* storage = static_cast<RamStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), storage->bytes.data() + offset, bytes.size());
    return true;
}

loader::Storage make_storage(RamStorage& storage) {
    return loader::Storage{
        .ctx = &storage,
        .base_address = 0x24040000U,
        .capacity_bytes = static_cast<std::uint32_t>(storage.bytes.size()),
        .write = ram_write,
        .read = ram_read,
    };
}

app_abi::AppStoreWritableMedia make_media(MemoryNor& nor) {
    return app_abi::AppStoreWritableMedia{
        .ctx = &nor,
        .capacity = static_cast<std::uint32_t>(nor.bytes.size()),
        .erase_block_size = 64,
        .write_align = 4,
        .erase = nor_erase,
        .write = nor_write,
        .read = nor_read,
    };
}

app_abi::AppStoreReader make_reader(MemoryNor& nor, std::uint32_t base_offset) {
    struct ReaderCtx {
        MemoryNor* nor{};
        std::uint32_t base_offset{};
    };
    static ReaderCtx ctx{};
    ctx = ReaderCtx{.nor = &nor, .base_offset = base_offset};
    return app_abi::AppStoreReader{
        .ctx = &ctx,
        .read = [](void* raw, std::uint32_t offset, std::span<std::byte> bytes) noexcept -> bool {
            auto* local = static_cast<ReaderCtx*>(raw);
            if (local == nullptr || local->nor == nullptr) {
                return false;
            }
            return nor_read(local->nor, local->base_offset + offset, bytes);
        },
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

    const std::array<std::byte, 12> payload{
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
    };

    std::array<std::byte, 256> store{};
    const std::array<app_abi::AppStoreBuildEntry, 1> entries{
        app_abi::AppStoreBuildEntry{.name = "hello_app", .payload = payload},
    };
    const auto built = app_abi::app_store_build_image(entries, store);
    ok = expect(built.code == app_abi::AppStoreBuildCode::ok, "store image builds") && ok;

    MemoryNor nor{};
    const std::uint32_t store_offset = 64;
    const auto installed = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(nor),
        .target_offset = store_offset,
        .image = std::span<const std::byte>{store.data(), built.bytes_written},
    });
    ok = expect(installed.code == app_abi::AppStoreInstallCode::ok,
                "store installs into memory nor") && ok;

    auto reader = make_reader(nor, store_offset);
    std::array<std::byte, 64> cache{};
    const auto staged = app_abi::app_store_stage_named_image(reader, "hello_app", cache);
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok, "store payload stages") && ok;
    ok = expect(staged.image.image_size == payload.size(), "staged payload size") && ok;

    const auto* staged_bytes = static_cast<const std::byte*>(staged.image.image_base);
    const auto crc = loader::crc32_update(0, std::span<const std::byte>{staged_bytes, staged.image.image_size});

    RamStorage ram{};
    loader::BinaryReceiveRuntime receiver{make_storage(ram)};
    auto result = receiver.begin(static_cast<std::uint32_t>(staged.image.image_size), crc, true);
    ok = expect(result.stage == loader::Stage::receiving, "receive starts from staged image") && ok;
    result = receiver.write(std::span<const std::byte>{staged_bytes, 5});
    ok = expect(result.stage == loader::Stage::receiving && receiver.cursor() == 5,
                "first staged chunk received") && ok;
    result = receiver.write(std::span<const std::byte>{staged_bytes + 5, staged.image.image_size - 5});
    ok = expect(result.stage == loader::Stage::complete,
                "final staged chunk completes receive") && ok;
    result = receiver.verify();
    ok = expect(result.stage == loader::Stage::verified && result.code == loader::Code::ok,
                "staged payload verifies") && ok;
    result = receiver.mark_launch_ready();
    ok = expect(result.stage == loader::Stage::launch_ready,
                "staged payload reaches launch-ready dry-run") && ok;
    ok = expect(std::memcmp(ram.bytes.data(), payload.data(), payload.size()) == 0,
                "received RAM bytes match store payload") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-store-receive-smoke] ok");
    return 0;
}
