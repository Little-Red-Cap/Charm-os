#pragma once

#include <cstdint>

#include "armv7a_runtime_trap_contract.hpp"

Armv7aRuntimeTrapObservation armv7a_capture_runtime_trap_ingress() noexcept;
Armv7aRuntimeTrapObservation armv7a_capture_runtime_trap_ingress_for_service(
    std::uint32_t service_id) noexcept;
void armv7a_print_runtime_trap_ingress();
