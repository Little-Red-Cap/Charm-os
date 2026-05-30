#include "charm_app_store.hpp"
#include "charm_app_staged_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace {

namespace app_abi = charm::app_abi;

struct HostState {
    int argc_seen{0};
    bool entered{false};
};

HostState* g_host = nullptr;

extern "C" int staged_store_app_main(const CharmAppApi*, int argc, char** argv) {
    if (g_host == nullptr || argv == nullptr || argc < 2) {
        return 90;
    }
    if (std::string_view{argv[0]} != "hello_app" || std::string_view{argv[1]} != "store_arg") {
        return 91;
    }
    g_host->argc_seen = argc;
    g_host->entered = true;
    return 23;
}

CharmAppApi make_api() {
    CharmAppApi api{};
    api.magic = CHARM_APP_API_MAGIC;
    api.version = CHARM_APP_API_VERSION;
    api.size = sizeof(CharmAppApi);
    return api;
}

struct StagedLoadCtx {
    CharmAppMainFn entry{staged_store_app_main};
};

app_abi::AppLoadResult load_staged_store_image(void* ctx,
                                               const app_abi::AppImage& image,
                                               const app_abi::AppLoadBuffer&) noexcept {
    auto* load = static_cast<StagedLoadCtx*>(ctx);
    if (load == nullptr) {
        return {.code = app_abi::AppRunCode::image_not_found};
    }
    return app_abi::AppLoadResult{
        .code = app_abi::AppRunCode::ok,
        .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, load->entry),
    };
}

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

    HostState host{};
    g_host = &host;
    StagedLoadCtx load_ctx{};
    auto staged_for_runtime = staged.image;
    staged_for_runtime.format = app_abi::AppImageFormat::function;
    app_abi::StagedAppImageSource staged_source_ctx{
        .image = staged_for_runtime,
        .load_ctx = &load_ctx,
        .load = load_staged_store_image,
    };
    auto staged_source = app_abi::make_staged_app_image_source(staged_source_ctx);
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> runtime{};
    const auto run = runtime.run(app_abi::AppRunConfig{
        .source = &staged_source,
        .api = &api,
        .name = "hello_app",
        .arg_text = "store_arg",
    });
    ok = expect(run.stage == app_abi::AppRunStage::exit &&
                    run.code == app_abi::AppRunCode::ok &&
                    run.exited &&
                    run.exit_code == 23,
                "store staged AppImage runs through staged runtime adapter") && ok;
    ok = expect(host.entered && host.argc_seen == 2,
                "store staged AppImage receives argv through AppRuntime") && ok;

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
