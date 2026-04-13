#include <cstdint>

import posix.programs.exec.tests;

namespace {
    struct UartCmsdk {
        static constexpr std::uint32_t base = 0x40004000u;
        static constexpr std::uint32_t data = base + 0x00u;
        static constexpr std::uint32_t state = base + 0x04u;
        static constexpr std::uint32_t ctrl = base + 0x08u;
        static constexpr std::uint32_t state_txbf = 1u << 1;
        static constexpr std::uint32_t ctrl_tx_enable = 1u << 0;
        static constexpr std::uint32_t ctrl_rx_enable = 1u << 1;

        static void init() noexcept {
            auto* reg = reinterpret_cast<volatile std::uint32_t*>(ctrl);
            *reg = ctrl_tx_enable | ctrl_rx_enable;
        }

        static void write_byte(char ch) noexcept {
            auto* status = reinterpret_cast<volatile std::uint32_t*>(state);
            auto* out = reinterpret_cast<volatile std::uint32_t*>(data);
            while ((*status & state_txbf) != 0u) {
            }
            *out = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
        }

        static void write(const char* text) noexcept {
            static bool inited = false;
            if (!inited) {
                init();
                inited = true;
            }
            if (!text) return;
            while (*text) {
                write_byte(*text++);
            }
        }
    };

    inline void log_line(const char* msg) noexcept {
        UartCmsdk::write(msg);
        UartCmsdk::write("\n");
    }
}

extern "C" void posix_smoke_emit(const char* msg) noexcept {
    log_line(msg);
}

int main() {
    log_line("[posix-smoke] begin");
    run_posix_program_stdio_smoke_tests();
    log_line("[posix-smoke] end ok");
    while (true) {
    }
    return 0;
}
