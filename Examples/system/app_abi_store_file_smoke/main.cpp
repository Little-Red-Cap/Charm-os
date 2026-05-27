#include "charm_app_store.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace {

namespace app_abi = charm::app_abi;
namespace fs = std::filesystem;

struct FileReader {
    fs::path path{};
};

bool file_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* reader = static_cast<FileReader*>(ctx);
    if (reader == nullptr) {
        return false;
    }
    std::ifstream file(reader->path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

bool write_file(const fs::path& path, std::span<const std::byte> bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
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
    const std::array<app_abi::AppStoreBuildEntry, 2> entries{
        app_abi::AppStoreBuildEntry{.name = "hello_app", .payload = hello},
        app_abi::AppStoreBuildEntry{.name = "player_min", .payload = player},
    };

    std::array<std::byte, 256> store{};
    const auto built = app_abi::app_store_build_image(entries, store);
    ok = expect(built.code == app_abi::AppStoreBuildCode::ok, "store image builds") && ok;

    const fs::path path = fs::temp_directory_path() / "charm_app_abi_store_file_smoke.appstore.bin";
    ok = expect(write_file(path, std::span<const std::byte>{store.data(), built.bytes_written}),
                "store file is written") && ok;

    FileReader file_reader{.path = path};
    app_abi::AppStoreReader reader{.ctx = &file_reader, .read = file_read};

    app_abi::AppStoreHeader header{};
    ok = expect(app_abi::app_store_read_header(reader, header) == app_abi::AppStoreReadCode::ok,
                "file header reads") && ok;
    ok = expect(header.entry_count == 2, "file header entry count") && ok;

    const auto found = app_abi::app_store_find_entry(reader, "player_min");
    ok = expect(found.code == app_abi::AppStoreReadCode::ok &&
                    found.entry.size == player.size(),
                "file lookup finds player_min") && ok;

    std::array<std::byte, 16> cache{};
    const auto staged = app_abi::app_store_stage_named_image(reader, "player_min", cache);
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok, "file stage succeeds") && ok;
    ok = expect(staged.image.name == "player_min" &&
                    staged.image.image_base == cache.data() &&
                    staged.image.image_size == player.size(),
                "file stage returns AppImage") && ok;
    ok = expect(std::memcmp(cache.data(), player.data(), player.size()) == 0,
                "file staged payload matches source") && ok;

    const auto missing = app_abi::app_store_stage_named_image(reader, "missing", cache);
    ok = expect(missing.code == app_abi::AppStoreReadCode::image_not_found,
                "file stage reports missing image") && ok;

    std::error_code ec;
    fs::remove(path, ec);

    if (!ok) {
        return 1;
    }
    std::puts("[app-abi-store-file-smoke] ok");
    return 0;
}
