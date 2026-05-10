#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

import io.device_i2c;
import io.device_i2c_facts;
import platform.board;
import platform.board_facts;
import platform.board.stm32_stub;

namespace {
    struct EvidenceFact {
        const char* name{};
        const char* kind{};
        const char* source{};
        const char* role{};
        bool declared{false};
        bool required{false};
        bool provided{false};
    };

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

    template <std::size_t N, typename Predicate>
    bool seen_before(const std::array<EvidenceFact, N>& facts,
                     std::size_t index,
                     const char* name,
                     Predicate predicate) noexcept {
        for (std::size_t i = 0; i < index; ++i) {
            if (predicate(facts[i]) && std::strcmp(facts[i].name, name) == 0) {
                return true;
            }
        }
        return false;
    }

    template <std::size_t N, typename Predicate>
    void write_fact_array(std::FILE* file,
                          const char* field,
                          const std::array<EvidenceFact, N>& facts,
                          Predicate predicate) noexcept {
        std::fprintf(file, "    \"%s\": [", field);
        bool first = true;
        for (std::size_t i = 0; i < facts.size(); ++i) {
            const auto fact = facts[i];
            if (!predicate(fact) || seen_before(facts, i, fact.name, predicate)) {
                continue;
            }
            if (!first) {
                std::fputs(", ", file);
            }
            std::fprintf(file, "\"%s\"", fact.name);
            first = false;
        }
        std::fputs("]", file);
    }

