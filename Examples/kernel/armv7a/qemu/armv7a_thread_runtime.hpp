#pragma once

#include <cstdint>

struct Armv7aThreadRuntimeObservation {
    std::uint64_t task = 0u;
    std::uint64_t current_stack_pointer = 0u;
    std::uintptr_t prepared_sp = 0u;
    bool current_ready = false;
    bool prepare_ready = false;
    bool switch_ready = false;
    bool runtime_ready = false;
};

constexpr bool armv7a_thread_runtime_ready(
    const Armv7aThreadRuntimeObservation& observation) noexcept
{
    return observation.current_ready && observation.prepare_ready &&
           observation.switch_ready && observation.runtime_ready;
}

Armv7aThreadRuntimeObservation armv7a_capture_thread_runtime_observation() noexcept;
void armv7a_print_thread_runtime_observation();
