#include "armv7a_runtime_handoff.hpp"

#include "armv7a_handoff_entry.hpp"
#include "armv7a_handoff_launch.hpp"
#include "armv7a_handoff_live.hpp"
#include "armv7a_handoff_transfer.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_handoff_prepare.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_leaf_ports.hpp"

namespace {
Armv7aRuntimeHandoffContract g_last_runtime_handoff{};
bool g_last_runtime_handoff_valid = false;
Armv7aRuntimeHandoffLandingObservation g_last_runtime_handoff_landing{};
Armv7aRuntimeHandoffLeafLandingObservation
    g_last_runtime_handoff_leaf_landing{};
Armv7aRuntimeHandoffBindingLandingObservation
    g_last_runtime_handoff_binding_landing{};
Armv7aRuntimeHandoffSessionLandingObservation
    g_last_runtime_handoff_session_landing{};
Armv7aRuntimeHandoffPackageLandingObservation
    g_last_runtime_handoff_package_landing{};
Armv7aRuntimeHandoffLandingBundleObservation
    g_last_runtime_handoff_landing_bundle{};
Armv7aRuntimeHandoffConsumerObservation g_last_runtime_handoff_consumer{};
Armv7aRuntimeHandoffPathObservation g_last_runtime_handoff_path{};
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

const Armv7aRuntimeHandoffPackageLandingObservation&
armv7a_make_runtime_handoff_package_landing_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeLeafPortsContract& rearmed_ports,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeLiveObservation& runtime_live) noexcept
{
    const auto runtime_live_ready = armv7a_runtime_live_ready(runtime_live);
    const auto leaf_contract =
        armv7a_make_runtime_leaf_bundle(rearmed_ports, runtime_live_ready);
    const auto contract = armv7a_make_runtime_package(leaf_contract);
    const auto handoff_ready = handoff != nullptr &&
        armv7a_runtime_handoff_runtime_ready(*handoff);
    const auto& leaf_landing = armv7a_make_runtime_handoff_leaf_landing_observation(
        handoff,
        rearmed_ports,
        runtime_package,
        runtime_live);
    const auto& binding_landing =
        armv7a_make_runtime_handoff_binding_landing_observation(handoff,
                                                                rearmed_ports,
                                                                runtime_package,
                                                                runtime_live);
    const auto& session_landing =
        armv7a_make_runtime_handoff_session_landing_observation(handoff,
                                                                rearmed_ports,
                                                                runtime_package,
                                                                binding_landing,
                                                                runtime_live);

    g_last_runtime_handoff_package_landing =
        Armv7aRuntimeHandoffPackageLandingObservation{
            .contract = contract,
            .runtime_package = runtime_package,
            .runtime_live = runtime_live,
            .leaf_landing = leaf_landing,
            .binding_landing = binding_landing,
            .session_landing = session_landing,
            .handoff_present = handoff != nullptr,
            .package_from_handoff =
                handoff_ready &&
                armv7a_runtime_package_equal(contract, handoff->runtime),
            .package_recaptured =
                armv7a_runtime_package_observation_ready(runtime_package) &&
                armv7a_runtime_package_equal(contract, runtime_package.contract),
            .runtime_live_consumed =
                armv7a_runtime_package_observation_ready(runtime_package) &&
                runtime_live_ready,
        };
    return g_last_runtime_handoff_package_landing;
}

