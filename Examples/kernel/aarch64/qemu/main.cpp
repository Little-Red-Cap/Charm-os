extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

int main()
{
    early_uart_puts("Charm AArch64 QEMU skeleton\n");
    early_uart_puts("Leaf target is in place; top-level module split is still pending.\n");
    charm_spin();
}
