#pragma once

#include <cstdint>

void armv7a_diag_put_hex(std::uintptr_t value, int digits = 8);
void armv7a_diag_put_dec(std::uint32_t value);
const char* armv7a_diag_yes_no(bool value);
