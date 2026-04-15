#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_diagnostics.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_exception_observation.hpp"

namespace {
volatile bool g_svc_observation_seen = false;
volatile std::uint32_t g_svc_observation_spsr = 0;
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto current_cpsr = armv7a_read_cpsr();
    g_svc_observation_seen = true;
    g_svc_observation_spsr = frame->spsr;
    armv7a_exception_print_svc_active(*frame, current_cpsr);
}

bool armv7a_svc_observation_seen()
{
    return g_svc_observation_seen;
}

std::uint32_t armv7a_svc_observation_spsr()
{
    return g_svc_observation_spsr;
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    armv7a_exception_print_fatal_and_halt(*frame);
}
