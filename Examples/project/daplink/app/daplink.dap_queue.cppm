module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module daplink.dap_queue;

import daplink.dap_core;
import daplink.swd_engine;

export namespace daplink::dap_queue {
    template <std::size_t Count = daplink::dap_core::kPacketCount>
    struct Queue {
        using Packet = std::array<std::uint8_t, daplink::dap_core::kPacketSize>;

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

        template <daplink::swd::Backend Backend>
        bool enqueue(daplink::dap_core::State& state,
                     daplink::dap_core::DeviceInfo info,
                     std::span<const std::uint8_t, daplink::dap_core::kPacketSize> in) noexcept {
            if (free_count == 0) {
                return false;
            }
            auto& slot = packets[recv_idx];
            auto out = std::span<std::uint8_t, daplink::dap_core::kPacketSize>(slot);
            daplink::dap_core::process_packet<Backend>(state, info, in, out);
            resp_len[recv_idx] = static_cast<std::uint16_t>(slot.size());
            recv_idx = static_cast<std::uint8_t>((recv_idx + 1) % Count);
            --free_count;
            ++send_count;
            return true;
        }

        std::span<std::uint8_t, daplink::dap_core::kPacketSize> peek() noexcept {
            return std::span<std::uint8_t, daplink::dap_core::kPacketSize>(packets[send_idx]);
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
