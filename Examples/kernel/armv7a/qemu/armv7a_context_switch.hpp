#pragma once

#include "armv7a_thread_context_contract.hpp"

#include <cstdint>

extern "C" bool armv7a_context_switch(std::uintptr_t* outgoing_sp,
                                      std::uintptr_t incoming_sp);

std::uintptr_t armv7a_prepare_cooperative_thread_context(
    std::uintptr_t stack_top,
    std::uintptr_t entry_addr,
    std::uintptr_t argument) noexcept;

Armv7aThreadContextFrameObservation armv7a_make_thread_context_observation(
    std::uintptr_t stack_base,
    std::uintptr_t stack_top,
    std::uintptr_t prepared_sp,
    std::uintptr_t entry_addr,
    std::uintptr_t argument) noexcept;

void armv7a_print_thread_context_frame(
    const Armv7aThreadContextFrameObservation& observation);
