#include <array>
#include <cstdio>

import driver.i2c_whoami_probe;
import io.device_i2c;
import io.device_i2c_mock;
import util.core;
import util.error;

namespace {
    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    constexpr io::device::I2cAddress kAddress = 0x18;
    constexpr util::u8 kWhoAmIRegister = 0x0F;
    constexpr util::u8 kExpectedId = 0x33;
    constexpr std::array<util::u8, 1> whoami_reg{kWhoAmIRegister};

    {
        io::device::mock::I2cScriptBus<1, 1, 1> bus{};
        constexpr std::array<util::u8, 1> id{kExpectedId};
        auto scripted = bus.expect_write_read(kAddress, whoami_reg, id);
        if (!expect(static_cast<bool>(scripted), "failed to prepare success script")) return 1;

        auto dev_ref = io::device::make_i2c_device_ref(io::device::make_i2c_bus_ref(bus), kAddress);
        driver::i2c::WhoAmIProbe probe{
            dev_ref,
            driver::i2c::WhoAmIProbeConfig{kWhoAmIRegister, kExpectedId}
        };

        auto result = probe.probe();
        if (!expect(static_cast<bool>(result), "whoami probe success path failed")) return 2;
        if (!expect(bus.all_satisfied(), "success script not fully consumed")) return 3;
    }

    {
        io::device::mock::I2cScriptBus<1, 1, 1> bus{};
        constexpr std::array<util::u8, 1> wrong_id{0x00};
        auto scripted = bus.expect_write_read(kAddress, whoami_reg, wrong_id);
        if (!expect(static_cast<bool>(scripted), "failed to prepare mismatch script")) return 4;

        auto dev_ref = io::device::make_i2c_device_ref(io::device::make_i2c_bus_ref(bus), kAddress);
        driver::i2c::WhoAmIProbe probe{
            dev_ref,
            driver::i2c::WhoAmIProbeConfig{kWhoAmIRegister, kExpectedId}
        };

        auto result = probe.probe();
        if (!expect(!result && result.error() == util::Errc::bad_state,
                    "whoami mismatch did not return bad_state")) return 5;
        if (!expect(bus.all_satisfied(), "mismatch script not fully consumed")) return 6;
    }

    {
        io::device::mock::I2cScriptBus<1, 1, 1> bus{};
        constexpr std::array<util::u8, 1> ignored_id{0x00};
        auto scripted = bus.expect_write_read(kAddress,
                                              whoami_reg,
                                              ignored_id,
                                              io::device::I2cErrorKind::nack_address);
        if (!expect(static_cast<bool>(scripted), "failed to prepare backend failure script")) return 7;

        auto dev_ref = io::device::make_i2c_device_ref(io::device::make_i2c_bus_ref(bus), kAddress);
        driver::i2c::WhoAmIProbe probe{
            dev_ref,
            driver::i2c::WhoAmIProbeConfig{kWhoAmIRegister, kExpectedId}
        };

        auto result = probe.probe();
        if (!expect(!result && result.error() == util::Errc::io,
                    "whoami backend failure did not propagate io error")) return 8;
        if (!expect(bus.all_satisfied(), "backend failure script not fully consumed")) return 9;
    }

    std::puts("i2c whoami probe smoke: ok");
    return 0;
}
