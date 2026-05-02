module;

#include <array>
#include <span>

export module driver.i2c_register_device;

import io.device_i2c;
import util.core;
import util.error;
import util.expected;

export namespace driver::i2c {
    template <util::usize MaxPayload = 8>
    class RegisterDevice8 {
    public:
        static constexpr util::usize max_payload{MaxPayload};

        explicit constexpr RegisterDevice8(io::device::I2cDeviceRef device) noexcept
            : device_(device) {}

        [[nodiscard]] constexpr bool valid() const noexcept {
            return device_.valid();
        }

        [[nodiscard]] util::Result<util::u8> read_u8(util::u8 reg) noexcept {
            std::array<util::u8, 1> tx{reg};
            std::array<util::u8, 1> rx{};
            auto result = device_.write_read(tx, rx);
            if (!result) {
                return util::unexpected(result.error());
            }
            return rx[0];
        }

        [[nodiscard]] util::Result<void> write_u8(util::u8 reg, util::u8 value) noexcept {
            std::array<util::u8, 2> tx{reg, value};
            return device_.write(tx);
        }

        [[nodiscard]] util::Result<void> read(util::u8 start_reg,
                                              std::span<util::u8> rx) noexcept {
            std::array<util::u8, 1> tx{start_reg};
            return device_.write_read(tx, rx);
        }

        [[nodiscard]] util::Result<void> write(util::u8 start_reg,
                                               std::span<const util::u8> payload) noexcept {
            if (payload.size() > max_payload) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            std::array<util::u8, max_payload + 1> tx{};
            tx[0] = start_reg;
            for (util::usize i = 0; i < payload.size(); ++i) {
                tx[i + 1] = payload[i];
            }
            return device_.write(std::span<const util::u8>{tx.data(), payload.size() + 1});
        }

    private:
        io::device::I2cDeviceRef device_{};
    };
}
