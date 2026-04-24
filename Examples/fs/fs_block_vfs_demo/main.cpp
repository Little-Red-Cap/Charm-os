#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

import block.device;
import block.registry;
import charm.system.bringup.block;
import charm.system.app_host;
import charm.system.caps;
import charm.system.init_block;
import fs_fatfs;
import fs_vfs;
import init.node;
import init.plan;
import out.api;
import platform.board.win_stub;
import platform.win.irq_guard;
import platform.win.time_source;
import platform.win.wakeup;
import util.core;
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

    bool parse_fat_partition_lba(const std::array<std::uint8_t, 512>& sector0,
                                 std::uint32_t& out_lba) {
        if (sector0[510] != 0x55 || sector0[511] != 0xAA) return false;
        const auto* parts = reinterpret_cast<const MbrPartition*>(sector0.data() + 446);
        for (int i = 0; i < 4; ++i) {
            const auto& p = parts[i];
            if (p.type == 0x0B || p.type == 0x0C) {
                out_lba = p.lba_first;
                return true;
            }
        }
        out_lba = 0;
        return true;
    }

    struct OffsetDevice {
        block::Device* base{nullptr};
        std::uint32_t lba_offset{0};
        block::Device device{};

        static block::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->read(self->base->ctx, lba + self->lba_offset, out);
        }
        static block::Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> in) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->write(self->base->ctx, lba + self->lba_offset, in);
        }
        static block::Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->erase(self->base->ctx, lba + self->lba_offset, count);
        }
        static block::Status flush_impl(void* ctx) noexcept {
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
    if (std::FILE* f = std::fopen(argv[1], "rb"); !f) {
        const int err = errno;
        if (err == ENOENT) {
            (void)out::println<"[ERR] image not found: {}">(sink, argv[1]);
        } else {
            (void)out::println<"[ERR] open image failed: {} err={}">(sink, argv[1], err);
        }
        return 1;
    } else {
        std::fclose(f);
    }

    auto caps = platform::board::win_stub::make_block_caps();
    using PumpCaps = charm::system::SystemCaps<
        platform::win::SpinIrqGuard,
        platform::win::NoopWakeup>;
    PumpCaps pump_caps{};
    charm::system::AppHost<PumpCaps> host{pump_caps};
    charm::system::BringupBlock<8, 16, 8> bringup{caps, host};

    charm::system::FileInitChain<block::Registry<8>> file_chain{
        bringup.block_registry(),
        argv[1],
        512,
        "block.sd0"
    };

    auto r = bringup.start_plan(
        file_chain.plan(),
        static_cast<util::u32>(init::Runlevel::all),
        init::Phase::app);
    if (!r) {
        (void)out::println<"[ERR] bringup failed err={}">(sink, static_cast<int>(r.error()));
        return 1;
    }

    auto* dev = bringup.block_registry().open_device("block.sd0");
    if (!dev) {
        (void)out::println<"[ERR] block capability not found: block.sd0">(sink);
        return 1;
    }

    std::array<std::uint8_t, 512> sector0{};
    auto read0 = dev->read(dev->ctx, 0,
        std::span<util::u8>(reinterpret_cast<util::u8*>(sector0.data()), sector0.size()));
    if (!read0) {
        (void)out::println<"[ERR] read MBR failed err={}">(sink, static_cast<int>(read0.err));
        return 1;
    }
    std::uint32_t lba = 0;
    const bool mbr_ok = parse_fat_partition_lba(sector0, lba);
    if (!mbr_ok) {
        (void)out::println<"[ERR] invalid MBR or signature missing">(sink);
        return 1;
    }
    if (lba == 0) {
        (void)out::println<"[ERR] no FAT partition found">(sink);
        return 1;
    }
    (void)out::println<"[block_vfs] MBR partition LBA={}">(sink, lba);

    OffsetDevice part_dev{};
    part_dev.init(*dev, lba);

    fs::FatFsMount fat{};
    auto st = fat.mount(part_dev.device, false);
    if (!st) {
        (void)out::println<"[ERR] mount failed at lba={} err={}">(sink, lba, static_cast<int>(st.err));
        return 1;
    }

    fs::clear_mounts();
    (void)fs::add_mount("/", fat.mount_point());

    fs::File f{};
    st = fs::vfs_open("/hello.txt", f);
    if (!st) {
        (void)out::println<"[ERR] open /hello.txt failed err={}">(sink, static_cast<int>(st.err));
        return 1;
    }

    std::array<util::u8, 64> buf{};
    (void)fs::read(f, std::span<util::u8>(buf.data(), buf.size()));
    buf.back() = 0;
    (void)out::println<"[block_vfs] hello.txt: {}">(sink, reinterpret_cast<char*>(buf.data()));
    (void)fs::vfs_close(f);

    return 0;
}
