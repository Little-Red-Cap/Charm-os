#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

import charm.foundation;
import charm.runtime;
import out.api;
import util.expected;

namespace {
    struct StdoutSink {
        out::result<std::size_t> write(out::bytes b) noexcept {
            if (std::fwrite(b.data(), 1, b.size(), stdout) != b.size()) {
                return util::unexpected(out::errc::io_error);
            }
            return out::ok(b.size());
        }
        out::result<std::size_t> flush() noexcept {
            return std::fflush(stdout) == 0 ? out::ok<std::size_t>(0u)
                                            : util::unexpected(out::errc::io_error);
        }
    };

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
        block::Device* base{nullptr};
        std::uint32_t lba_offset{0};
        block::Device device{};

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

        void init(block::Device& dev, std::uint32_t offset) noexcept {
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
}

int main(int argc, char** argv) {
    StdoutSink sink{};
    if (argc < 2) {
        (void)out::println<"usage: fs-block-vfs-demo <disk.img|vhd>">(sink);
        return 1;
    }

    block::FileDevice file_dev;
    auto st = file_dev.open(argv[1], 512);
    if (!st) {
        (void)out::println<"[block_vfs] open failed err={}">(sink, static_cast<int>(st.err));
        return 1;
    }

    block::Registry<4> registry{};
    registry.init();
    block::DeviceDesc desc{"block.sd0", block::cap_id("block.sd0")};
    auto reg_st = registry.register_device(desc, file_dev.device());
    if (!reg_st) {
        (void)out::println<"[block_vfs] registry failed err={}">(sink, static_cast<int>(reg_st.error()));
        return 1;
    }

    auto* dev = registry.open_device("block.sd0");
    if (!dev) {
        (void)out::println<"[block_vfs] device not found">(sink);
        return 1;
    }

    std::array<std::uint8_t, 512> sector0{};
    auto read0 = dev->read(dev->ctx, 0,
        std::span<util::u8>(reinterpret_cast<util::u8*>(sector0.data()), sector0.size()));
    if (!read0) {
        (void)out::println<"[block_vfs] read MBR failed err={}">(sink, static_cast<int>(read0.err));
        return 1;
    }
    const auto lba = find_fat_partition_lba(sector0);
    if (lba != 0) {
        (void)out::println<"[block_vfs] MBR partition LBA={}">(sink, lba);
    }

    OffsetDevice part_dev{};
    part_dev.init(*dev, lba);

    fs::FatFsMount fat{};
    st = fat.mount(part_dev.device, false);
    if (!st) {
        (void)out::println<"[block_vfs] mount failed err={}">(sink, static_cast<int>(st.err));
        return 1;
    }

    fs::clear_mounts();
    (void)fs::add_mount("/", fat.mount_point());

    fs::File f{};
    st = fs::vfs_open("/hello.txt", f);
    if (!st) {
        (void)out::println<"[block_vfs] open /hello.txt failed err={}">(sink, static_cast<int>(st.err));
        return 1;
    }

    std::array<util::u8, 64> buf{};
    (void)fs::read(f, std::span<util::u8>(buf.data(), buf.size()));
    buf.back() = 0;
    (void)out::println<"[block_vfs] hello.txt: {}">(sink, reinterpret_cast<char*>(buf.data()));
    (void)fs::vfs_close(f);

    return 0;
}
