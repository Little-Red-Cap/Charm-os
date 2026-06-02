#include "console.h"

#include "port.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstring>

#ifndef H747_CONSOLE_TX_DMA_ENABLED
#define H747_CONSOLE_TX_DMA_ENABLED 1
#endif

namespace h747::console {

namespace {

constexpr std::uint32_t kRxDmaBufferSize = 4096U;
constexpr std::uint32_t kTxDmaBufferSize = 8192U;
constexpr std::uintptr_t kCacheLineSize = 32U;

RxStats g_rx_stats{};
alignas(32) __attribute__((section(".dma_buffer")))
std::array<std::uint8_t, kRxDmaBufferSize> g_rx_dma_buffer{};
alignas(32) __attribute__((section(".dma_buffer")))
std::array<std::uint8_t, kTxDmaBufferSize> g_tx_dma_buffer{};
std::uint32_t g_rx_dma_read_pos{0};
bool g_rx_dma_start_attempted{false};
bool g_rx_dma_active{false};
volatile std::uint32_t g_tx_head{0};
volatile std::uint32_t g_tx_tail{0};
volatile std::uint32_t g_tx_dma_active_len{0};
volatile bool g_tx_dma_busy{false};
volatile bool g_tx_dma_disabled{false};

std::uint32_t enter_critical() noexcept {
    const auto primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void leave_critical(const std::uint32_t primask) noexcept {
    __set_PRIMASK(primask);
}

void clear_uart_overrun(UART_HandleTypeDef* uart) {
    if ((uart == nullptr) || (uart->Instance == nullptr)) {
        return;
    }
    if ((uart->Instance->ISR & USART_ISR_ORE) != 0U) {
        uart->Instance->ICR = USART_ICR_ORECF;
        ++g_rx_stats.overrun_clears;
    }
}

constexpr std::uintptr_t cache_align_down(const std::uintptr_t address) noexcept {
    return address & ~(kCacheLineSize - 1U);
}

constexpr std::uintptr_t cache_align_up(const std::uintptr_t address) noexcept {
    return (address + kCacheLineSize - 1U) & ~(kCacheLineSize - 1U);
}

void invalidate_rx_cache_range(const std::uint32_t offset, const std::uint32_t length) {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (((SCB->CCR & SCB_CCR_DC_Msk) == 0U) || length == 0U) {
        return;
    }

    const auto start = reinterpret_cast<std::uintptr_t>(g_rx_dma_buffer.data() + offset);
    const auto aligned_start = cache_align_down(start);
    const auto aligned_end = cache_align_up(start + length);
    SCB_InvalidateDCache_by_Addr(reinterpret_cast<std::uint32_t*>(aligned_start),
                                 static_cast<std::int32_t>(aligned_end - aligned_start));
#else
    (void)offset;
    (void)length;
#endif
}

void clean_tx_cache_range(const std::uint32_t offset, const std::uint32_t length) {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (((SCB->CCR & SCB_CCR_DC_Msk) == 0U) || length == 0U) {
        return;
    }

    const auto start = reinterpret_cast<std::uintptr_t>(g_tx_dma_buffer.data() + offset);
    const auto aligned_start = cache_align_down(start);
    const auto aligned_end = cache_align_up(start + length);
    SCB_CleanDCache_by_Addr(reinterpret_cast<std::uint32_t*>(aligned_start),
                            static_cast<std::int32_t>(aligned_end - aligned_start));
#else
    (void)offset;
    (void)length;
#endif
}

void invalidate_rx_cache_span(const std::uint32_t read_pos, const std::uint32_t write_pos) {
    if (read_pos == write_pos) {
        return;
    }
    if (read_pos < write_pos) {
        invalidate_rx_cache_range(read_pos, write_pos - read_pos);
        return;
    }
    invalidate_rx_cache_range(read_pos, kRxDmaBufferSize - read_pos);
    invalidate_rx_cache_range(0U, write_pos);
}

void try_start_rx_dma() {
    if (g_rx_dma_start_attempted) {
        return;
    }
    g_rx_dma_start_attempted = true;

    auto* uart = h747::port::uart1_handle();
    if ((uart == nullptr) || (uart->Instance == nullptr) || (uart->hdmarx == nullptr)) {
        ++g_rx_stats.dma_start_failed;
        return;
    }

    clear_uart_overrun(uart);
    if (HAL_DMA_DeInit(uart->hdmarx) != HAL_OK) {
        ++g_rx_stats.dma_start_failed;
        return;
    }
    uart->hdmarx->Init.Mode = DMA_CIRCULAR;
    if (HAL_DMA_Init(uart->hdmarx) != HAL_OK) {
        ++g_rx_stats.dma_start_failed;
        return;
    }
    g_rx_dma_read_pos = 0;
    invalidate_rx_cache_range(0U, kRxDmaBufferSize);
    if (HAL_UART_Receive_DMA(uart, g_rx_dma_buffer.data(), g_rx_dma_buffer.size()) != HAL_OK) {
        ++g_rx_stats.dma_start_failed;
        return;
    }

    g_rx_dma_active = true;
    ++g_rx_stats.dma_started;
}

std::uint32_t rx_dma_write_pos() {
    auto* uart = h747::port::uart1_handle();
    if (!g_rx_dma_active || uart == nullptr || uart->hdmarx == nullptr) {
        return g_rx_dma_read_pos;
    }
    const auto remaining = __HAL_DMA_GET_COUNTER(uart->hdmarx);
    if (remaining > g_rx_dma_buffer.size()) {
        return g_rx_dma_read_pos;
    }
    return static_cast<std::uint32_t>((g_rx_dma_buffer.size() - remaining) % g_rx_dma_buffer.size());
}

std::uint32_t tx_ring_used_unlocked() noexcept {
    const auto head = g_tx_head;
    const auto tail = g_tx_tail;
    if (head >= tail) {
        return head - tail;
    }
    return static_cast<std::uint32_t>(kTxDmaBufferSize - tail + head);
}

void update_tx_stats_unlocked() noexcept {
    g_rx_stats.tx_busy = g_tx_dma_busy ? 1U : 0U;
    g_rx_stats.tx_ring_used = tx_ring_used_unlocked();
    g_rx_stats.tx_ring_size = kTxDmaBufferSize;
}

bool is_console_uart(UART_HandleTypeDef* uart) noexcept {
    auto* console_uart = h747::port::uart1_handle();
    return (uart != nullptr) && (console_uart != nullptr) &&
           (uart->Instance == console_uart->Instance);
}

bool tx_dma_can_run(UART_HandleTypeDef* uart) noexcept {
#if H747_CONSOLE_TX_DMA_ENABLED
    return (uart != nullptr) && (uart->Instance != nullptr) && (uart->hdmatx != nullptr) &&
           !g_tx_dma_disabled;
#else
    (void)uart;
    return false;
#endif
}

void blocking_write_byte(const std::uint8_t byte) {
    auto* uart = h747::port::uart1_handle();
    if ((uart == nullptr) || (uart->Instance == nullptr)) {
        ++g_rx_stats.tx_dropped_bytes;
        return;
    }

    auto mutable_byte = byte;
    if (HAL_UART_Transmit(uart, &mutable_byte, 1U, 5U) == HAL_OK) {
        ++g_rx_stats.tx_fallback_bytes;
        return;
    }
    ++g_rx_stats.tx_dropped_bytes;
}

bool enqueue_tx_byte(const std::uint8_t byte) {
    const auto primask = enter_critical();
    const auto used = tx_ring_used_unlocked();
    if (used >= (kTxDmaBufferSize - 1U)) {
        ++g_rx_stats.tx_dropped_bytes;
        update_tx_stats_unlocked();
        leave_critical(primask);
        return false;
    }

    g_tx_dma_buffer[g_tx_head] = byte;
    g_tx_head = (g_tx_head + 1U) % kTxDmaBufferSize;
    update_tx_stats_unlocked();
    leave_critical(primask);
    return true;
}

bool try_start_tx_dma() {
    auto* uart = h747::port::uart1_handle();
    if (!tx_dma_can_run(uart)) {
        return false;
    }

    std::uint32_t tail = 0U;
    std::uint32_t length = 0U;
    auto primask = enter_critical();
    if (g_tx_dma_busy) {
        update_tx_stats_unlocked();
        leave_critical(primask);
        return false;
    }

    const auto used = tx_ring_used_unlocked();
    if (used == 0U) {
        update_tx_stats_unlocked();
        leave_critical(primask);
        return false;
    }

    tail = g_tx_tail;
    length = (g_tx_head > g_tx_tail) ? (g_tx_head - g_tx_tail)
                                    : (kTxDmaBufferSize - g_tx_tail);
    g_tx_dma_busy = true;
    g_tx_dma_active_len = length;
    update_tx_stats_unlocked();
    leave_critical(primask);

    clean_tx_cache_range(tail, length);
    const auto status = HAL_UART_Transmit_DMA(
        uart, g_tx_dma_buffer.data() + tail, static_cast<std::uint16_t>(length));
    if (status == HAL_OK) {
        primask = enter_critical();
        ++g_rx_stats.tx_dma_started;
        g_rx_stats.tx_dma_bytes += length;
        update_tx_stats_unlocked();
        leave_critical(primask);
        return true;
    }

    primask = enter_critical();
    g_tx_dma_busy = false;
    g_tx_dma_active_len = 0U;
    if (status != HAL_BUSY) {
        g_tx_dma_disabled = true;
        g_tx_tail = (g_tx_tail + length) % kTxDmaBufferSize;
        g_rx_stats.tx_dropped_bytes += length;
        const auto remaining = tx_ring_used_unlocked();
        if (remaining != 0U) {
            g_tx_tail = g_tx_head;
            g_rx_stats.tx_dropped_bytes += remaining;
        }
    }
    update_tx_stats_unlocked();
    leave_critical(primask);
    return false;
}

void complete_tx_dma(UART_HandleTypeDef* uart) {
    if (!is_console_uart(uart)) {
        return;
    }

    const auto primask = enter_critical();
    const auto length = g_tx_dma_active_len;
    if (g_tx_dma_busy && (length != 0U)) {
        g_tx_tail = (g_tx_tail + length) % kTxDmaBufferSize;
        ++g_rx_stats.tx_dma_done;
    }
    g_tx_dma_busy = false;
    g_tx_dma_active_len = 0U;
    const auto has_more = tx_ring_used_unlocked() != 0U;
    update_tx_stats_unlocked();
    leave_critical(primask);

    if (has_more) {
        (void)try_start_tx_dma();
    }
}

void abort_tx_dma(UART_HandleTypeDef* uart) {
    if (!is_console_uart(uart)) {
        return;
    }

    const auto primask = enter_critical();
    const auto length = g_tx_dma_active_len;
    if (g_tx_dma_busy && (length != 0U)) {
        g_tx_tail = (g_tx_tail + length) % kTxDmaBufferSize;
        g_rx_stats.tx_dropped_bytes += length;
    }
    g_tx_dma_busy = false;
    g_tx_dma_active_len = 0U;
    const auto has_more = tx_ring_used_unlocked() != 0U;
    update_tx_stats_unlocked();
    leave_critical(primask);

    if (has_more) {
        (void)try_start_tx_dma();
    }
}

bool poll_dma_byte(std::uint8_t& byte) {
    try_start_rx_dma();
    if (!g_rx_dma_active) {
        return false;
    }

    const auto write_pos = rx_dma_write_pos();
    g_rx_stats.dma_read_pos = g_rx_dma_read_pos;
    g_rx_stats.dma_write_pos = write_pos;
    g_rx_stats.dma_buffer_size = g_rx_dma_buffer.size();
    if (g_rx_dma_read_pos == write_pos) {
        return false;
    }

    invalidate_rx_cache_span(g_rx_dma_read_pos, write_pos);
    byte = g_rx_dma_buffer[g_rx_dma_read_pos];
    g_rx_dma_read_pos = (g_rx_dma_read_pos + 1U) % g_rx_dma_buffer.size();
    ++g_rx_stats.bytes;
    ++g_rx_stats.dma_bytes;
    g_rx_stats.last_byte = byte;
    g_rx_stats.dma_read_pos = g_rx_dma_read_pos;
    return true;
}

std::uint32_t poll_dma_bytes(std::span<std::uint8_t> bytes) {
    try_start_rx_dma();
    if (!g_rx_dma_active || bytes.empty()) {
        return 0U;
    }

    const auto write_pos = rx_dma_write_pos();
    g_rx_stats.dma_read_pos = g_rx_dma_read_pos;
    g_rx_stats.dma_write_pos = write_pos;
    g_rx_stats.dma_buffer_size = g_rx_dma_buffer.size();
    if (g_rx_dma_read_pos == write_pos) {
        return 0U;
    }

    const auto available = (g_rx_dma_read_pos < write_pos)
                               ? (write_pos - g_rx_dma_read_pos)
                               : (kRxDmaBufferSize - g_rx_dma_read_pos);
    const auto count = std::min<std::uint32_t>(available, static_cast<std::uint32_t>(bytes.size()));
    invalidate_rx_cache_range(g_rx_dma_read_pos, count);
    std::memcpy(bytes.data(), g_rx_dma_buffer.data() + g_rx_dma_read_pos, count);
    g_rx_dma_read_pos = (g_rx_dma_read_pos + count) % g_rx_dma_buffer.size();
    g_rx_stats.bytes += count;
    g_rx_stats.dma_bytes += count;
    g_rx_stats.last_byte = bytes[count - 1U];
    g_rx_stats.dma_read_pos = g_rx_dma_read_pos;
    return count;
}

bool poll_fallback_byte(std::uint8_t& byte) {
    auto* uart = h747::port::uart1_handle();
    if ((uart == nullptr) || (uart->Instance == nullptr)) {
        return false;
    }

    clear_uart_overrun(uart);
    if ((uart->Instance->ISR & UART_FLAG_RXNE) == 0U) {
        return false;
    }

    byte = static_cast<std::uint8_t>(uart->Instance->RDR);
    ++g_rx_stats.bytes;
    ++g_rx_stats.fallback_bytes;
    g_rx_stats.last_byte = byte;
    return true;
}

void write_hex_n(std::uint32_t value, const int nibbles) {
    constexpr char kHex[] = "0123456789ABCDEF";
    write("0x");
    for (int shift = (nibbles - 1) * 4; shift >= 0; shift -= 4) {
        write_char(kHex[(value >> shift) & 0xFU]);
    }
}

} // namespace

void write_char(const char c) {
    auto* uart = h747::port::uart1_handle();
    if ((uart == nullptr) || (uart->Instance == nullptr)) {
        return;
    }
    if (!tx_dma_can_run(uart)) {
        if (c == '\n') {
            blocking_write_byte('\r');
        }
        blocking_write_byte(static_cast<std::uint8_t>(c));
        return;
    }

    if (c == '\n') {
        (void)enqueue_tx_byte('\r');
    }
    (void)enqueue_tx_byte(static_cast<std::uint8_t>(c));
    (void)try_start_tx_dma();
}

void write(const char* text) {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        write_char(*text++);
    }
}

void write_line(const char* text) {
    write(text);
    write_char('\n');
}

void write_dec(std::uint32_t value) {
    if (value == 0U) {
        write_char('0');
        return;
    }

    char buf[12];
    int len = 0;
    while (value > 0U) {
        buf[len++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    }
    while (len > 0) {
        write_char(buf[--len]);
    }
}

void write_hex8(const std::uint8_t value) {
    write_hex_n(value, 2);
}

void write_hex16(const std::uint16_t value) {
    write_hex_n(value, 4);
}

void write_hex32(const std::uint32_t value) {
    write_hex_n(value, 8);
}

bool poll_line(char* buffer, const std::uint32_t capacity, std::uint32_t& length) {
    if ((buffer == nullptr) || (capacity == 0U)) {
        return false;
    }

    std::uint8_t raw = 0;
    while (poll_byte(raw)) {
        const char c = static_cast<char>(raw);
        if ((c == '\r') || (c == '\n')) {
            buffer[length] = '\0';
            write_char('\r');
            write_char('\n');
            length = 0U;
            ++g_rx_stats.lines;
            return true;
        }

        if ((c == 0x03) || (c == 0x15)) {
            length = 0U;
            write("^U");
            continue;
        }

        if ((c == '\b') || (c == 0x7F)) {
            if (length > 0U) {
                --length;
                write_char('\b');
                write_char(' ');
                write_char('\b');
            }
            continue;
        }

        if ((c >= 32) && (c < 127) && (length + 1U < capacity)) {
            buffer[length++] = c;
            write_char(c);
        }
    }

    return false;
}

bool poll_byte(std::uint8_t& byte) {
    if (poll_dma_byte(byte)) {
        return true;
    }
    if (g_rx_dma_active) {
        return false;
    }
    return poll_fallback_byte(byte);
}

std::uint32_t poll_bytes(std::span<std::uint8_t> bytes) {
    const auto count = poll_dma_bytes(bytes);
    if (count != 0U || g_rx_dma_active || bytes.empty()) {
        return count;
    }

    std::uint8_t byte = 0;
    if (!poll_fallback_byte(byte)) {
        return 0U;
    }
    bytes[0] = byte;
    return 1U;
}

RxStats rx_stats() {
    try_start_rx_dma();
    g_rx_stats.dma_read_pos = g_rx_dma_read_pos;
    g_rx_stats.dma_write_pos = rx_dma_write_pos();
    g_rx_stats.dma_buffer_size = g_rx_dma_buffer.size();
    const auto primask = enter_critical();
    update_tx_stats_unlocked();
    leave_critical(primask);
    return g_rx_stats;
}

} // namespace h747::console

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    h747::console::complete_tx_dma(huart);
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    if ((huart != nullptr) && (huart->hdmatx != nullptr) &&
        (huart->hdmatx->ErrorCode != HAL_DMA_ERROR_NONE)) {
        h747::console::abort_tx_dma(huart);
    }
}
