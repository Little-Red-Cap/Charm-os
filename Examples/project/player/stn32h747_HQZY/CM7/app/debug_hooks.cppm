module;

#include <cstdio>
#include <cstring>

#include "stm32h7xx_hal.h"

export module player.stm32h7.debug_hooks;

import init.node;
import util.core;

extern "C" UART_HandleTypeDef huart1;

namespace {
    void debug_uart_print(const char* msg) noexcept {
        if (!msg || msg[0] == '\0') return;
        const auto len = static_cast<uint16_t>(std::strlen(msg));
        (void)HAL_UART_Transmit(&huart1, reinterpret_cast<uint8_t*>(const_cast<char*>(msg)), len, 100);
    }
}

extern "C" void charm_init_debug_duplicate_cap(util::u32 cap) noexcept {
    struct CapName {
        util::u32 id;
        const char* name;
    };
    static constexpr CapName kCaps[] = {
        {init::cap_id("system.clock"), "system.clock"},
        {init::cap_id("io.registry"), "io.registry"},
        {init::cap_id("block.registry"), "block.registry"},
        {init::cap_id("io.reactor"), "io.reactor"},
        {init::cap_id("kernel.eda"), "kernel.eda"},
        {init::cap_id("system.reactor_pump"), "system.reactor_pump"},
        {init::cap_id("platform.irq"), "platform.irq"},
        {init::cap_id("hal.uart1"), "hal.uart1"},
        {init::cap_id("io.uart1"), "io.uart1"},
        {init::cap_id("io.console0"), "io.console0"},
        {init::cap_id("block.sd0"), "block.sd0"},
        {init::cap_id("block.flash0"), "block.flash0"},
        {init::cap_id("hal.spi1"), "hal.spi1"},
        {init::cap_id("hal.i2c1"), "hal.i2c1"},
    };
    const char* hit = "unknown";
    for (const auto& entry : kCaps) {
        if (entry.id == cap) {
            hit = entry.name;
            break;
        }
    }
    char buf[80]{};
    const int n = std::snprintf(buf, sizeof(buf),
        "dbg: dup cap=0x%08lX %s\n",
        static_cast<unsigned long>(cap), hit);
    if (n > 0) {
        debug_uart_print(buf);
    }
}

extern "C" void charm_io_registry_debug_exist(const char* name, util::u32 cap) noexcept {
    char buf[96]{};
    const char* n = (name && *name) ? name : "unknown";
    const int r = std::snprintf(buf, sizeof(buf),
        "dbg: io exist cap=0x%08lX %s\n",
        static_cast<unsigned long>(cap), n);
    if (r > 0) {
        debug_uart_print(buf);
    }
}

extern "C" void charm_block_registry_debug_exist(const char* name, util::u32 cap) noexcept {
    char buf[96]{};
    const char* n = (name && *name) ? name : "unknown";
    const int r = std::snprintf(buf, sizeof(buf),
        "dbg: block exist cap=0x%08lX %s\n",
        static_cast<unsigned long>(cap), n);
    if (r > 0) {
        debug_uart_print(buf);
    }
}
