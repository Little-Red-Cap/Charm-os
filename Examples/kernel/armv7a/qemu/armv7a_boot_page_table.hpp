#pragma once

#include <cstdint>

void armv7a_prepare_boot_identity_map();
std::uintptr_t armv7a_boot_l1_table_base();
std::uint32_t armv7a_boot_l1_descriptor(std::uintptr_t virtual_address);
