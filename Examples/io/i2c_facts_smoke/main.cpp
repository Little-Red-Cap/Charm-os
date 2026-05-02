#include <array>
#include <cstdio>
#include <string_view>

import io.device_i2c;
import io.device_i2c_facts;
import util.core;

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
    using namespace io::device;

    constexpr I2cAddress kAddress = 0x18;
    constexpr std::array<I2cFact, 7> facts{
        provided_i2c_bus("i2c1", "board.hqzy"),
        provided_i2c_device("i2c1", kAddress, "board.hqzy"),
        provided_i2c_support_fact(I2cFactKind::controller, "hal.i2c1", "init.graph"),
        provided_i2c_support_fact(I2cFactKind::clock_domain, "rcc.i2c1", "board.hqzy"),
        required_i2c_support_fact(I2cFactKind::pinmux, "pb8/pb9.af4", "board.hqzy"),
        optional_i2c_fact(I2cFactKind::power_domain, "sensor.vdd", "board.hqzy"),
        provided_i2c_support_fact(I2cFactKind::backend, "io.device_i2c_hal", "adapter"),
    };

    constexpr auto resolution = resolve_i2c_facts(facts);
    static_assert(resolution.required_count == 6);
    static_assert(resolution.provided_count == 5);
    static_assert(resolution.missing_count == 1);
    static_assert(resolution.optional_unknown_count == 1);
    static_assert(!resolution.satisfied());
    static_assert(to_string(I2cFactKind::device) == "i2c.device");

    if (!expect(resolution.required_count == 6, "required fact count mismatch")) return 1;
    if (!expect(resolution.provided_count == 5, "provided fact count mismatch")) return 2;
    if (!expect(resolution.missing_count == 1, "missing fact count mismatch")) return 3;
    if (!expect(resolution.optional_unknown_count == 1, "optional unknown fact count mismatch")) return 4;
    if (!expect(!resolution.satisfied(), "resolution should not be satisfied")) return 5;
    if (!expect(is_missing(facts[4]), "pinmux fact should be missing")) return 6;
    if (!expect(!is_missing(facts[5]), "optional power fact should not block")) return 7;
    if (!expect(facts[1].address == kAddress, "device address fact mismatch")) return 8;

    std::puts("i2c facts smoke: ok");
    std::printf("required=%zu provided=%zu missing=%zu optional_unknown=%zu\n",
                resolution.required_count,
                resolution.provided_count,
                resolution.missing_count,
                resolution.optional_unknown_count);
    return 0;
}
