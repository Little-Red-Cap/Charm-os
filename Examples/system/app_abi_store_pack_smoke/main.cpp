#include "charm_app_store.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace app_abi = charm::app_abi;

struct StoreMemory {
    std::span<std::byte> bytes{};
};

bool store_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* memory = static_cast<StoreMemory*>(ctx);
    if (memory == nullptr || offset > memory->bytes.size() ||
        bytes.size() > (memory->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), memory->bytes.data() + offset, bytes.size());
    return true;
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
    const std::array<std::byte, 5> player{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
        std::byte{0x40},
        std::byte{0x50},
    };

    std::array<std::byte, 256> store{};
    const std::array<app_abi::AppStoreBuildEntry, 2> entries{
        app_abi::AppStoreBuildEntry{
            .name = "hello_app",
            .payload = hello,
            .flags = app_abi::app_store_format_flags(app_abi::AppImageFormat::elf),
        },
        app_abi::AppStoreBuildEntry{
            .name = "player_min",
            .payload = player,
            .flags = app_abi::app_store_format_flags(app_abi::AppImageFormat::modulex),
        },
    };

    const auto built = app_abi::app_store_build_image(entries, store, 16);
    ok = expect(built.code == app_abi::AppStoreBuildCode::ok, "store build succeeds") && ok;
    ok = expect(built.entry_count == 2 && built.bytes_written != 0, "build result records size") && ok;

    StoreMemory memory{.bytes = store};
    app_abi::AppStoreReader reader{.ctx = &memory, .read = store_read};

    app_abi::AppStoreHeader header{};
    ok = expect(app_abi::app_store_read_header(reader, header) == app_abi::AppStoreReadCode::ok,
                "built header is readable") && ok;
    ok = expect(header.entry_count == 2, "built header has entry count") && ok;

    app_abi::AppStoreEntry first{};
    app_abi::AppStoreEntry second{};
    ok = expect(app_abi::app_store_read_entry(reader, header, 0, first) == app_abi::AppStoreReadCode::ok,
                "first entry readable") && ok;
    ok = expect(app_abi::app_store_read_entry(reader, header, 1, second) == app_abi::AppStoreReadCode::ok,
                "second entry readable") && ok;
    ok = expect(app_abi::app_store_entry_name(first) == "hello_app" &&
                    app_abi::app_store_entry_name(second) == "player_min",
                "entry names round trip") && ok;
    ok = expect(app_abi::app_store_entry_format(first) == app_abi::AppImageFormat::elf &&
                    app_abi::app_store_entry_format(second) == app_abi::AppImageFormat::modulex,
                "entry format flags round trip") && ok;
    ok = expect(first.offset % 16U == 0U && second.offset % 16U == 0U,
                "payloads are aligned") && ok;

    const auto found = app_abi::app_store_find_entry(reader, "player_min");
    ok = expect(found.code == app_abi::AppStoreReadCode::ok &&
                    found.entry.offset == second.offset &&
                    found.entry.size == player.size(),
                "lookup finds packed payload") && ok;

    std::array<std::byte, 8> cache{};
    const auto staged = app_abi::app_store_stage_named_image(reader, "hello_app", cache);
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok, "packed image stages") && ok;
    ok = expect(cache[0] == std::byte{0x7f} && cache[1] == std::byte{'E'} &&
                    cache[2] == std::byte{'L'} && cache[3] == std::byte{'F'},
                "staged bytes match source payload") && ok;

    const std::array<app_abi::AppStoreBuildEntry, 2> duplicate{
        app_abi::AppStoreBuildEntry{.name = "same", .payload = hello},
        app_abi::AppStoreBuildEntry{.name = "same", .payload = player},
    };
    ok = expect(app_abi::app_store_build_image(duplicate, store).code ==
                    app_abi::AppStoreBuildCode::duplicate_name,
                "duplicate names rejected") && ok;

    const std::array<app_abi::AppStoreBuildEntry, 1> empty_name{
        app_abi::AppStoreBuildEntry{.name = "", .payload = hello},
    };
    ok = expect(app_abi::app_store_build_image(empty_name, store).code ==
                    app_abi::AppStoreBuildCode::empty_name,
                "empty name rejected") && ok;

    const std::array<app_abi::AppStoreBuildEntry, 1> long_name{
        app_abi::AppStoreBuildEntry{
            .name = "this_name_is_longer_than_the_store_entry_limit",
            .payload = hello,
        },
    };
    ok = expect(app_abi::app_store_build_image(long_name, store).code ==
                    app_abi::AppStoreBuildCode::name_too_long,
                "long name rejected") && ok;

    std::array<app_abi::AppStoreBuildEntry, app_abi::kAppStoreMaxEntries + 1U> too_many{};
    for (std::size_t i = 0; i < too_many.size(); ++i) {
        too_many[i] = app_abi::AppStoreBuildEntry{.name = "x", .payload = hello};
    }
    ok = expect(app_abi::app_store_build_image(too_many, store).code ==
                    app_abi::AppStoreBuildCode::too_many_entries,
                "too many entries rejected before duplicate check") && ok;

    std::array<std::byte, 32> tiny_store{};
    ok = expect(app_abi::app_store_build_image(entries, tiny_store).code ==
                    app_abi::AppStoreBuildCode::output_too_small,
                "small output buffer rejected") && ok;

    const std::array<app_abi::AppStoreBuildEntry, 1> invalid_payload{
        app_abi::AppStoreBuildEntry{.name = "empty_payload", .payload = {}},
    };
    ok = expect(app_abi::app_store_build_image(invalid_payload, store).code ==
                    app_abi::AppStoreBuildCode::invalid_argument,
                "empty payload rejected") && ok;
    ok = expect(app_abi::app_store_build_image({}, store).code ==
                    app_abi::AppStoreBuildCode::invalid_argument,
                "empty entry list rejected") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[app-abi-store-pack-smoke] ok");
    return 0;
}
