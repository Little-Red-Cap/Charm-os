extern "C" void qemu_semihost_write0(const char* text);
extern "C" void early_uart_init();
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

int main()
{
#if defined(CHARM_QEMU_SEMIHOST_DEBUG)
    qemu_semihost_write0("semihost: entering main\n");
#endif
    early_uart_init();
    early_uart_puts("Charm ARMv7-A QEMU skeleton\r\n");
    early_uart_puts("Targeting Cortex-A7 first, RK3506 later.\r\n");
    charm_spin();
}
