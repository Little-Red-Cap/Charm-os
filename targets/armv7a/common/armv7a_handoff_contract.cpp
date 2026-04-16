#include "armv7a_handoff_contract.hpp"

namespace {
void* select_hook_ctx(void* default_ctx, void* hook_ctx) noexcept
{
    return hook_ctx ? hook_ctx : default_ctx;
}

bool invoke_prepare_step(bool enabled,
                         bool (*fn)(void*, const Armv7aHandoffPrepareContext&) noexcept,
                         void* default_ctx,
                         void* hook_ctx,
                         const Armv7aHandoffPrepareContext& context) noexcept
{
    if (!enabled || !fn) {
        return true;
    }

    return fn(select_hook_ctx(default_ctx, hook_ctx), context);
}
} // namespace

Armv7aHandoffPrepareReport armv7a_run_handoff_prepare(
    const Armv7aHandoffPrepareContext& context,
    const Armv7aHandoffPrepareContract& contract) noexcept
{
    Armv7aHandoffPrepareReport report{};
    const auto& hooks = contract.hooks;
    const auto& policy = contract.policy;

    report.mask_cpu_exceptions = invoke_prepare_step(policy.mask_cpu_exceptions,
                                                     hooks.mask_cpu_exceptions,
                                                     nullptr,
                                                     hooks.ctx,
                                                     context);
    if (!report.mask_cpu_exceptions) {
        return report;
    }

    report.quiesce_interrupt_controller =
        invoke_prepare_step(policy.quiesce_interrupt_controller,
                            hooks.quiesce_interrupt_controller,
                            nullptr,
                            hooks.ctx,
                            context);
    if (!report.quiesce_interrupt_controller) {
        return report;
    }

    report.activate_payload_mapping = invoke_prepare_step(policy.activate_payload_mapping,
                                                          hooks.activate_payload_mapping,
                                                          nullptr,
                                                          hooks.ctx,
                                                          context);
    if (!report.activate_payload_mapping) {
        return report;
    }

    report.clean_data_cache = invoke_prepare_step(policy.clean_data_cache,
                                                  hooks.clean_data_cache,
                                                  nullptr,
                                                  hooks.ctx,
                                                  context);
    if (!report.clean_data_cache) {
        return report;
    }

    report.invalidate_instruction_cache =
        invoke_prepare_step(policy.invalidate_instruction_cache,
                            hooks.invalidate_instruction_cache,
                            nullptr,
                            hooks.ctx,
                            context);
    if (!report.invalidate_instruction_cache) {
        return report;
    }

    report.invalidate_tlb = invoke_prepare_step(policy.invalidate_tlb,
                                                hooks.invalidate_tlb,
                                                nullptr,
                                                hooks.ctx,
                                                context);
    if (!report.invalidate_tlb) {
        return report;
    }

    report.switch_exception_vectors = invoke_prepare_step(policy.switch_exception_vectors,
                                                          hooks.switch_exception_vectors,
                                                          nullptr,
                                                          hooks.ctx,
                                                          context);
    if (!report.switch_exception_vectors) {
        return report;
    }

    report.sync_context = invoke_prepare_step(policy.sync_context,
                                              hooks.sync_context,
                                              nullptr,
                                              hooks.ctx,
                                              context);
    return report;
}
