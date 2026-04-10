module;

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_pcd.h"

export module player.stm32h7.app_post_bringup;

import init.graph;
import init.materialize;
import init.meta;
import init.node;
import init.plan;
import init.recipe;
import player.stm32h7.board_usb;
import player.stm32h7.display_st7305;
import player.stm32h7.fs_demo;
import player.runtime.hqzy_cm7.usb_storage_bridge;
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
        using UsbRunCap = init::cap_c<"svc.usb">;
        using DisplayCap = init::cap_c<"svc.display">;

        util::Result<void> init_usb_run(Context& ctx) noexcept {
            if (!ctx.cfg.enable_usb_msc) return {};
            player::stm32h7::board::usb_enable_hooks(!ctx.cfg.use_st_usb_stack);
            if (ctx.cfg.use_st_usb_stack) {
                auto* usb_dev = fs_sd_block_device();
                if (!usb_dev && ctx.print) {
                    ctx.print("boot: usb block device not ready\n");
                }
                usb_system_init(usb_dev, false);
                if (ctx.print) ctx.print("boot: usb device init ok\n");
                if (HAL_PCD_Start(&hpcd_USB_OTG_FS) == HAL_OK) {
                    if (ctx.print) ctx.print("boot: usb pcd start ok\n");
                } else {
                    if (ctx.print) ctx.print("boot: usb pcd start failed\n");
                }
            } else {
                auto& dcd_ops = player::stm32h7::board::usb_dcd_ops();
                (void)dcd_ops.connect(&hpcd_USB_OTG_FS, true);
                if (ctx.print) ctx.print("boot: usb pcd start ok\n");
            }
            return {};
        }

        util::Result<void> init_display(Context& ctx) noexcept {
            if (!ctx.cfg.enable_display) return {};
            if (ctx.print) ctx.print("boot: display init begin\n");
            if (display_st7305_init()) {
                ctx.display_ready = true;
                if (ctx.print) ctx.print("boot: display init ok\n");
            } else {
                if (ctx.print) ctx.print("boot: display init failed\n");
            }
            return {};
        }

        using UsbRunRecipe = init::recipe_desc<
            "usb.run",
            init::Phase::app,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<UsbRunCap>,
            init::cap_list<>,
            Context,
            &init_usb_run>;

        using DisplayRecipe = init::recipe_desc<
            "display.init",
            init::Phase::app,
            static_cast<util::u32>(init::Runlevel::all),
            init::cap_list<DisplayCap>,
            init::cap_list<>,
            Context,
            &init_display>;
    } // namespace detail

    util::Result<void> run(Context& ctx) noexcept {
        const auto bringup_plan = init::compose(
            init::bind<detail::UsbRunRecipe>(ctx),
            init::bind<detail::DisplayRecipe>(ctx));
        auto materialized = init::materialize<4, 8>(bringup_plan);
        if (!materialized) {
            return util::unexpected(materialized.error());
        }
        init::Graph<4, 8> graph{};
        auto r = graph.build(materialized->node_ptr_span(),
                             materialized->build_runlevel_mask(),
                             materialized->build_max_phase());
        if (!r) return r;
        return graph.start();
    }
}
