#pragma once

#include <cstdint>

enum class Armv7aBootSectionType {
    kFault,
    kNormalExecutable,
    kNormalExecuteNever,
    kNormalNoAccessExecuteNever,
    kDeviceData,
};

enum class Armv7aBootSmallPageType {
    kFault,
    kNormalExecutable,
    kNormalExecuteNever,
    kNormalNoAccessExecuteNever,
    kDeviceData,
};

void armv7a_prepare_boot_identity_map();
void armv7a_boot_l1_map_section(std::uintptr_t virtual_address,
                                std::uintptr_t physical_address,
                                Armv7aBootSectionType type,
                                std::uint32_t domain = 0u);
void armv7a_boot_l2_prepare_table(std::uintptr_t virtual_address,
                                  std::uint32_t domain = 0u);
void armv7a_boot_l2_map_small_page(std::uintptr_t virtual_address,
                                   std::uintptr_t physical_address,
                                   Armv7aBootSmallPageType type,
                                   std::uint32_t domain = 0u);
std::uintptr_t armv7a_boot_l1_table_base();
std::uintptr_t armv7a_boot_l1_descriptor_address(std::uintptr_t virtual_address);
std::uintptr_t armv7a_boot_l2_descriptor_address(std::uintptr_t virtual_address);
std::uint32_t armv7a_boot_l1_descriptor(std::uintptr_t virtual_address);
std::uint32_t armv7a_boot_l2_descriptor(std::uintptr_t virtual_address);
