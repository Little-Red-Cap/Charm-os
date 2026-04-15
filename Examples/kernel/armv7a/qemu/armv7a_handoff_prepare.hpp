#pragma once

#include <cstdint>

struct Armv7aHandoffPrepareContext {
    std::uintptr_t vector_base = 0u;
    std::uintptr_t translation_table_base = 0u;
    std::uintptr_t image_load_base = 0u;
};

// This leaf-local contract intentionally mirrors platform.board.armv7a_stub's
// prepare hooks so QEMU can validate the same shape without importing the full
// shared boot/board module stack.
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

Armv7aHandoffPrepareContext armv7a_current_handoff_prepare_context();
Armv7aHandoffPrepareContract armv7a_make_qemu_handoff_prepare_contract() noexcept;
Armv7aHandoffPrepareReport armv7a_run_handoff_prepare(
    const Armv7aHandoffPrepareContext& context,
    const Armv7aHandoffPrepareContract& contract) noexcept;

bool armv7a_handoff_mask_cpu_exceptions(void* ctx,
                                        const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_quiesce_interrupt_controller(
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_activate_payload_mapping(
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_clean_data_cache(void* ctx,
                                     const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_invalidate_instruction_cache(
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_invalidate_tlb(void* ctx,
                                   const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_switch_exception_vectors(
    void* ctx, const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_sync_context(void* ctx,
                                 const Armv7aHandoffPrepareContext& context) noexcept;

void armv7a_run_handoff_prepare_dry_run();
