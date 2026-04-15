#include <array>
#include <cstddef>
#include <cstdio>
#include <span>

import block.device;
import block.device.slot_export;
import block.registry;
import device.manager;
import io.channel;
import io.channel.slot_export;
import io.reactor;
import io.registry;
import usb.host.runtime_block;
import usb.host.runtime_channel;
import usb.host.runtime_manager;
import util.core;

#include "../support/usb_host_runtime_assert_support.hpp"
#include "../support/usb_host_runtime_block_support.hpp"
#include "../support/usb_host_runtime_channel_support.hpp"

namespace {
    using examples::usb::support::CdcRuntimeHarness;
    using examples::usb::support::expect;
    using examples::usb::support::expect_error;
    using examples::usb::support::expect_ok;
    using examples::usb::support::expect_status;
    using examples::usb::support::MemoryDisk;
    using examples::usb::support::MscRuntimeHarness;
    using examples::usb::support::read_lba0;
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
    if (!expect_ok(msc.add_to(runtime), "failed to add MSC exported binding")) {
        return 1;
    }
    if (!expect_ok(cdc.add_to(runtime), "failed to add CDC exported binding")) {
        return 1;
    }
    if (!expect(block_registry.publish_state("block.usb0") == block::PublishState::published,
                "block registry should report MSC capability as published")) return 1;
    if (!expect(io_registry.publish_state("io.usb0") == io::PublishState::published,
                "io registry should report CDC capability as published")) return 1;
    if (!expect(msc.export_state() == block::ExportState::detached,
                "MSC binding should start exported but detached")) return 1;
    if (!expect(cdc.export_state() == io::ExportState::detached,
                "CDC binding should start exported but detached")) return 1;

    auto* stable_block = msc.stable();
    auto* stable_channel = cdc.stable();
    if (!expect(stable_block == &msc.exported_slot().device(), "stable block slot mismatch")) return 1;
    if (!expect(stable_channel == &cdc.exported_slot().channel(), "stable channel slot mismatch")) return 1;

    std::array<util::u8, MemoryDisk::block_size> block_buf{};
    std::array<util::u8, 4> read_buf{};
    std::array<util::u8, 4> write_buf{
        static_cast<util::u8>('P'),
        static_cast<util::u8>('I'),
        static_cast<util::u8>('N'),
        static_cast<util::u8>('G')
    };

    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "detached block slot should read as noent")) return 1;
    if (!expect_error(stable_channel->read(read_buf), io::errc::noent,
                      "detached channel slot should read as noent")) return 1;

    device::BusManager<1> bus_manager{};
    const auto runtime_bus = runtime.bus().bus();
    if (!expect(runtime_bus.ops.try_enumerate != nullptr,
                "runtime host bus should expose try_enumerate")) return 1;
    if (!expect(bus_manager.add_bus(runtime_bus),
                "failed to add runtime host bus")) return 1;

    if (!expect_ok(bus_manager.try_enumerate_all(runtime.registry()),
                   "runtime host bus enumerate failed")) return 1;
    if (!expect_ok(runtime.registry().try_match_detected(),
                   "runtime registry match_detected failed")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should contain two devices")) return 1;
    if (!expect(msc.enumerated_in(runtime) && cdc.enumerated_in(runtime),
                "runtime manager did not enumerate all records")) return 1;

    if (!expect(msc.attached(), "MSC slot was not attached")) return 1;
    if (!expect(cdc.attached(), "CDC slot was not attached")) return 1;
    if (!expect(runtime.export_state(msc.binding) == block::ExportState::attached,
                "runtime should report MSC export as attached")) return 1;
    if (!expect(runtime.export_state(cdc.binding) == io::ExportState::attached,
                "runtime should report CDC export as attached")) return 1;
    const auto cdc_generation_before = cdc.generation();

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

    if (!expect_ok(msc.try_remove_from(runtime), "failed to remove MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 1, "runtime registry should keep only CDC after MSC remove")) return 1;
    if (!expect(!msc.attached(), "MSC slot should detach after remove")) return 1;
    if (!expect(msc.export_state() == block::ExportState::detached,
                "removed MSC binding should remain exported but detached")) return 1;
    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "removed MSC slot should read as noent")) return 1;

    if (!expect_ok(msc.try_rediscover_in(runtime), "failed to rediscover MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should restore MSC after re-enumeration")) return 1;
    if (!expect(msc.attached(), "MSC slot should reattach after re-enumeration")) return 1;
    if (!expect(static_cast<bool>(read_lba0(*stable_block, block_buf)),
                "re-enumerated MSC slot should read successfully")) return 1;
    if (!expect(cdc.generation() == cdc_generation_before,
                "rediscovering MSC should not reinitialize unchanged CDC")) return 1;

    if (!expect_ok(cdc.try_remove_from(runtime), "failed to remove CDC device")) return 1;
    if (!expect_ok(msc.try_remove_from(runtime), "failed to remove MSC device the second time")) return 1;

    if (!expect(!msc.attached(), "MSC slot should be detached after remove")) return 1;
    if (!expect(!cdc.attached(), "CDC slot should be detached after remove")) return 1;
    if (!expect(msc.export_state() == block::ExportState::detached,
                "MSC should be detached before forget")) return 1;
    if (!expect(cdc.export_state() == io::ExportState::detached,
                "CDC should be detached before forget")) return 1;
    if (!expect(runtime.registry().device_count() == 0, "runtime registry should be empty after both removes")) return 1;
    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "detached block slot should return noent after remove")) return 1;
    if (!expect_error(stable_channel->read(read_buf), io::errc::noent,
                      "detached channel slot should return noent after remove")) return 1;
    if (!expect_error(stable_channel->write(write_buf), io::errc::noent,
                      "detached channel slot should reject writes after remove")) return 1;
    if (!expect_error(stable_channel->flush(), io::errc::noent,
                      "detached channel slot should reject flush after remove")) return 1;

    if (!expect_ok(cdc.try_forget_from(runtime), "failed to forget CDC binding")) return 1;
    if (!expect_ok(msc.try_forget_from(runtime), "failed to forget MSC binding")) return 1;
    if (!expect(!runtime.contains(msc.binding) && !runtime.contains(cdc.binding),
                "forgotten bindings should be removed from runtime bus")) return 1;
    if (!expect(block_registry.publish_state("block.usb0") == block::PublishState::missing,
                "forgotten MSC capability should become unpublished")) return 1;
    if (!expect(io_registry.publish_state("io.usb0") == io::PublishState::missing,
                "forgotten CDC capability should become unpublished")) return 1;
    if (!expect(msc.export_state() == block::ExportState::missing,
                "forgotten MSC export should become missing")) return 1;
    if (!expect(cdc.export_state() == io::ExportState::missing,
                "forgotten CDC export should become missing")) return 1;
    if (!expect(block_registry.open_device("block.usb0") == nullptr,
                "forgotten MSC capability should be removed from block registry")) return 1;
    if (!expect(io_registry.open_channel("io.usb0") == nullptr,
                "forgotten CDC capability should be removed from io registry")) return 1;
    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "forgotten block slot pointer should remain revoked")) return 1;
    if (!expect_error(stable_channel->read(read_buf), io::errc::noent,
                      "forgotten channel slot pointer should remain revoked")) return 1;

    std::puts("[OK] usb-host-runtime-multi-smoke passed");
    return 0;
}
