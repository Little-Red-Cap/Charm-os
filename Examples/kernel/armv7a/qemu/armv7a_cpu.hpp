#pragma once

#include <cstdint>

extern "C" void armv7a_data_sync_barrier();
extern "C" void armv7a_instruction_sync_barrier();
extern "C" void armv7a_enable_irq();
extern "C" void armv7a_disable_irq();
extern "C" std::uint32_t armv7a_read_cpsr();
extern "C" std::uint32_t armv7a_read_mpidr();
extern "C" std::uint32_t armv7a_read_sctlr();
extern "C" std::uint32_t armv7a_read_vbar();
extern "C" void armv7a_svc_smoke_test();

bool armv7a_irq_masked(std::uint32_t psr);
const char* armv7a_mode_name(std::uint32_t psr);