    template <std::size_t N, typename Predicate>
    std::size_t count_unique(const std::array<EvidenceFact, N>& facts,
                             Predicate predicate) noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < facts.size(); ++i) {
            if (predicate(facts[i]) && !seen_before(facts, i, facts[i].name, predicate)) {
                ++count;
            }
        }
        return count;
    }

    template <std::size_t N>
    bool has_provided_fact(const std::array<EvidenceFact, N>& facts,
                           const char* name) noexcept {
        for (auto fact : facts) {
            if (fact.provided && std::strcmp(fact.name, name) == 0) {
                return true;
            }
        }
        return false;
    }

    template <std::size_t N>
    std::size_t count_satisfied_required(
        const std::array<EvidenceFact, N>& facts) noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < facts.size(); ++i) {
            const auto fact = facts[i];
            if (!fact.required || seen_before(facts, i, fact.name, [](EvidenceFact item) {
                    return item.required;
                })) {
                continue;
            }
            if (has_provided_fact(facts, fact.name)) {
                ++count;
            }
        }
        return count;
    }

    template <std::size_t N>
    std::size_t count_missing_required(
        const std::array<EvidenceFact, N>& facts) noexcept {
        const auto required = count_unique(facts, [](EvidenceFact fact) {
            return fact.required;
        });
        const auto satisfied = count_satisfied_required(facts);
        return required >= satisfied ? required - satisfied : 0;
    }

    template <std::size_t N>
    bool write_fact_evidence(const char* path,
                             const std::array<EvidenceFact, N>& facts) noexcept {
        const auto required = count_unique(facts, [](EvidenceFact fact) {
            return fact.required;
        });
        const auto provided = count_unique(facts, [](EvidenceFact fact) {
            return fact.provided;
        });
        const auto satisfied = count_satisfied_required(facts);
        const auto missing = count_missing_required(facts);
        const auto optionalUnknown = count_unique(facts, [](EvidenceFact fact) {
            return !fact.required && !fact.provided;
        });

        auto* file = open_write_binary(path);
        if (file == nullptr) {
            std::fprintf(stderr, "[ERR] cannot open fact evidence path: %s\n", path);
            return false;
        }

        std::fputs("{\n", file);
        std::fputs("  \"schema\": \"system_compiler.fact_evidence/v0\",\n", file);
        std::fputs("  \"source\": \"Examples/platform/board_i2c_fact_composition_smoke\",\n", file);
        std::fputs("  \"producer\": \"platform.board_facts+io.device_i2c_facts\",\n", file);
        std::fputs("  \"summary\": {\n", file);
        std::fprintf(file, "    \"required_count\": %zu,\n", required);
        std::fprintf(file, "    \"provided_count\": %zu,\n", provided);
        std::fprintf(file, "    \"satisfied_count\": %zu,\n", satisfied);
        std::fprintf(file, "    \"missing_count\": %zu,\n", missing);
        std::fprintf(file, "    \"optional_unknown_count\": %zu,\n", optionalUnknown);
        std::fputs("    \"source_count\": 3\n", file);
        std::fputs("  },\n", file);
        std::fputs("  \"facts\": {\n", file);
        write_fact_array(file, "declared_facts", facts, [](EvidenceFact fact) {
            return fact.declared;
        });
        std::fputs(",\n", file);
        write_fact_array(file, "required_facts", facts, [](EvidenceFact fact) {
            return fact.required;
        });
        std::fputs(",\n", file);
        write_fact_array(file, "audit_provided_facts", facts, [](EvidenceFact fact) {
            return fact.provided;
        });
        std::fputs("\n  },\n", file);
        std::fputs("  \"raw_facts\": [\n", file);
        for (std::size_t i = 0; i < facts.size(); ++i) {
            const auto fact = facts[i];
            std::fprintf(file,
                         "    { \"name\": \"%s\", \"kind\": \"%s\", \"source\": \"%s\", \"role\": \"%s\", \"required\": %s, \"state\": \"%s\" }%s\n",
                         fact.name,
                         fact.kind,
                         fact.source,
                         fact.role,
                         fact.required ? "true" : "false",
                         fact.provided ? "provided" : "unknown",
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
    using namespace platform::board;

    constexpr I2cAddress kAddress = 0x18;
    constexpr std::array<I2cFact, 7> contractFacts{
        required_i2c_bus("i2c1", "io.device_i2c.contract"),
        required_i2c_device("i2c1", kAddress, "io.device_i2c.contract"),
        required_i2c_support_fact(I2cFactKind::controller, "hal.i2c1", "io.device_i2c.contract"),
        required_i2c_support_fact(I2cFactKind::clock_domain, "rcc.i2c1", "io.device_i2c.contract"),
        required_i2c_support_fact(I2cFactKind::pinmux, "pb8/pb9.af4", "io.device_i2c.contract"),
        required_i2c_support_fact(I2cFactKind::backend, "io.device_i2c_hal", "io.device_i2c.contract"),
        optional_i2c_fact(I2cFactKind::power_domain, "sensor.vdd", "io.device_i2c.contract"),
    };
    constexpr auto contractResolution = resolve_i2c_facts(contractFacts);
    static_assert(contractResolution.required_count == 6);
    static_assert(contractResolution.missing_count == 6);
    static_assert(to_string(BoardFactKind::i2c_bus) == "i2c.bus");
    static_assert(to_string(BoardFactKind::i2c_device) == "i2c.device");
    static_assert(to_string(BoardFactKind::i2c_controller) == "i2c.controller");

    const auto caps = stm32_stub::make_board_caps();
    const std::array<EvidenceFact, 15> facts{
        EvidenceFact{"board.stm32_stub", "board", "platform.board.stm32_stub", "board_package", true, false, true},
        EvidenceFact{"capability:hal.i2c1", "capability", "platform.board.stm32_stub", "board_package", true, false, true},
        EvidenceFact{"i2c.bus:i2c1", "i2c.bus", "io.device_i2c.contract", "contract_required", true, true, false},
        EvidenceFact{"i2c.device:i2c1@0x18", "i2c.device", "io.device_i2c.contract", "contract_required", true, true, false},
        EvidenceFact{"i2c.controller:hal.i2c1", "i2c.controller", "io.device_i2c.contract", "contract_required", false, true, false},
        EvidenceFact{"clock.domain:rcc.i2c1", "clock.domain", "io.device_i2c.contract", "contract_required", false, true, false},
        EvidenceFact{"pinmux:pb8/pb9.af4", "pinmux", "io.device_i2c.contract", "contract_required", false, true, false},
        EvidenceFact{"i2c.backend:io.device_i2c_hal", "i2c.backend", "io.device_i2c.contract", "contract_required", false, true, false},
        EvidenceFact{"power.domain:sensor.vdd", "power.domain", "io.device_i2c.contract", "contract_optional", false, false, false},
        EvidenceFact{"i2c.bus:i2c1", "i2c.bus", "platform.board.stm32_stub", "board_package", false, false, true},
        EvidenceFact{"i2c.device:i2c1@0x18", "i2c.device", "platform.board.stm32_stub", "board_package", false, false, true},
        EvidenceFact{"i2c.controller:hal.i2c1", "i2c.controller", "platform.board.stm32_stub", "board_package", false, false, true},
        EvidenceFact{"clock.domain:rcc.i2c1", "clock.domain", "platform.board.stm32_stub", "board_package", false, false, true},
        EvidenceFact{"pinmux:pb8/pb9.af4", "pinmux", "platform.board.stm32_stub", "board_package", false, false, true},
        EvidenceFact{"i2c.backend:io.device_i2c_hal", "i2c.backend", "io.device_i2c_hal", "adapter", false, false, true},
    };

    if (!expect(caps.i2c1.handle.ops != nullptr, "stm32_stub i2c1 ops should be present")) return 1;
    if (!expect(std::strcmp(caps.i2c1.hal_cap, "hal.i2c1") == 0, "stm32_stub i2c1 hal cap mismatch")) return 2;
    if (!expect(count_unique(facts, [](EvidenceFact fact) { return fact.required; }) == 6,
                "required fact count mismatch")) return 3;
    if (!expect(count_satisfied_required(facts) == 6,
                "satisfied fact count mismatch")) return 4;
    if (!expect(count_missing_required(facts) == 0,
                "missing fact count mismatch")) return 5;
    if (!expect(count_unique(facts, [](EvidenceFact fact) {
            return !fact.required && !fact.provided;
        }) == 1, "optional unknown fact count mismatch")) return 6;

    const char* factEvidencePath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fact-evidence") == 0) {
            if (i + 1 >= argc) {
                std::fputs("[ERR] --fact-evidence requires a path\n", stderr);
                return 7;
            }
            factEvidencePath = argv[++i];
            continue;
        }
        std::fprintf(stderr, "[ERR] unknown argument: %s\n", argv[i]);
        return 8;
    }

    if (factEvidencePath != nullptr && !write_fact_evidence(factEvidencePath, facts)) {
        return 9;
    }

    std::puts("board i2c fact composition smoke: ok");
    std::printf("required=%zu satisfied=%zu missing=%zu provided=%zu optional_unknown=%zu\n",
                count_unique(facts, [](EvidenceFact fact) { return fact.required; }),
                count_satisfied_required(facts),
                count_missing_required(facts),
                count_unique(facts, [](EvidenceFact fact) { return fact.provided; }),
                count_unique(facts, [](EvidenceFact fact) {
                    return !fact.required && !fact.provided;
                }));
    if (factEvidencePath != nullptr) {
        std::printf("fact_evidence=%s\n", factEvidencePath);
    }
    return 0;
}
