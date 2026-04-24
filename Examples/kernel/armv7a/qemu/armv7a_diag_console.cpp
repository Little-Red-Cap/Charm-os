#include "armv7a_diag_console.hpp"

#include "armv7a_platform.hpp"

void armv7a_diag_put_hex(std::uintptr_t value, int digits)
{
    constexpr char kHex[] = "0123456789ABCDEF";

    if (digits <= 0) {
        return;
    }

    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

void armv7a_diag_put_hex64(std::uint64_t value, int digits)
{
    constexpr char kHex[] = "0123456789ABCDEF";

    if (digits <= 0) {
        return;
    }

    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(
            kHex[static_cast<unsigned int>((value >> shift) & 0x0Fu)]);
    }
}

void armv7a_diag_put_dec(std::uint32_t value)
{
    char buffer[10]{};
    int index = 0;

    do {
        buffer[index++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (index > 0) {
        armv7a_platform_early_console_putc(buffer[--index]);
    }
}

const char* armv7a_diag_yes_no(bool value)
{
    return value ? "yes" : "no";
}
