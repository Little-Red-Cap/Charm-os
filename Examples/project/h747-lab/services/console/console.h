#pragma once

#include <cstdint>

namespace h747::console {

void write(const char* text);
void write_line(const char* text);
void write_char(char c);
void write_dec(std::uint32_t value);
void write_hex8(std::uint8_t value);
void write_hex16(std::uint16_t value);
void write_hex32(std::uint32_t value);
bool poll_line(char* buffer, std::uint32_t capacity, std::uint32_t& length);

} // namespace h747::console
