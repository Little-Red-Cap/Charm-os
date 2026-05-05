#include <array>
#include <cstdio>
#include <cstring>

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

    std::FILE* open_write_binary(const char* path) noexcept {
#if defined(_WIN32)
        std::FILE* file = nullptr;
        if (fopen_s(&file, path, "wb") != 0) {
            return nullptr;
        }
        return file;
#else
        return std::fopen(path, "wb");
#endif
    }

    bool write_fact_evidence(const char* path) noexcept {
        auto* file = open_write_binary(path);
        if (file == nullptr) {
            std::fprintf(stderr, "[ERR] cannot open fact evidence path: %s\n", path);
            return false;
        }

        std::fputs("{\n", file);
        std::fputs("  \"schema\": \"system_compiler.fact_evidence/v0\",\n", file);
        std::fputs("  \"source\": \"Examples/io/i2c_whoami_probe_smoke\",\n", file);
        std::fputs("  \"producer\": \"driver.i2c_whoami_probe\",\n", file);
        std::fputs("  \"summary\": {\n", file);
        std::fputs("    \"probe_count\": 3,\n", file);
        std::fputs("    \"success_count\": 1,\n", file);
        std::fputs("    \"failure_path_count\": 2,\n", file);
        std::fputs("    \"required_count\": 6,\n", file);
        std::fputs("    \"provided_count\": 5,\n", file);
        std::fputs("    \"missing_count\": 1\n", file);
        std::fputs("  },\n", file);
        std::fputs("  \"facts\": {\n", file);
        std::fputs("    \"declared_facts\": [\"i2c.device:i2c1@0x18\", \"i2c.evidence:whoami_probe\"],\n", file);
        std::fputs("    \"required_facts\": [\"i2c.device:i2c1@0x18\", \"i2c.register:0x0f\", \"i2c.expected_id:0x33\", \"i2c.backend:io.device_i2c_mock\", \"i2c.evidence:whoami_probe\", \"i2c.probe.board_real\"],\n", file);
        std::fputs("    \"audit_provided_facts\": [\"i2c.device:i2c1@0x18\", \"i2c.register:0x0f\", \"i2c.expected_id:0x33\", \"i2c.backend:io.device_i2c_mock\", \"i2c.evidence:whoami_probe\"]\n", file);
        std::fputs("  },\n", file);
        std::fputs("  \"raw_facts\": [\n", file);
        std::fputs("    { \"name\": \"i2c.device:i2c1@0x18\", \"kind\": \"i2c.device\", \"source\": \"driver.i2c_whoami_probe\", \"role\": \"probe_target\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.register:0x0f\", \"kind\": \"i2c.register\", \"source\": \"driver.i2c_whoami_probe\", \"role\": \"probe_register\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.expected_id:0x33\", \"kind\": \"i2c.expected_id\", \"source\": \"driver.i2c_whoami_probe\", \"role\": \"probe_expectation\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.backend:io.device_i2c_mock\", \"kind\": \"i2c.backend\", \"source\": \"io.device_i2c_mock\", \"role\": \"no_hardware_backend\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.evidence:whoami_probe\", \"kind\": \"i2c.evidence\", \"source\": \"Examples/io/i2c_whoami_probe_smoke\", \"role\": \"no_hardware_probe_evidence\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.probe.board_real\", \"kind\": \"i2c.probe\", \"source\": \"board.bringup\", \"role\": \"real_board_probe_gap\", \"required\": true, \"state\": \"missing\" }\n", file);
        std::fputs("  ]\n", file);
        std::fputs("}\n", file);
        std::fclose(file);
        return true;
    }
}

int main(int argc, char** argv) {
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

    const char* factEvidencePath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fact-evidence") == 0) {
            if (i + 1 >= argc) {
                std::fputs("[ERR] --fact-evidence requires a path\n", stderr);
                return 10;
            }
            factEvidencePath = argv[++i];
            continue;
        }
        std::fprintf(stderr, "[ERR] unknown argument: %s\n", argv[i]);
        return 11;
    }

    if (factEvidencePath != nullptr && !write_fact_evidence(factEvidencePath)) {
        return 12;
    }

    std::puts("i2c whoami probe smoke: ok");
    if (factEvidencePath != nullptr) {
        std::printf("fact_evidence=%s\n", factEvidencePath);
    }
    return 0;
}
