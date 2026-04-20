module;

#include <array>
#include <cstdint>

export module daplink.dap_policy;

import daplink.app_config;
import daplink.board;
import daplink.usb_minimal;
import daplink.ring_buffer;
import io.channel;
import util.core;

export namespace daplink::dap_policy {
    constexpr bool kCdcLoopbackTest = daplink::app_config::kCdcLoopbackTestValue;

    enum class CdcPolicy : std::uint8_t {
        block_when_dap_busy = 0,
        allow_always = 1,
    };

    struct CdcLine {
        std::uint32_t baud = 0;
        std::uint8_t stop_bits = 0;
        std::uint8_t parity = 0;
        std::uint8_t data_bits = 0;
    };

    struct UsbScheduler {
        CdcPolicy cdc_policy = CdcPolicy::block_when_dap_busy;

        std::uint8_t dap_burst_limit() const noexcept {
            return daplink::app_config::kConfig.dap.burst_limit;
        }

        bool allow_cdc(bool dap_busy) const noexcept {
            if (cdc_policy == CdcPolicy::allow_always) {
                return true;
            }
            return !dap_busy;
        }

        static CdcLine to_line(const daplink::usb_minimal::cdc_line_config& line) noexcept {
            return CdcLine{
                line.baud,
                line.stop_bits,
                line.parity,
                line.data_bits
            };
        }

        bool should_apply_line(const CdcLine& last, const CdcLine& next) const noexcept {
            return last.baud != next.baud ||
                last.stop_bits != next.stop_bits ||
                last.parity != next.parity ||
                last.data_bits != next.data_bits;
        }

        void apply_line(CdcLine& last, const CdcLine& next) const noexcept {
            last = next;
        }

        template <std::size_t Chunk, std::size_t BufSize>
        void pump_cdc(io::Channel& usb_cdc,
                      io::Channel& uart,
                      daplink::ring_buffer::Buffer<BufSize>& uart_tx,
                      daplink::ring_buffer::Buffer<BufSize>& uart_rx) const noexcept {
            const auto pump_read = [](io::Channel& ch,
                                      daplink::ring_buffer::Buffer<BufSize>& rb) noexcept {
                const std::uint16_t free = rb.free();
                if (free == 0) {
                    return;
                }
                std::array<util::u8, Chunk> temp{};
                const std::size_t want = (free < temp.size()) ? free : temp.size();
                auto r = ch.read(io::MutByteView{temp.data(), want});
                if (!r) {
                    return;
                }
                const auto count = static_cast<std::uint16_t>(r.value());
                for (std::uint16_t i = 0; i < count; ++i) {
                    (void)rb.push(static_cast<std::uint8_t>(temp[i]));
                }
            };

            const auto pump_write = [](io::Channel& ch,
                                       daplink::ring_buffer::Buffer<BufSize>& rb) noexcept {
                const std::uint16_t available = rb.count();
                if (available == 0) {
                    return;
                }
                std::array<util::u8, Chunk> temp{};
                const std::uint16_t want =
                    static_cast<std::uint16_t>((available < temp.size()) ? available : temp.size());
                const auto len = rb.peek(reinterpret_cast<std::uint8_t*>(temp.data()), want);
                if (len == 0) {
                    return;
                }
                auto r = ch.write(io::ByteView{temp.data(), len});
                if (r) {
                    rb.drop(static_cast<std::uint16_t>(r.value()));
                }
            };

            pump_read(usb_cdc, uart_tx);
            pump_read(uart, uart_rx);
            pump_write(uart, uart_tx);
            pump_write(usb_cdc, uart_rx);
        }

        template <std::size_t Chunk, std::size_t BufSize>
        void pump_cdc_loopback(io::Channel& usb_cdc,
                               daplink::ring_buffer::Buffer<BufSize>& loopback) const noexcept {
            const auto pump_read = [](io::Channel& ch,
                                      daplink::ring_buffer::Buffer<BufSize>& rb) noexcept {
                const std::uint16_t free = rb.free();
                if (free == 0) {
                    return;
                }
                std::array<util::u8, Chunk> temp{};
                const std::size_t want = (free < temp.size()) ? free : temp.size();
                auto r = ch.read(io::MutByteView{temp.data(), want});
                if (!r) {
                    return;
                }
                const auto count = static_cast<std::uint16_t>(r.value());
                for (std::uint16_t i = 0; i < count; ++i) {
                    (void)rb.push(static_cast<std::uint8_t>(temp[i]));
                }
            };

            const auto pump_write = [](io::Channel& ch,
                                       daplink::ring_buffer::Buffer<BufSize>& rb) noexcept {
                const std::uint16_t available = rb.count();
                if (available == 0) {
                    return;
                }
                std::array<util::u8, Chunk> temp{};
                const std::uint16_t want =
                    static_cast<std::uint16_t>((available < temp.size()) ? available : temp.size());
                const auto len = rb.peek(reinterpret_cast<std::uint8_t*>(temp.data()), want);
                if (len == 0) {
                    return;
                }
                auto r = ch.write(io::ByteView{temp.data(), len});
                if (r) {
                    rb.drop(static_cast<std::uint16_t>(r.value()));
                }
            };

            pump_read(usb_cdc, loopback);
            pump_write(usb_cdc, loopback);
        }

        template <std::size_t Chunk, std::size_t BufSize, typename HidTransport, typename ResetFn>
        void tick(HidTransport& dap_transport,
                  ResetFn&& reset_handler,
                  io::Channel& usb_cdc,
                  io::Channel& uart,
                  daplink::ring_buffer::Buffer<BufSize>& uart_tx,
                  daplink::ring_buffer::Buffer<BufSize>& uart_rx,
                  CdcLine& last_line,
                  bool enable_hid,
                  bool enable_cdc) const noexcept {
            if (daplink::usb_minimal::take_reset()) {
                reset_handler();
            }

            daplink::usb_minimal::poll();

            if (enable_hid) {
                dap_transport.poll_in(dap_burst_limit());
                dap_transport.poll_out();
            }

            if (enable_cdc && allow_cdc(dap_transport.busy())) {
                if constexpr (kCdcLoopbackTest) {
                    pump_cdc_loopback<Chunk>(usb_cdc, uart_tx);
                } else {
                    const auto line = to_line(daplink::usb_minimal::cdc_line());
                    if (should_apply_line(last_line, line)) {
                        apply_line(last_line, line);
                        daplink::board::cdc_uart_apply_line(
                            last_line.baud, last_line.stop_bits, last_line.parity, last_line.data_bits);
                    }
                    pump_cdc<Chunk>(usb_cdc, uart, uart_tx, uart_rx);
                }
            }
        }
    };
}
