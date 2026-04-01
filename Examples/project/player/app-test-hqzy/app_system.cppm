module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

export module player.app_test_hqzy.app_system;

import charm.system.app_host;
import charm.system.caps;
import charm.system.clock;
import charm.system.init_block;
import charm.system.init_core;
import charm.system.init_usb;
import charm.system.time;
import block.device;
import block.registry;
import block.sdmmc;
import init.graph;
import init.node;
import kernel.capabilities;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import usb.device_driver;
import usb.dsl;
import util.core;
import util.error;
import player.app_test_hqzy.usb_glue;
import player.app_test_hqzy.sdmmc_glue;
import player.app_test_hqzy.board_platform;
import player.app_test_hqzy.runtime_bringup;
import player.app_test_hqzy.boot_log;
import init.node_wrap;

extern "C" void Error_Handler(void);

export namespace player::app_test_hqzy::app_system {
    int run();
}

namespace player::app_test_hqzy::app_system {
    struct System {
        sdmmc_glue::SdmmcRuntime sdmmc{};
        board_platform::Context board{};
        runtime_bringup::Context runtime{};
        player::app_test_hqzy::usb_glue::UsbGlue usb{};
        charm::system::Clock* clock{nullptr};
    };

    namespace detail {
        constexpr usb::u16 kLangs[] = { 0x0409 };
        constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
        constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
        constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm MSC");
        constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

        static const usb::StringTable<4> kUsbStrings{
            std::array<std::span<const usb::u8>, 4>{
                std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
                std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
                std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
                std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
            }
        };

    } // namespace detail

