#pragma once

#include <cstdint>

struct Armv7aHandoffPrepareContext {
    std::uintptr_t vector_base = 0u;
    std::uintptr_t translation_table_base = 0u;
    std::uintptr_t image_load_base = 0u;
};

Armv7aHandoffPrepareContext armv7a_current_handoff_prepare_context();
bool armv7a_handoff_mask_cpu_exceptions(const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_quiesce_interrupt_controller(
    const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_activate_payload_mapping(
    const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_clean_data_cache(const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_invalidate_instruction_cache(
    const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_invalidate_tlb(const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_switch_exception_vectors(
    const Armv7aHandoffPrepareContext& context) noexcept;
bool armv7a_handoff_sync_context(const Armv7aHandoffPrepareContext& context) noexcept;

void armv7a_run_handoff_prepare_dry_run();
