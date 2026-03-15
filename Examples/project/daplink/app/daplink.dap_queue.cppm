module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module daplink.dap_queue;

import daplink.cmsis_dap;
import daplink.dap_ops;
import daplink.swd_engine;
import daplink.dap_backend;

export namespace daplink::dap_queue {
    template <std::size_t Count = daplink::cmsis_dap::kPacketCount>
    struct Queue {
        using Packet = std::array<std::uint8_t, daplink::cmsis_dap::kPacketSize>;

        std::array<Packet, Count> packets{};
        std::array<std::uint16_t, Count> resp_len{};
        std::uint8_t recv_idx = 0;
        std::uint8_t send_idx = 0;
        std::uint8_t free_count = Count;
        std::uint8_t send_count = 0;

        void reset() noexcept {
            recv_idx = 0;
            send_idx = 0;
            free_count = Count;
            send_count = 0;
        }

        bool can_accept() const noexcept {
            return free_count != 0;
        }

        bool has_pending() const noexcept {
            return send_count != 0;
        }

        template <daplink::dap_backend::SwdBackend Backend,
                  daplink::dap_backend::DapOps Ops = daplink::cmsis_dap::DefaultOps<Backend>>
        bool enqueue(daplink::cmsis_dap::State& state,
                     daplink::cmsis_dap::DeviceInfo info,
                     std::span<const std::uint8_t, daplink::cmsis_dap::kPacketSize> in) noexcept {
            if (free_count == 0) {
                return false;
            }
            auto& slot = packets[recv_idx];
            auto out = std::span<std::uint8_t, daplink::cmsis_dap::kPacketSize>(slot);
            daplink::cmsis_dap::process_packet<Backend, Ops>(state, info, in, out);
            resp_len[recv_idx] = static_cast<std::uint16_t>(slot.size());
            recv_idx = static_cast<std::uint8_t>((recv_idx + 1) % Count);
            --free_count;
            ++send_count;
            return true;
        }

        std::span<std::uint8_t, daplink::cmsis_dap::kPacketSize> peek() noexcept {
            return std::span<std::uint8_t, daplink::cmsis_dap::kPacketSize>(packets[send_idx]);
        }

        std::uint16_t peek_len() const noexcept {
            return resp_len[send_idx];
        }

        void consume() noexcept {
            if (send_count == 0) {
                return;
            }
            send_idx = static_cast<std::uint8_t>((send_idx + 1) % Count);
            --send_count;
            ++free_count;
        }
    };
}
