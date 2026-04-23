#pragma once

#include <cstdint>

// ARMv7-A leaf targets can share this prepare-stage contract and the execution
// request metadata it needs without pulling in boot/session modules. QEMU
// consumes it today; future Cortex-A targets can plug in their own hook tables
// and reuse the same execution order.
enum class Armv7aHandoffLoadKind : std::uint8_t {
    copy_to_ram = 0,
    xip,
};

struct Armv7aHandoffExecRequest {
    Armv7aHandoffLoadKind kind = Armv7aHandoffLoadKind::copy_to_ram;
    std::uintptr_t payload_base = 0u;
    std::uintptr_t entry_addr = 0u;
    std::uint32_t storage_payload_offset = 0u;
    std::uint32_t storage_entry_offset = 0u;
    std::uint32_t entry_offset = 0u;
    std::uint32_t payload_size = 0u;
    std::uint32_t image_size = 0u;
    std::uint16_t image_flags = 0u;
};

struct Armv7aHandoffPrepareContext {
    Armv7aHandoffExecRequest exec{};
    std::uintptr_t vector_base = 0u;
    std::uintptr_t translation_table_base = 0u;
    std::uintptr_t image_load_base = 0u;
};

struct Armv7aHandoffPrepareHooks {
    void* ctx = nullptr;
    bool (*mask_cpu_exceptions)(void* ctx,
                                const Armv7aHandoffPrepareContext& context) noexcept = nullptr;
    bool (*quiesce_interrupt_controller)(void* ctx,
                                         const Armv7aHandoffPrepareContext& context) noexcept =
        nullptr;
    bool (*activate_payload_mapping)(void* ctx,
                                     const Armv7aHandoffPrepareContext& context) noexcept =
        nullptr;
    bool (*clean_data_cache)(void* ctx,
                             const Armv7aHandoffPrepareContext& context) noexcept = nullptr;
    bool (*invalidate_instruction_cache)(void* ctx,
                                         const Armv7aHandoffPrepareContext& context) noexcept =
        nullptr;
    bool (*invalidate_tlb)(void* ctx,
                           const Armv7aHandoffPrepareContext& context) noexcept = nullptr;
    bool (*switch_exception_vectors)(void* ctx,
                                     const Armv7aHandoffPrepareContext& context) noexcept =
        nullptr;
    bool (*sync_context)(void* ctx,
                         const Armv7aHandoffPrepareContext& context) noexcept = nullptr;
};

struct Armv7aHandoffPreparePolicy {
    bool mask_cpu_exceptions = true;
    bool quiesce_interrupt_controller = true;
    bool activate_payload_mapping = true;
    bool clean_data_cache = true;
    bool invalidate_instruction_cache = true;
    bool invalidate_tlb = true;
    bool switch_exception_vectors = true;
    bool sync_context = true;
};

struct Armv7aHandoffPrepareContract {
    Armv7aHandoffPrepareHooks hooks{};
    Armv7aHandoffPreparePolicy policy{};
};

struct Armv7aHandoffPrepareReport {
    bool mask_cpu_exceptions = true;
    bool quiesce_interrupt_controller = true;
    bool activate_payload_mapping = true;
    bool clean_data_cache = true;
    bool invalidate_instruction_cache = true;
    bool invalidate_tlb = true;
    bool switch_exception_vectors = true;
    bool sync_context = true;

    constexpr explicit operator bool() const noexcept
    {
        return mask_cpu_exceptions && quiesce_interrupt_controller &&
               activate_payload_mapping && clean_data_cache &&
               invalidate_instruction_cache && invalidate_tlb &&
               switch_exception_vectors && sync_context;
    }
};

constexpr bool armv7a_handoff_exec_request_equal(
    const Armv7aHandoffExecRequest& lhs,
    const Armv7aHandoffExecRequest& rhs) noexcept
{
    return lhs.kind == rhs.kind && lhs.payload_base == rhs.payload_base &&
           lhs.entry_addr == rhs.entry_addr &&
           lhs.storage_payload_offset == rhs.storage_payload_offset &&
           lhs.storage_entry_offset == rhs.storage_entry_offset &&
           lhs.entry_offset == rhs.entry_offset &&
           lhs.payload_size == rhs.payload_size &&
           lhs.image_size == rhs.image_size &&
           lhs.image_flags == rhs.image_flags;
}

