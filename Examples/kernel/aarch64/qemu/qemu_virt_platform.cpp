extern "C" [[noreturn]] void charm_spin()
{
    for (;;) {
        asm volatile("wfe");
    }
}
