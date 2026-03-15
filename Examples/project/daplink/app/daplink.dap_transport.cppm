module;

#include <cstdint>
#include <span>

export module daplink.dap_transport;

import daplink.dap_core;
import daplink.dap_queue;
import daplink.swd_engine;
import daplink.dap_backend;
import daplink.usb_minimal;

export namespace daplink::dap_transport {
    template <daplink::dap_backend::SwdBackend Backend>
    struct HidTransport {
        daplink::dap_core::State& state;
        daplink::dap_core::DeviceInfo info;
        daplink::dap_queue::Queue<> queue{};

        HidTransport(daplink::dap_core::State& s, daplink::dap_core::DeviceInfo i) noexcept
            : state(s), info(i) {}

        void reset() noexcept {
            queue.reset();
        }

        bool busy() const noexcept {
            return daplink::usb_minimal::out_ready() ||
                daplink::usb_minimal::hid_in_busy() ||
                queue.has_pending();
        }

        void poll_in(const std::uint8_t burst_limit) noexcept {
            std::uint8_t processed = 0;
            while (daplink::usb_minimal::out_ready() && processed < burst_limit) {
                if (!queue.can_accept()) {
                    break;
                }
                auto in = daplink::usb_minimal::out_packet();
                if (!queue.enqueue<Backend>(state, info, in)) {
                    break;
                }
                daplink::usb_minimal::consume_out();
                ++processed;
            }
        }

        void poll_out() noexcept {
            if (!queue.has_pending()) {
                return;
            }
            auto pending = queue.peek();
            auto out = daplink::usb_minimal::in_packet();
            const auto len = queue.peek_len();
            for (std::uint16_t i = 0; i < len; ++i) {
                out[i] = pending[i];
            }
            if (daplink::usb_minimal::try_send_in_packet(len)) {
                queue.consume();
            }
        }
    };
}
