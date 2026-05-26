module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module player.profile.hqzy_cm7_usb_storage.system;

import block.device;
import block.registry;
import charm.system.init_block;
import charm.system.init_usb;
import init.graph;
import init.materialize;
import init.plan;
import player.stm32h7.board_sdmmc;
import player.stm32h7.board_usb;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import util.core;

extern "C" void Error_Handler(void);
export namespace player::app_test_hqzy::usb_storage_system {
    int run();
}

namespace player::app_test_hqzy::usb_storage_system {
    namespace defaults {
        constexpr usb::u16 kLangs[] = {0x0409};
        constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
        constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
        constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm USB Storage");
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

    int run() {
        constexpr util::usize kMaxBlockDevices = 4;
        constexpr util::usize kMaxNodes = 8;
        constexpr util::usize kMaxCaps = 16;

        block::Registry<kMaxBlockDevices> registry{};
        registry.init();

        auto sdmmc_handle = player::stm32h7::board::sdmmc_handle();
        auto sdmmc_cfg = player::stm32h7::board::sdmmc_config();
        charm::system::SdmmcInitChain<block::Registry<kMaxBlockDevices>> sdmmc_chain{
            registry,
            sdmmc_handle,
            sdmmc_cfg,
            "block.sd0"
        };

        player::stm32h7::board::usb_init_early(false);
        player::stm32h7::board::usb_enable_hooks(true);

        usb::device::MscBlockDesc usb_desc{};
        usb_desc.cap_name = "usb.msc0";
        usb_desc.block_cap = "block.sd0";
        usb_desc.dcd = player::stm32h7::board::usb_dcd_ops();
        usb_desc.dcd_ctx = player::stm32h7::board::usb_pcd_handle();
        usb_desc.adapter = &player::stm32h7::board::usb_adapter();
        usb_desc.dev_info.vendor_id = 0x1209;
        usb_desc.dev_info.product_id = 0x0002;
        usb_desc.dev_info.i_manufacturer = 1;
        usb_desc.dev_info.i_product = 2;
        usb_desc.dev_info.i_serial = 3;
        usb_desc.msc_cfg.ep_out = 0x01;
        usb_desc.msc_cfg.ep_in = 0x81;
        usb_desc.msc_cfg.ep_mps = 64;
        usb_desc.strings = std::span<const std::span<const usb::u8>>(
            defaults::kUsbStrings.entries.data(),
            defaults::kUsbStrings.entries.size());
        usb_desc.storage_cfg.read_only = true;
        usb_desc.on_ready = &player::stm32h7::board::usb_set_ready;

        charm::system::UsbMscBlockInitChain<block::Registry<kMaxBlockDevices>> usb_chain{
            registry,
            usb_desc
        };

        const auto plan = init::compose(
            sdmmc_chain.plan(),
            usb_chain.plan());

        init::Graph<kMaxNodes, kMaxCaps> graph{};
        auto build = init::build_graph(graph, plan);
        if (!build) {
            Error_Handler();
        }

        auto started = graph.start();
        if (!started) {
            Error_Handler();
        }

        while (true) {
            player::stm32h7::board::usb_poll_msc(player::stm32h7::board::usb_pcd_handle());
        }
    }
}
