#include <array>
#include <cstring>
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

    const char* fact_name(io::device::I2cFact fact) noexcept {
        return fact.name.data();
    }

    const char* fact_source(io::device::I2cFact fact) noexcept {
        return fact.source.data();
    }

    void write_fact_name(std::FILE* file, io::device::I2cFact fact) noexcept {
        using namespace io::device;

        if (fact.kind == I2cFactKind::device) {
            std::fprintf(file, "\"%.*s:%.*s@0x%02x\"",
                         static_cast<int>(to_string(fact.kind).size()),
                         to_string(fact.kind).data(),
                         static_cast<int>(fact.name.size()),
                         fact_name(fact),
                         static_cast<unsigned>(fact.address));
            return;
        }

        std::fprintf(file, "\"%.*s:%.*s\"",
                     static_cast<int>(to_string(fact.kind).size()),
                     to_string(fact.kind).data(),
                     static_cast<int>(fact.name.size()),
                     fact_name(fact));
    }

    template <std::size_t N, typename Predicate>
    void write_fact_array(std::FILE* file,
                          const char* name,
                          const std::array<io::device::I2cFact, N>& facts,
                          Predicate predicate) noexcept {
        std::fprintf(file, "    \"%s\": [", name);
        bool first = true;
        for (auto fact : facts) {
            if (!predicate(fact)) {
                continue;
            }
            if (!first) {
                std::fputs(", ", file);
            }
            write_fact_name(file, fact);
            first = false;
        }
        std::fputs("]", file);
    }

    template <std::size_t N>
    bool write_fact_evidence(const char* path,
                             const std::array<io::device::I2cFact, N>& facts,
                             io::device::I2cFactResolution resolution) noexcept {
        auto* file = open_write_binary(path);
        if (file == nullptr) {
            std::fprintf(stderr, "[ERR] cannot open fact evidence path: %s\n", path);
            return false;
        }

        using namespace io::device;
        std::fputs("{\n", file);
        std::fputs("  \"schema\": \"system_compiler.fact_evidence/v0\",\n", file);
        std::fputs("  \"source\": \"Examples/io/i2c_facts_smoke\",\n", file);
        std::fputs("  \"producer\": \"io.device_i2c_facts\",\n", file);
        std::fputs("  \"summary\": {\n", file);
        std::fprintf(file, "    \"required_count\": %zu,\n", resolution.required_count);
        std::fprintf(file, "    \"provided_count\": %zu,\n", resolution.provided_count);
        std::fprintf(file, "    \"missing_count\": %zu,\n", resolution.missing_count);
        std::fprintf(file, "    \"optional_unknown_count\": %zu\n", resolution.optional_unknown_count);
        std::fputs("  },\n", file);
        std::fputs("  \"facts\": {\n", file);
        write_fact_array(file, "declared_facts", facts, [](I2cFact fact) {
            return fact.kind == I2cFactKind::bus || fact.kind == I2cFactKind::device;
        });
        std::fputs(",\n", file);
        write_fact_array(file, "required_facts", facts, [](I2cFact fact) {
            return fact.requirement == I2cFactRequirement::required;
        });
        std::fputs(",\n", file);
        write_fact_array(file, "audit_provided_facts", facts, [](I2cFact fact) {
            return fact.state == I2cFactState::provided;
        });
        std::fputs("\n  },\n", file);
        std::fputs("  \"raw_facts\": [\n", file);
        for (std::size_t i = 0; i < facts.size(); ++i) {
            auto fact = facts[i];
            std::fputs("    { \"name\": ", file);
            write_fact_name(file, fact);
            std::fprintf(file,
                         ", \"kind\": \"%.*s\", \"source\": \"%.*s\", \"required\": %s, \"state\": \"%s\" }%s\n",
                         static_cast<int>(to_string(fact.kind).size()),
                         to_string(fact.kind).data(),
                         static_cast<int>(fact.source.size()),
                         fact_source(fact),
                         fact.requirement == I2cFactRequirement::required ? "true" : "false",
                         fact.state == I2cFactState::provided
                             ? "provided"
                             : (fact.state == I2cFactState::missing ? "missing" : "unknown"),
                         i + 1 == facts.size() ? "" : ",");
        }
        std::fputs("  ]\n", file);
        std::fputs("}\n", file);
        std::fclose(file);
        return true;
    }
}

int main(int argc, char** argv) {
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

    const char* factEvidencePath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fact-evidence") == 0) {
            if (i + 1 >= argc) {
                std::fputs("[ERR] --fact-evidence requires a path\n", stderr);
                return 9;
            }
            factEvidencePath = argv[++i];
            continue;
        }
        std::fprintf(stderr, "[ERR] unknown argument: %s\n", argv[i]);
        return 10;
    }

    if (factEvidencePath != nullptr && !write_fact_evidence(factEvidencePath, facts, resolution)) {
        return 11;
    }

    std::puts("i2c facts smoke: ok");
    std::printf("required=%zu provided=%zu missing=%zu optional_unknown=%zu\n",
                resolution.required_count,
                resolution.provided_count,
                resolution.missing_count,
                resolution.optional_unknown_count);
    if (factEvidencePath != nullptr) {
        std::printf("fact_evidence=%s\n", factEvidencePath);
    }
    return 0;
}
