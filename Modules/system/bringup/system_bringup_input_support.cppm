module;

export module charm.system.bringup.input_support;

import charm.system.clock;
import charm.system.init_input;
import hal_input;
export import input.raw_sink;
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
                                       input::RawSinkRef sink,
                                       InputInitCfg cfg) noexcept {
        if (!desc.driver) {
            return false;
        }
        slot.emplace(hal::RawInputSource{*desc.driver},
                     clock,
                     host.input_pump(),
                     host.input_pump_ports(),
                     host.input_pump_id(),
                     sink,
                     cfg,
                     input_caps_from(desc));
        return true;
    }
}
