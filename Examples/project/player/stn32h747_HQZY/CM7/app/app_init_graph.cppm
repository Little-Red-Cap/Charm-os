module;

#include <array>
#include <optional>
#include <span>

export module player.stm32h7.app_init_graph;

import charm.system.init_usb;
import init.node;
import player.stm32h7.board_usb;
import usb.class_msc_block.node;
import usb.common;
import util.core;

extern "C" {
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

export namespace player::stm32h7::app::init_graph {
    struct UsbMscInitConfig {
        bool enable{false};
        bool use_st_stack{true};
        std::span<const std::span<const usb::u8>> strings{};
        usb::u16 vendor_id{0x1209};
        usb::u16 product_id{0x0002};
        bool read_only{true};
    };

    template <typename RegistryT>
    struct UsbMscInitPlan {
        std::optional<charm::system::UsbMscBlockInitChain<RegistryT>> chain{};
        std::array<const init::Node*, 1> nodes{};

        std::span<const init::Node* const> node_span() const noexcept {
            if (!chain) return {};
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }
    };

    template <typename RegistryT>
    UsbMscInitPlan<RegistryT> build_usb_msc_plan(RegistryT& registry,
                                                 const UsbMscInitConfig& cfg) noexcept {
        UsbMscInitPlan<RegistryT> plan{};
        if (!cfg.enable || cfg.use_st_stack) {
            return plan;
        }
        plan.chain.emplace(
            registry,
            usb::device::MscBlockDesc{},
            init::Phase::app,
            static_cast<util::u32>(init::Runlevel::all));

        auto& binding = plan.chain->binding;
        auto& dcd_ops = player::stm32h7::board::usb_dcd_ops();
        binding.desc.cap_name = "usb.msc0";
        binding.desc.block_cap = "block.sd0";
        binding.desc.dcd = dcd_ops;
        binding.desc.dcd_ctx = &hpcd_USB_OTG_FS;
        binding.desc.adapter = &player::stm32h7::board::usb_adapter();
        binding.desc.dev_info.vendor_id = cfg.vendor_id;
        binding.desc.dev_info.product_id = cfg.product_id;
        binding.desc.dev_info.i_manufacturer = 1;
        binding.desc.dev_info.i_product = 2;
        binding.desc.dev_info.i_serial = 3;
        binding.desc.msc_cfg.ep_out = 0x01;
        binding.desc.msc_cfg.ep_in = 0x81;
        binding.desc.msc_cfg.ep_mps = 64;
        binding.desc.strings = cfg.strings;
        binding.desc.storage_cfg.read_only = cfg.read_only;
        binding.desc.on_ready = &player::stm32h7::board::usb_set_ready;
        binding.desc.on_ready_ctx = nullptr;
        plan.nodes = {&binding.node};
        return plan;
    }
}