    int run() {
        System sys{};

        using PumpCaps = charm::system::SystemCaps<
            kernel::NoopIrqGuard,
            kernel::NoopWakeup>;

        PumpCaps pump_caps{};
        charm::system::AppHost<PumpCaps> host{pump_caps};

        constexpr util::usize kMaxNodes = 16;
        constexpr util::usize kMaxCaps = 32;
        constexpr util::usize kMaxEndpoints = 16;

        charm::system::CoreSystemChain<kMaxEndpoints> core{
            charm::system::ClockOps{},
            nullptr,
            host.pump(),
            host.post_fn(),
            host.post_ctx(),
            host.pump_id(),
            8
        };
        sys.clock = &core.clock;

        block::SdmmcHandle sdmmc_handle{&sys.sdmmc, &sdmmc_glue::kOps};
        block::SdmmcConfig sdmmc_cfg{};
        sdmmc_cfg.clock_hz = 0;
        sdmmc_cfg.bus_width = 4;
        sdmmc_cfg.use_dma = false;

        charm::system::SdmmcInitChain<block::Registry<kMaxEndpoints>> sdmmc_chain{
            core.block_registry,
            sdmmc_handle,
            sdmmc_cfg,
            "block.sd0"
        };

        usb::device::MscBlockDesc usb_desc{};
        usb_desc.cap_name = "usb.msc0";
        usb_desc.block_cap = "block.sd0";
        usb_desc.dcd = player::app_test_hqzy::usb_glue::dcd_ops(sys.usb);
        usb_desc.dcd_ctx = &sys.usb;
        usb_desc.adapter = &player::app_test_hqzy::usb_glue::adapter(sys.usb);
        usb_desc.dev_info.vendor_id = 0x1209;
        usb_desc.dev_info.product_id = 0x0002;
        usb_desc.dev_info.i_manufacturer = 1;
        usb_desc.dev_info.i_product = 2;
        usb_desc.dev_info.i_serial = 3;
        usb_desc.msc_cfg.ep_out = 0x01;
        usb_desc.msc_cfg.ep_in = 0x81;
        usb_desc.msc_cfg.ep_mps = 64;
        usb_desc.strings = std::span<const std::span<const usb::u8>>(
            detail::kUsbStrings.entries.data(), detail::kUsbStrings.entries.size());
        usb_desc.storage_cfg.read_only = true;
        usb_desc.on_ready = &player::app_test_hqzy::usb_glue::on_ready;
        usb_desc.on_ready_ctx = &sys.usb;

        charm::system::UsbMscBlockInitChain<block::Registry<kMaxEndpoints>> usb_chain{
            core.block_registry, usb_desc
        };
        static constexpr init::CapId kCapBoard = init::cap_id("board.ready");
        static constexpr init::CapId kCapPlatform = init::cap_id("platform.ready");
        static constexpr init::CapId kCapClock = init::cap_id("system.clock");
        static constexpr init::CapId kProvidesBoard[] = {kCapBoard};
        static constexpr init::CapId kProvidesPlatform[] = {kCapPlatform};
        static constexpr init::CapId kRequiresBoard[] = {kCapBoard};
        static constexpr init::CapId kRequiresPlatform[] = {kCapPlatform};
        static constexpr init::CapId kProvidesClock[] = {kCapClock};

        static constexpr util::usize kMaxSdmmcNodes =
            std::tuple_size_v<decltype(sdmmc_chain.nodes)>;
        static constexpr util::usize kMaxUsbNodes =
            std::tuple_size_v<decltype(usb_chain.nodes)>;
        auto sdmmc_wrapped = init::wrap_nodes_with_requires<decltype(sdmmc_chain), kMaxSdmmcNodes>(
            sdmmc_chain,
            std::span<const init::CapId>(kRequiresPlatform, 1));
        if (!sdmmc_wrapped) {
            boot_log::print_err("boot: sdmmc nodes wrap failed", sdmmc_wrapped.error());
            Error_Handler();
        }
        auto usb_wrapped = init::wrap_nodes_with_requires<decltype(usb_chain), kMaxUsbNodes>(
            usb_chain,
            std::span<const init::CapId>(kRequiresPlatform, 1));
        if (!usb_wrapped) {
            boot_log::print_err("boot: usb nodes wrap failed", usb_wrapped.error());
            Error_Handler();
        }

        const init::Node board_node{
            "board.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesBoard, 1),
            {},
            [](void* ctx) noexcept -> util::Result<void> {
                auto* sys_ctx = static_cast<System*>(ctx);
                if (!sys_ctx) return util::unexpected(util::Errc::invalid_arg);
                auto r = board_platform::init();
                if (!r) {
                    boot_log::print_err("boot: board init failed", r.error());
                    return util::unexpected(r.error());
                }
                sys_ctx->board = *r;
                return {};
            },
            nullptr,
            &sys
        };

        const init::Node runtime_node{
            "runtime.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesPlatform, 1),
            std::span<const init::CapId>(kRequiresBoard, 1),
            [](void* ctx) noexcept -> util::Result<void> {
                auto* sys_ctx = static_cast<System*>(ctx);
                if (!sys_ctx) return util::unexpected(util::Errc::invalid_arg);
                auto r = runtime_bringup::init();
                if (!r) {
                    boot_log::print_err("boot: runtime init failed", r.error());
                    return util::unexpected(r.error());
                }
                sys_ctx->runtime = *r;
                sys_ctx->sdmmc.sd = sys_ctx->runtime.sd;
                player::app_test_hqzy::usb_glue::init(sys_ctx->usb, sys_ctx->runtime.pcd);
                return {};
            },
            nullptr,
            &sys
        };

        const init::Node clock_node{
            "clock.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesClock, 1),
            std::span<const init::CapId>(kRequiresBoard, 1),
            [](void* ctx) noexcept -> util::Result<void> {
                auto* sys_ctx = static_cast<System*>(ctx);
                if (!sys_ctx || !sys_ctx->clock) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                sys_ctx->clock->reset(nullptr, charm::system::ClockOps{
                    &board_platform::now_ms,
                    nullptr
                });
                return {};
            },
            nullptr,
            &sys
        };

        init::Graph<kMaxNodes, kMaxCaps> graph{};
        std::array<const init::Node*, kMaxNodes> nodes{};
        util::usize idx = 0;
        nodes[idx++] = &board_node;
        nodes[idx++] = &runtime_node;
        nodes[idx++] = &clock_node;
        const auto core_nodes = core.node_span();
        for (util::usize i = 0; i < core_nodes.size(); ++i) {
            nodes[idx++] = core_nodes[i];
        }
        const auto sd_nodes = sdmmc_chain.node_span();
        for (util::usize i = 0; i < sd_nodes.size(); ++i) {
            nodes[idx++] = sdmmc_wrapped->ptrs[i];
        }
        const auto usb_nodes = usb_chain.node_span();
        for (util::usize i = 0; i < usb_nodes.size(); ++i) {
            nodes[idx++] = usb_wrapped->ptrs[i];
        }

        auto r = graph.build(std::span<const init::Node* const>(nodes.data(), idx),
                             static_cast<util::u32>(init::Runlevel::all),
                             init::Phase::app);
        if (!r) {
            boot_log::print_err("boot: graph build failed", r.error());
            Error_Handler();
        }
        auto r_start = graph.start();
        if (!r_start) {
            boot_log::print_err("boot: graph start failed", r_start.error());
            Error_Handler();
        }

        boot_log::print("boot: usb msc ready\n");

        while (true) {
            (void)host.run_once();
            player::app_test_hqzy::usb_glue::poll_msc(sys.usb);
        }
    }
}
