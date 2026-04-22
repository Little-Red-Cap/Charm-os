#pragma once

#include "targets/armv7a/common/armv7a_runtime_thread_contract.hpp"

Armv7aRuntimeThreadObservation armv7a_capture_runtime_thread_observation()
    noexcept;
void armv7a_print_runtime_thread_observation();
