#pragma once

#include <cstdint>

#include "armv7a_vector_entry_contract.hpp"

void armv7a_print_handler_stack_evidence(const char* vector_tag, std::uint32_t current_cpsr);
void armv7a_print_return_state_evidence(const char* vector_tag,
                                        const Armv7aVectorEntryObservation& entry,
                                        std::uint32_t current_cpsr);
