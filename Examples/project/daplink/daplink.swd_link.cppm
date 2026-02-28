module;

#include "main.h"

#include <cstdint>

export module daplink.swd_link;

namespace daplink::swd::detail {
    constexpr std::uint8_t kReqApndp = 1U << 0;
    constexpr std::uint8_t kReqRnw = 1U << 1;
    constexpr std::uint8_t kReqA2 = 1U << 2;
    constexpr std::uint8_t kReqA3 = 1U << 3;

    constexpr std::uint8_t kAckOk = 1U;
    constexpr std::uint8_t kAckWait = 2U;
    constexpr std::uint8_t kAckFault = 4U;
    constexpr std::uint8_t kAckError = 8U;

    struct swd_cfg {
        std::uint8_t turnaround = 1;
        std::uint8_t idle_cycles = 0;
        std::uint16_t retry_count = 64;
        bool active = false;
    };

    inline swd_cfg g_cfg{};

    inline void pin_delay() noexcept {
        __NOP();
        __NOP();
    }

    inline void swclk_low() noexcept {
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_RESET);
    }

    inline void swclk_high() noexcept {
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_SET);
    }

    inline void swdio_write(const std::uint8_t bit) noexcept {
        HAL_GPIO_WritePin(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, bit ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    inline std::uint8_t swdio_read() noexcept {
        return (HAL_GPIO_ReadPin(T_DIO_IN_GPIO_Port, T_DIO_IN_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    }

    inline void swd_cycle() noexcept {
        swclk_low();
        pin_delay();
        swclk_high();
        pin_delay();
    }

    inline void swd_write_bit(const std::uint8_t bit) noexcept {
        swdio_write(bit & 1U);
        swd_cycle();
    }

    inline std::uint8_t swd_read_bit() noexcept {
        swclk_low();
        pin_delay();
        const auto bit = swdio_read();
        swclk_high();
        pin_delay();
        return bit;
    }

    inline std::uint8_t parity32(std::uint32_t v) noexcept {
        v ^= v >> 16;
        v ^= v >> 8;
        v ^= v >> 4;
        v ^= v >> 2;
        v ^= v >> 1;
        return static_cast<std::uint8_t>(v & 1U);
    }

    inline void setup_swd_pins_active() noexcept {
        GPIO_InitTypeDef gpio = {};

        gpio.Pin = T_CLK_Pin;
        gpio.Mode = GPIO_MODE_OUTPUT_OD;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(T_CLK_GPIO_Port, &gpio);

        gpio.Pin = T_DIO_OUT_Pin | T_CLKB4_Pin;
        gpio.Mode = GPIO_MODE_OUTPUT_OD;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &gpio);

        gpio.Pin = T_DIO_IN_Pin;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(T_DIO_IN_GPIO_Port, &gpio);

        gpio.Pin = T_RST_Pin;
        gpio.Mode = GPIO_MODE_OUTPUT_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(T_RST_GPIO_Port, &gpio);

        // Keep line high in idle and enable B4 control pin by default.
        HAL_GPIO_WritePin(T_CLK_GPIO_Port, T_CLK_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(T_DIO_OUT_GPIO_Port, T_DIO_OUT_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(T_CLKB4_GPIO_Port, T_CLKB4_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, GPIO_PIN_SET);
    }

    inline void setup_pins_hi_z() noexcept {
        GPIO_InitTypeDef gpio = {};
        gpio.Pin = T_CLK_Pin;
        gpio.Mode = GPIO_MODE_ANALOG;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(T_CLK_GPIO_Port, &gpio);

        gpio.Pin = T_RST_Pin;
        HAL_GPIO_Init(T_RST_GPIO_Port, &gpio);

        gpio.Pin = T_DIO_IN_Pin | T_CLKB4_Pin | T_DIO_OUT_Pin;
        HAL_GPIO_Init(GPIOB, &gpio);
    }

    inline void swj_sequence(const std::uint8_t* data, std::uint32_t bits) noexcept {
        std::uint8_t cur = 0;
        std::uint8_t n = 0;
        while (bits--) {
            if (n == 0) {
                cur = *data++;
                n = 8;
            }
            swdio_write(cur & 1U);
            swd_cycle();
            cur = static_cast<std::uint8_t>(cur >> 1);
            --n;
        }
    }

    inline void swd_line_reset() noexcept {
        // At least 50 cycles with SWDIO high.
        swdio_write(1U);
        for (int i = 0; i < 56; ++i) {
            swd_cycle();
        }
    }

    inline void swj_switch_jtag_to_swd() noexcept {
        constexpr std::uint8_t seq[] = {0x9E, 0xE7}; // 0xE79E, LSB first
        swj_sequence(seq, 16);
    }
}

export namespace daplink::swd {
    using namespace detail;

    inline void set_transfer_config(const std::uint8_t idle_cycles, const std::uint16_t retry_count) noexcept {
        g_cfg.idle_cycles = idle_cycles;
        g_cfg.retry_count = retry_count;
    }

    inline void set_swd_config(const std::uint8_t turnaround) noexcept {
        g_cfg.turnaround = static_cast<std::uint8_t>((turnaround & 0x3U) + 1U);
    }

    inline bool connect_swd() noexcept {
        setup_swd_pins_active();
        swd_line_reset();
        swj_switch_jtag_to_swd();
        swd_line_reset();
        g_cfg.active = true;
        return true;
    }

    inline void disconnect() noexcept {
        g_cfg.active = false;
        setup_pins_hi_z();
    }

    inline std::uint8_t swj_pins(const std::uint8_t value, const std::uint8_t select) noexcept {
        if ((select & (1U << 0)) != 0U) {
            if ((value & (1U << 0)) != 0U) swclk_high();
            else swclk_low();
        }
        if ((select & (1U << 1)) != 0U) {
            swdio_write((value >> 1) & 1U);
        }
        if ((select & (1U << 7)) != 0U) {
            HAL_GPIO_WritePin(T_RST_GPIO_Port, T_RST_Pin, ((value >> 7) & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        std::uint8_t pin_state = 0;
        pin_state |= (HAL_GPIO_ReadPin(T_CLK_GPIO_Port, T_CLK_Pin) == GPIO_PIN_SET) ? (1U << 0) : 0U;
        pin_state |= (HAL_GPIO_ReadPin(T_DIO_IN_GPIO_Port, T_DIO_IN_Pin) == GPIO_PIN_SET) ? (1U << 1) : 0U;
        pin_state |= (HAL_GPIO_ReadPin(T_RST_GPIO_Port, T_RST_Pin) == GPIO_PIN_SET) ? (1U << 7) : 0U;
        return pin_state;
    }

    inline void swj_sequence_bits(const std::uint8_t* data, std::uint32_t bits) noexcept {
        swj_sequence(data, bits);
    }

    inline std::uint8_t transfer_once(const std::uint8_t request, std::uint32_t& data) noexcept {
        if (!g_cfg.active) {
            return kAckError;
        }

        const auto req_apndp = (request >> 0) & 1U;
        const auto req_rnw = (request >> 1) & 1U;
        const auto req_a2 = (request >> 2) & 1U;
        const auto req_a3 = (request >> 3) & 1U;
        const auto req_parity = static_cast<std::uint8_t>((req_apndp ^ req_rnw ^ req_a2 ^ req_a3) & 1U);

        swd_write_bit(1U);
        swd_write_bit(req_apndp);
        swd_write_bit(req_rnw);
        swd_write_bit(req_a2);
        swd_write_bit(req_a3);
        swd_write_bit(req_parity);
        swd_write_bit(0U);
        swd_write_bit(1U);

        for (std::uint8_t i = 0; i < g_cfg.turnaround; ++i) {
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
                    const auto b = swd_read_bit();
                    val |= (static_cast<std::uint32_t>(b) << i);
                    parity ^= b;
                }
                const auto p = swd_read_bit();
                if ((parity & 1U) != (p & 1U)) {
                    ack = kAckError;
                } else {
                    data = val;
                }

                for (std::uint8_t i = 0; i < g_cfg.turnaround; ++i) {
                    swd_cycle();
                }
            } else {
                for (std::uint8_t i = 0; i < g_cfg.turnaround; ++i) {
                    swd_cycle();
                }
                const auto p = parity32(data);
                for (std::uint8_t i = 0; i < 32; ++i) {
                    swd_write_bit(static_cast<std::uint8_t>((data >> i) & 1U));
                }
                swd_write_bit(p);
            }
        } else if (ack == kAckWait || ack == kAckFault) {
            // Backoff over data phase and turnaround for robust recovery.
            for (int i = 0; i < (32 + 1 + 2); ++i) {
                swd_cycle();
            }
        } else {
            ack = kAckError;
            for (int i = 0; i < (32 + 1 + 2); ++i) {
                swd_cycle();
            }
        }

        if (g_cfg.idle_cycles != 0U) {
            swdio_write(0U);
            for (std::uint8_t i = 0; i < g_cfg.idle_cycles; ++i) {
                swd_cycle();
            }
            swdio_write(1U);
        }

        return ack;
    }

    inline std::uint8_t transfer(const std::uint8_t request, std::uint32_t& data) noexcept {
        std::uint16_t retry = g_cfg.retry_count;
        while (true) {
            const auto ack = transfer_once(request, data);
            if (ack != kAckWait) {
                return ack;
            }
            if (retry == 0U) {
                return ack;
            }
            --retry;
        }
    }
}
