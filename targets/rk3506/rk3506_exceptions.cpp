#include "rk3506_armv7a_state.hpp"
#include "rk3506_exception_frame.hpp"
#include "rk3506_platform.hpp"

#include <cstdint>

namespace {
void put_hex_word(std::uint32_t value) noexcept
{
    constexpr char kHexDigits[] = "0123456789abcdef";

    for (int shift = 28; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<std::uint32_t>((value >> shift) & 0x0fu);
        rk3506_platform_early_console_putc(kHexDigits[nibble]);
    }
}

void put_labeled_hex(const char* label, std::uint32_t value) noexcept
{
    rk3506_platform_early_console_puts(label);
    rk3506_platform_early_console_puts("0x");
    put_hex_word(value);
    rk3506_platform_early_console_puts("\n");
}

void put_labeled_text(const char* label, const char* value) noexcept
{
    rk3506_platform_early_console_puts(label);
    rk3506_platform_early_console_puts(value ? value : "(null)");
    rk3506_platform_early_console_puts("\n");
}

[[noreturn]] void print_exception_and_halt(const char* reason,
                                           const Rk3506ExceptionFrame* frame) noexcept
{
    // Fatal exceptions can happen before rk3506_boot_main() reaches the normal
    // console init path, so re-run the early UART setup on the fatal path to
    // maximize the odds of seeing diagnostics on the board.
    rk3506_platform_early_console_init();
    rk3506_platform_early_console_puts("\nRK3506 exception entry\n");
    put_labeled_text("Reason: ", reason);

    if (frame) {
        const auto handler_cpsr = rk3506::armv7a::read_cpsr();
        put_labeled_text("Vector: ", rk3506_exception_name(*frame));
        put_labeled_hex("Vector id: ", frame->vector_id);
        put_labeled_hex("Origin SPSR: ", frame->spsr);
        put_labeled_text("Origin mode: ",
            rk3506::armv7a::mode_name(
                rk3506::armv7a::cpu_mode(frame->spsr)));
        put_labeled_hex("Handler CPSR: ", handler_cpsr);
        put_labeled_text("Handler mode: ",
            rk3506::armv7a::mode_name(
                rk3506::armv7a::cpu_mode(handler_cpsr)));
        put_labeled_hex("Return PC: ", rk3506_exception_return_pc(*frame));
        put_labeled_hex("LR: ", frame->lr);
        put_labeled_hex("R0: ", frame->r0);
        put_labeled_hex("R1: ", frame->r1);
        put_labeled_hex("R2: ", frame->r2);
        put_labeled_hex("R3: ", frame->r3);
        put_labeled_hex("R12: ", frame->r12);
    }

    rk3506_platform_early_console_puts(
        "Exception path is now routed through a real handler skeleton; halting for diagnosis.\n");
    rk3506_platform_idle_forever();
}
} // namespace

extern "C" void rk3506_handle_svc(Rk3506ExceptionFrame* frame)
{
    print_exception_and_halt("svc-handler", frame);
}

extern "C" void rk3506_handle_irq(Rk3506ExceptionFrame* frame)
{
    if (rk3506_platform_handle_irq_exception(frame)) {
        return;
    }
    print_exception_and_halt("irq-handler", frame);
}

extern "C" void rk3506_handle_fiq(Rk3506ExceptionFrame* frame)
{
    print_exception_and_halt("fiq-handler", frame);
}

extern "C" [[noreturn]] void rk3506_exception_fatal(const Rk3506ExceptionFrame* frame)
{
    print_exception_and_halt("fatal-vector", frame);
}
