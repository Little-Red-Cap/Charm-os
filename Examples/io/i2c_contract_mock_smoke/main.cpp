#include <array>
#include <cstdio>
#include <span>

import io.device_i2c;
import io.device_i2c_mock;
import util.core;
import util.error;

namespace {
    class RegisterProbe {
    public:
        explicit constexpr RegisterProbe(io::device::I2cDeviceRef dev) noexcept
            : dev_(dev) {}

        [[nodiscard]] util::Result<util::u8> read_reg(util::u8 reg) noexcept {
            std::array<util::u8, 1> tx{reg};
            std::array<util::u8, 1> rx{};
            auto result = dev_.write_read(tx, rx);
            if (!result) {
                return util::unexpected(result.error());
            }
            return rx[0];
        }

        [[nodiscard]] util::Result<void> write_reg(util::u8 reg, util::u8 value) noexcept {
            std::array<util::u8, 2> tx{reg, value};
            return dev_.write(tx);
        }

    private:
        io::device::I2cDeviceRef dev_{};
    };

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    io::device::mock::I2cScriptBus<4, 8, 8> bus{};

    constexpr io::device::I2cAddress kAddress = 0x18;
    constexpr std::array<util::u8, 1> whoami_reg{0x0F};
    constexpr std::array<util::u8, 1> whoami_value{0x33};
    constexpr std::array<util::u8, 2> ctrl_write{0x20, 0x57};

    auto expect_id = bus.expect_write_read(kAddress, whoami_reg, whoami_value);
    auto expect_cfg = bus.expect_write(kAddress, ctrl_write);
    if (!expect(expect_id && expect_cfg, "failed to prepare i2c script")) return 1;

    auto ref = io::device::make_i2c_bus_ref(bus);
    auto dev = io::device::make_i2c_device_ref(ref, kAddress);
    RegisterProbe probe{dev};

    auto id = probe.read_reg(0x0F);
    if (!expect(id && id.value() == 0x33, "register read mismatch")) return 2;

    auto configured = probe.write_reg(0x20, 0x57);
    if (!expect(static_cast<bool>(configured), "register write failed")) return 3;
    if (!expect(bus.all_satisfied(), "i2c script not fully consumed")) return 4;

    std::array<util::u8, 1> rx{};
    auto extra = ref.read(kAddress, rx);
    if (!expect(!extra && extra.error() == util::Errc::noent, "unexpected transaction did not fail")) return 5;
    if (!expect(bus.first_script_error() == util::Errc::noent, "first mock script error mismatch")) return 6;

    io::device::mock::I2cScriptBus<1, 8, 8> detached_bus{};
    auto expect_detached = detached_bus.expect_write(
        kAddress,
        ctrl_write,
        io::device::I2cErrorKind::target_detached);
    if (!expect(static_cast<bool>(expect_detached), "failed to prepare detached script")) return 7;

    RegisterProbe detached_probe{
        io::device::make_i2c_device_ref(io::device::make_i2c_bus_ref(detached_bus), kAddress)
    };
    auto detached = detached_probe.write_reg(0x20, 0x57);
    if (!expect(!detached && detached.error() == util::Errc::noent, "expected detached error mismatch")) return 8;
    if (!expect(detached_bus.all_satisfied(), "expected failure did not satisfy script")) return 9;
    if (!expect(detached_bus.first_script_error() == util::Errc::ok, "expected failure marked script error")) return 10;

    std::puts("i2c contract mock smoke: ok");
    std::printf("transactions=%zu\n", bus.consumed_count());
    return 0;
}