const Armv7aRuntimeHandoffLeafLandingObservation&
armv7a_make_runtime_handoff_leaf_landing_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeLeafPortsContract& rearmed_ports,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeLiveObservation& runtime_live) noexcept
{
    const auto runtime_live_ready = armv7a_runtime_live_ready(runtime_live);
    const auto contract =
        armv7a_make_runtime_leaf_bundle(rearmed_ports, runtime_live_ready);
    const auto handoff_ready = handoff != nullptr &&
        armv7a_runtime_handoff_runtime_ready(*handoff);

    g_last_runtime_handoff_leaf_landing =
        Armv7aRuntimeHandoffLeafLandingObservation{
            .contract = contract,
            .runtime_live = runtime_live,
            .handoff_present = handoff != nullptr,
            .leaf_from_rearmed_ports =
                armv7a_runtime_leaf_bundle_matches_leaf_ports(
                    contract,
                    rearmed_ports,
                    runtime_live_ready),
            .live_from_runtime = runtime_live_ready,
            .leaf_from_handoff =
                handoff_ready &&
                armv7a_runtime_leaf_bundle_equal(contract, handoff->runtime.leaf),
            .leaf_recaptured =
                armv7a_runtime_package_observation_ready(runtime_package) &&
                armv7a_runtime_leaf_bundle_equal(contract,
                                                runtime_package.contract.leaf),
        };
    return g_last_runtime_handoff_leaf_landing;
}

