#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

import block.device;
import block.device.slot_export;
import block.registry;
import device.bus;
import device.desc;
import device.manager;
import device.registry;
import device.runtime_driver;
import device.types;
import util.core;
import util.error;

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
            std::memcpy(bytes.data() + 3, "CHARMUSB", 8);
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

        static block::Status read_cb(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
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

    struct RuntimeContext {
        block::DeviceSlotExport<block::Registry<4>>* exported{nullptr};
        MemoryDisk* disk{nullptr};
    };

    using RuntimeBinding = device::RuntimeDriverBinding<RuntimeContext>;

    struct RuntimeBusContext {
        RuntimeBinding* binding{nullptr};
        device::DeviceDesc desc{};
        bool enumerated{false};
    };

    bool expect(bool cond, const char* message) {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool expect_ok(const util::Result<void>& result, const char* message) {
        if (!result) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d\n",
                         message,
                         static_cast<int>(result.error()));
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

    util::Result<void> runtime_try_enumerate(void* ctx, device::RegistryBase& reg) noexcept {
        auto* self = static_cast<RuntimeBusContext*>(ctx);
        if (!self || !self->binding) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (self->enumerated) {
            return {};
        }
        auto added = reg.try_add_device(self->desc, self->binding);
        if (added) {
            self->enumerated = true;
        }
        return added;
    }

    bool runtime_enumerate(void* ctx, device::RegistryBase& reg) noexcept {
        return static_cast<bool>(runtime_try_enumerate(ctx, reg));
    }

    util::Result<void> runtime_try_probe(RuntimeContext& ctx, device::Device& dev) noexcept {
        if (ctx.exported != nullptr &&
            ctx.disk != nullptr &&
            dev.desc.class_id == 0x08 &&
            dev.desc.vendor_id == 0x1234 &&
            dev.desc.product_id == 0x5678 &&
            dev.desc.type.compare("usb.msc") == 0) {
            return {};
        }
        return util::unexpected(util::Errc::bad_state);
    }

    bool runtime_probe(RuntimeContext& ctx, device::Device& dev) noexcept {
        return static_cast<bool>(runtime_try_probe(ctx, dev));
    }

    util::Result<void> runtime_try_init(RuntimeContext& ctx, device::Device&) noexcept {
        if (!ctx.exported || !ctx.disk) {
            return util::unexpected(util::Errc::bad_state);
        }
        return ctx.exported->attach(ctx.disk->device);
    }

    bool runtime_init(RuntimeContext& ctx, device::Device& dev) noexcept {
        return static_cast<bool>(runtime_try_init(ctx, dev));
    }

    void runtime_remove(RuntimeContext& ctx, device::Device&) noexcept {
        if (!ctx.exported) {
            return;
        }
        ctx.exported->detach();
    }
}

int main() {
    constexpr auto kCapName = "block.usb0";

    block::Registry<4> block_registry{};
    block_registry.init();

    block::DeviceSlotExport<block::Registry<4>> exported{block_registry, kCapName};
    MemoryDisk disk{};

    auto register_r = exported.ensure_exported();
    if (!register_r) {
        std::fprintf(stderr, "[ERR] block registry register failed err=%d\n", static_cast<int>(register_r.error()));
        return 1;
    }

    auto* stable = block_registry.open_device(kCapName);
    if (!expect(stable == &exported.device(), "registry did not expose the stable slot")) return 1;
    if (!expect(exported.exported(), "device export should stay published")) return 1;
    if (!expect(!exported.attached(), "slot should start detached")) return 1;
    if (!expect(exported.generation() == 0, "slot generation should start at 0")) return 1;

    std::array<util::u8, MemoryDisk::block_size> buffer{};
    if (!expect_status(read_lba0(*stable, buffer), block::Errc::noent, "detached slot should read as noent")) {
        return 1;
    }

    RuntimeContext runtime_ctx{&exported, &disk};
    RuntimeBinding binding{
        &runtime_ctx,
        device::RuntimeDriverHook<RuntimeContext>{
            .probe = runtime_probe,
            .init = runtime_init,
            .remove = runtime_remove,
            .try_probe = runtime_try_probe,
            .try_init = runtime_try_init
        }
    };
    RuntimeBusContext bus_ctx{
        .binding = &binding,
        .desc = device::DeviceDesc{
            .class_id = 0x08,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.msc"
        }
    };

    auto driver = device::make_runtime_driver<RuntimeContext>(
        bus_ctx.desc,
        "demo.runtime.block.slot");

    device::Bus bus{
        .name = "demo.usb.host",
        .ctx = &bus_ctx,
        .ops = device::BusOps{
            .enumerate = runtime_enumerate,
            .try_enumerate = runtime_try_enumerate
        }
    };

    device::Registry<4, 4> registry{};
    device::BusManager<1> bus_manager{};

    if (!expect_ok(registry.try_add_driver(driver), "failed to add runtime driver")) return 1;
    if (!expect_ok(bus_manager.try_add_bus(bus), "failed to add runtime bus")) return 1;

    if (!expect_ok(bus_manager.try_enumerate_all(registry), "runtime bus enumerate failed")) return 1;
    if (!expect(bus_ctx.enumerated, "runtime bus did not enumerate the device")) return 1;
    if (!expect(registry.device_count() == 1, "runtime registry should contain one device")) return 1;

    if (!expect_ok(registry.try_match_all(), "runtime registry match failed")) return 1;
    if (!expect(exported.attached(), "runtime init did not attach the block slot")) return 1;
    if (!expect(exported.generation() == 1, "slot generation should advance after attach")) return 1;
    if (!expect(registry.device_at(0).state == device::DeviceState::running,
                "runtime device should be running after match_all")) {
        return 1;
    }

    auto* stable_after_attach = block_registry.open_device(block::cap_id(kCapName));
    if (!expect(stable_after_attach == stable, "registry pointer should stay stable after attach")) return 1;

    auto attached_read = read_lba0(*stable, buffer);
    if (!expect(static_cast<bool>(attached_read), "attached slot should read successfully")) return 1;
    if (!expect(buffer[0] == 0xEB && buffer[1] == 0x3C && buffer[2] == 0x90,
                "attached slot returned unexpected block header")) {
        return 1;
    }
    if (!expect(buffer[510] == 0x55 && buffer[511] == 0xAA,
                "attached slot returned unexpected block trailer")) {
        return 1;
    }

    if (!expect_ok(registry.try_dispatch(registry.device_at(0), device::DeviceEvent::remove),
                   "runtime remove did not complete")) {
        return 1;
    }
    if (!expect(!exported.attached(), "runtime remove did not detach the block slot")) return 1;
    if (!expect(exported.generation() == 2, "slot generation should advance after detach")) return 1;
    if (!expect(block_registry.open_device(kCapName) == stable,
                "registry pointer should remain stable after detach")) {
        return 1;
    }

    buffer.fill(0);
    if (!expect_status(read_lba0(*stable, buffer), block::Errc::noent, "detached slot should return noent after remove")) {
        return 1;
    }

    std::puts("[OK] runtime block slot demo passed");
    return 0;
}
