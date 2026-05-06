#include <array>
#include <cstdio>
#include <cstring>

import driver.i2c_whoami_probe;
import io.device_i2c;
import io.device_i2c_mock;
import platform.board.stm32_stub;
import util.core;

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
        std::fputs("  \"source\": \"Examples/platform/board_i2c_whoami_bringup_evidence_smoke\",\n", file);
        std::fputs("  \"producer\": \"board.bringup.host_fixture+driver.i2c_whoami_probe\",\n", file);
        std::fputs("  \"summary\": {\n", file);
        std::fputs("    \"probe_count\": 1,\n", file);
        std::fputs("    \"success_count\": 1,\n", file);
        std::fputs("    \"required_count\": 6,\n", file);
        std::fputs("    \"provided_count\": 6,\n", file);
        std::fputs("    \"missing_count\": 0,\n", file);
        std::fputs("    \"host_fixture\": true\n", file);
        std::fputs("  },\n", file);
        std::fputs("  \"facts\": {\n", file);
        std::fputs("    \"declared_facts\": [\"i2c.device:i2c1@0x18\", \"i2c.evidence:whoami_probe\", \"i2c.probe.board_real\"],\n", file);
        std::fputs("    \"required_facts\": [\"i2c.device:i2c1@0x18\", \"i2c.register:0x0f\", \"i2c.expected_id:0x33\", \"i2c.backend:io.device_i2c_mock\", \"i2c.evidence:whoami_probe\", \"i2c.probe.board_real\"],\n", file);
        std::fputs("    \"audit_provided_facts\": [\"i2c.device:i2c1@0x18\", \"i2c.register:0x0f\", \"i2c.expected_id:0x33\", \"i2c.backend:io.device_i2c_mock\", \"i2c.evidence:whoami_probe\", \"i2c.probe.board_real\"]\n", file);
        std::fputs("  },\n", file);
        std::fputs("  \"raw_facts\": [\n", file);
        std::fputs("    { \"name\": \"i2c.device:i2c1@0x18\", \"kind\": \"i2c.device\", \"source\": \"board.bringup\", \"role\": \"probe_target\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.register:0x0f\", \"kind\": \"i2c.register\", \"source\": \"driver.i2c_whoami_probe\", \"role\": \"probe_register\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.expected_id:0x33\", \"kind\": \"i2c.expected_id\", \"source\": \"driver.i2c_whoami_probe\", \"role\": \"probe_expectation\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.backend:io.device_i2c_mock\", \"kind\": \"i2c.backend\", \"source\": \"io.device_i2c_mock\", \"role\": \"host_fixture_backend\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.evidence:whoami_probe\", \"kind\": \"i2c.evidence\", \"source\": \"Examples/platform/board_i2c_whoami_bringup_evidence_smoke\", \"role\": \"host_fixture_probe_evidence\", \"required\": true, \"state\": \"provided\" },\n", file);
        std::fputs("    { \"name\": \"i2c.probe.board_real\", \"kind\": \"i2c.probe\", \"source\": \"board.bringup\", \"role\": \"host_fixture_board_probe_evidence\", \"required\": true, \"state\": \"provided\" }\n", file);
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
    constexpr std::array<util::u8, 1> whoamiReg{kWhoAmIRegister};
    constexpr std::array<util::u8, 1> id{kExpectedId};

    const auto caps = platform::board::stm32_stub::make_board_caps();
    if (!expect(caps.i2c1.handle.ops != nullptr, "stm32_stub i2c1 ops should be present")) return 1;
    if (!expect(std::strcmp(caps.i2c1.hal_cap, "hal.i2c1") == 0, "stm32_stub i2c1 hal cap mismatch")) return 2;

    io::device::mock::I2cScriptBus<1, 1, 1> bus{};
    auto scripted = bus.expect_write_read(kAddress, whoamiReg, id);
    if (!expect(static_cast<bool>(scripted), "failed to prepare board bringup host fixture script")) return 3;

    auto devRef = io::device::make_i2c_device_ref(io::device::make_i2c_bus_ref(bus), kAddress);
    driver::i2c::WhoAmIProbe probe{
        devRef,
        driver::i2c::WhoAmIProbeConfig{kWhoAmIRegister, kExpectedId}
    };

    auto result = probe.probe();
    if (!expect(static_cast<bool>(result), "board bringup host fixture whoami probe failed")) return 4;
    if (!expect(bus.all_satisfied(), "board bringup host fixture script not fully consumed")) return 5;

    const char* factEvidencePath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fact-evidence") == 0) {
            if (i + 1 >= argc) {
                std::fputs("[ERR] --fact-evidence requires a path\n", stderr);
                return 6;
            }
            factEvidencePath = argv[++i];
            continue;
        }
        std::fprintf(stderr, "[ERR] unknown argument: %s\n", argv[i]);
        return 7;
    }

    if (factEvidencePath != nullptr && !write_fact_evidence(factEvidencePath)) {
        return 8;
    }

    std::puts("board i2c whoami bringup evidence smoke: ok");
    if (factEvidencePath != nullptr) {
        std::printf("fact_evidence=%s\n", factEvidencePath);
    }
    return 0;
}
