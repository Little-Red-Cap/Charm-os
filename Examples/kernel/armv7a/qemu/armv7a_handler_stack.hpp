#pragma once

#include <cstdint>

void armv7a_print_handler_stack_evidence(const char* vector_tag, std::uint32_t current_cpsr);
void armv7a_print_return_state_evidence(const char* vector_tag,
                                        std::uint32_t origin_psr,
                                        std::uint32_t current_cpsr);
