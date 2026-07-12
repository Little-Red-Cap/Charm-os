extern "C" {
    void Reset_Handler(void) noexcept;
    void Default_Handler(void) noexcept;

    extern unsigned long _estack;
    extern unsigned long _sidata;
    extern unsigned long _sdata;
    extern unsigned long _edata;
    extern unsigned long _sbss;
    extern unsigned long _ebss;
}

using isr_fn = void (*)();

extern "C" __attribute__((section(".isr_vector"))) const isr_fn vector_table[] = {
    reinterpret_cast<isr_fn>(&_estack),
    Reset_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
};

extern "C" int charm_capability_mvp_qemu_main() noexcept;

extern "C" void Reset_Handler(void) noexcept {
    auto* source = &_sidata;
    auto* destination = &_sdata;
    while (destination < &_edata) {
        *destination++ = *source++;
    }
    destination = &_sbss;
    while (destination < &_ebss) {
        *destination++ = 0;
    }
    (void)charm_capability_mvp_qemu_main();
    while (true) {
    }
}

extern "C" void Default_Handler(void) noexcept {
    while (true) {
    }
}
