module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

export module player.profile.hqzy_cm7_usb_self_msc.system;

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
import init.materialize;
import init.meta;
import init.node;
import init.plan;
import init.recipe;
import kernel.capabilities;
import player.bundle.hqzy_cm7_usb_storage;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import usb.device_driver;
import usb.dsl;
import util.core;
import util.error;
import player.runtime.hqzy_cm7.usb_glue;
import player.runtime.hqzy_cm7.sdmmc_glue;
import player.runtime.hqzy_cm7.board_platform;
import player.runtime.hqzy_cm7.runtime_bringup;
import player.runtime.hqzy_cm7.boot_log;
import player.runtime.hqzy_cm7.foundation;

extern "C" void Error_Handler(void);

export namespace player::app_test_hqzy::app_system {
    int run();
}

namespace player::app_test_hqzy::app_system {
    struct System {
        sdmmc_glue::SdmmcRuntime sdmmc{};
        player::foundation::Runtime foundation{};
        runtime_bringup::Context runtime{};
        player::app_test_hqzy::usb_glue::UsbGlue usb{};
        charm::system::Clock* clock{nullptr};
    };

    using BoardReady = init::cap_c<"board.ready">;
    using PlatformReady = init::cap_c<"platform.ready">;

    util::Result<void> board_start(System& sys) noexcept {
        player::foundation::print(sys.foundation, "foundation: board node reuse\n");
        if (sys.clock) {
            sys.clock->reset(nullptr, charm::system::ClockOps{
                &board_platform::now_ms,
                nullptr
            });
        }
        return {};
    }

    util::Result<void> runtime_start(System& sys) noexcept {
        auto r = runtime_bringup::init();
        if (!r) {
            boot_log::print_err("boot: runtime init failed", r.error());
            return util::unexpected(r.error());
        }
        sys.runtime = *r;
        sys.sdmmc.sd = sys.runtime.sd;
        player::app_test_hqzy::usb_glue::init(sys.usb, sys.runtime.pcd);
        return {};
    }

    using BoardRecipe = init::recipe_desc<
        "board.init",
        init::Phase::early,
        static_cast<util::u32>(init::Runlevel::all),
        init::cap_list<BoardReady>,
        init::cap_list<>,
        System,
        &board_start>;

    using RuntimeRecipe = init::recipe_desc<
        "runtime.init",
        init::Phase::early,
        static_cast<util::u32>(init::Runlevel::all),
        init::cap_list<PlatformReady>,
        init::cap_list<BoardReady>,
        System,
        &runtime_start>;

    int run() {
        System sys{};
        auto foundation = player::foundation::init({
            "player",
            "stm32h747-hal",
            "hqzy_cm7",
            "usb_self_msc"
        });
        if (!foundation) {
            Error_Handler();
        }
        sys.foundation = *foundation;
        player::foundation::print(sys.foundation, "msc: enter run\n");

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
            host.post_io_ready_fn(),
            host.post_ctx(),
            host.pump_id(),
            8
        };
        player::foundation::print(sys.foundation, "msc: core chain ok\n");
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

        auto usb_cfg = player::bundle::hqzy_cm7_usb_storage::make_default_config();

        auto usb_plan = player::bundle::hqzy_cm7_usb_storage::build(core.block_registry, sys.usb, usb_cfg);
        const auto system_plan = init::compose(
            init::bind<BoardRecipe>(sys),
            init::bind<RuntimeRecipe>(sys),
            core.plan(),
            sdmmc_chain.plan().after<PlatformReady>(),
            init::maybe(usb_plan).after<PlatformReady>());
        player::foundation::print(sys.foundation, "msc: materialize plan\n");
        init::Graph<kMaxNodes, kMaxCaps> graph{};
        auto r = init::build_graph(graph, system_plan);
        player::foundation::print(sys.foundation, "msc: graph build returned\n");
        if (!r) {
            boot_log::print_err("boot: graph build failed", r.error());
            Error_Handler();
        }
        auto r_start = graph.start();
        player::foundation::print(sys.foundation, "msc: graph start returned\n");
        if (!r_start) {
            boot_log::print_err("boot: graph start failed", r_start.error());
            Error_Handler();
        }

        player::foundation::print(sys.foundation, "boot: usb msc ready\n");

        while (true) {
            (void)host.run_once();
            player::app_test_hqzy::usb_glue::poll_msc(sys.usb);
            if (sys.usb.msc.bot && charm::system::time::bound()) {
                static util::u64 last_trace_ms = 0;
                const auto now = charm::system::time::now_ms();
                if ((now - last_trace_ms) >= 1000u) {
                    last_trace_ms = now;
                    const auto& bot = *sys.usb.msc.bot;
                    boot_log::printf(
                        "msc: phase=%u scsi=0x%02X st=%u sense=%u/%u/%u csw=%u resid=%lu in=%u wait=%u clr=%lu clr_in=%u rearm=%u\n",
                        static_cast<unsigned>(bot.phase_code()),
                        static_cast<unsigned>(bot.last_scsi_cmd()),
                        static_cast<unsigned>(bot.last_scsi_status()),
                        static_cast<unsigned>(bot.last_sense_key()),
                        static_cast<unsigned>(bot.last_sense_asc()),
                        static_cast<unsigned>(bot.last_sense_ascq()),
                        static_cast<unsigned>(bot.last_csw_status()),
                        static_cast<unsigned long>(bot.last_csw_residue()),
                        static_cast<unsigned>(bot.last_in_result()),
                        static_cast<unsigned>(bot.stall_wait_csw() ? 1u : 0u),
                        static_cast<unsigned long>(bot.clear_stall_count()),
                        static_cast<unsigned>(bot.last_clear_stall_in_ep() ? 1u : 0u),
                        static_cast<unsigned>(bot.out_rearm_pending() ? 1u : 0u));
                }
            }
        }
    }
}
