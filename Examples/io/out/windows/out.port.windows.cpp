module;
#include <atomic>
#include <chrono>
#include <cstdio>
#include <expected>
#include <windows.h>

module out.port;
import out.core;


namespace out::port {
    static std::atomic<console_sink*> g_default_console{nullptr};

    namespace detail {
        inline void enable_vt() noexcept {
            static std::atomic<bool> init{false};
            bool expected = false;
            if (!init.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

            HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
            if (h == INVALID_HANDLE_VALUE || h == nullptr) return;

            DWORD mode = 0;
            if (!::GetConsoleMode(h, &mode)) return;
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            ::SetConsoleMode(h, mode);
        }
    }

    void set_default_console(console_sink* p) noexcept {
        g_default_console.store(p, std::memory_order_release);
    }

    console_sink& default_console() noexcept {
        if (auto* p = g_default_console.load(std::memory_order_acquire)) return *p;
        static console_sink inst{};
        return inst;
    }

    result<std::size_t> console_sink::write(const bytes b) noexcept {
        detail::enable_vt();
        auto n = std::fwrite(b.data(), 1, b.size(), stdout);
        if (n != b.size()) return std::unexpected(errc::io_error);
        return ok(n);
    }

    result<std::size_t> console_sink::flush() noexcept {
        if (0 != std::fflush(stdout)) return std::unexpected(errc::io_error);
        return ok(0u);
    }

    result<std::size_t> uart_sink::write(const bytes b) const noexcept {
        auto* f = static_cast<std::FILE*>(handle);
        if (!f) return std::unexpected(errc::io_error);
        auto n = std::fwrite(b.data(), 1, b.size(), f);
        if (n != b.size()) return std::unexpected(errc::io_error);
        return ok(n);
    }

    tick_t now_ms() noexcept {
        using namespace std::chrono;
        auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        return static_cast<tick_t>(ms);
    }

}
