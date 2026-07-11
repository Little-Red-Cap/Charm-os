#include "Backends/contract/backend_evidence.hpp"

#include <array>
#include <cstdio>

namespace be = charm::backend::contract;

namespace {
    constexpr std::array host_exports{
        be::CapabilityExport{
            .capability_name = "TextSink",
            .requirement_role = "log",
            .provider_instance = "host.buffered_console",
            .provider_type = "host buffered console provider",
            .runtime_domain = "host_process",
            .adapter = "host_memory_console_adapter",
        },
        be::CapabilityExport{
            .capability_name = "BlockDevice",
            .requirement_role = "app_store",
            .provider_instance = "host.memory_block_app_store",
            .provider_type = "host memory block provider",
            .runtime_domain = "host_process",
            .adapter = "host_memory_block_adapter",
        },
    };

    constexpr std::array host_bindings{
        be::BindingEvidence{
            .capability_name = "TextSink",
            .requirement_role = "log",
            .provider_instance = "host.buffered_console",
            .selection = "explicit_binding",
        },
        be::BindingEvidence{
            .capability_name = "BlockDevice",
            .requirement_role = "app_store",
            .provider_instance = "host.memory_block_app_store",
            .selection = "explicit_binding",
        },
    };

    constexpr std::array host_facts{
        be::BackendFact{
            .kind = "console",
            .name = "stdout",
            .requirement = be::FactRequirement::required,
            .state = be::FactState::provided,
            .source = "host smoke",
        },
        be::BackendFact{
            .kind = "storage",
            .name = "memory_block",
            .requirement = be::FactRequirement::required,
            .state = be::FactState::provided,
            .source = "host smoke",
        },
        be::BackendFact{
            .kind = "input",
            .name = "scripted_line",
            .requirement = be::FactRequirement::optional,
            .state = be::FactState::unknown,
            .source = "optional test input",
        },
    };

    constexpr std::array board_facts{
        be::BackendFact{
            .kind = "console",
            .name = "usart1",
            .requirement = be::FactRequirement::required,
            .state = be::FactState::provided,
            .source = "board status line",
        },
        be::BackendFact{
            .kind = "storage",
            .name = "emmc",
            .requirement = be::FactRequirement::required,
            .state = be::FactState::missing,
            .source = "board capture",
        },
        be::BackendFact{
            .kind = "clock",
            .name = "system_clock",
            .requirement = be::FactRequirement::required,
            .state = be::FactState::unknown,
            .source = "pending board evidence",
        },
    };

    static_assert(be::count_required_facts(host_facts) == 2U);
    static_assert(be::count_provided_facts(host_facts) == 2U);
    static_assert(be::count_missing_required_facts(host_facts) == 0U);
    static_assert(be::count_unknown_required_facts(host_facts) == 0U);
    static_assert(be::required_facts_ready(host_facts));

    static_assert(be::count_required_facts(board_facts) == 3U);
    static_assert(be::count_provided_facts(board_facts) == 1U);
    static_assert(be::count_missing_required_facts(board_facts) == 1U);
    static_assert(be::count_unknown_required_facts(board_facts) == 1U);
    static_assert(!be::required_facts_ready(board_facts));

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        const be::BackendEvidenceView host_view{
            .identity = be::BackendIdentity{
                .kind = be::BackendKind::host,
                .name = "win32",
                .architecture = "x64",
                .status = be::BackendStatus::prototype,
            },
            .capability_exports = host_exports,
            .selected_bindings = host_bindings,
            .facts = host_facts,
        };

        const be::BackendEvidenceView board_view{
            .identity = be::BackendIdentity{
                .kind = be::BackendKind::board,
                .name = "h747",
                .architecture = "cortex-m7",
                .status = be::BackendStatus::landing,
            },
            .facts = board_facts,
        };

        const auto host_summary = be::summarize_backend_evidence(host_view);
        const auto board_summary = be::summarize_backend_evidence(board_view);

        bool ok = true;
        ok &= expect(host_view.identity.kind == be::BackendKind::host, "host identity should keep backend kind");
        ok &= expect(host_summary.capability_export_count == 2U, "summary should count capability exports");
        ok &= expect(host_summary.selected_binding_count == 2U, "summary should count selected bindings");
        ok &= expect(host_summary.required_fact_count == 2U, "summary should count required facts");
        ok &= expect(host_summary.provided_fact_count == 2U, "summary should count provided facts");
        ok &= expect(host_summary.missing_required_fact_count == 0U, "host should have no missing required facts");
        ok &= expect(host_summary.unknown_required_fact_count == 0U, "host should have no unknown required facts");
        ok &= expect(host_summary.required_facts_are_ready, "host required facts should be ready");

        ok &= expect(board_view.identity.kind == be::BackendKind::board, "board identity should keep backend kind");
        ok &= expect(board_summary.required_fact_count == 3U, "board summary should count required facts");
        ok &= expect(board_summary.provided_fact_count == 1U, "board summary should count provided facts");
        ok &= expect(board_summary.missing_required_fact_count == 1U, "board summary should count missing required facts");
        ok &= expect(board_summary.unknown_required_fact_count == 1U, "board summary should count unknown required facts");
        ok &= expect(!board_summary.required_facts_are_ready, "board required facts should not be ready with missing/unknown facts");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-contract-evidence-header-smoke] ok");
    return 0;
}
