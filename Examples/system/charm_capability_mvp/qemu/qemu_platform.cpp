#include "qemu_platform.hpp"

#include <cstddef>
#include <cstdint>

namespace charm::mvp::qemu {
    namespace {
        constexpr std::uintptr_t uart_base = 0x09000000U;
        constexpr std::uintptr_t uart_flags = uart_base + 0x18U;
        constexpr std::uint32_t tx_full = 1U << 5U;
    }

    void write_char(const char value) noexcept {
        while ((*reinterpret_cast<volatile std::uint32_t*>(uart_flags) & tx_full) != 0U) {
        }
        *reinterpret_cast<volatile std::uint32_t*>(uart_base) =
            static_cast<std::uint32_t>(value);
    }

    void write_text(const std::string_view text) noexcept {
        for (const char value : text) {
            write_char(value);
        }
    }

    void write_u64(std::uint64_t value) noexcept {
        char digits[20]{};
        std::size_t count = 0;
        do {
            digits[count++] = static_cast<char>('0' + value % 10U);
            value /= 10U;
        } while (value != 0U);
        while (count != 0U) {
            write_char(digits[--count]);
        }
    }

    void write_hex_u32(const std::uint32_t value) noexcept {
        constexpr std::string_view digits{"0123456789abcdef"};
        for (int shift = 28; shift >= 0; shift -= 4) {
            write_char(digits[(value >> static_cast<unsigned int>(shift)) & 0x0fU]);
        }
    }

    [[noreturn]] void exit(const int status) noexcept {
        struct ExitBlock {
            std::uint32_t reason;
            std::uint32_t status;
        };
        const ExitBlock block{0x20026U, static_cast<std::uint32_t>(status)};
        register std::uint32_t operation asm("r0") = 0x20U;
        register const ExitBlock* argument asm("r1") = &block;
        asm volatile("svc 0x123456" : : "r"(operation), "r"(argument) : "memory");
        while (true) {
            asm volatile("wfi");
        }
    }
}

extern "C" void* memcpy(void* destination,
                         const void* source,
                         const std::size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    for (std::size_t index = 0; index < count; ++index) {
        out[index] = in[index];
    }
    return destination;
}

extern "C" void* memmove(void* destination,
                          const void* source,
                          const std::size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    if (out < in) {
        for (std::size_t index = 0; index < count; ++index) {
            out[index] = in[index];
        }
    } else if (out > in) {
        for (std::size_t index = count; index != 0U; --index) {
            out[index - 1U] = in[index - 1U];
        }
    }
    return destination;
}

extern "C" void* memset(void* destination,
                         const int value,
                         const std::size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    for (std::size_t index = 0; index < count; ++index) {
        out[index] = static_cast<unsigned char>(value);
    }
    return destination;
}

extern "C" std::size_t strlen(const char* text) noexcept {
    std::size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}
