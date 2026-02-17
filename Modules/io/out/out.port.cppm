module;
#include <cstddef>
#include <cstdint>

export module out.port;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: out.core, out.sink
// Forbidden out.* imports: out.format, out.ansi, out.logger, out.api, out.print, out.domain
// Rationale: platform I/O backend + default console plumbing. Must not pull formatting/logging.
// If you need functionality from a higher layer, add an extension point in this layer instead.

import out.core;
import out.sink;

export namespace out::port {

    // Platform console output (PC: stdout; embedded: SWO/ITM/etc).
    struct console_sink {
        result<std::size_t> write(bytes b) noexcept;
        result<std::size_t> flush() noexcept;
    };

    // Returns a reference to the active console sink.
    // set_default_console(nullptr) restores the built-in default console sink.
    console_sink& default_console() noexcept;
    // The pointer is not owned; it must remain valid for the duration of use.
    // Reads are lock-free; pointer-sized access is assumed atomic on the target.
    void set_default_console(console_sink* p) noexcept;

    // Serial output (STM32: handle = UART_HandleTypeDef*; PC: handle = FILE*).
    struct uart_sink {
        void* handle{};
        result<std::size_t> write(bytes b) const noexcept;
    };

    // Optional time source for timestamps and profiling.
    using tick_t = std::uint64_t;
    // Returns a monotonic millisecond tick; may wrap depending on platform.
    tick_t now_ms() noexcept;

}
