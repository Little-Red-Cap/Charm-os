#pragma once

void armv7a_diag_print_context(const char* subsystem);
[[noreturn]] void armv7a_diag_report_and_halt(const char* subsystem, const char* message);
