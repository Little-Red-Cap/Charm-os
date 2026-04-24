module;
#include <cstdint>
export module daplink.swd_engine;

import daplink.app_config;
import daplink.dap_backend;

export namespace daplink::swd {
    struct Config {
        std::uint8_t turnaround = daplink::app_config::kConfig.swd.turnaround;
        std::uint8_t idle_cycles = daplink::app_config::kConfig.swd.idle_cycles;
        std::uint16_t retry_count = daplink::app_config::kConfig.swd.retry_count; // CMSIS-DAP default retry count.
        bool data_phase = false;
    };

    template <daplink::dap_backend::SwdBackend B>
    struct Engine {
        static void line_reset() noexcept {
            B::swdio_set_output();
            B::swdio_write(1U);
            for (int i = 0; i < 56; ++i) {
                swd_cycle();
            }
        }

        static void swj_sequence(const std::uint8_t* data, std::uint32_t bits) noexcept {
            B::swdio_set_output();
            std::uint8_t cur = 0;
            std::uint8_t n = 0;
            while (bits--) {
                if (n == 0) {
                    cur = *data++;
                    n = 8;
                }
                B::swdio_write(cur & 1U);
                swd_cycle();
                cur = static_cast<std::uint8_t>(cur >> 1);
                --n;
            }
        }

        static std::uint8_t transfer_once(const Config& cfg, std::uint8_t request, std::uint32_t& data) noexcept {
            constexpr std::uint8_t kAckOk = 1U;
            constexpr std::uint8_t kAckWait = 2U;
            constexpr std::uint8_t kAckFault = 4U;
            constexpr std::uint8_t kAckError = 8U;

            const auto req_apndp = (request >> 0) & 1U;
            const auto req_rnw = (request >> 1) & 1U;
            const auto req_a2 = (request >> 2) & 1U;
            const auto req_a3 = (request >> 3) & 1U;
            const auto req_parity = static_cast<std::uint8_t>((req_apndp ^ req_rnw ^ req_a2 ^ req_a3) & 1U);

            B::swdio_set_output();
            swd_write_bit(1U);
            swd_write_bit(req_apndp);
            swd_write_bit(req_rnw);
            swd_write_bit(req_a2);
            swd_write_bit(req_a3);
            swd_write_bit(req_parity);
            swd_write_bit(0U);
            swd_write_bit(1U);

            B::swdio_set_input();
            B::pin_delay();
            for (std::uint8_t i = 0; i < cfg.turnaround; ++i) {
                swd_cycle();
            }

            std::uint8_t ack = 0;
            ack |= static_cast<std::uint8_t>(swd_read_bit() << 0);
            ack |= static_cast<std::uint8_t>(swd_read_bit() << 1);
            ack |= static_cast<std::uint8_t>(swd_read_bit() << 2);

            if (ack == kAckOk) {
                if (req_rnw != 0U) {
                    std::uint32_t val = 0;
                    std::uint8_t parity = 0;
                    for (std::uint8_t i = 0; i < 32; ++i) {
                        const auto bit = swd_read_bit();
                        val |= (static_cast<std::uint32_t>(bit) << i);
                        parity ^= bit;
                    }
                    const auto p = swd_read_bit();
                    if ((parity & 1U) != (p & 1U)) {
                        ack = kAckError;
                    } else {
                        data = val;
                    }

                    for (std::uint8_t i = 0; i < cfg.turnaround; ++i) {
                        swd_cycle();
                    }
                    B::swdio_set_output();
                    B::pin_delay();
                    B::swdio_write(1U);
                } else {
                    for (std::uint8_t i = 0; i < cfg.turnaround; ++i) {
                        swd_cycle();
                    }
                    B::swdio_set_output();
                    B::pin_delay();
                    const auto p = parity32(data);
                    for (std::uint8_t i = 0; i < 32; ++i) {
                        swd_write_bit(static_cast<std::uint8_t>((data >> i) & 1U));
                    }
                    swd_write_bit(p);
                    B::swdio_write(1U);
                }
            } else if (ack == kAckWait || ack == kAckFault) {
                if (cfg.data_phase && (req_rnw != 0U)) {
                    for (int i = 0; i < (32 + 1); ++i) {
                        swd_cycle();
                    }
                }
                for (std::uint8_t i = 0; i < cfg.turnaround; ++i) {
                    swd_cycle();
                }
                B::swdio_set_output();
                B::pin_delay();
                if (cfg.data_phase && (req_rnw == 0U)) {
                    B::swdio_write(0U);
                    for (int i = 0; i < (32 + 1); ++i) {
                        swd_cycle();
                    }
                }
                B::swdio_write(1U);
            } else {
                for (int i = 0; i < (static_cast<int>(cfg.turnaround) + 32 + 1); ++i) {
                    swd_cycle();
                }
                B::swdio_set_output();
                B::pin_delay();
                B::swdio_write(1U);
            }

            if (cfg.idle_cycles != 0U) {
                B::swdio_set_output();
                B::swdio_write(0U);
                for (std::uint8_t i = 0; i < cfg.idle_cycles; ++i) {
                    swd_cycle();
                }
                B::swdio_write(1U);
            }

            return ack;
        }

        static std::uint8_t transfer(const Config& cfg, std::uint8_t request, std::uint32_t& data) noexcept {
            constexpr std::uint8_t kAckWait = 2U;
            std::uint16_t retry = cfg.retry_count;
            while (true) {
                const auto ack = transfer_once(cfg, request, data);
                if (ack != kAckWait) {
                    return ack;
                }
                if (retry == 0U) {
                    return ack;
                }
                --retry;
            }
        }

    private:
        static void swd_cycle() noexcept {
            B::swclk_low();
            B::pin_delay();
            B::swclk_high();
            B::pin_delay();
        }

        static void swd_write_bit(const std::uint8_t bit) noexcept {
            B::swdio_write(bit & 1U);
            swd_cycle();
        }

        static std::uint8_t swd_read_bit() noexcept {
            B::swclk_low();
            B::pin_delay();
            const auto bit = B::swdio_read();
            B::swclk_high();
            B::pin_delay();
            return bit;
        }

        static std::uint8_t parity32(std::uint32_t v) noexcept {
            v ^= v >> 16;
            v ^= v >> 8;
            v ^= v >> 4;
            v ^= v >> 2;
            v ^= v >> 1;
            return static_cast<std::uint8_t>(v & 1U);
        }
    };
}
