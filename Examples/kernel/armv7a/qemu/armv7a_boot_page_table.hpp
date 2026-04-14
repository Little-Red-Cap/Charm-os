#pragma once

#include <cstdint>

enum class Armv7aBootSectionType {
    kFault,
    kNormalExecutable,
    kNormalExecuteNever,
    kDeviceData,
};

void armv7a_prepare_boot_identity_map();
void armv7a_boot_l1_map_section(std::uintptr_t virtual_address,
                                std::uintptr_t physical_address,
                                Armv7aBootSectionType type,
                                std::uint32_t domain = 0u);
std::uintptr_t armv7a_boot_l1_table_base();
std::uint32_t armv7a_boot_l1_descriptor(std::uintptr_t virtual_address);
