#pragma once

#include <cstdint>

#include "armv7a_exception_contract.hpp"
#include "armv7a_fault_status_contract.hpp"
#include "armv7a_translation_decode_contract.hpp"

struct Armv7aFaultRegistersSnapshot {
    std::uint32_t syndrome = 0u;
    std::uint32_t fault_address = 0u;
    std::uint32_t aux_syndrome = 0u;
    Armv7aFaultStatusDecode decode{};
};

struct Armv7aFaultMapSnapshot {
    std::uint32_t fault_address = 0u;
    std::uint32_t ttbr0 = 0u;
    Armv7aL1DescriptorDecode l1{};
    std::uint32_t l2_descriptor = 0u;
    Armv7aL2DescriptorDecode l2{};
};

struct Armv7aFaultContextSnapshot {
    std::uint32_t sctlr = 0u;
    std::uint32_t ttbr0 = 0u;
    std::uint32_t ttbcr = 0u;
    std::uint32_t dacr = 0u;
};

struct Armv7aFaultObservation {
    Armv7aExceptionKind kind = kArmv7aExceptionReserved;
    bool registers_valid = false;
    Armv7aFaultRegistersSnapshot registers{};
    bool map_valid = false;
    Armv7aFaultMapSnapshot map{};
    Armv7aFaultContextSnapshot context{};
};

constexpr bool armv7a_exception_has_fault_registers(Armv7aExceptionKind kind) noexcept
{
    return kind == kArmv7aExceptionPrefetchAbort || kind == kArmv7aExceptionDataAbort;
}

constexpr bool armv7a_fault_map_has_domain(Armv7aL1DescriptorKind kind) noexcept
{
    return kind == Armv7aL1DescriptorKind::kPageTable ||
           kind == Armv7aL1DescriptorKind::kSection ||
           kind == Armv7aL1DescriptorKind::kSupersection;
}

constexpr bool armv7a_fault_map_uses_l2(Armv7aL1DescriptorKind kind) noexcept
{
    return kind == Armv7aL1DescriptorKind::kPageTable;
}

constexpr bool armv7a_fault_map_has_l1_attributes(Armv7aL1DescriptorKind kind) noexcept
{
    return kind == Armv7aL1DescriptorKind::kSection ||
           kind == Armv7aL1DescriptorKind::kSupersection;
}

constexpr bool armv7a_fault_map_has_l2_attributes(Armv7aL2DescriptorKind kind) noexcept
{
    return kind == Armv7aL2DescriptorKind::kSmallPage ||
           kind == Armv7aL2DescriptorKind::kLargePage;
}

constexpr bool armv7a_fault_observation_has_registers(
    const Armv7aFaultObservation& observation) noexcept
{
    return observation.registers_valid;
}

constexpr bool armv7a_fault_observation_has_map(
    const Armv7aFaultObservation& observation) noexcept
{
    return observation.map_valid;
}
