#include "armv7a_runtime_handoff.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_handoff_prepare.hpp"
#include "armv7a_platform.hpp"

namespace {
Armv7aRuntimeHandoffContract g_last_runtime_handoff{};
bool g_last_runtime_handoff_valid = false;
Armv7aRuntimeHandoffLandingObservation g_last_runtime_handoff_landing{};
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

const Armv7aRuntimeHandoffContract* armv7a_runtime_handoff_export() noexcept
{
    return g_last_runtime_handoff_valid ? &g_last_runtime_handoff : nullptr;
}

std::uint32_t armv7a_runtime_handoff_export_size() noexcept
{
    return static_cast<std::uint32_t>(sizeof(Armv7aRuntimeHandoffContract));
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

const Armv7aRuntimeHandoffLandingObservation&
armv7a_make_runtime_handoff_landing_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    bool rearmed_leaf_ready,
    bool payload_matches_rearmed_leaf,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeLiveObservation& runtime_live) noexcept
{
    const auto handoff_ready = handoff != nullptr &&
        armv7a_runtime_handoff_runtime_ready(*handoff);

    g_last_runtime_handoff_landing = Armv7aRuntimeHandoffLandingObservation{
        .runtime_package = runtime_package,
        .runtime_live = runtime_live,
        .handoff_present = handoff != nullptr,
        .package_ready = handoff_ready,
        .rearmed_leaf_ready = rearmed_leaf_ready,
        .payload_matches_rearmed_leaf = payload_matches_rearmed_leaf,
        .package_from_handoff =
            payload_matches_rearmed_leaf &&
            armv7a_runtime_leaf_bundle_equal(handoff->runtime.leaf,
                                            runtime_package.contract.leaf) &&
            armv7a_runtime_binding_bundle_equal(
                handoff->runtime.binding,
                runtime_package.contract.binding),
        .runtime_live_consumed = payload_matches_rearmed_leaf,
    };
    return g_last_runtime_handoff_landing;
}

void armv7a_print_runtime_handoff_landing_observation(
    const Armv7aRuntimeHandoffLandingObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff landing, package=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_package_ready(observation)));
    armv7a_platform_early_console_puts(", rearm=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_rearm_ready(observation)));
    armv7a_platform_early_console_puts(", payload=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_payload_ready(observation)));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_binding_ready(observation)));
    armv7a_platform_early_console_puts(", current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_current_ready(observation)));
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_trap_ready(observation)));
    armv7a_platform_early_console_puts(", thread=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_thread_ready(observation)));
    armv7a_platform_early_console_puts(", loop=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_loop_ready(observation)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_live_ready(observation)));
    armv7a_platform_early_console_puts(", landed=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
