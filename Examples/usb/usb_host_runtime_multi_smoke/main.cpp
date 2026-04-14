#include <array>
#include <cstddef>
#include <cstdio>
#include <span>

import block.device;
import block.registry;
import io.channel;
import io.reactor;
import io.registry;
import usb.host.runtime_block;
import usb.host.runtime_channel;
import usb.host.runtime_manager;
import util.core;

#include "../support/usb_host_runtime_block_support.hpp"
#include "../support/usb_host_runtime_channel_support.hpp"

namespace {
    using examples::usb::support::CdcRuntimeHarness;
    using examples::usb::support::MemoryDisk;
    using examples::usb::support::MscRuntimeHarness;
    using examples::usb::support::read_lba0;

    bool expect(bool cond, const char* message) {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool expect_block_status(block::Status st, block::Errc want, const char* message) {
        if (st.err != want) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d want=%d\n",
                         message,
                         static_cast<int>(st.err),
                         static_cast<int>(want));
            return false;
        }
        return true;
    }

    bool expect_io_error(const io::result& r, io::errc want, const char* message) {
        if (r.error() != want) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d want=%d\n",
                         message,
                         static_cast<int>(r.error()),
                         static_cast<int>(want));
            return false;
        }
        return true;
    }

}

int main() {
    block::Registry<4> block_registry{};
    io::Registry<4> io_registry{};
    block_registry.init();
    io_registry.init();

    MscRuntimeHarness<block::Registry<4>> msc{
        block_registry,
        "block.usb0",
        0x1209,
        0x0010,
        "MULTIUSB"
    };

    CdcRuntimeHarness<io::Registry<4>> cdc{
        io_registry,
        "io.usb0",
        0x1209,
        0x0011
    };

    usb::host::RuntimeManager<8, 8> runtime{"usb.host.multi"};
    auto add_msc = runtime.add_exported(msc.binding);
    if (!add_msc) {
        std::fprintf(stderr,
                     "[ERR] failed to add MSC exported binding err=%d\n",
                     static_cast<int>(add_msc.error()));
        return 1;
    }
    auto add_cdc = runtime.add_exported(cdc.binding);
    if (!add_cdc) {
        std::fprintf(stderr,
                     "[ERR] failed to add CDC exported binding err=%d\n",
                     static_cast<int>(add_cdc.error()));
        return 1;
    }

    auto* stable_block = msc.stable();
    auto* stable_channel = cdc.stable();
    if (!expect(stable_block == &msc.binding.exported_slot().device(), "stable block slot mismatch")) return 1;
    if (!expect(stable_channel == &cdc.binding.exported_slot().channel(), "stable channel slot mismatch")) return 1;

    std::array<util::u8, MemoryDisk::block_size> block_buf{};
    std::array<util::u8, 4> read_buf{};
    std::array<util::u8, 4> write_buf{
        static_cast<util::u8>('P'),
        static_cast<util::u8>('I'),
        static_cast<util::u8>('N'),
        static_cast<util::u8>('G')
    };

    if (!expect_block_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                             "detached block slot should read as noent")) return 1;
    if (!expect_io_error(stable_channel->read(read_buf), io::errc::noent,
                         "detached channel slot should read as noent")) return 1;

    if (!expect(runtime.scan(), "runtime manager scan failed")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should contain two devices")) return 1;
    if (!expect(runtime.enumerated(msc.binding) && runtime.enumerated(cdc.binding),
                "runtime manager did not enumerate all records")) return 1;

    if (!expect(msc.binding.attached(), "MSC slot was not attached")) return 1;
    if (!expect(cdc.binding.attached(), "CDC slot was not attached")) return 1;
    const auto cdc_generation_before = cdc.binding.generation();

    auto block_read = read_lba0(*stable_block, block_buf);
    if (!expect(static_cast<bool>(block_read), "attached block slot should read successfully")) return 1;
    if (!expect(block_buf[0] == 0xEB && block_buf[510] == 0x55 && block_buf[511] == 0xAA,
                "attached block slot returned unexpected data")) return 1;

    auto channel_read = stable_channel->read(read_buf);
    if (!expect(static_cast<bool>(channel_read) && channel_read.value() == 2,
                "attached channel slot should read two bytes")) return 1;
    if (!expect(read_buf[0] == static_cast<util::u8>('O') &&
                read_buf[1] == static_cast<util::u8>('K'),
                "attached channel slot returned unexpected read data")) return 1;

    auto channel_write = stable_channel->write(write_buf);
    if (!expect(static_cast<bool>(channel_write) && channel_write.value() == write_buf.size(),
                "attached channel slot should write the full payload")) return 1;
    if (!expect(cdc.backend.tx_size == write_buf.size() &&
                cdc.backend.tx_data[0] == static_cast<util::u8>('P'),
                "backend channel did not observe the expected write payload")) return 1;
    if (!expect(static_cast<bool>(stable_channel->flush()) && cdc.backend.flushed,
                "attached channel slot should flush successfully")) return 1;

    if (!expect(runtime.remove(msc.binding), "failed to remove MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 1, "runtime registry should keep only CDC after MSC remove")) return 1;
    if (!expect(!msc.binding.attached(), "MSC slot should detach after remove")) return 1;
    if (!expect_block_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                             "removed MSC slot should read as noent")) return 1;

    if (!expect(runtime.rediscover(msc.binding), "failed to rediscover MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should restore MSC after re-enumeration")) return 1;
    if (!expect(msc.binding.attached(), "MSC slot should reattach after re-enumeration")) return 1;
    if (!expect(static_cast<bool>(read_lba0(*stable_block, block_buf)),
                "re-enumerated MSC slot should read successfully")) return 1;
    if (!expect(cdc.binding.generation() == cdc_generation_before,
                "rediscovering MSC should not reinitialize unchanged CDC")) return 1;

    if (!expect(runtime.remove(cdc.binding), "failed to remove CDC device")) return 1;
    if (!expect(runtime.remove(msc.binding), "failed to remove MSC device the second time")) return 1;

    if (!expect(!msc.binding.attached(), "MSC slot should be detached after remove")) return 1;
    if (!expect(!cdc.binding.attached(), "CDC slot should be detached after remove")) return 1;
    if (!expect(runtime.registry().device_count() == 0, "runtime registry should be empty after both removes")) return 1;
    if (!expect_block_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                             "detached block slot should return noent after remove")) return 1;
    if (!expect_io_error(stable_channel->read(read_buf), io::errc::noent,
                         "detached channel slot should return noent after remove")) return 1;
    if (!expect_io_error(stable_channel->write(write_buf), io::errc::noent,
                         "detached channel slot should reject writes after remove")) return 1;
    if (!expect_io_error(stable_channel->flush(), io::errc::noent,
                         "detached channel slot should reject flush after remove")) return 1;

    std::puts("[OK] usb-host-runtime-multi-smoke passed");
    return 0;
}
