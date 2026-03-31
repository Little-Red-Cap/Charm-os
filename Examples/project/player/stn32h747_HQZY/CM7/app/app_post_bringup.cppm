module;

#include <array>
#include <span>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_pcd.h"

export module player.stm32h7.app_post_bringup;

import init.graph;
import init.node;
import player.stm32h7.board_usb;
import player.stm32h7.display_st7305;
import player.stm32h7.fs_demo;
import player.stm32h7.usb_system;
import util.core;
import util.error;

extern "C" {
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

export namespace player::stm32h7::app::post_bringup {
    struct Config {
        bool enable_usb_msc{true};
        bool use_st_usb_stack{true};
        bool enable_display{false};
    };

    using PrintFn = void (*)(const char*) noexcept;

    struct Context {
        Config cfg{};
        PrintFn print{nullptr};
        bool display_ready{false};
    };

    namespace detail {
        static constexpr init::CapId kCapUsbRun = init::cap_id("svc.usb");
        static constexpr init::CapId kCapDisplay = init::cap_id("svc.display");

        util::Result<void> init_usb_run(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            if (!c->cfg.enable_usb_msc) return {};
            player::stm32h7::board::usb_enable_hooks(!c->cfg.use_st_usb_stack);
            if (c->cfg.use_st_usb_stack) {
                auto* usb_dev = fs_sd_block_device();
                if (!usb_dev && c->print) {
                    c->print("boot: usb block device not ready\n");
                }
                usb_system_init(usb_dev, false);
                if (c->print) c->print("boot: usb device init ok\n");
                if (HAL_PCD_Start(&hpcd_USB_OTG_FS) == HAL_OK) {
                    if (c->print) c->print("boot: usb pcd start ok\n");
                } else {
                    if (c->print) c->print("boot: usb pcd start failed\n");
                }
            } else {
                auto& dcd_ops = player::stm32h7::board::usb_dcd_ops();
                (void)dcd_ops.connect(&hpcd_USB_OTG_FS, true);
                if (c->print) c->print("boot: usb pcd start ok\n");
            }
            return {};
        }

        util::Result<void> init_display(void* ctx) noexcept {
            auto* c = static_cast<Context*>(ctx);
            if (!c) return util::unexpected(util::Errc::invalid_arg);
            if (!c->cfg.enable_display) return {};
            if (c->print) c->print("boot: display init begin\n");
            if (display_st7305_init()) {
                c->display_ready = true;
                if (c->print) c->print("boot: display init ok\n");
            } else {
                if (c->print) c->print("boot: display init failed\n");
            }
            return {};
        }
    } // namespace detail

    util::Result<void> run(Context& ctx) noexcept {
        static constexpr init::CapId kProvidesUsb[] = {detail::kCapUsbRun};
        static constexpr init::CapId kProvidesDisplay[] = {detail::kCapDisplay};

        const init::Node usb_node{
            "usb.run",
            init::Phase::app,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesUsb, 1),
            {},
            &detail::init_usb_run,
            nullptr,
            &ctx
        };
        const init::Node display_node{
            "display.init",
            init::Phase::app,
            static_cast<util::u32>(init::Runlevel::all),
            std::span<const init::CapId>(kProvidesDisplay, 1),
            {},
            &detail::init_display,
            nullptr,
            &ctx
        };
        const init::Node* nodes[] = {&usb_node, &display_node};
        init::Graph<4, 8> graph{};
        auto r = graph.build(std::span<const init::Node* const>(nodes, 2),
                             static_cast<util::u32>(init::Runlevel::all),
                             init::Phase::app);
        if (!r) return r;
        return graph.start();
    }
}