void armv7a_print_runtime_handoff_leaf_landing_observation(
    const Armv7aRuntimeHandoffLeafLandingObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff leaf landing, ports=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_leaf_landing_ports_ready(observation)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_leaf_landing_live_ready(observation)));
    armv7a_platform_early_console_puts(", handoff=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.handoff_present && observation.leaf_from_handoff));
    armv7a_platform_early_console_puts(", recaptured=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.leaf_recaptured));
    armv7a_platform_early_console_puts(", leaf=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_leaf_landing_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

const Armv7aRuntimeHandoffBindingLandingObservation&
armv7a_make_runtime_handoff_binding_landing_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeLeafPortsContract& rearmed_ports,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeLiveObservation& runtime_live) noexcept
{
    const auto runtime_live_ready = armv7a_runtime_live_ready(runtime_live);
    const auto contract =
        armv7a_make_runtime_binding_bundle(rearmed_ports, runtime_live_ready);
    const auto handoff_ready = handoff != nullptr &&
        armv7a_runtime_handoff_runtime_ready(*handoff);

    g_last_runtime_handoff_binding_landing =
        Armv7aRuntimeHandoffBindingLandingObservation{
            .contract = contract,
            .runtime_live = runtime_live,
            .handoff_present = handoff != nullptr,
            .binding_from_rearmed_ports =
                armv7a_runtime_binding_bundle_matches_leaf_ports(
                    contract,
                    rearmed_ports,
                    runtime_live_ready),
            .shared_runtime_context =
                armv7a_runtime_handoff_binding_shared_runtime_context(
                    contract,
                    rearmed_ports),
            .thread_from_trap_call =
                armv7a_runtime_handoff_binding_thread_from_trap_call(
                    contract,
                    rearmed_ports),
            .binding_from_handoff =
                handoff_ready &&
                armv7a_runtime_binding_bundle_equal(
                    contract,
                    handoff->runtime.binding),
            .binding_recaptured =
                armv7a_runtime_package_observation_ready(runtime_package) &&
                armv7a_runtime_binding_bundle_equal(
                    contract,
                    runtime_package.contract.binding),
        };
    return g_last_runtime_handoff_binding_landing;
}

void armv7a_print_runtime_handoff_binding_landing_observation(
    const Armv7aRuntimeHandoffBindingLandingObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff binding landing, current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_binding_landing_current_ready(observation)));
    armv7a_platform_early_console_puts(", trap=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_binding_landing_trap_ready(observation)));
    armv7a_platform_early_console_puts(", thread=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_binding_landing_thread_ready(observation)));
    armv7a_platform_early_console_puts(", loop=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_binding_landing_loop_ready(observation)));
    armv7a_platform_early_console_puts(", shared=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_binding_landing_shared_ready(observation)));
    armv7a_platform_early_console_puts(", handoff=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.handoff_present && observation.binding_from_handoff));
    armv7a_platform_early_console_puts(", recaptured=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.binding_recaptured));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_binding_landing_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

const Armv7aRuntimeHandoffSessionLandingObservation&
armv7a_make_runtime_handoff_session_landing_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeLeafPortsContract& rearmed_ports,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeHandoffBindingLandingObservation& binding_landing,
    const Armv7aRuntimeLiveObservation& runtime_live) noexcept
{
    g_last_runtime_handoff_session_landing =
        Armv7aRuntimeHandoffSessionLandingObservation{
            .runtime_context = runtime_live.runtime_context,
            .session = runtime_live.session,
            .shared = runtime_live.shared,
            .trap = runtime_live.trap,
            .handoff_present = handoff != nullptr,
            .live_identity_ready =
                armv7a_runtime_live_identity_ready(runtime_live),
            .ports_from_runtime_context =
                armv7a_runtime_handoff_runtime_context_matches_leaf_ports(
                    runtime_live.runtime_context,
                    rearmed_ports),
            .binding_from_runtime_context =
                armv7a_runtime_handoff_runtime_context_matches_binding(
                    runtime_live.runtime_context,
                    binding_landing.contract),
            .package_from_runtime_context =
                armv7a_runtime_package_observation_ready(runtime_package) &&
                armv7a_runtime_handoff_runtime_context_matches_package(
                    runtime_live.runtime_context,
                    runtime_package.contract),
        };
    return g_last_runtime_handoff_session_landing;
}

void armv7a_print_runtime_handoff_session_landing_observation(
    const Armv7aRuntimeHandoffSessionLandingObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff session landing, live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_session_landing_live_ready(observation)));
    armv7a_platform_early_console_puts(", ports=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_session_landing_ports_ready(observation)));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_session_landing_binding_ready(observation)));
    armv7a_platform_early_console_puts(", package=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_session_landing_package_ready(observation)));
    armv7a_platform_early_console_puts(", session=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_session_landing_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_print_runtime_handoff_package_landing_observation(
    const Armv7aRuntimeHandoffPackageLandingObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff package landing, leaf=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_package_landing_leaf_ready(observation)));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_package_landing_binding_ready(observation)));
    armv7a_platform_early_console_puts(", package=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_package_landing_package_ready(observation)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_package_landing_live_ready(observation)));
    armv7a_platform_early_console_puts(", consumer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_package_landing_consumer_ready(observation)));
    armv7a_platform_early_console_puts(", landing=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_package_landing_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

const Armv7aRuntimeHandoffConsumerObservation&
armv7a_make_runtime_handoff_consumer_observation(
    const Armv7aRuntimeLeafPortsContract& rearmed_ports,
    const Armv7aRuntimePackageObservation& runtime_package,
    const Armv7aRuntimeLiveObservation& runtime_live,
    const Armv7aRuntimeHandoffLandingBundleObservation& landing_bundle,
    const Armv7aRuntimeHandoffPathObservation& path,
    bool handoff_present) noexcept
{
    g_last_runtime_handoff_consumer.rearmed_ports = rearmed_ports;
    g_last_runtime_handoff_consumer.runtime_package = runtime_package;
    g_last_runtime_handoff_consumer.runtime_live = runtime_live;
    g_last_runtime_handoff_consumer.landing_bundle =
        armv7a_runtime_handoff_landing_bundle_summary(landing_bundle);
    g_last_runtime_handoff_consumer.path = path;
    g_last_runtime_handoff_consumer.handoff_present = handoff_present;
    return g_last_runtime_handoff_consumer;
}

const Armv7aRuntimeHandoffConsumerObservation&
armv7a_capture_runtime_handoff_consumer_observation(
    const Armv7aRuntimeHandoffContract* handoff) noexcept
{
    const auto package_ready =
        handoff != nullptr &&
        armv7a_runtime_package_ready(handoff->runtime);
    const auto rearmed_ports = package_ready
        ? armv7a_prepare_runtime_leaf_ports()
        : Armv7aRuntimeLeafPortsContract{};
    const auto rearmed_leaf_ready =
        armv7a_runtime_leaf_ports_ready(rearmed_ports);
    const auto payload_ready =
        package_ready &&
        rearmed_leaf_ready &&
        armv7a_runtime_leaf_bundle_matches_leaf_ports(
            handoff->runtime.leaf,
            rearmed_ports,
            handoff->runtime.leaf.runtime_live_ready) &&
        armv7a_runtime_binding_bundle_matches_leaf_ports(
            handoff->runtime.binding,
            rearmed_ports,
            handoff->runtime.leaf.runtime_live_ready);
    const auto runtime_live = payload_ready
        ? armv7a_run_runtime_live_observation(handoff->runtime)
        : Armv7aRuntimeLiveObservation{};
    const auto runtime_package = payload_ready
        ? armv7a_capture_runtime_package_observation()
        : Armv7aRuntimePackageObservation{};
    const auto& package_landing =
        armv7a_make_runtime_handoff_package_landing_observation(handoff,
                                                                rearmed_ports,
                                                                runtime_package,
                                                                runtime_live);
    const auto& landing =
        armv7a_make_runtime_handoff_landing_observation(handoff,
                                                        rearmed_leaf_ready,
                                                        payload_ready,
                                                        runtime_package,
                                                        runtime_live);
    const auto& landing_bundle =
        armv7a_make_runtime_handoff_landing_bundle_observation(handoff,
                                                               package_landing,
                                                               landing);
    const auto& path =
        armv7a_make_runtime_handoff_path_observation(handoff, landing_bundle);
    return armv7a_make_runtime_handoff_consumer_observation(rearmed_ports,
                                                            runtime_package,
                                                            runtime_live,
                                                            landing_bundle,
                                                            path,
                                                            handoff != nullptr);
}

void armv7a_print_runtime_handoff_consumer_observation(
    const Armv7aRuntimeHandoffConsumerObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff consumer, ports=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_consumer_ports_ready(observation)));
    armv7a_platform_early_console_puts(", recaptured=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_consumer_recaptured_ready(observation)));
    armv7a_platform_early_console_puts(", package=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_consumer_package_ready(observation)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_consumer_live_ready(observation)));
    armv7a_platform_early_console_puts(", landing=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_summary_ready(
            observation.landing_bundle)));
    armv7a_platform_early_console_puts(", path=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_consumer_path_ready(observation)));
    armv7a_platform_early_console_puts(", consumer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_consumer_ready(observation)));
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
            armv7a_runtime_package_observation_ready(runtime_package) &&
            armv7a_runtime_package_equal(handoff->runtime,
                                         runtime_package.contract),
        .runtime_live_consumed =
            payload_matches_rearmed_leaf &&
            armv7a_runtime_live_ready(runtime_live),
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

const Armv7aRuntimeHandoffLandingBundleObservation&
armv7a_make_runtime_handoff_landing_bundle_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeHandoffPackageLandingObservation& package_landing,
    const Armv7aRuntimeHandoffLandingObservation& landing) noexcept
{
    g_last_runtime_handoff_landing_bundle =
        Armv7aRuntimeHandoffLandingBundleObservation{
            .package_landing = package_landing,
            .landing = landing,
            .handoff_present = handoff != nullptr,
            .payload_shared =
                armv7a_runtime_package_observation_ready(
                    package_landing.runtime_package) &&
                armv7a_runtime_package_observation_ready(landing.runtime_package) &&
                armv7a_runtime_package_equal(
                    package_landing.runtime_package.contract,
                    landing.runtime_package.contract),
            .live_shared =
                armv7a_runtime_live_ready(package_landing.runtime_live) &&
                armv7a_runtime_live_ready(landing.runtime_live) &&
                armv7a_runtime_live_equal(package_landing.runtime_live,
                                          landing.runtime_live),
        };
    return g_last_runtime_handoff_landing_bundle;
}

void armv7a_print_runtime_handoff_landing_bundle_observation(
    const Armv7aRuntimeHandoffLandingBundleObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff landing bundle, leaf=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_leaf_ready(observation)));
    armv7a_platform_early_console_puts(", binding=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_binding_ready(observation)));
    armv7a_platform_early_console_puts(", session=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_session_ready(observation)));
    armv7a_platform_early_console_puts(", package=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_package_ready(observation)));
    armv7a_platform_early_console_puts(", landing=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_landing_ready(observation)));
    armv7a_platform_early_console_puts(", payload=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_payload_ready(observation)));
    armv7a_platform_early_console_puts(", live=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_live_ready(observation)));
    armv7a_platform_early_console_puts(", bundle=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_handoff_landing_bundle_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

const Armv7aRuntimeHandoffPathObservation&
armv7a_make_runtime_handoff_path_observation(
    const Armv7aRuntimeHandoffContract* handoff,
    const Armv7aRuntimeHandoffLandingBundleObservation& landing_bundle) noexcept
{
    const auto* export_contract = armv7a_runtime_handoff_export();
    const auto export_ready = handoff != nullptr &&
        export_contract != nullptr &&
        export_contract == handoff &&
        armv7a_runtime_handoff_ready(*handoff) &&
        armv7a_runtime_handoff_equal(*export_contract, *handoff);

    const auto entry = armv7a_last_handoff_entry();
    const auto entry_ready = export_ready &&
        armv7a_handoff_entry_ready(entry) &&
        armv7a_runtime_handoff_equal(entry.handoff, *handoff);

    const auto transfer = armv7a_last_handoff_transfer();
    const auto transfer_ready = entry_ready &&
        armv7a_handoff_transfer_ready(transfer) &&
        armv7a_handoff_entry_equal(transfer.entry, entry) &&
        transfer.arg0_handoff == reinterpret_cast<std::uintptr_t>(handoff) &&
        transfer.arg0_size == armv7a_runtime_handoff_export_size();

    const auto launch = armv7a_last_handoff_launch();
    const auto live = armv7a_last_handoff_live_observation();
    const auto launch_transfer_ready = transfer_ready &&
        armv7a_handoff_launch_ready(launch) &&
        armv7a_handoff_entry_equal(launch.transfer.entry, transfer.entry) &&
        launch.transfer.arg0_handoff == transfer.arg0_handoff &&
        launch.transfer.arg0_size == transfer.arg0_size &&
        launch.transfer.expect_arm_state == transfer.expect_arm_state;
    const auto launch_route_ready =
        armv7a_handoff_launch_trampoline_route(launch) &&
        !launch.route.returnable;
    const auto launch_live_ready = armv7a_handoff_live_ready(live) &&
        live.branch_target == launch.transfer.entry.branch_target &&
        live.arg0_handoff == launch.transfer.arg0_handoff &&
        live.entry_stack == launch.transfer.stack_pointer;
    const auto launch_ready = launch_transfer_ready && launch_route_ready &&
        launch_live_ready;

    g_last_runtime_handoff_path = Armv7aRuntimeHandoffPathObservation{
        .export_ready = export_ready,
        .entry_ready = entry_ready,
        .transfer_ready = transfer_ready,
        .launch_ready = launch_ready,
        .landing_ready =
            armv7a_runtime_handoff_landing_bundle_ready(landing_bundle),
    };
    return g_last_runtime_handoff_path;
}

void armv7a_print_runtime_handoff_path_observation(
    const Armv7aRuntimeHandoffPathObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A runtime handoff path, export=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.export_ready));
    armv7a_platform_early_console_puts(", entry=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.entry_ready));
    armv7a_platform_early_console_puts(", transfer=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.transfer_ready));
    armv7a_platform_early_console_puts(", launch=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.launch_ready));
    armv7a_platform_early_console_puts(", landing=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.landing_ready));
    armv7a_platform_early_console_puts(", path=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_runtime_handoff_path_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}
