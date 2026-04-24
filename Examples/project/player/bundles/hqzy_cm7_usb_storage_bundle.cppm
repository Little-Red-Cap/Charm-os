module;

#include <array>
#include <optional>
#include <span>

export module player.bundle.hqzy_cm7_usb_storage;

import block.registry;
import charm.system.init_usb;
import player.runtime.hqzy_cm7.usb_glue;
import usb.class_msc_block.node;
import usb.common;
import util.core;

export namespace player::bundle::hqzy_cm7_usb_storage {
    struct Config {
        const char* cap_name{"usb.msc0"};
        const char* block_cap{"block.sd0"};
        usb::u16 vendor_id{0x1209};
        usb::u16 product_id{0x0002};
        usb::u8 i_manufacturer{1};
        usb::u8 i_product{2};
        usb::u8 i_serial{3};
        usb::u8 ep_out{0x01};
        usb::u8 ep_in{0x81};
        usb::u16 ep_mps{64};
        bool read_only{true};
        std::span<const std::span<const usb::u8>> strings{};
    };

    namespace defaults {
        constexpr usb::u16 kLangs[] = {0x0409};
        constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
        constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
        constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm MSC");
        constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

        inline const usb::StringTable<4> kUsbStrings{
            std::array<std::span<const usb::u8>, 4>{
                std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
                std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
                std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
                std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
            }
        };
    }

    inline Config make_default_config() noexcept {
        Config cfg{};
        cfg.strings = std::span<const std::span<const usb::u8>>(
            defaults::kUsbStrings.entries.data(), defaults::kUsbStrings.entries.size());
        return cfg;
    }

    template <typename RegistryT>
    inline std::optional<charm::system::UsbMscBlockInitChain<RegistryT>> build(
        RegistryT& registry,
        player::app_test_hqzy::usb_glue::UsbGlue& glue,
        const Config& cfg) noexcept {
        std::optional<charm::system::UsbMscBlockInitChain<RegistryT>> plan{};
        plan.emplace(registry, usb::device::MscBlockDesc{});

        auto& binding = plan->binding;
        binding.desc.cap_name = cfg.cap_name;
        binding.desc.block_cap = cfg.block_cap;
        binding.desc.dcd = player::app_test_hqzy::usb_glue::dcd_ops(glue);
        binding.desc.dcd_ctx = &glue;
        binding.desc.adapter = &player::app_test_hqzy::usb_glue::adapter(glue);
        binding.desc.dev_info.vendor_id = cfg.vendor_id;
        binding.desc.dev_info.product_id = cfg.product_id;
        binding.desc.dev_info.i_manufacturer = cfg.i_manufacturer;
        binding.desc.dev_info.i_product = cfg.i_product;
        binding.desc.dev_info.i_serial = cfg.i_serial;
        binding.desc.msc_cfg.ep_out = cfg.ep_out;
        binding.desc.msc_cfg.ep_in = cfg.ep_in;
        binding.desc.msc_cfg.ep_mps = cfg.ep_mps;
        binding.desc.strings = cfg.strings;
        binding.desc.storage_cfg.read_only = cfg.read_only;
        binding.desc.on_ready = &player::app_test_hqzy::usb_glue::on_ready;
        binding.desc.on_ready_ctx = &glue;
        return plan;
    }
}
