#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_diagnostics.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_exception_observation.hpp"

namespace {
volatile bool g_svc_observation_seen = false;
volatile std::uint32_t g_svc_observation_spsr = 0;
volatile std::uint32_t g_svc_observation_handler_cpsr = 0;
volatile std::uint32_t g_svc_observation_return_pc = 0;
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto current_cpsr = armv7a_read_cpsr();
    g_svc_observation_seen = true;
    g_svc_observation_spsr = frame->spsr;
    g_svc_observation_handler_cpsr = current_cpsr;
    g_svc_observation_return_pc = armv7a_exception_return_pc(*frame);
    armv7a_exception_print_svc_active(*frame, current_cpsr);
}

Armv7aSvcObservation armv7a_svc_last_observation()
{
    return Armv7aSvcObservation{
        .seen = g_svc_observation_seen,
        .origin_spsr = g_svc_observation_spsr,
        .handler_cpsr = g_svc_observation_handler_cpsr,
        .return_pc = g_svc_observation_return_pc,
    };
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    armv7a_exception_print_fatal_and_halt(*frame);
}
