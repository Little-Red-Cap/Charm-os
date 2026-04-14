#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

import block.device;
import block.registry;
import device.types;
import usb.host.runtime_block;
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
            std::memcpy(bytes.data() + 3, "HOSTMSC0", 8);
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

    bool expect(bool cond, const char* message) {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool expect_status(block::Status st, block::Errc want, const char* message) {
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

    block::Status read_lba0(block::Device& dev, std::span<util::u8> out) noexcept {
        if (!dev.read) {
            return {block::Errc::nosys};
        }
        return dev.read(dev.ctx, 0, out);
    }
}

int main() {
    constexpr auto kCapName = "block.usb0";

    block::Registry<4> block_registry{};
    block_registry.init();
    usb::host::RuntimeManager<4, 4> runtime{"usb.host.block"};

    MemoryDisk disk{};
    usb::host::MscBlockRuntimeBinding<block::Registry<4>> binding{
        block_registry,
        kCapName,
        disk.device,
        0x1209,
        0x0005
    };

    auto add_r = runtime.add_exported(binding);
    if (!add_r) {
        std::fprintf(stderr,
                     "[ERR] runtime manager add_exported failed err=%d\n",
                     static_cast<int>(add_r.error()));
        return 1;
    }

    auto* stable = block_registry.open_device(kCapName);
    if (!expect(stable == &binding.exported_slot().device(),
                "registry did not expose the stable block slot")) {
        return 1;
    }
    if (!expect(binding.exported(), "slot export should stay published")) return 1;
    if (!expect(!binding.attached(), "slot should start detached")) return 1;
    if (!expect(binding.generation() == 0, "slot generation should start at 0")) return 1;

    std::array<util::u8, MemoryDisk::block_size> buffer{};
    if (!expect_status(read_lba0(*stable, buffer),
                       block::Errc::noent,
                       "detached slot should read as noent")) {
        return 1;
    }

    if (!expect(runtime.scan(), "runtime manager scan failed")) {
        return 1;
    }
    if (!expect(runtime.enumerated(binding),
                "runtime manager did not enumerate the MSC device")) {
        return 1;
    }
    if (!expect(runtime.registry().device_count() == 1,
                "runtime registry should contain one MSC device")) {
        return 1;
    }
    if (!expect(binding.attached(), "runtime init did not attach the MSC slot")) return 1;
    if (!expect(binding.generation() == 1,
                "slot generation should advance after attach")) {
        return 1;
    }
    if (!expect(runtime.registry().device_at(0).state == device::DeviceState::running,
                "runtime MSC device should be running after match_all")) {
        return 1;
    }

    auto* stable_after_attach = block_registry.open_device(block::cap_id(kCapName));
    if (!expect(stable_after_attach == stable,
                "registry pointer should stay stable after attach")) {
        return 1;
    }

    auto attached_read = read_lba0(*stable, buffer);
    if (!expect(static_cast<bool>(attached_read), "attached slot should read successfully")) {
        return 1;
    }
    if (!expect(buffer[0] == 0xEB && buffer[1] == 0x3C && buffer[2] == 0x90,
                "attached slot returned unexpected block header")) {
        return 1;
    }
    if (!expect(buffer[510] == 0x55 && buffer[511] == 0xAA,
                "attached slot returned unexpected block trailer")) {
        return 1;
    }

    if (!expect(runtime.remove(binding),
                "runtime remove did not detach the MSC slot")) {
        return 1;
    }
    if (!expect(!binding.attached(), "MSC slot should be detached after remove")) return 1;
    if (!expect(binding.generation() == 2,
                "slot generation should advance after detach")) {
        return 1;
    }
    if (!expect(block_registry.open_device(kCapName) == stable,
                "registry pointer should remain stable after detach")) {
        return 1;
    }

    buffer.fill(0);
    if (!expect_status(read_lba0(*stable, buffer),
                       block::Errc::noent,
                       "detached slot should return noent after remove")) {
        return 1;
    }

    std::puts("[OK] usb-host-runtime-block-smoke passed");
    return 0;
}
