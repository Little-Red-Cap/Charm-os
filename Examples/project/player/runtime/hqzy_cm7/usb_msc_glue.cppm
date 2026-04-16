export module player.stm32h7.usb_msc_glue;

import player.stm32h7.usb_glue_core;
import usb.class_msc;
import usb.device_driver;
import usb.driver;

export namespace player::stm32h7::usb_msc_glue {
    struct State {
        usb::class_driver::MscBot* bot{nullptr};
        const usb::class_driver::MscConfig* cfg{nullptr};
    };

    inline void set_ready(State& state,
                          usb::class_driver::MscBot* bot,
                          const usb::class_driver::MscConfig* cfg) noexcept {
        state.bot = bot;
        state.cfg = cfg;
    }

    inline bool poll(player::stm32h7::usb_glue_core::Core& core,
                     State& state,
                     const usb::driver::DcdOps& dcd,
                     void* dcd_ctx) noexcept {
        if (!core.pcd || !state.bot || !state.cfg) return false;

        const auto& cfg = *state.cfg;
        bool progressed = false;

        if (state.bot->take_out_rearm()) {
            progressed = player::stm32h7::usb_glue_core::rearm_out(core, cfg.ep_out) || progressed;
        }
        if (state.bot->take_stall_in()) {
            return (dcd.ep.stall && dcd.ep.stall(dcd_ctx, cfg.ep_in)) || progressed;
        }
        if (state.bot->take_stall_out()) {
            return (dcd.ep.stall && dcd.ep.stall(dcd_ctx, cfg.ep_out)) || progressed;
        }
        if (player::stm32h7::usb_glue_core::endpoint_busy(core, cfg.ep_in)) {
            return progressed;
        }
        return usb::device::examples::send_msc_in_packet(
            dcd,
            dcd_ctx,
            *state.bot,
            cfg) || progressed;
    }
}
