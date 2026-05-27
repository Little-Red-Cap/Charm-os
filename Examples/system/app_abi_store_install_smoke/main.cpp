#include "charm_app_store.hpp"
#include "charm_app_store_install.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace app_abi = charm::app_abi;

struct MemoryNor {
    std::array<std::byte, 512> bytes{};
    std::uint32_t erase_block_size{64};
    std::uint32_t write_align{4};
    bool fail_erase{false};
    bool fail_write{false};
    bool fail_read{false};

    MemoryNor() {
        bytes.fill(std::byte{0xff});
    }
};

bool nor_erase(void* ctx, std::uint32_t offset, std::uint32_t size) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || nor->fail_erase || size == 0U ||
        offset > nor->bytes.size() || size > (nor->bytes.size() - offset) ||
        (offset % nor->erase_block_size) != 0U || (size % nor->erase_block_size) != 0U) {
        return false;
    }
    for (std::uint32_t i = 0; i < size; ++i) {
        nor->bytes[offset + i] = std::byte{0xff};
    }
    return true;
}

bool nor_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* nor = static_cast<MemoryNor*>(ctx);
    if (nor == nullptr || nor->fail_write || bytes.empty() ||
        offset > nor->bytes.size() || bytes.size() > (nor->bytes.size() - offset) ||
        (offset % nor->write_align) != 0U) {
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
    if (nor == nullptr || nor->fail_read ||
        offset > nor->bytes.size() || bytes.size() > (nor->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), nor->bytes.data() + offset, bytes.size());
    return true;
}

app_abi::AppStoreWritableMedia make_media(MemoryNor& nor) {
    return app_abi::AppStoreWritableMedia{
        .ctx = &nor,
        .capacity = static_cast<std::uint32_t>(nor.bytes.size()),
        .erase_block_size = nor.erase_block_size,
        .write_align = nor.write_align,
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
    const std::array<std::byte, 4> hello{
        std::byte{0x7f},
        std::byte{'E'},
        std::byte{'L'},
        std::byte{'F'},
    };
    const std::array<std::byte, 6> player{
        std::byte{0x50},
        std::byte{0x4c},
        std::byte{0x41},
        std::byte{0x59},
        std::byte{0x45},
        std::byte{0x52},
    };
    std::array<std::byte, 256> store{};
    const std::array<app_abi::AppStoreBuildEntry, 2> entries{
        app_abi::AppStoreBuildEntry{.name = "hello_app", .payload = hello},
        app_abi::AppStoreBuildEntry{.name = "player_min", .payload = player},
    };
    const auto built = app_abi::app_store_build_image(entries, store);
    ok = expect(built.code == app_abi::AppStoreBuildCode::ok, "store image builds") && ok;

    MemoryNor nor{};
    const std::uint32_t target_offset = 128;
    const auto installed = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(nor),
        .target_offset = target_offset,
        .image = std::span<const std::byte>{store.data(), built.bytes_written},
    });
    ok = expect(installed.code == app_abi::AppStoreInstallCode::ok, "install succeeds") && ok;
    ok = expect(installed.bytes_written == built.bytes_written &&
                    installed.bytes_erased == app_abi::app_store_install_align_up(built.bytes_written, 64),
                "install records write and erase sizes") && ok;
    ok = expect(nor.bytes[0] == std::byte{0xff} &&
                    nor.bytes[target_offset] != std::byte{0xff},
                "install writes only target region") && ok;

    auto reader = make_reader(nor, target_offset);
    app_abi::AppStoreHeader header{};
    ok = expect(app_abi::app_store_read_header(reader, header) == app_abi::AppStoreReadCode::ok,
                "installed header reads") && ok;
    ok = expect(header.entry_count == 2, "installed header entry count") && ok;

    const auto found = app_abi::app_store_find_entry(reader, "player_min");
    ok = expect(found.code == app_abi::AppStoreReadCode::ok &&
                    found.entry.size == player.size(),
                "installed store lookup works") && ok;
    std::array<std::byte, 16> cache{};
    const auto staged = app_abi::app_store_stage_named_image(reader, "player_min", cache);
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok &&
                    std::memcmp(cache.data(), player.data(), player.size()) == 0,
                "installed store stages payload") && ok;

    MemoryNor too_small{};
    const auto too_large = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(too_small),
        .target_offset = 448,
        .image = std::span<const std::byte>{store.data(), built.bytes_written},
    });
    ok = expect(too_large.code == app_abi::AppStoreInstallCode::image_too_large,
                "image too large is reported") && ok;

    MemoryNor unaligned{};
    const auto bad_offset = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(unaligned),
        .target_offset = 4,
        .image = std::span<const std::byte>{store.data(), built.bytes_written},
    });
    ok = expect(bad_offset.code == app_abi::AppStoreInstallCode::unaligned_offset,
                "unaligned target offset is rejected") && ok;

    MemoryNor erase_fail{};
    erase_fail.fail_erase = true;
    const auto erase_failed = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(erase_fail),
        .target_offset = 0,
        .image = std::span<const std::byte>{store.data(), built.bytes_written},
    });
    ok = expect(erase_failed.code == app_abi::AppStoreInstallCode::erase_failed,
                "erase failure is reported") && ok;

    MemoryNor write_fail{};
    write_fail.fail_write = true;
    const auto write_failed = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(write_fail),
        .target_offset = 0,
        .image = std::span<const std::byte>{store.data(), built.bytes_written},
    });
    ok = expect(write_failed.code == app_abi::AppStoreInstallCode::write_failed,
                "write failure is reported") && ok;

    MemoryNor verify_fail{};
    verify_fail.fail_read = true;
    const auto verify_failed = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{
        .media = make_media(verify_fail),
        .target_offset = 0,
        .image = std::span<const std::byte>{store.data(), built.bytes_written},
    });
    ok = expect(verify_failed.code == app_abi::AppStoreInstallCode::verify_failed,
                "verify failure is reported") && ok;

    const auto invalid = app_abi::app_store_install_image(app_abi::AppStoreInstallConfig{});
    ok = expect(invalid.code == app_abi::AppStoreInstallCode::invalid_argument,
                "invalid install config is rejected") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[app-abi-store-install-smoke] ok");
    return 0;
}
