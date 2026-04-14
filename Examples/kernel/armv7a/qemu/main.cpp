import out.format;
import out.sink;

extern "C" void qemu_semihost_write0(const char* text);
extern "C" void armv7a_irq_smoke_test();
extern "C" void armv7a_svc_smoke_test();
extern "C" void early_uart_init();
extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
void early_uart_write(auto text)
{
    for (char ch : text) {
        early_uart_putc(ch);
    }
}

void early_uart_put_hex32(unsigned int value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0xFu]);
    }
}

void print_charm_module_status()
{
    out::buffer_sink<96> buffer{};
    auto status = out::vprint<"Charm out.format import active, PL011 @ 0x{:08X}\r\n">(buffer, 0x09000000u);
    if (!status) {
        early_uart_puts("out.format failed, err=0x");
        early_uart_put_hex32(static_cast<unsigned int>(status.error()));
        early_uart_puts("\r\n");
        return;
    }
    if (buffer.view().empty()) {
        early_uart_puts("out.format produced an empty buffer\r\n");
        return;
    }
    early_uart_write(buffer.view());
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
    armv7a_svc_smoke_test();
    armv7a_irq_smoke_test();
    charm_spin();
}
