#pragma once

#include <cstdint>

std::uint32_t armv7a_timer_read_cntfrq();
std::uint64_t armv7a_timer_read_cntpct();
std::uint32_t armv7a_timer_read_ctrl();
void armv7a_timer_write_ctrl(std::uint32_t value);
void armv7a_timer_write_tval(std::uint32_t value);
