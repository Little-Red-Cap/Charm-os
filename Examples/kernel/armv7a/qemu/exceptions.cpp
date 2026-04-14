#include <cstdint>

extern "C" void early_uart_init();
extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
enum ExceptionKind : unsigned int {
    kUndefined = 1,
    kPrefetchAbort = 2,
    kDataAbort = 3,
    kReserved = 4,
    kIrq = 5,
    kFiq = 6,
};

void early_uart_write_hex(std::uint32_t value, int digits)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

const char* exception_name(unsigned int kind)
{
    switch (kind) {
    case kUndefined:
        return "undefined";
    case kPrefetchAbort:
        return "prefetch abort";
    case kDataAbort:
        return "data abort";
    case kReserved:
        return "reserved vector";
    case kIrq:
        return "irq";
    case kFiq:
        return "fiq";
    default:
        return "unknown";
    }
}
} // namespace

extern "C" void armv7a_svc_smoke_test()
{
    asm volatile("svc #0x43" ::: "memory");
}

extern "C" void armv7a_handle_svc(unsigned int lr, unsigned int)
{
    const auto* instruction = reinterpret_cast<const std::uint32_t*>(lr - 4u);
    const auto immediate = *instruction & 0x00FFFFFFu;
    early_uart_puts("ARMv7-A SVC vector active, imm=0x");
    early_uart_write_hex(immediate, 6);
    early_uart_puts("\r\n");
}

extern "C" [[noreturn]] void armv7a_exception_fatal(unsigned int kind,
                                                    unsigned int lr,
                                                    unsigned int spsr)
{
    early_uart_init();
    early_uart_puts("ARMv7-A exception: ");
    early_uart_puts(exception_name(kind));
    early_uart_puts(", lr=0x");
    early_uart_write_hex(lr, 8);
    early_uart_puts(", spsr=0x");
    early_uart_write_hex(spsr, 8);
    early_uart_puts("\r\n");
    charm_spin();
}
