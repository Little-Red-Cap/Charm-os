#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

import platform.board;
import platform.board_facts;
import platform.board.stm32_stub;

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

    bool uses_dot_name(platform::board::BoardFactKind kind) noexcept {
        using namespace platform::board;
        return kind == BoardFactKind::board
            || kind == BoardFactKind::profile
            || kind == BoardFactKind::facet;
    }

    const char* fact_name(platform::board::BoardFact fact) noexcept {
        return fact.name.data();
    }

    const char* fact_source(platform::board::BoardFact fact) noexcept {
        return fact.source.data();
    }

    void write_fact_name(std::FILE* file, platform::board::BoardFact fact) noexcept {
        using namespace platform::board;

        const auto kind = to_string(fact.kind);
        const auto separator = uses_dot_name(fact.kind) ? '.' : ':';
        std::fprintf(file, "\"%.*s%c%.*s\"",
                     static_cast<int>(kind.size()),
                     kind.data(),
                     separator,
                     static_cast<int>(fact.name.size()),
                     fact_name(fact));
    }

    template <std::size_t N, typename Predicate>
    void write_fact_array(std::FILE* file,
                          const char* name,
                          const std::array<platform::board::BoardFact, N>& facts,
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

    const char* state_name(platform::board::BoardFactState state) noexcept {
        using namespace platform::board;

        switch (state) {
        case BoardFactState::provided:
            return "provided";
        case BoardFactState::missing:
            return "missing";
        case BoardFactState::unknown:
        default:
            return "unknown";
        }
    }

    template <std::size_t N>
    bool write_fact_evidence(
        const char* path,
        const std::array<platform::board::BoardFact, N>& facts,
        platform::board::BoardFactResolution resolution) noexcept {
        auto* file = open_write_binary(path);
        if (file == nullptr) {
            std::fprintf(stderr, "[ERR] cannot open fact evidence path: %s\n", path);
            return false;
        }

        using namespace platform::board;
        std::fputs("{\n", file);
        std::fputs("  \"schema\": \"system_compiler.fact_evidence/v0\",\n", file);
        std::fputs("  \"source\": \"Examples/platform/board_facts_smoke\",\n", file);
        std::fputs("  \"producer\": \"platform.board_facts\",\n", file);
        std::fputs("  \"summary\": {\n", file);
        std::fprintf(file, "    \"required_count\": %zu,\n", resolution.required_count);
        std::fprintf(file, "    \"provided_count\": %zu,\n", resolution.provided_count);
        std::fprintf(file, "    \"missing_count\": %zu,\n", resolution.missing_count);
        std::fprintf(file, "    \"optional_unknown_count\": %zu\n", resolution.optional_unknown_count);
        std::fputs("  },\n", file);
        std::fputs("  \"facts\": {\n", file);
        write_fact_array(file, "declared_facts", facts, [](BoardFact fact) {
            return fact.state == BoardFactState::provided;
        });
        std::fputs(",\n", file);
        write_fact_array(file, "required_facts", facts, [](BoardFact fact) {
            return fact.requirement == BoardFactRequirement::required;
        });
        std::fputs(",\n", file);
        write_fact_array(file, "audit_provided_facts", facts, [](BoardFact fact) {
            return fact.state == BoardFactState::provided;
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
                         fact.requirement == BoardFactRequirement::required ? "true" : "false",
                         state_name(fact.state),
                         i + 1 == facts.size() ? "" : ",");
        }
        std::fputs("  ]\n", file);
        std::fputs("}\n", file);
        std::fclose(file);
        return true;
    }
}

int main(int argc, char** argv) {
    using namespace platform::board;

    const auto caps = stm32_stub::make_board_caps();
    constexpr std::string_view kSource = "platform.board.stm32_stub";
    const std::array<BoardFact, 8> facts{
        provided_board_fact(BoardFactKind::board, "stm32_stub", kSource),
        provided_board_fact(BoardFactKind::capability, caps.uart1.io_cap, kSource),
        provided_board_fact(BoardFactKind::capability, caps.uart1.hal_cap, kSource),
        provided_board_fact(BoardFactKind::capability, caps.console_cap, kSource),
        provided_board_fact(BoardFactKind::clock_domain, "system.clock", kSource),
        provided_board_fact(BoardFactKind::capability, caps.input.service_cap, kSource),
        provided_board_fact(BoardFactKind::capability, caps.can0.io_cap, kSource),
        optional_board_fact(BoardFactKind::storage, "block.sd0", kSource),
    };

    const auto resolution = resolve_board_facts(facts);

    if (!expect(caps.uart1.handle.ops != nullptr, "stm32_stub uart1 ops should be present")) return 1;
    if (!expect(caps.clock.now_ms != nullptr, "stm32_stub clock now_ms should be present")) return 2;
    if (!expect(caps.clock.now_us != nullptr, "stm32_stub clock now_us should be present")) return 3;
    if (!expect(caps.input.driver != nullptr, "stm32_stub input driver should be present")) return 4;
    if (!expect(caps.can0.channel != nullptr, "stm32_stub can0 channel should be present")) return 5;
    if (!expect(resolution.required_count == 7, "required fact count mismatch")) return 6;
    if (!expect(resolution.provided_count == 7, "provided fact count mismatch")) return 7;
    if (!expect(resolution.missing_count == 0, "missing fact count mismatch")) return 8;
    if (!expect(resolution.optional_unknown_count == 1, "optional unknown fact count mismatch")) return 9;
    if (!expect(resolution.satisfied(), "board fact resolution should be satisfied")) return 10;

    const char* factEvidencePath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fact-evidence") == 0) {
            if (i + 1 >= argc) {
                std::fputs("[ERR] --fact-evidence requires a path\n", stderr);
                return 11;
            }
            factEvidencePath = argv[++i];
            continue;
        }
        std::fprintf(stderr, "[ERR] unknown argument: %s\n", argv[i]);
        return 12;
    }

    if (factEvidencePath != nullptr && !write_fact_evidence(factEvidencePath, facts, resolution)) {
        return 13;
    }

    std::puts("board facts smoke: ok");
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
