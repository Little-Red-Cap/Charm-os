#include "charm_app_store.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace app_abi = charm::app_abi;

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

struct StoreMemory {
    std::array<std::byte, 256> bytes{};
    bool fail_header{false};
    bool fail_entry1{false};
    bool fail_image{false};
};

bool store_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* memory = static_cast<StoreMemory*>(ctx);
    if (memory == nullptr || offset > memory->bytes.size() ||
        bytes.size() > (memory->bytes.size() - offset)) {
        return false;
    }
    if (memory->fail_header && offset == 0U) {
        return false;
    }
    if (memory->fail_entry1 &&
        offset == sizeof(app_abi::AppStoreHeader) + sizeof(app_abi::AppStoreEntry)) {
        return false;
    }
    if (memory->fail_image && offset == 0x80U) {
        return false;
    }
    std::memcpy(bytes.data(), memory->bytes.data() + offset, bytes.size());
    return true;
}

void write_bytes(StoreMemory& memory, std::uint32_t offset, const void* data, std::size_t size) {
    std::memcpy(memory.bytes.data() + offset, data, size);
}

} // namespace

int main() {
    bool ok = true;

    app_abi::AppStoreHeader header{
        .magic = app_abi::kAppStoreMagic,
        .version = app_abi::kAppStoreVersion,
        .header_size = sizeof(app_abi::AppStoreHeader),
        .entry_count = 2,
        .entry_size = sizeof(app_abi::AppStoreEntry),
    };
    ok = expect(app_abi::app_store_header_valid(header), "valid header accepted") && ok;
    ok = expect(app_abi::app_store_entry_offset(header, 0) == sizeof(app_abi::AppStoreHeader),
                "entry 0 offset") && ok;
    ok = expect(app_abi::app_store_entry_offset(header, 1) ==
                    sizeof(app_abi::AppStoreHeader) + sizeof(app_abi::AppStoreEntry),
                "entry 1 offset") && ok;

    app_abi::AppStoreEntry entry{};
    std::memcpy(entry.name, "player_min", sizeof("player_min"));
    entry.offset = 0x1000;
    entry.size = 0x2000;
    ok = expect(app_abi::app_store_entry_name(entry) == "player_min",
                "entry name view") && ok;
    ok = expect(app_abi::app_store_entry_runnable(entry), "entry runnable") && ok;

    entry.size = 0;
    ok = expect(!app_abi::app_store_entry_runnable(entry),
                "zero-size entry is not runnable") && ok;

    header.magic = 0;
    ok = expect(!app_abi::app_store_header_valid(header),
                "bad magic rejected") && ok;
    header.magic = app_abi::kAppStoreMagic;
    header.entry_count = app_abi::kAppStoreMaxEntries + 1;
    ok = expect(!app_abi::app_store_header_valid(header),
                "too many entries rejected") && ok;

    StoreMemory memory{};
    header.entry_count = 2;
    write_bytes(memory, 0, &header, sizeof(header));

    app_abi::AppStoreEntry hello{};
    std::memcpy(hello.name, "hello_app", sizeof("hello_app"));
    hello.offset = 0x80;
    hello.size = 4;
    write_bytes(memory, app_abi::app_store_entry_offset(header, 0), &hello, sizeof(hello));

    app_abi::AppStoreEntry player{};
    std::memcpy(player.name, "player_min", sizeof("player_min"));
    player.offset = 0x90;
    player.size = 8;
    write_bytes(memory, app_abi::app_store_entry_offset(header, 1), &player, sizeof(player));

    const std::array<std::byte, 4> image{
        std::byte{0xCA},
        std::byte{0xFE},
        std::byte{0xBA},
        std::byte{0xBE},
    };
    write_bytes(memory, hello.offset, image.data(), image.size());

    app_abi::AppStoreReader reader{
        .ctx = &memory,
        .read = store_read,
    };
    app_abi::AppStoreHeader read_header{};
    ok = expect(app_abi::app_store_read_header(reader, read_header) == app_abi::AppStoreReadCode::ok,
                "reader accepts valid header") && ok;
    ok = expect(read_header.entry_count == 2, "reader returns header content") && ok;

    const auto found = app_abi::app_store_find_entry(reader, "player_min");
    ok = expect(found.code == app_abi::AppStoreReadCode::ok, "lookup finds named entry") && ok;
    ok = expect(found.entry_index == 1 && found.entry.offset == 0x90,
                "lookup returns selected entry metadata") && ok;

    const auto missing = app_abi::app_store_find_entry(reader, "missing");
    ok = expect(missing.code == app_abi::AppStoreReadCode::image_not_found,
                "missing image has stable error") && ok;

    std::array<std::byte, 8> destination{};
    ok = expect(app_abi::app_store_read_image(reader, hello.offset, hello.size, destination) ==
                    app_abi::AppStoreReadCode::ok,
                "image read succeeds") && ok;
    ok = expect(destination[0] == std::byte{0xCA} && destination[3] == std::byte{0xBE},
                "image bytes are copied") && ok;
    ok = expect(app_abi::app_store_read_image(reader, hello.offset, 64, destination) ==
                    app_abi::AppStoreReadCode::image_too_large,
                "oversized image rejected before read") && ok;

    memory.fail_header = true;
    ok = expect(app_abi::app_store_find_entry(reader, "hello_app").code ==
                    app_abi::AppStoreReadCode::header_unreadable,
                "header read failure is reported") && ok;
    memory.fail_header = false;

    header.magic = 0;
    write_bytes(memory, 0, &header, sizeof(header));
    ok = expect(app_abi::app_store_find_entry(reader, "hello_app").code ==
                    app_abi::AppStoreReadCode::header_invalid,
                "invalid header is reported") && ok;
    header.magic = app_abi::kAppStoreMagic;
    write_bytes(memory, 0, &header, sizeof(header));

    memory.fail_entry1 = true;
    ok = expect(app_abi::app_store_find_entry(reader, "player_min").code ==
                    app_abi::AppStoreReadCode::entry_read_failed,
                "entry read failure is reported") && ok;
    memory.fail_entry1 = false;

    memory.fail_image = true;
    ok = expect(app_abi::app_store_read_image(reader, hello.offset, hello.size, destination) ==
                    app_abi::AppStoreReadCode::image_read_failed,
                "image read failure is reported") && ok;
    memory.fail_image = false;

    std::array<std::byte, 8> stage_cache{};
    const auto staged = app_abi::app_store_stage_named_image(reader, "hello_app", stage_cache);
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok,
                "staging named image succeeds") && ok;
    ok = expect(staged.image.name == "hello_app" &&
                    staged.image.image_base == stage_cache.data() &&
                    staged.image.image_size == hello.size,
                "staging returns AppImage view") && ok;
    ok = expect(stage_cache[0] == std::byte{0xCA} && stage_cache[3] == std::byte{0xBE},
                "staging copies payload") && ok;

    const auto staged_missing = app_abi::app_store_stage_named_image(reader, "missing", stage_cache);
    ok = expect(staged_missing.code == app_abi::AppStoreReadCode::image_not_found,
                "staging reports missing image") && ok;

    std::array<std::byte, 2> small_cache{};
    const auto staged_too_large = app_abi::app_store_stage_named_image(reader, "hello_app", small_cache);
    ok = expect(staged_too_large.code == app_abi::AppStoreReadCode::image_too_large,
                "staging reports oversized image") && ok;

    memory.fail_image = true;
    const auto staged_read_failed = app_abi::app_store_stage_named_image(reader, "hello_app", stage_cache);
    ok = expect(staged_read_failed.code == app_abi::AppStoreReadCode::image_read_failed,
                "staging reports image read failure") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[app-abi-store-smoke] ok");
    return 0;
}
