#pragma once

#include <cstdint>

enum class Armv7aL1DescriptorKind : std::uint32_t {
    kFault,
    kPageTable,
    kSection,
    kSupersection,
    kReserved,
};

enum class Armv7aMemoryType : std::uint32_t {
    kUnknown,
    kStronglyOrdered,
    kDevice,
    kNormalCached,
};

struct Armv7aL1DescriptorDecode {
    std::uint32_t index;
    std::uint32_t descriptor;
    Armv7aL1DescriptorKind kind;
    std::uint32_t table_base;
    std::uint32_t domain;
    std::uint32_t tex;
    std::uint32_t access_permission;
    Armv7aMemoryType memory_type;
    bool execute_never;
    bool shareable;
    bool cacheable;
    bool bufferable;
};

enum class Armv7aL2DescriptorKind : std::uint32_t {
    kFault,
    kLargePage,
    kSmallPage,
    kReserved,
};

struct Armv7aL2DescriptorDecode {
    std::uint32_t index;
    std::uint32_t descriptor;
    Armv7aL2DescriptorKind kind;
    std::uint32_t physical_base;
    std::uint32_t tex;
    std::uint32_t access_permission;
    Armv7aMemoryType memory_type;
    bool execute_never;
    bool shareable;
    bool cacheable;
    bool bufferable;
};

std::uint32_t armv7a_l1_descriptor_from_ttbr0(std::uint32_t ttbr0, std::uintptr_t virtual_address);
Armv7aL1DescriptorDecode armv7a_decode_l1_descriptor(std::uintptr_t virtual_address,
                                                     std::uint32_t descriptor);
const char* armv7a_l1_descriptor_kind_name(Armv7aL1DescriptorKind kind);
std::uint32_t armv7a_l2_descriptor_from_l1(std::uint32_t l1_descriptor,
                                           std::uintptr_t virtual_address);
Armv7aL2DescriptorDecode armv7a_decode_l2_descriptor(std::uintptr_t virtual_address,
                                                     std::uint32_t descriptor);
const char* armv7a_l2_descriptor_kind_name(Armv7aL2DescriptorKind kind);
const char* armv7a_memory_type_name(Armv7aMemoryType type);
