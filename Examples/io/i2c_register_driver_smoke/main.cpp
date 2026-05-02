#include <array>
#include <cstdio>
#include <span>

import driver.i2c_register_device;
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
    io::device::mock::I2cScriptBus<5, 12, 8> bus{};

    constexpr std::array<util::u8, 1> whoami_reg{0x0F};
    constexpr std::array<util::u8, 1> whoami_value{0x33};
    constexpr std::array<util::u8, 2> ctrl_write{0x20, 0x57};
    constexpr std::array<util::u8, 1> burst_start{0x28};
    constexpr std::array<util::u8, 3> burst_value{0x11, 0x22, 0x33};
    constexpr std::array<util::u8, 4> burst_write{0x30, 0x01, 0x02, 0x03};

    auto expect_id = bus.expect_write_read(kAddress, whoami_reg, whoami_value);
    auto expect_cfg = bus.expect_write(kAddress, ctrl_write);
    auto expect_burst_read = bus.expect_write_read(kAddress, burst_start, burst_value);
    auto expect_burst_write = bus.expect_write(kAddress, burst_write);
    if (!expect(expect_id && expect_cfg && expect_burst_read && expect_burst_write,
                "failed to prepare i2c script")) return 1;

    auto dev_ref = io::device::make_i2c_device_ref(io::device::make_i2c_bus_ref(bus), kAddress);
    driver::i2c::RegisterDevice8 regs{dev_ref};

    auto id = regs.read_u8(0x0F);
    if (!expect(id && id.value() == 0x33, "register driver read_u8 mismatch")) return 2;

    auto configured = regs.write_u8(0x20, 0x57);
    if (!expect(static_cast<bool>(configured), "register driver write_u8 failed")) return 3;

    std::array<util::u8, 3> rx{};
    auto burst_read = regs.read(0x28, rx);
    if (!expect(static_cast<bool>(burst_read), "register driver burst read failed")) return 4;
    if (!expect(rx[0] == 0x11 && rx[1] == 0x22 && rx[2] == 0x33,
                "register driver burst read mismatch")) return 5;

    constexpr std::array<util::u8, 3> payload{0x01, 0x02, 0x03};
    auto burst_write_result = regs.write(0x30, payload);
    if (!expect(static_cast<bool>(burst_write_result), "register driver burst write failed")) return 6;
    if (!expect(bus.all_satisfied(), "register driver script not fully consumed")) return 7;

    std::array<util::u8, 9> oversized{};
    auto too_large = regs.write(0x40, oversized);
    if (!expect(!too_large && too_large.error() == util::Errc::buffer_overflow,
                "oversized write did not fail with buffer_overflow")) return 8;
    if (!expect(bus.all_satisfied(), "oversized write touched backend script")) return 9;

    std::puts("i2c register driver smoke: ok");
    std::printf("transactions=%zu\n", bus.consumed_count());
    return 0;
}
