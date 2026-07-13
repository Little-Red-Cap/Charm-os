#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

import player.cover_resource;
import player.storage;
import fs_core;
import fs_errno;
import fs_stream;

namespace {
    struct MountProbe {
        const char* expected_path{nullptr};
        std::size_t calls{0};
    };

    fs::Status mount_probe(void* ctx, const char* path) {
        auto& probe = *static_cast<MountProbe*>(ctx);
        ++probe.calls;
        return std::string_view{path ? path : ""} == probe.expected_path
            ? fs::Status{fs::Errc::nosys}
            : fs::Status{fs::Errc::inval};
    }

    bool expect(bool value, const char* message) {
        if (!value) std::printf("[player-instance-isolation-smoke] fail: %s\n", message);
        return value;
    }
}

int main() {
    MountProbe first_mount{"first"};
    MountProbe second_mount{"second"};
    player::StorageSession first_storage{{&first_mount, &mount_probe, "first"}};
    player::StorageSession second_storage{{&second_mount, &mount_probe, "second"}};

    (void)first_storage.scan();
    (void)second_storage.scan();
    (void)first_storage.scan();
    (void)second_storage.scan();
    if (!expect(first_mount.calls == 1 && second_mount.calls == 1,
                "each storage binding scans once")
        || !expect(first_storage.scan_count() == 1 && second_storage.scan_count() == 1,
                   "scan counters remain instance-local")) {
        return 1;
    }

    constexpr std::array<std::uint32_t, 1> first_pixels{0xff112233u};
    constexpr std::array<std::uint32_t, 1> second_pixels{0xff445566u};
    const std::array<player::PlayerCoverResourceRecord, 1> first_records{{
        {"/first", player::CoverResourceKind::FolderFile, "first-cover", first_pixels, 1, 1},
    }};
    const std::array<player::PlayerCoverResourceRecord, 1> second_records{{
        {"/second", player::CoverResourceKind::FolderFile, "second-cover", second_pixels, 1, 1},
    }};
    const player::PlayerCoverResourceRecordTableView first_table{first_records};
    const player::PlayerCoverResourceRecordTableView second_table{second_records};
    const auto first_cover = player::make_cover_resource_record_table_binding(first_table);
    const auto second_cover = player::make_cover_resource_record_table_binding(second_table);
    player::CoverResourceView first_view{};
    player::CoverResourceView second_view{};
    if (!expect(player::resolve_cover_resource(
                    first_cover, {"/first", player::CoverResourceKind::FolderFile, {}}, first_view),
                "first cover resolves")
        || !expect(player::resolve_cover_resource(
                    second_cover, {"/second", player::CoverResourceKind::FolderFile, {}}, second_view),
                "second cover resolves")
        || !expect(first_view.key == "first-cover" && second_view.key == "second-cover",
                   "cover results remain instance-local")
        || !expect(!player::resolve_cover_resource(
                    first_cover, {"/second", player::CoverResourceKind::FolderFile, {}}, first_view),
                   "cross-instance cover lookup rejected")) {
        return 1;
    }

    std::puts("[player-instance-isolation-smoke] ok");
    return 0;
}
