#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

import block.device;
import block.registry;
import io.channel;
import io.registry;
import usb.host.runtime_block;
import usb.host.runtime_channel;
import usb.host.runtime_manager;
import util.core;

namespace {
    struct MemoryDisk {
        static constexpr std::size_t block_size = 512;
        static constexpr std::size_t block_count = 4;

        std::array<util::u8, block_size * block_count> bytes{};
        block::Device device{};

        MemoryDisk() noexcept {
            bytes[0] = 0xEB;
            bytes[1] = 0x3C;
            bytes[2] = 0x90;
            std::memcpy(bytes.data() + 3, "MULTIUSB", 8);
            bytes[510] = 0x55;
            bytes[511] = 0xAA;

            device.ctx = this;
            device.read = &MemoryDisk::read_cb;
            device.write = nullptr;
            device.erase = nullptr;
            device.flush = nullptr;
            device.block_size = block_size;
            device.block_count = block_count;
            device.caps = block::to_bits(block::Caps::read);
        }

        static block::Status read_cb(void* ctx,
                                     util::u64 lba,
                                     std::span<util::u8> out) noexcept {
            auto* self = static_cast<MemoryDisk*>(ctx);
            if (!self || out.empty() || (out.size() % block_size) != 0) {
                return {block::Errc::invalid_arg};
            }
            const auto blocks = static_cast<util::u64>(out.size() / block_size);
            if (lba + blocks > block_count) {
                return {block::Errc::invalid_arg};
            }
            const auto offset = static_cast<std::size_t>(lba) * block_size;
            std::memcpy(out.data(), self->bytes.data() + offset, out.size());
            return {};
        }
    };

    struct DummyChannel {
        std::array<util::u8, 8> rx_data{
            static_cast<util::u8>('O'),
            static_cast<util::u8>('K')
        };
        util::usize rx_size{2};
        util::usize rx_pos{0};
        std::array<util::u8, 16> tx_data{};
        util::usize tx_size{0};
        bool flushed{false};
        io::Channel channel{};

        DummyChannel() noexcept
            : channel{
                  this,
                  io::ChannelOps{
                      &DummyChannel::read_cb,
                      &DummyChannel::write_cb,
                      &DummyChannel::flush_cb
                  }
              } {
        }

        static io::result read_cb(void* ctx, io::MutByteView out) noexcept {
            auto* self = static_cast<DummyChannel*>(ctx);
            if (!self || out.empty()) {
                return io::fail(io::errc::invalid_arg);
            }
            if (self->rx_pos >= self->rx_size) {
                return io::fail(io::errc::end_of_stream);
            }

            const auto available = self->rx_size - self->rx_pos;
            const auto count = available < out.size() ? available : out.size();
            for (util::usize i = 0; i < count; ++i) {
                out[i] = self->rx_data[self->rx_pos + i];
            }
            self->rx_pos += count;
            return io::ok(count);
        }

        static io::result write_cb(void* ctx, io::ByteView in) noexcept {
            auto* self = static_cast<DummyChannel*>(ctx);
            if (!self || in.empty()) {
                return io::fail(io::errc::invalid_arg);
            }
            if (in.size() > self->tx_data.size()) {
                return io::fail(io::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < in.size(); ++i) {
                self->tx_data[i] = in[i];
            }
            self->tx_size = in.size();
            return io::ok(in.size());
        }

        static io::result flush_cb(void* ctx) noexcept {
            auto* self = static_cast<DummyChannel*>(ctx);
            if (!self) {
                return io::fail(io::errc::invalid_arg);
            }
            self->flushed = true;
            return io::ok(1);
        }
    };

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

    block::Status read_lba0(block::Device& dev, std::span<util::u8> out) noexcept {
        if (!dev.read) {
            return {block::Errc::nosys};
        }
        return dev.read(dev.ctx, 0, out);
    }
}

int main() {
    block::Registry<4> block_registry{};
    io::Registry<4> io_registry{};
    block_registry.init();
    io_registry.init();

    MemoryDisk disk{};
    DummyChannel channel{};

    usb::host::MscBlockRuntimeBinding<block::Registry<4>> msc{
        block_registry,
        "block.usb0",
        disk.device,
        0x1209,
        0x0010
    };

    usb::host::CdcChannelRuntimeBinding<io::Registry<4>> cdc{
        io_registry,
        "io.usb0",
        channel.channel,
        0x1209,
        0x0011
    };

    usb::host::RuntimeManager<8, 8> runtime{"usb.host.multi"};
    auto add_msc = runtime.add_exported(msc);
    if (!add_msc) {
        std::fprintf(stderr,
                     "[ERR] failed to add MSC exported binding err=%d\n",
                     static_cast<int>(add_msc.error()));
        return 1;
    }
    auto add_cdc = runtime.add_exported(cdc);
    if (!add_cdc) {
        std::fprintf(stderr,
                     "[ERR] failed to add CDC exported binding err=%d\n",
                     static_cast<int>(add_cdc.error()));
        return 1;
    }

    auto* stable_block = block_registry.open_device("block.usb0");
    auto* stable_channel = io_registry.open_channel("io.usb0");
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

    if (!expect_block_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                             "detached block slot should read as noent")) return 1;
    if (!expect_io_error(stable_channel->read(read_buf), io::errc::noent,
                         "detached channel slot should read as noent")) return 1;

    if (!expect(runtime.scan(), "runtime manager scan failed")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should contain two devices")) return 1;
    if (!expect(runtime.enumerated(msc) && runtime.enumerated(cdc),
                "runtime manager did not enumerate all records")) return 1;

    if (!expect(msc.attached(), "MSC slot was not attached")) return 1;
    if (!expect(cdc.attached(), "CDC slot was not attached")) return 1;
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
    if (!expect(channel.tx_size == write_buf.size() && channel.tx_data[0] == static_cast<util::u8>('P'),
                "backend channel did not observe the expected write payload")) return 1;
    if (!expect(static_cast<bool>(stable_channel->flush()) && channel.flushed,
                "attached channel slot should flush successfully")) return 1;

    if (!expect(runtime.remove(msc), "failed to remove MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 1, "runtime registry should keep only CDC after MSC remove")) return 1;
    if (!expect(!msc.attached(), "MSC slot should detach after remove")) return 1;
    if (!expect_block_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                             "removed MSC slot should read as noent")) return 1;

    if (!expect(runtime.rediscover(msc), "failed to rediscover MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should restore MSC after re-enumeration")) return 1;
    if (!expect(msc.attached(), "MSC slot should reattach after re-enumeration")) return 1;
    if (!expect(static_cast<bool>(read_lba0(*stable_block, block_buf)),
                "re-enumerated MSC slot should read successfully")) return 1;
    if (!expect(cdc.generation() == cdc_generation_before,
                "rediscovering MSC should not reinitialize unchanged CDC")) return 1;

    if (!expect(runtime.remove(cdc), "failed to remove CDC device")) return 1;
    if (!expect(runtime.remove(msc), "failed to remove MSC device the second time")) return 1;

    if (!expect(!msc.attached(), "MSC slot should be detached after remove")) return 1;
    if (!expect(!cdc.attached(), "CDC slot should be detached after remove")) return 1;
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
