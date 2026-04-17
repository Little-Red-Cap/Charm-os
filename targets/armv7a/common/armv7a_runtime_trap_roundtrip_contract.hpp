#pragma once

#include <cstdint>

#include "armv7a_runtime_trap_dispatch_contract.hpp"

enum class Armv7aRuntimeTrapRoundtripPath : std::uint8_t {
    none = 0,
    svc_return,
};

constexpr const char* armv7a_runtime_trap_roundtrip_path_name(
    Armv7aRuntimeTrapRoundtripPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapRoundtripPath::svc_return:
        return "svc-return";
    case Armv7aRuntimeTrapRoundtripPath::none:
    default:
        return "none";
    }
}

constexpr bool armv7a_runtime_trap_roundtrip_value_fits_return_register(
    std::uint64_t value) noexcept
{
    return value <= 0xffffffffull;
}

struct Armv7aRuntimeTrapRoundtripObservation {
    Armv7aRuntimeTrapIngressResult result{};
    std::uint32_t service_id = 0u;
    std::uint32_t return_value = 0u;
    std::uint64_t expected_value = 0u;
    Armv7aRuntimeTrapRoundtripPath path =
        Armv7aRuntimeTrapRoundtripPath::none;
    bool service_ready = false;
    bool value_fits_return_register = false;
    bool return_matches_result = false;
    bool return_matches_expected = false;
};

constexpr bool armv7a_runtime_trap_roundtrip_ready(
    const Armv7aRuntimeTrapRoundtripObservation& observation) noexcept
{
    return observation.path == Armv7aRuntimeTrapRoundtripPath::svc_return &&
           observation.result.ok() && observation.service_ready &&
           observation.value_fits_return_register &&
           observation.return_matches_result &&
           observation.return_matches_expected;
}
