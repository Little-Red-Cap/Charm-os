module;

#include <array>

export module driver.i2c_whoami_probe;

import io.device_i2c;
import util.core;
import util.error;
import util.expected;

export namespace driver::i2c {
    struct WhoAmIProbeConfig {
        util::u8 register_address{0};
        util::u8 expected_value{0};
    };

    class WhoAmIProbe {
    public:
        explicit constexpr WhoAmIProbe(io::device::I2cDeviceRef device,
                                       WhoAmIProbeConfig config) noexcept
            : device_(device), config_(config) {}

        [[nodiscard]] constexpr bool valid() const noexcept {
            return device_.valid();
        }

        [[nodiscard]] util::Result<util::u8> read_id() noexcept {
            std::array<util::u8, 1> tx{config_.register_address};
            std::array<util::u8, 1> rx{};
            auto result = device_.write_read(tx, rx);
            if (!result) {
                return util::unexpected(result.error());
            }
            return rx[0];
        }

        [[nodiscard]] util::Result<void> probe() noexcept {
            auto id = read_id();
            if (!id) {
                return util::unexpected(id.error());
            }
            if (id.value() != config_.expected_value) {
                return util::unexpected(util::Errc::bad_state);
            }
            return {};
        }

        [[nodiscard]] constexpr WhoAmIProbeConfig config() const noexcept {
            return config_;
        }

    private:
        io::device::I2cDeviceRef device_{};
        WhoAmIProbeConfig config_{};
    };
}
