module;

export module charm.system.bringup.input_support;

import charm.system.clock;
import charm.system.init_input;
import hal_input;
import input.pump;
import platform.board;

export namespace charm::system::detail {
    inline InputInitCaps input_caps_from(const platform::board::InputDesc& desc) noexcept {
        return InputInitCaps{
            desc.service_cap,
            desc.router_cap,
            desc.pump_cap,
            "system.clock",
            "kernel.eda"
        };
    }

    template <typename InputChainSlot, typename Host>
    bool emplace_input_chain_from_host(InputChainSlot& slot,
                                       const platform::board::InputDesc& desc,
                                       charm::system::Clock& clock,
                                       Host& host,
                                       input::SinkFn sink,
                                       void* sink_ctx,
                                       InputInitCfg cfg) noexcept {
        if (!desc.driver) {
            return false;
        }
        slot.emplace(hal::RawInputSource{*desc.driver},
                     clock,
                     host.input_pump(),
                     host.schedule_fn(),
                     host.schedule_ctx(),
                     host.post_demand_fn(),
                     host.post_ctx(),
                     host.input_pump_id(),
                     sink,
                     sink_ctx,
                     cfg,
                     input_caps_from(desc));
        return true;
    }
}
