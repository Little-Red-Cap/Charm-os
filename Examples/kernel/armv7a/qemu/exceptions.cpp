#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_diagnostics.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_exception_observation.hpp"

namespace {
volatile bool g_svc_observation_seen = false;
volatile std::uint32_t g_svc_observation_origin_psr = 0;
volatile std::uint32_t g_svc_observation_handler_psr = 0;
volatile std::uint32_t g_svc_observation_return_pc = 0;
volatile std::uint32_t g_svc_observation_immediate = 0;
volatile std::uint32_t g_svc_observation_arg0 = 0;
volatile std::uint32_t g_svc_observation_arg1 = 0;
volatile std::uint32_t g_svc_observation_arg2 = 0;
volatile std::uint32_t g_svc_observation_arg3 = 0;
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto current_cpsr = armv7a_read_cpsr();
    const auto* instruction =
        reinterpret_cast<const std::uint32_t*>(armv7a_exception_pc(*frame));
    g_svc_observation_seen = true;
    g_svc_observation_origin_psr = frame->spsr;
    g_svc_observation_handler_psr = current_cpsr;
    g_svc_observation_return_pc = armv7a_exception_return_pc(*frame);
    g_svc_observation_immediate = *instruction & 0x00FFFFFFu;
    g_svc_observation_arg0 = frame->r0;
    g_svc_observation_arg1 = frame->r1;
    g_svc_observation_arg2 = frame->r2;
    g_svc_observation_arg3 = frame->r3;
    armv7a_exception_print_svc_active(*frame, current_cpsr);
}

Armv7aSvcObservation armv7a_svc_last_observation()
{
    return Armv7aSvcObservation{
        .entry =
            g_svc_observation_seen
                ? armv7a_make_vector_entry_observation(g_svc_observation_origin_psr,
                                                       g_svc_observation_handler_psr,
                                                       g_svc_observation_return_pc)
                : armv7a_make_unobserved_vector_entry(),
        .immediate = g_svc_observation_immediate,
        .arg0 = g_svc_observation_arg0,
        .arg1 = g_svc_observation_arg1,
        .arg2 = g_svc_observation_arg2,
        .arg3 = g_svc_observation_arg3,
        .arguments_sampled = g_svc_observation_seen,
    };
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    armv7a_exception_print_fatal_and_halt(*frame);
}
