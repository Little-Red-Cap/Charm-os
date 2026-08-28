#pragma once

#include <cstdint>
#include <string_view>

namespace charm::mvp::qemu {
    void write_char(char value) noexcept;
    void write_text(std::string_view text) noexcept;
    void write_u64(std::uint64_t value) noexcept;
    void write_hex_u32(std::uint32_t value) noexcept;
    [[noreturn]] void exit(int status) noexcept;
}
