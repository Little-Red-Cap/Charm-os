#pragma once

#include "armv7a_runtime_trap_contract.hpp"

Armv7aRuntimeTrapObservation armv7a_capture_runtime_trap_ingress() noexcept;
void armv7a_print_runtime_trap_ingress();
