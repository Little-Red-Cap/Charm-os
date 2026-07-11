#include "charm_app_fat_catalog.hpp"
#include "charm_app_staged_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

namespace {

namespace app_abi = charm::app_abi;

constexpr std::uint32_t kSectorSize = app_abi::kFirmwareCatalogSectorSize;
constexpr std::uint32_t kPartitionLba = 1;
constexpr std::uint32_t kFatLba = 2;
constexpr std::uint32_t kDataLba = 3;
constexpr std::uint32_t kRootCluster = 2;
constexpr std::uint32_t kCharmCluster = 3;
constexpr std::uint32_t kAppsCluster = 4;
constexpr std::uint32_t kHelloCluster = 5;
constexpr std::uint32_t kPlayerCluster = 6;
constexpr std::uint32_t kBrokenCluster = 7;
constexpr std::uint32_t kReadmeCluster = 8;

struct MemoryDisk {
    std::vector<std::byte> bytes;
};

bool disk_read(void* ctx, std::uint32_t lba, std::span<std::byte> out) noexcept {
    auto* disk = static_cast<MemoryDisk*>(ctx);
    if (disk == nullptr || out.size() != kSectorSize) {
        return false;
    }
    const auto offset = static_cast<std::size_t>(lba) * kSectorSize;
    if (offset > disk->bytes.size() || out.size() > (disk->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(out.data(), disk->bytes.data() + offset, out.size());
    return true;
}

app_abi::FatBlockReader reader_for(MemoryDisk& disk) noexcept {
    return app_abi::FatBlockReader{
        .ctx = &disk,
        .read = disk_read,
        .block_count = static_cast<std::uint32_t>(disk.bytes.size() / kSectorSize),
    };
}

std::byte* sector(MemoryDisk& disk, std::uint32_t lba) noexcept {
    return disk.bytes.data() + (static_cast<std::size_t>(lba) * kSectorSize);
}

void put16(std::byte* p, std::uint16_t value) noexcept {
    p[0] = static_cast<std::byte>(value & 0xFFU);
    p[1] = static_cast<std::byte>((value >> 8U) & 0xFFU);
}

void put32(std::byte* p, std::uint32_t value) noexcept {
    p[0] = static_cast<std::byte>(value & 0xFFU);
    p[1] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    p[2] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    p[3] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

std::uint32_t cluster_lba(std::uint32_t cluster) noexcept {
    return kDataLba + (cluster - 2U);
}

void set_fat(MemoryDisk& disk, std::uint32_t cluster, std::uint32_t next) noexcept {
    put32(sector(disk, kFatLba) + (cluster * 4U), next);
}

void set_short_name(std::byte* entry, std::string_view base, std::string_view ext) noexcept {
    for (std::size_t i = 0; i < 11U; ++i) {
        entry[i] = std::byte{' '};
    }
    for (std::size_t i = 0; i < base.size() && i < 8U; ++i) {
        entry[i] = static_cast<std::byte>(base[i]);
    }
    for (std::size_t i = 0; i < ext.size() && i < 3U; ++i) {
        entry[8U + i] = static_cast<std::byte>(ext[i]);
    }
}

void put_dir(std::byte* dir,
             std::uint32_t index,
             std::string_view base,
             std::string_view ext,
             std::uint8_t attr,
             std::uint32_t first_cluster,
             std::uint32_t size) noexcept {
    auto* entry = dir + (index * 32U);
    set_short_name(entry, base, ext);
    entry[11] = static_cast<std::byte>(attr);
    put16(entry + 20, static_cast<std::uint16_t>((first_cluster >> 16U) & 0xFFFFU));
    put16(entry + 26, static_cast<std::uint16_t>(first_cluster & 0xFFFFU));
    put32(entry + 28, size);
}

std::vector<std::byte> payload(std::string_view text) {
    std::vector<std::byte> out(text.size());
    std::memcpy(out.data(), text.data(), text.size());
    return out;
}

MemoryDisk make_disk(bool include_apps = true, bool include_files = true) {
    MemoryDisk disk{.bytes = std::vector<std::byte>(16U * kSectorSize, std::byte{})};
    auto* bs = sector(disk, kPartitionLba);
    bs[0] = std::byte{0xEB};
    bs[1] = std::byte{0x58};
    bs[2] = std::byte{0x90};
    std::memcpy(bs + 3, "MSDOS5.0", 8);
    put16(bs + 11, kSectorSize);
    bs[13] = std::byte{1};
    put16(bs + 14, 1);
    bs[16] = std::byte{1};
    put16(bs + 17, 0);
    put16(bs + 19, 0);
    bs[21] = std::byte{0xF8};
    put16(bs + 22, 0);
    put32(bs + 32, 15);
    put32(bs + 36, 1);
    put32(bs + 44, kRootCluster);
    std::memcpy(bs + 0x52, "FAT32   ", 8);
    bs[510] = std::byte{0x55};
    bs[511] = std::byte{0xAA};

    set_fat(disk, 0, 0x0FFFFFF8U);
    set_fat(disk, 1, 0x0FFFFFFFU);
    set_fat(disk, kRootCluster, 0x0FFFFFFFU);
    set_fat(disk, kCharmCluster, 0x0FFFFFFFU);
    set_fat(disk, kAppsCluster, 0x0FFFFFFFU);
    set_fat(disk, kHelloCluster, 0x0FFFFFFFU);
    set_fat(disk, kPlayerCluster, 0x0FFFFFFFU);
    set_fat(disk, kBrokenCluster, 0x00000000U);
    set_fat(disk, kReadmeCluster, 0x0FFFFFFFU);

    auto* root = sector(disk, cluster_lba(kRootCluster));
    put_dir(root, 0, "CHARM", "", 0x10, kCharmCluster, 0);

    auto* charm = sector(disk, cluster_lba(kCharmCluster));
    if (include_apps) {
        put_dir(charm, 0, "APPS", "", 0x10, kAppsCluster, 0);
    }

    auto* apps = sector(disk, cluster_lba(kAppsCluster));
    if (include_files) {
        const auto hello = payload("hello elf payload");
        const auto player = payload("player elf payload");
        const auto readme = payload("not firmware");
        put_dir(apps, 0, "HELLOAPP", "ELF", 0x20, kHelloCluster, static_cast<std::uint32_t>(hello.size()));
        put_dir(apps, 1, "PLAYER", "ELF", 0x20, kPlayerCluster, static_cast<std::uint32_t>(player.size()));
        put_dir(apps, 2, "README", "TXT", 0x20, kReadmeCluster, static_cast<std::uint32_t>(readme.size()));
        std::memcpy(sector(disk, cluster_lba(kHelloCluster)), hello.data(), hello.size());
        std::memcpy(sector(disk, cluster_lba(kPlayerCluster)), player.data(), player.size());
        std::memcpy(sector(disk, cluster_lba(kReadmeCluster)), readme.data(), readme.size());
    }
    return disk;
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

int fake_main(const CharmAppApi*, int argc, char** argv) {
    if (argc != 2 || argv == nullptr || std::string_view{argv[0]} != "HELLOAPP.ELF" ||
        std::string_view{argv[1]} != "arg1") {
        return 41;
    }
    return 7;
}

app_abi::AppLoadResult fake_load(void*, const app_abi::AppImage& image, const app_abi::AppLoadBuffer&) noexcept {
    if (image.format != app_abi::AppImageFormat::elf || image.image_base == nullptr || image.image_size == 0U) {
        return {.code = app_abi::AppRunCode::load_failed};
    }
    return {
        .code = app_abi::AppRunCode::ok,
        .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, fake_main),
    };
}

CharmAppApi make_api() {
    CharmAppApi api{};
    api.magic = CHARM_APP_API_MAGIC;
    api.version = CHARM_APP_API_VERSION;
    api.size = sizeof(CharmAppApi);
    return api;
}

bool test_catalog_success_and_runtime() {
    bool ok = true;
    auto disk = make_disk();
    app_abi::FirmwareCatalog catalog{};
    const auto listed = app_abi::firmware_catalog_scan_fat(
        reader_for(disk), kPartitionLba, app_abi::kFirmwareCatalogDefaultDirectory, catalog);
    ok = expect(listed.code == app_abi::FirmwareCatalogCode::ok && listed.count == 2U,
                "FAT catalog lists ELF files and ignores non-ELF files") && ok;
    ok = expect(app_abi::fat_array_view(catalog.entries[0].name) == "HELLOAPP.ELF",
                "catalog preserves short ELF name") && ok;

    std::array<std::byte, 128> cache{};
    const auto staged = app_abi::firmware_file_stage_fat(
        reader_for(disk), kPartitionLba, catalog.entries[0], cache);
    ok = expect(staged.code == app_abi::FirmwareCatalogCode::ok &&
                    staged.image.format == app_abi::AppImageFormat::elf &&
                    staged.bytes_read == catalog.entries[0].size,
                "catalog file stages as ELF AppImage") && ok;

    app_abi::StagedAppImageSource staged_source{
        .image = staged.image,
        .load_ctx = nullptr,
        .load = fake_load,
    };
    auto source = app_abi::make_staged_app_image_source(staged_source);
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> runtime{};
    std::array<std::byte, 64> load{};
    const auto result = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{.base = load.data(), .size = load.size(), .align = 1},
        .api = &api,
        .name = staged.image.name,
        .arg_text = "arg1",
    });
    ok = expect(result.stage == app_abi::AppRunStage::exit &&
                    result.code == app_abi::AppRunCode::ok &&
                    result.exited &&
                    result.exit_code == 7,
                "staged file AppImage enters AppRuntime through fake loader") && ok;
    return ok;
}

bool test_error_paths() {
    bool ok = true;

    auto bad = make_disk();
    sector(bad, kPartitionLba)[510] = std::byte{0};
    app_abi::FirmwareCatalog catalog{};
    auto result = app_abi::firmware_catalog_scan_fat(
        reader_for(bad), kPartitionLba, app_abi::kFirmwareCatalogDefaultDirectory, catalog);
    ok = expect(result.code == app_abi::FirmwareCatalogCode::fat_invalid,
                "bad boot sector rejected") && ok;

    auto missing = make_disk(false, false);
    result = app_abi::firmware_catalog_scan_fat(
        reader_for(missing), kPartitionLba, app_abi::kFirmwareCatalogDefaultDirectory, catalog);
    ok = expect(result.code == app_abi::FirmwareCatalogCode::directory_missing,
                "missing /CHARM/APPS reports directory_missing") && ok;

    auto empty = make_disk(true, false);
    result = app_abi::firmware_catalog_scan_fat(
        reader_for(empty), kPartitionLba, app_abi::kFirmwareCatalogDefaultDirectory, catalog);
    ok = expect(result.code == app_abi::FirmwareCatalogCode::no_firmware,
                "empty apps directory reports no_firmware") && ok;

    auto disk = make_disk();
    result = app_abi::firmware_catalog_scan_fat(
        reader_for(disk), kPartitionLba, app_abi::kFirmwareCatalogDefaultDirectory, catalog);
    ok = expect(result.code == app_abi::FirmwareCatalogCode::ok, "fixture catalog still valid") && ok;
    std::array<std::byte, 4> tiny{};
    auto staged = app_abi::firmware_file_stage_fat(reader_for(disk), kPartitionLba, catalog.entries[0], tiny);
    ok = expect(staged.code == app_abi::FirmwareCatalogCode::image_too_large,
                "small stage cache reports image_too_large") && ok;

    app_abi::FirmwareEntry broken = catalog.entries[0];
    broken.first_cluster = kBrokenCluster;
    broken.size = 600U;
    std::array<std::byte, 1024> cache{};
    staged = app_abi::firmware_file_stage_fat(reader_for(disk), kPartitionLba, broken, cache);
    ok = expect(staged.code == app_abi::FirmwareCatalogCode::file_read_failed,
                "broken cluster chain reports file_read_failed") && ok;
    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok = test_catalog_success_and_runtime() && ok;
    ok = test_error_paths() && ok;
    if (!ok) {
        return 1;
    }
    std::puts("[resident-launcher-fat-catalog-smoke] ok");
    return 0;
}
