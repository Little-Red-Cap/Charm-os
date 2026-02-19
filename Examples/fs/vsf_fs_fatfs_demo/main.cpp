#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

import charm.foundation;
import charm.runtime;

namespace {
    struct MbrPartition {
        std::uint8_t status;
        std::uint8_t chs_first[3];
        std::uint8_t type;
        std::uint8_t chs_last[3];
        std::uint32_t lba_first;
        std::uint32_t sectors;
    };

    std::uint32_t find_fat_partition_lba(const std::array<std::uint8_t, 512>& sector0) {
        if (sector0[510] != 0x55 || sector0[511] != 0xAA) return 0;
        const auto* parts = reinterpret_cast<const MbrPartition*>(sector0.data() + 446);
        for (int i = 0; i < 4; ++i) {
            const auto& p = parts[i];
            if (p.type == 0x0B || p.type == 0x0C) {
                return p.lba_first;
            }
        }
        return 0;
    }

    struct OffsetDevice {
        fs::BlockDevice* base{nullptr};
        std::uint32_t lba_offset{0};

        fs::BlockDevice device{};

        static fs::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->read(self->base->ctx, lba + self->lba_offset, out);
        }
        static fs::Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> in) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->write(self->base->ctx, lba + self->lba_offset, in);
        }
        static fs::Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->erase(self->base->ctx, lba + self->lba_offset, count);
        }
        static fs::Status flush_impl(void* ctx) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->flush(self->base->ctx);
        }

        void init(fs::BlockDevice& dev, std::uint32_t offset) noexcept {
            base = &dev;
            lba_offset = offset;
            device.ctx = this;
            device.read = &OffsetDevice::read_impl;
            device.write = &OffsetDevice::write_impl;
            device.erase = &OffsetDevice::erase_impl;
            device.flush = &OffsetDevice::flush_impl;
            device.block_size = dev.block_size;
            device.block_count = dev.block_count > offset ? (dev.block_count - offset) : 0;
        }
    };

    fs::Status list_root() {
        return fs::vfs_list("/", nullptr, +[](void*, const fs::MountOps::ListEntry& e) noexcept {
            std::printf("[fatfs] %s %.*s (%llu)\n",
                        e.type == fs::NodeType::dir ? "d" : "f",
                        static_cast<int>(e.name.size()),
                        e.name.data(),
                        static_cast<unsigned long long>(e.size));
            return fs::Status{fs::Err::ok};
        });
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: vsf-fs-fatfs-demo <disk.img|vhd>\n");
        return 1;
    }

    fs::BlockFile file_dev;
    auto st = file_dev.open(argv[1], 512);
    if (!st) {
        std::printf("[fatfs] open failed err=%d\n", static_cast<int>(st.err));
        return 1;
    }

    std::array<std::uint8_t, 512> sector0{};
    auto read0 = file_dev.device().read(file_dev.device().ctx, 0,
        std::span<util::u8>(reinterpret_cast<util::u8*>(sector0.data()), sector0.size()));
    if (!read0) {
        std::printf("[fatfs] read MBR failed err=%d\n", static_cast<int>(read0.err));
        return 1;
    }
    const auto lba = find_fat_partition_lba(sector0);
    if (lba != 0) {
        std::printf("[fatfs] MBR partition LBA=%u\n", lba);
    }

    OffsetDevice part_dev{};
    part_dev.init(file_dev.device(), lba);

    fs::FatFsMount fat{};
    st = fat.mount(part_dev.device, false);
    if (!st) {
        std::printf("[fatfs] mount failed err=%d\n", static_cast<int>(st.err));
        return 1;
    }

    fs::clear_mounts();
    (void)fs::add_mount("/", fat.mount_point());

    (void)list_root();

    fs::File f{};
    st = fs::vfs_open("/hello.txt", f);
    if (st) {
        std::array<util::u8, 64> buf{};
        (void)fs::read(f, std::span<util::u8>(buf.data(), buf.size()));
        buf.back() = 0;
        std::printf("[fatfs] hello.txt: %s\n", reinterpret_cast<char*>(buf.data()));
        (void)fs::vfs_close(f);
    } else {
        std::printf("[fatfs] open /hello.txt failed err=%d\n", static_cast<int>(st.err));
    }

    return 0;
}
