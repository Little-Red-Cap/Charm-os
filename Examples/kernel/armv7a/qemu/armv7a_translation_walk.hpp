#pragma once

#include "armv7a_translation_decode_contract.hpp"

#include <cstdint>

std::uint32_t armv7a_l1_descriptor_from_ttbr0(std::uint32_t ttbr0, std::uintptr_t virtual_address);
std::uint32_t armv7a_l2_descriptor_from_l1(std::uint32_t l1_descriptor,
                                           std::uintptr_t virtual_address);
