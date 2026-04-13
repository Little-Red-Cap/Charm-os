extern "C" void qemu_semihost_write0(const char* text)
{
    register int op asm("r0") = 0x04;
    register const char* ptr asm("r1") = text;
    asm volatile("bkpt 0xab" : : "r"(op), "r"(ptr) : "memory");
}

extern "C" [[noreturn]] void charm_spin()
{
    for (;;) {
        asm volatile("wfe");
    }
}
