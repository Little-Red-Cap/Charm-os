module;

export module charm.system.bringup.input_support;

import charm.system.clock;
import charm.system.init_input;
import hal_input;
import input.pump;
import input.raw_event;
import platform.board;

export namespace charm::system::detail {
    template <typename Sink>
    bool input_sink_trampoline(void* ctx, const input::RawInputEvent& ev) noexcept {
        auto* sink = static_cast<Sink*>(ctx);
        if (!sink) {
            return true;
        }
        if constexpr (requires(Sink& value, const input::RawInputEvent& event) {
                          value.on_raw(event);
                      }) {
            return sink->on_raw(ev);
        } else {
            return (*sink)(ev);
        }
    }

    inline InputInitCaps input_caps_from(const platform::board::InputDesc& desc) noexcept {
        return InputInitCaps{
            desc.service_cap,
            desc.router_cap,
            desc.pump_cap,
            "system.clock",
            "kernel.eda"
        };
    }
}

export namespace charm::system {
    class InputSinkRef {
    public:
        constexpr InputSinkRef() noexcept = default;

        static constexpr InputSinkRef raw(input::SinkFn fn, void* ctx) noexcept {
            return InputSinkRef{fn, ctx};
        }

        template <typename Sink>
        static constexpr InputSinkRef bind(Sink& sink) noexcept {
            return InputSinkRef{&detail::input_sink_trampoline<Sink>, &sink};
        }

        [[nodiscard]] constexpr input::SinkFn fn() const noexcept {
            return fn_;
        }

        [[nodiscard]] constexpr void* ctx() const noexcept {
            return ctx_;
        }

    private:
        constexpr InputSinkRef(input::SinkFn fn, void* ctx) noexcept
            : fn_(fn),
              ctx_(ctx) {}

        input::SinkFn fn_{nullptr};
        void* ctx_{nullptr};
    };
}

export namespace charm::system::detail {
    template <typename InputChainSlot, typename Host>
    bool emplace_input_chain_from_host(InputChainSlot& slot,
                                       const platform::board::InputDesc& desc,
                                       charm::system::Clock& clock,
                                       Host& host,
                                       InputSinkRef sink,
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
                     sink.fn(),
                     sink.ctx(),
                     cfg,
                     input_caps_from(desc));
        return true;
    }
}
