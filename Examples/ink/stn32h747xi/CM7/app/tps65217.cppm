// tps65217.cppm
// Soft-I2C helper + TPS65217 register access for swapped PB7/PB6.

module;
#include <array>
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
    constexpr std::uint8_t kRegDefDcdc1 = 0x0E;
    constexpr std::uint8_t kRegDefDcdc2 = 0x0F;
    constexpr std::uint8_t kRegDefDcdc3 = 0x10;
    constexpr std::uint8_t kRegDefSlew  = 0x11;
    constexpr std::uint8_t kRegDefLdo1  = 0x12;
    constexpr std::uint8_t kRegDefLdo2  = 0x13;
    constexpr std::uint8_t kRegDefLs1   = 0x14;
    constexpr std::uint8_t kRegDefLs2   = 0x15;

    constexpr std::uint8_t kMaskDcdc = 0x3F;
    constexpr std::uint8_t kMaskLdo1 = 0x0F;
    constexpr std::uint8_t kMaskLdo2 = 0x3F;
    constexpr std::uint8_t kMaskLdo34 = 0x1F;
    constexpr std::uint8_t kBitDcdcXadj = 0x80;
    constexpr std::uint8_t kBitDcdcGo = 0x80;
    constexpr std::uint8_t kBitLdo2Track = 0x40;
    constexpr std::uint8_t kBitLdo3En = 0x20;
    constexpr std::uint8_t kBitLdo4En = 0x20;

    constexpr std::array<int, 16> kLdo1TableUv{
        1000000, 1100000, 1200000, 1250000,
        1300000, 1350000, 1400000, 1500000,
        1600000, 1800000, 2500000, 2750000,
        2800000, 3000000, 3100000, 3300000,
    };

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

    static bool decode_uv1(std::uint8_t code, int& uv) noexcept
    {
        if (code <= 24) {
            uv = 900000 + 25000 * code;
            return true;
        }
        if (code <= 52) {
            uv = 1550000 + 50000 * (code - 25);
            return true;
        }
        if (code <= 55) {
            uv = 3000000 + 100000 * (code - 53);
            return true;
        }
        if (code <= 63) {
            uv = 3300000;
            return true;
        }
        return false;
    }

    static bool decode_uv2(std::uint8_t code, int& uv) noexcept
    {
        if (code <= 8) {
            uv = 1500000 + 50000 * code;
            return true;
        }
        if (code <= 13) {
            uv = 2000000 + 100000 * (code - 9);
            return true;
        }
        if (code <= 31) {
            uv = 2450000 + 50000 * (code - 14);
            return true;
        }
        return false;
    }

    static bool decode_ldo1(std::uint8_t code, int& uv) noexcept
    {
        if (code >= kLdo1TableUv.size()) {
            return false;
        }
        uv = kLdo1TableUv[code];
        return true;
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

    struct VoltageSetting {
        std::uint8_t reg{};
        std::uint8_t raw{};
        std::uint8_t code{};
        int uv{};
        bool uv_valid{false};
    };

    struct DcdcSetting : VoltageSetting {
        bool xadj{false};
    };

    struct LdoSetting : VoltageSetting {
        bool track{false};
    };

    struct LsSetting : VoltageSetting {
        bool ldo_enabled{false};
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

    bool read_dcdc_setting(Dcdc id, DcdcSetting& out) noexcept
    {
        out = {};
        const std::uint8_t reg = static_cast<std::uint8_t>(detail::kRegDefDcdc1 + static_cast<std::uint8_t>(id));
        std::uint8_t raw = 0;
        if (!detail::read_reg(reg, raw)) return false;
        out.reg = reg;
        out.raw = raw;
        out.code = static_cast<std::uint8_t>(raw & detail::kMaskDcdc);
        out.xadj = (raw & detail::kBitDcdcXadj) != 0;
        if (!out.xadj) {
            out.uv_valid = detail::decode_uv1(out.code, out.uv);
        }
        return true;
    }

    bool read_ldo_setting(Ldo id, LdoSetting& out) noexcept
    {
        out = {};
        const std::uint8_t reg = static_cast<std::uint8_t>(detail::kRegDefLdo1 + static_cast<std::uint8_t>(id));
        std::uint8_t raw = 0;
        if (!detail::read_reg(reg, raw)) return false;
        out.reg = reg;
        out.raw = raw;

        if (id == Ldo::Ldo1) {
            out.code = static_cast<std::uint8_t>(raw & detail::kMaskLdo1);
            out.uv_valid = detail::decode_ldo1(out.code, out.uv);
            return true;
        }

        out.track = (raw & detail::kBitLdo2Track) != 0;
        out.code = static_cast<std::uint8_t>(raw & detail::kMaskLdo2);
        if (out.track) {
            DcdcSetting dcdc3{};
            if (read_dcdc_setting(Dcdc::Dcdc3, dcdc3) && dcdc3.uv_valid) {
                out.uv = dcdc3.uv;
                out.uv_valid = true;
            }
            return true;
        }

        out.uv_valid = detail::decode_uv1(out.code, out.uv);
        return true;
    }

    bool read_ls_setting(Ls id, LsSetting& out) noexcept
    {
        out = {};
        const std::uint8_t reg = static_cast<std::uint8_t>(detail::kRegDefLs1 + static_cast<std::uint8_t>(id));
        std::uint8_t raw = 0;
        if (!detail::read_reg(reg, raw)) return false;
        out.reg = reg;
        out.raw = raw;
        out.code = static_cast<std::uint8_t>(raw & detail::kMaskLdo34);

        const std::uint8_t en_mask = (id == Ls::Ls1) ? detail::kBitLdo3En : detail::kBitLdo4En;
        out.ldo_enabled = (raw & en_mask) != 0;
        if (out.ldo_enabled) {
            out.uv_valid = detail::decode_uv2(out.code, out.uv);
        }
        return true;
    }

    bool set_dcdc_voltage(Dcdc id, std::uint8_t code) noexcept
    {
        const std::uint8_t reg = static_cast<std::uint8_t>(detail::kRegDefDcdc1 + static_cast<std::uint8_t>(id));
        return detail::write_level2(reg, code);
    }

    bool set_dcdc_go() noexcept
    {
        return detail::write_level2(detail::kRegDefSlew, detail::kBitDcdcGo);
    }

    bool set_dcdc_voltage_go(Dcdc id, std::uint8_t code) noexcept
    {
        if (!set_dcdc_voltage(id, code)) return false;
        return set_dcdc_go();
    }

    bool set_ldo_voltage(Ldo id, std::uint8_t code) noexcept
    {
        const std::uint8_t reg = static_cast<std::uint8_t>(detail::kRegDefLdo1 + static_cast<std::uint8_t>(id));
        return detail::write_level2(reg, code);
    }

    bool set_ls_voltage(Ls id, std::uint8_t code) noexcept
    {
        const std::uint8_t reg = static_cast<std::uint8_t>(detail::kRegDefLs1 + static_cast<std::uint8_t>(id));
        return detail::write_level2(reg, code);
    }

    bool set_enable(std::uint8_t mask) noexcept
    {
        return detail::write_level1(0x16, mask);
    }
}
