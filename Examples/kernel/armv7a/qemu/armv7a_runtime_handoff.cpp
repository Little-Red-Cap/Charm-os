#include "armv7a_runtime_handoff.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_handoff_prepare.hpp"
#include "armv7a_platform.hpp"

namespace {
Armv7aRuntimeHandoffContract g_last_runtime_handoff{};
bool g_last_runtime_handoff_valid = false;
}

Armv7aRuntimeHandoffContract armv7a_prepare_runtime_handoff() noexcept
{
    const auto runtime = armv7a_prepare_runtime_package();
    const auto context = armv7a_current_handoff_prepare_context();
    const auto prepare = armv7a_make_qemu_handoff_prepare_contract();
    const auto contract =
        armv7a_make_runtime_handoff(runtime, context, prepare);
    g_last_runtime_handoff = contract;
    g_last_runtime_handoff_valid = true;
    return contract;
}

Armv7aRuntimeHandoffContract armv7a_last_runtime_handoff() noexcept
{
    return g_last_runtime_handoff_valid ? g_last_runtime_handoff
                                        : Armv7aRuntimeHandoffContract{};
}

Armv7aRuntimeHandoffObservation armv7a_capture_runtime_handoff_observation(
    const Armv7aHandoffPrepareReport& report) noexcept
{
    const auto contract = armv7a_prepare_runtime_handoff();
    const auto runtime_package = armv7a_capture_runtime_package_observation();
    const auto context = armv7a_current_handoff_prepare_context();
    const auto prepare = armv7a_make_qemu_handoff_prepare_contract();

    return Armv7aRuntimeHandoffObservation{
        .contract = contract,
        .runtime_package = runtime_package,
        .report = report,
        .from_runtime_package =
            armv7a_runtime_package_ready(contract.runtime) &&
            armv7a_runtime_leaf_bundle_equal(contract.runtime.leaf,
                                            runtime_package.contract.leaf) &&
            armv7a_runtime_binding_bundle_equal(contract.runtime.binding,
                                               runtime_package.contract.binding),
        .from_handoff_context = armv7a_handoff_prepare_context_equal(
            contract.context,
            context),
        .from_handoff_prepare = armv7a_handoff_prepare_contract_equal(
            contract.prepare,
            prepare),
        .report_ready = armv7a_handoff_prepare_report_ready(report),
    };
}

void armv7a_print_runtime_handoff_observation(
    const Armv7aHandoffPrepareReport& report)
{
    const auto observation = armv7a_capture_runtime_handoff_observation(report);

    armv7a_platform_early_console_puts("ARMv7-A runtime handoff, runtime=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_runtime_ready(observation.contract) &&
        armv7a_runtime_package_observation_ready(observation.runtime_package)));
    armv7a_platform_early_console_puts(", context=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_context_ready(observation.contract)));
    armv7a_platform_early_console_puts(", hooks=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_prepare_ready(observation.contract)));
    armv7a_platform_early_console_puts(", vector=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_vector_ready(observation.contract) &&
        report.switch_exception_vectors));
    armv7a_platform_early_console_puts(", report=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.report_ready));
    armv7a_platform_early_console_puts(", export=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_export_ready(observation)));
    armv7a_platform_early_console_puts(", handoff=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
