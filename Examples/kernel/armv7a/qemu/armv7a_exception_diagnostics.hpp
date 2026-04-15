#pragma once

struct Armv7aExceptionFrame;

void armv7a_exception_print_svc_active(const Armv7aExceptionFrame& frame,
                                       unsigned int current_cpsr);
[[noreturn]] void armv7a_exception_print_fatal_and_halt(const Armv7aExceptionFrame& frame);
