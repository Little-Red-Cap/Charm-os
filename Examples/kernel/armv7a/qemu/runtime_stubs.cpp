#include <cstddef>

#include "armv7a_diag_context.hpp"
#include "armv7a_platform.hpp"

namespace {
[[noreturn]] void runtime_trap(const char* reason, const char* detail = nullptr)
{
    armv7a_diag_print_context("runtime");
    armv7a_platform_early_console_puts("runtime trap: ");
    armv7a_platform_early_console_puts(reason);
    if (detail != nullptr) {
        armv7a_platform_early_console_puts(": ");
        armv7a_platform_early_console_puts(detail);
    }
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_idle_forever();
}
} // namespace

extern "C" void* memset(void* dest, int value, std::size_t count)
{
    auto* out = static_cast<unsigned char*>(dest);
    const auto byte = static_cast<unsigned char>(value);
    for (std::size_t i = 0; i < count; ++i) {
        out[i] = byte;
    }
    return dest;
}

extern "C" void* memcpy(void* dest, const void* src, std::size_t count)
{
    auto* out = static_cast<unsigned char*>(dest);
    const auto* in = static_cast<const unsigned char*>(src);
    for (std::size_t i = 0; i < count; ++i) {
        out[i] = in[i];
    }
    return dest;
}

extern "C" void* memmove(void* dest, const void* src, std::size_t count)
{
    auto* out = static_cast<unsigned char*>(dest);
    const auto* in = static_cast<const unsigned char*>(src);

    if (out == in || count == 0) {
        return dest;
    }

    if (out < in) {
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = in[i];
        }
        return dest;
    }

    for (std::size_t i = count; i != 0; --i) {
        out[i - 1] = in[i - 1];
    }
    return dest;
}

extern "C" int memcmp(const void* lhs, const void* rhs, std::size_t count)
{
    const auto* a = static_cast<const unsigned char*>(lhs);
    const auto* b = static_cast<const unsigned char*>(rhs);

    for (std::size_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) {
            return static_cast<int>(a[i]) - static_cast<int>(b[i]);
        }
    }
    return 0;
}

extern "C" void __aeabi_memcpy(void* dest, const void* src, std::size_t count)
{
    (void)memcpy(dest, src, count);
}

extern "C" void __aeabi_memcpy4(void* dest, const void* src, std::size_t count)
{
    (void)memcpy(dest, src, count);
}

extern "C" void __aeabi_memcpy8(void* dest, const void* src, std::size_t count)
{
    (void)memcpy(dest, src, count);
}

extern "C" void __aeabi_memmove(void* dest, const void* src, std::size_t count)
{
    (void)memmove(dest, src, count);
}

extern "C" void __aeabi_memset(void* dest, std::size_t count, int value)
{
    (void)memset(dest, value, count);
}

extern "C" void __aeabi_memset4(void* dest, std::size_t count, int value)
{
    (void)memset(dest, value, count);
}

extern "C" void __aeabi_memset8(void* dest, std::size_t count, int value)
{
    (void)memset(dest, value, count);
}

extern "C" void __aeabi_memclr(void* dest, std::size_t count)
{
    (void)memset(dest, 0, count);
}

extern "C" void __aeabi_memclr4(void* dest, std::size_t count)
{
    (void)memset(dest, 0, count);
}

extern "C" void __aeabi_memclr8(void* dest, std::size_t count)
{
    (void)memset(dest, 0, count);
}

extern "C" [[noreturn]] void __cxa_pure_virtual()
{
    runtime_trap("__cxa_pure_virtual");
}

extern "C" [[noreturn]] void __cxa_deleted_virtual()
{
    runtime_trap("__cxa_deleted_virtual");
}

extern "C" [[noreturn]] void __stack_chk_fail()
{
    runtime_trap("__stack_chk_fail");
}

extern "C" [[noreturn]] void abort()
{
    runtime_trap("abort");
}

namespace std {
[[noreturn]] void terminate() noexcept
{
    runtime_trap("std::terminate");
}

[[noreturn]] void __glibcxx_assert_fail(const char*,
                                        int,
                                        const char*,
                                        const char* condition) noexcept
{
    runtime_trap("libstdc++ assert", condition);
}
} // namespace std
