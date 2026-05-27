#include "main.h"
#include "usart.h"

#include <cstdint>

extern "C" DMA_HandleTypeDef hdma_usart1_tx;
extern "C" DMA_HandleTypeDef hdma_usart1_rx;

namespace {

struct ExceptionFrame {
    std::uint32_t r0;
    std::uint32_t r1;
    std::uint32_t r2;
    std::uint32_t r3;
    std::uint32_t r12;
    std::uint32_t lr;
    std::uint32_t pc;
    std::uint32_t xpsr;
};

void fault_uart_write(const char* text) {
    if ((text == nullptr) || (huart1.Instance == nullptr)) {
        return;
    }

    std::uint16_t len = 0U;
    while (text[len] != '\0') {
        ++len;
    }
    (void)HAL_UART_Transmit(&huart1,
                            reinterpret_cast<std::uint8_t*>(const_cast<char*>(text)),
                            len,
                            100U);
}

void fault_uart_write_hex32(const std::uint32_t value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    char buffer[11] = "0x00000000";
    for (int index = 0; index < 8; ++index) {
        const auto shift = static_cast<unsigned>((7 - index) * 4);
        buffer[2 + index] = kHex[(value >> shift) & 0x0FU];
    }
    fault_uart_write(buffer);
}

void fault_uart_write_line_hex(const char* key, const std::uint32_t value) {
    fault_uart_write(key);
    fault_uart_write_hex32(value);
    fault_uart_write("\r\n");
}

[[noreturn]] void fault_halt(const char* text,
                             const ExceptionFrame* frame,
                             const std::uint32_t exc_return) {
    while (true) {
        fault_uart_write("\r\n");
        fault_uart_write(text);
        fault_uart_write("\r\n");
        fault_uart_write_line_hex("fault.exc_return=", exc_return);
        fault_uart_write_line_hex("fault.control=", __get_CONTROL());
        fault_uart_write_line_hex("fault.msp=", __get_MSP());
        fault_uart_write_line_hex("fault.psp=", __get_PSP());
        fault_uart_write_line_hex("fault.icsr=", SCB->ICSR);
        fault_uart_write_line_hex("fault.shcsr=", SCB->SHCSR);
        fault_uart_write_line_hex("fault.cfsr=", SCB->CFSR);
        fault_uart_write_line_hex("fault.hfsr=", SCB->HFSR);
        fault_uart_write_line_hex("fault.dfsr=", SCB->DFSR);
        fault_uart_write_line_hex("fault.afsr=", SCB->AFSR);
        fault_uart_write_line_hex("fault.mmfar=", SCB->MMFAR);
        fault_uart_write_line_hex("fault.bfar=", SCB->BFAR);
        fault_uart_write_line_hex("fault.abfsr=", SCB->ABFSR);

        if (frame != nullptr) {
            fault_uart_write_line_hex("fault.r0=", frame->r0);
            fault_uart_write_line_hex("fault.r1=", frame->r1);
            fault_uart_write_line_hex("fault.r2=", frame->r2);
            fault_uart_write_line_hex("fault.r3=", frame->r3);
            fault_uart_write_line_hex("fault.r12=", frame->r12);
            fault_uart_write_line_hex("fault.lr=", frame->lr);
            fault_uart_write_line_hex("fault.pc=", frame->pc);
            fault_uart_write_line_hex("fault.xpsr=", frame->xpsr);
        }

        for (volatile std::uint32_t spin = 0U; spin < 2'000'000U; ++spin) {
            __NOP();
        }
    }
}

[[noreturn]] void fault_entry_common(const char* name,
                                     const ExceptionFrame* frame,
                                     const std::uint32_t exc_return) {
    __disable_irq();
    fault_halt(name, frame, exc_return);
}

} // namespace

extern "C" {

[[noreturn]] void fault_nmi_entry(const ExceptionFrame* frame, const std::uint32_t exc_return) {
    fault_entry_common("fault: NMI", frame, exc_return);
}

[[noreturn]] void fault_hardfault_entry(const ExceptionFrame* frame, const std::uint32_t exc_return) {
    fault_entry_common("fault: HardFault", frame, exc_return);
}

[[noreturn]] void fault_memmanage_entry(const ExceptionFrame* frame, const std::uint32_t exc_return) {
    fault_entry_common("fault: MemManage", frame, exc_return);
}

[[noreturn]] void fault_busfault_entry(const ExceptionFrame* frame, const std::uint32_t exc_return) {
    fault_entry_common("fault: BusFault", frame, exc_return);
}

[[noreturn]] void fault_usagefault_entry(const ExceptionFrame* frame, const std::uint32_t exc_return) {
    fault_entry_common("fault: UsageFault", frame, exc_return);
}

__attribute__((naked)) void NMI_Handler(void) {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b fault_nmi_entry\n");
}

__attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b fault_hardfault_entry\n");
}

__attribute__((naked)) void MemManage_Handler(void) {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b fault_memmanage_entry\n");
}

__attribute__((naked)) void BusFault_Handler(void) {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b fault_busfault_entry\n");
}

__attribute__((naked)) void UsageFault_Handler(void) {
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b fault_usagefault_entry\n");
}

void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
void SysTick_Handler(void) {
    HAL_IncTick();
}
void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}
void DMA1_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

__attribute__((weak)) void LTDC_IRQHandler(void) {}
__attribute__((weak)) void LTDC_ER_IRQHandler(void) {}
__attribute__((weak)) void DSI_IRQHandler(void) {}

}
