import util.core;

extern "C" void qemu_semihost_write0(const char* text);
extern "C" void early_uart_init();
extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
void early_uart_write_hex32(util::u32 value)
{
    constexpr char digits[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<unsigned>((value >> shift) & 0x0Fu);
        early_uart_putc(digits[nibble]);
    }
}

void print_charm_module_status()
{
    early_uart_puts("Charm util.core import active, PL011 @ 0x");
    early_uart_write_hex32(static_cast<util::u32>(0x09000000u));
    early_uart_puts("\r\n");
}
} // namespace

int main()
{
#if defined(CHARM_QEMU_SEMIHOST_DEBUG)
    qemu_semihost_write0("semihost: entering main\n");
#endif
    early_uart_init();
    early_uart_puts("Charm ARMv7-A QEMU skeleton\r\n");
    early_uart_puts("Targeting Cortex-A7 first, RK3506 later.\r\n");
    print_charm_module_status();
    charm_spin();
}
