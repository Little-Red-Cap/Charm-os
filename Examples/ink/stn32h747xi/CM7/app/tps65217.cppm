// tps65217.cppm
// Soft-I2C helper + TPS65217 register access for swapped PB7/PB6.

module;
#include <cstdint>
#include "main.h"

export module app.tps65217;

namespace app::tps65217::detail
{
    constexpr std::uint8_t kAddr7 = 0x24;
    constexpr std::uint8_t kRegChipId = 0x00;
    constexpr std::uint8_t kRegStatus = 0x0A;
    constexpr std::uint8_t kRegPgood  = 0x0C;
    constexpr std::uint8_t kRegPassword = 0x0B;

    static constexpr std::uint16_t kSclPin = GPIO_PIN_7; // physical SCL is on PB7
    static constexpr std::uint16_t kSdaPin = GPIO_PIN_6; // physical SDA is on PB6

    static void soft_delay() noexcept
    {
        for (volatile int i = 0; i < 200; ++i) {
            __NOP();
        }
    }

    static inline void scl_high() noexcept { HAL_GPIO_WritePin(GPIOB, kSclPin, GPIO_PIN_SET); }
    static inline void scl_low() noexcept  { HAL_GPIO_WritePin(GPIOB, kSclPin, GPIO_PIN_RESET); }
    static inline void sda_high() noexcept { HAL_GPIO_WritePin(GPIOB, kSdaPin, GPIO_PIN_SET); }
    static inline void sda_low() noexcept  { HAL_GPIO_WritePin(GPIOB, kSdaPin, GPIO_PIN_RESET); }
    static inline GPIO_PinState sda_read() noexcept { return HAL_GPIO_ReadPin(GPIOB, kSdaPin); }

    static void start() noexcept
    {
        sda_high();
        scl_high();
        soft_delay();
        sda_low();
        soft_delay();
        scl_low();
        soft_delay();
    }

    static void stop() noexcept
    {
        sda_low();
        soft_delay();
        scl_high();
        soft_delay();
        sda_high();
        soft_delay();
    }

    static bool write_byte(std::uint8_t byte) noexcept
    {
        for (int i = 0; i < 8; ++i) {
            if (byte & 0x80u) {
                sda_high();
            } else {
                sda_low();
            }
            soft_delay();
            scl_high();
            soft_delay();
            scl_low();
            soft_delay();
            byte <<= 1;
        }

        sda_high();
        soft_delay();
        scl_high();
        soft_delay();
        const bool ack = (sda_read() == GPIO_PIN_RESET);
        scl_low();
        soft_delay();
        return ack;
    }

    static std::uint8_t read_byte(bool ack) noexcept
    {
        std::uint8_t value = 0;
        sda_high();
        for (int i = 0; i < 8; ++i) {
            value <<= 1;
            scl_high();
            soft_delay();
            if (sda_read() == GPIO_PIN_SET) {
                value |= 1u;
            }
            scl_low();
            soft_delay();
        }
        if (ack) {
            sda_low();
        } else {
            sda_high();
        }
        soft_delay();
        scl_high();
        soft_delay();
        scl_low();
        soft_delay();
        sda_high();
        return value;
    }

    static bool write_reg(std::uint8_t reg, std::uint8_t val) noexcept
    {
        start();
        if (!write_byte((kAddr7 << 1) | 0u)) { stop(); return false; }
        if (!write_byte(reg)) { stop(); return false; }
        if (!write_byte(val)) { stop(); return false; }
        stop();
        return true;
    }

    static bool read_reg(std::uint8_t reg, std::uint8_t& out) noexcept
    {
        start();
        if (!write_byte((kAddr7 << 1) | 0u)) { stop(); return false; }
        if (!write_byte(reg)) { stop(); return false; }
        start();
        if (!write_byte((kAddr7 << 1) | 1u)) { stop(); return false; }
        out = read_byte(false);
        stop();
        return true;
    }

    static bool write_level1(std::uint8_t reg, std::uint8_t val) noexcept
    {
        const std::uint8_t key = reg ^ 0x7D;
        if (!write_reg(kRegPassword, key)) return false;
        return write_reg(reg, val);
    }

    static bool write_level2(std::uint8_t reg, std::uint8_t val) noexcept
    {
        const std::uint8_t key = reg ^ 0x7D;
        if (!write_reg(kRegPassword, key)) return false;
        if (!write_reg(reg, val)) return false;
        if (!write_reg(kRegPassword, key)) return false;
        return write_reg(reg, val);
    }
}

export namespace app::tps65217
{
    enum class Dcdc : std::uint8_t { Dcdc1 = 0, Dcdc2 = 1, Dcdc3 = 2 };
    enum class Ldo  : std::uint8_t { Ldo1 = 0,  Ldo2  = 1 };
    enum class Ls   : std::uint8_t { Ls1  = 0,  Ls2   = 1 };

    struct Pins {
        int scl{0};
        int sda{0};
    };

    void init_soft() noexcept
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitTypeDef gpio{};
        gpio.Pin = detail::kSclPin | detail::kSdaPin;
        gpio.Mode = GPIO_MODE_OUTPUT_OD;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &gpio);
        detail::scl_high();
        detail::sda_high();
    }

    Pins read_pins() noexcept
    {
        Pins p{};
        p.scl = (HAL_GPIO_ReadPin(GPIOB, detail::kSclPin) == GPIO_PIN_SET) ? 1 : 0;
        p.sda = (HAL_GPIO_ReadPin(GPIOB, detail::kSdaPin) == GPIO_PIN_SET) ? 1 : 0;
        return p;
    }

    void scan(void (*found)(std::uint8_t)) noexcept
    {
        for (std::uint8_t addr = 0x03; addr <= 0x77; ++addr) {
            detail::start();
            const bool ack = detail::write_byte((addr << 1) | 0u);
            detail::stop();
            if (ack && found) {
                found(addr);
            }
        }
    }

    bool read_chip_id(std::uint8_t& out) noexcept { return detail::read_reg(detail::kRegChipId, out); }
    bool read_status(std::uint8_t& out) noexcept  { return detail::read_reg(detail::kRegStatus, out); }
    bool read_pgood(std::uint8_t& out) noexcept   { return detail::read_reg(detail::kRegPgood, out); }

    bool set_dcdc_voltage(Dcdc id, std::uint8_t code) noexcept
    {
        const std::uint8_t reg = static_cast<std::uint8_t>(0x0E + static_cast<std::uint8_t>(id));
        return detail::write_level2(reg, code);
    }

    bool set_ldo_voltage(Ldo id, std::uint8_t code) noexcept
    {
        const std::uint8_t reg = static_cast<std::uint8_t>(0x12 + static_cast<std::uint8_t>(id));
        return detail::write_level2(reg, code);
    }

    bool set_ls_voltage(Ls id, std::uint8_t code) noexcept
    {
        const std::uint8_t reg = static_cast<std::uint8_t>(0x14 + static_cast<std::uint8_t>(id));
        return detail::write_level2(reg, code);
    }

    bool set_enable(std::uint8_t mask) noexcept
    {
        return detail::write_level1(0x16, mask);
    }
}
