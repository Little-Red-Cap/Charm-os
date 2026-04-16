#pragma once

#include "armv7a_handoff_contract.hpp"

Armv7aHandoffPrepareContext armv7a_current_handoff_prepare_context();
Armv7aHandoffPrepareContract armv7a_make_qemu_handoff_prepare_contract() noexcept;
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