constexpr bool armv7a_handoff_exec_request_ready(
    const Armv7aHandoffExecRequest& request) noexcept
{
    return request.payload_base != 0u && request.entry_addr != 0u;
}

constexpr bool armv7a_handoff_prepare_context_equal(
    const Armv7aHandoffPrepareContext& lhs,
    const Armv7aHandoffPrepareContext& rhs) noexcept
{
    return armv7a_handoff_exec_request_equal(lhs.exec, rhs.exec) &&
           lhs.vector_base == rhs.vector_base &&
           lhs.translation_table_base == rhs.translation_table_base &&
           lhs.image_load_base == rhs.image_load_base;
}

constexpr bool armv7a_handoff_prepare_context_ready(
    const Armv7aHandoffPrepareContext& context) noexcept
{
    return armv7a_handoff_exec_request_ready(context.exec) &&
           context.vector_base != 0u &&
           context.translation_table_base != 0u &&
           context.image_load_base != 0u;
}

constexpr bool armv7a_handoff_prepare_hooks_equal(
    const Armv7aHandoffPrepareHooks& lhs,
    const Armv7aHandoffPrepareHooks& rhs) noexcept
{
    return lhs.ctx == rhs.ctx &&
           lhs.mask_cpu_exceptions == rhs.mask_cpu_exceptions &&
           lhs.quiesce_interrupt_controller ==
               rhs.quiesce_interrupt_controller &&
           lhs.activate_payload_mapping == rhs.activate_payload_mapping &&
           lhs.clean_data_cache == rhs.clean_data_cache &&
           lhs.invalidate_instruction_cache ==
               rhs.invalidate_instruction_cache &&
           lhs.invalidate_tlb == rhs.invalidate_tlb &&
           lhs.switch_exception_vectors == rhs.switch_exception_vectors &&
           lhs.sync_context == rhs.sync_context;
}

constexpr bool armv7a_handoff_prepare_hooks_ready(
    const Armv7aHandoffPrepareHooks& hooks) noexcept
{
    return hooks.mask_cpu_exceptions != nullptr &&
           hooks.quiesce_interrupt_controller != nullptr &&
           hooks.activate_payload_mapping != nullptr &&
           hooks.clean_data_cache != nullptr &&
           hooks.invalidate_instruction_cache != nullptr &&
           hooks.invalidate_tlb != nullptr &&
           hooks.switch_exception_vectors != nullptr &&
           hooks.sync_context != nullptr;
}

constexpr bool armv7a_handoff_prepare_policy_equal(
    const Armv7aHandoffPreparePolicy& lhs,
    const Armv7aHandoffPreparePolicy& rhs) noexcept
{
    return lhs.mask_cpu_exceptions == rhs.mask_cpu_exceptions &&
           lhs.quiesce_interrupt_controller ==
               rhs.quiesce_interrupt_controller &&
           lhs.activate_payload_mapping == rhs.activate_payload_mapping &&
           lhs.clean_data_cache == rhs.clean_data_cache &&
           lhs.invalidate_instruction_cache ==
               rhs.invalidate_instruction_cache &&
           lhs.invalidate_tlb == rhs.invalidate_tlb &&
           lhs.switch_exception_vectors == rhs.switch_exception_vectors &&
           lhs.sync_context == rhs.sync_context;
}

constexpr bool armv7a_handoff_prepare_contract_equal(
    const Armv7aHandoffPrepareContract& lhs,
    const Armv7aHandoffPrepareContract& rhs) noexcept
{
    return armv7a_handoff_prepare_hooks_equal(lhs.hooks, rhs.hooks) &&
           armv7a_handoff_prepare_policy_equal(lhs.policy, rhs.policy);
}

constexpr bool armv7a_handoff_prepare_contract_ready(
    const Armv7aHandoffPrepareContract& contract) noexcept
{
    return armv7a_handoff_prepare_hooks_ready(contract.hooks);
}

constexpr bool armv7a_handoff_prepare_report_ready(
    const Armv7aHandoffPrepareReport& report) noexcept
{
    return static_cast<bool>(report);
}

Armv7aHandoffPrepareReport armv7a_run_handoff_prepare(
    const Armv7aHandoffPrepareContext& context,
    const Armv7aHandoffPrepareContract& contract) noexcept;
