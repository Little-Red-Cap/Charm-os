module;

#include <array>
#include <cstdint>
#include <span>

export module daplink.dap_transport;

import daplink.dap_queue;
import daplink.swd_engine;
import daplink.dap_backend;
import daplink.cmsis_dap;
import daplink.dap_ops;
import daplink.dap_strategy;
import daplink.usb_minimal;

export namespace daplink::dap_transport {
    namespace detail {
        constexpr std::uint8_t kCmsisDapQueueCommands = 0x7E;
        constexpr std::uint8_t kCmsisDapExecuteCommands = 0x7F;
    }

    template <daplink::dap_backend::SwdBackend Backend,
              daplink::dap_backend::DapOps Ops = daplink::cmsis_dap::DefaultOps<Backend>,
              typename Policy = daplink::dap_strategy::DefaultTransferPolicy<daplink::cmsis_dap::State>>
    struct HidTransport {
        using Packet = std::array<std::uint8_t, daplink::cmsis_dap::kPacketSize>;

        daplink::cmsis_dap::State& state;
        daplink::cmsis_dap::DeviceInfo info;
        daplink::dap_queue::Queue<> queue{};
        std::array<Packet, daplink::cmsis_dap::kPacketCount> queued_packets{};
        std::uint8_t queued_packet_count = 0;

        HidTransport(daplink::cmsis_dap::State& s, daplink::cmsis_dap::DeviceInfo i) noexcept
            : state(s), info(i) {}

        void reset() noexcept {
            queue.reset();
            queued_packet_count = 0;
        }

        bool busy() const noexcept {
            return daplink::usb_minimal::out_ready() ||
                daplink::usb_minimal::hid_in_busy() ||
                queue.has_pending() ||
                (queued_packet_count != 0);
        }

        bool queue_atomic_packet(
            std::span<const std::uint8_t, daplink::cmsis_dap::kPacketSize> in) noexcept {
            if (queued_packet_count >= queued_packets.size()) {
                return false;
            }
            auto& slot = queued_packets[queued_packet_count];
            for (std::size_t i = 0; i < slot.size(); ++i) {
                slot[i] = in[i];
            }
            ++queued_packet_count;
            return true;
        }

        bool flush_atomic_packets() noexcept {
            if (!queue.can_accept(static_cast<std::uint8_t>(queued_packet_count + 1U))) {
                return false;
            }
            for (std::uint8_t i = 0; i < queued_packet_count; ++i) {
                auto packet = queued_packets[i];
                packet[0] = detail::kCmsisDapExecuteCommands;
                auto exec = std::span<const std::uint8_t, daplink::cmsis_dap::kPacketSize>(packet);
                if (!queue.enqueue<Backend, Ops, Policy>(state, info, exec)) {
                    return false;
                }
            }
            queued_packet_count = 0;
            return true;
        }

        void poll_in(const std::uint8_t burst_limit) noexcept {
            std::uint8_t processed = 0;
            while (daplink::usb_minimal::out_ready() && processed < burst_limit) {
                auto in = daplink::usb_minimal::out_packet();
                if (in[0] == detail::kCmsisDapQueueCommands) {
                    if (!queue_atomic_packet(in)) {
                        break;
                    }
                    daplink::usb_minimal::consume_out();
                    ++processed;
                    continue;
                }

                if (!flush_atomic_packets()) {
                    break;
                }

                if (!queue.can_accept()) {
                    break;
                }
                if (!queue.enqueue<Backend, Ops, Policy>(state, info, in)) {
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
            if (daplink::usb_minimal::hid_in_busy()) {
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
