#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace examples::usb::support {
    struct MemoryDisk {
        static constexpr std::size_t block_size = 512;
        static constexpr std::size_t block_count = 4;

        std::array<util::u8, block_size * block_count> bytes{};
        block::Device device{};

        explicit MemoryDisk(std::string_view oem_name = "HOSTMSC0") noexcept {
            bytes[0] = 0xEB;
            bytes[1] = 0x3C;
            bytes[2] = 0x90;
            set_oem_name(oem_name);
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

        void set_oem_name(std::string_view oem_name) noexcept {
            constexpr std::size_t kOffset = 3;
            constexpr std::size_t kWidth = 8;
            for (std::size_t i = 0; i < kWidth; ++i) {
                bytes[kOffset + i] = i < oem_name.size()
                    ? static_cast<util::u8>(oem_name[i])
                    : static_cast<util::u8>(' ');
            }
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
            for (std::size_t i = 0; i < out.size(); ++i) {
                out[i] = self->bytes[offset + i];
            }
            return {};
        }
    };

    inline block::Status read_lba0(block::Device& dev, std::span<util::u8> out) noexcept {
        if (!dev.read) {
            return {block::Errc::nosys};
        }
        return dev.read(dev.ctx, 0, out);
    }

    template <typename BlockRegistryT>
    struct MscRuntimeHarness {
        BlockRegistryT* registry{nullptr};
        std::string_view cap_name{};
        MemoryDisk backend{};
        ::usb::host::MscBlockRuntimeBinding<BlockRegistryT> binding;

        MscRuntimeHarness(BlockRegistryT& registry,
                          std::string_view cap_name,
                          util::u16 vendor_id,
                          util::u16 product_id,
                          std::string_view oem_name = "HOSTMSC0",
                          std::string_view type = "usb.host.msc",
                          const char* driver_name = "usb.host.msc.runtime",
                          const char* bus_name = "usb.host",
                          util::u32 priority = 0) noexcept
            : registry(&registry),
              cap_name(cap_name),
              backend(oem_name),
              binding(registry,
                      cap_name,
                      backend.device,
                      vendor_id,
                      product_id,
                      type,
                      driver_name,
                      bus_name,
                      priority) {
        }

        block::Device* stable() noexcept {
            return registry ? registry->open_device(cap_name) : nullptr;
        }
    };
}
