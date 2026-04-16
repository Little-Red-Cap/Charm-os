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

Armv7aHandoffPrepareReport armv7a_run_handoff_prepare(
    const Armv7aHandoffPrepareContext& context,
    const Armv7aHandoffPrepareContract& contract) noexcept;
