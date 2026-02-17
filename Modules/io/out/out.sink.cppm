module;
#include <span>
#include <array>
#include <string_view>
#include <expected>
#include <cstring>

export module out.sink;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: out.core
// Forbidden out.* imports: out.format, out.ansi, out.logger, out.api, out.port, out.print
// Rationale: low-level I/O concept + basic sinks. Must stay formatting-agnostic.
// If you need functionality from a higher layer, add an extension point in this layer instead.

import out.core;
// TODO: add compile-time endian conversion helpers.
export namespace out {

    // Sink concept: any type that implements write(bytes).
    template <class S>
    concept Sink = requires(S& s, bytes b) {
        { s.write(b) } -> std::same_as<result<std::size_t>>;
    };

    template <class S>
    concept Flushable = requires(S& s) {
        { s.flush() } -> std::same_as<result<std::size_t>>;
    } || requires(const S& s) {
        { s.flush() } -> std::same_as<result<std::size_t>>;
    };

    // Convenience: write from string_view.
    template <Sink S>
    inline result<std::size_t> write(S& s, std::string_view sv) noexcept {
        auto b = bytes{reinterpret_cast<const std::byte*>(sv.data()), sv.size()};
        return s.write(b);
    }

    // ------------------------------------------------------------------
    // Lightweight adapters (no separate out.sinks module).

    // 1) Fixed-size buffer: useful for tests/output capture.
    // Intended for testing, logging capture, or diagnostics.
    // Not suitable as a general-purpose streaming sink.
    template <std::size_t N>
    struct buffer_sink {
        std::array<char, N> buf{};
        std::size_t pos = 0;

        result<std::size_t> write(bytes b) noexcept {
            if (pos + b.size() > N) return std::unexpected(errc::buffer_overflow);
            std::memcpy(buf.data() + pos, b.data(), b.size());
            pos += b.size();
            return ok(b.size());
        }

        std::string_view view() const noexcept { return {buf.data(), pos}; }
        void clear() noexcept { pos = 0; }
    };

    // 2) Dev sink: capture output + metrics for tests/diagnostics.
    // Intended for testing and profiling write behavior; not for production streaming.
    template <std::size_t N>
    struct dev_sink {
        std::array<char, N> buf{};
        std::size_t pos = 0;
        std::size_t bytes_calls = 0;
        std::size_t ansi_calls = 0;
        std::size_t flush_calls = 0;
        std::size_t bytes_total = 0;
        errc last_err = errc::ok;

        result<std::size_t> write(bytes b) noexcept {
            ++bytes_calls;
            return write_bytes(b);
        }

        result<std::size_t> write_ansi(std::string_view sv) noexcept {
            ++ansi_calls;
            auto b = bytes{reinterpret_cast<const std::byte*>(sv.data()), sv.size()};
            return write_bytes(b);
        }

        result<std::size_t> flush() noexcept {
            ++flush_calls;
            return ok<std::size_t>(0u);
        }

        std::string_view view() const noexcept { return {buf.data(), pos}; }
        void clear() noexcept { pos = 0; }
        void reset() noexcept { clear(); reset_metrics(); }
        std::size_t write_calls_total() const noexcept { return bytes_calls + ansi_calls; }
        void reset_metrics() noexcept {
            bytes_calls = 0;
            ansi_calls = 0;
            flush_calls = 0;
            bytes_total = 0;
            last_err = errc::ok;
        }

      private:
        result<std::size_t> write_bytes(bytes b) noexcept {
            if (pos + b.size() > N) {
                last_err = errc::buffer_overflow;
                return std::unexpected(errc::buffer_overflow);
            }
            std::memcpy(buf.data() + pos, b.data(), b.size());
            pos += b.size();
            bytes_total += b.size();
            return ok(b.size());
        }
    };

    // 3) Null sink: silent output for baselines.
    struct null_sink {
        result<std::size_t> write(bytes b) noexcept { return ok(b.size()); }
    };

    // 4) Line buffer: flushes on newline (useful for UART/slow devices).
    template <Sink BaseSink, std::size_t BufSize = 128>
    struct line_buffered_sink {
        BaseSink& base;
        std::array<char, BufSize> buf{};
        std::size_t pos = 0;

        explicit line_buffered_sink(BaseSink& s) : base(s) {}

        result<std::size_t> write(bytes b) noexcept {
            auto data = reinterpret_cast<const char*>(b.data());
            for (std::size_t i = 0; i < b.size(); ++i) {
                if (pos >= BufSize) {
                    auto r = flush();
                    if (!r) return std::unexpected(r.error());
                }

                buf[pos++] = data[i];

                if (data[i] == '\n') {
                    auto r = flush();
                    if (!r) return std::unexpected(r.error());
                }
            }
            return ok(b.size());
        }

        result<std::size_t> flush() noexcept {
            // Note: flush() only flushes the line buffer to the base sink.
            // It does NOT call base.flush().
            if (pos == 0) return ok<std::size_t>(0u);
            auto b = bytes{reinterpret_cast<const std::byte*>(buf.data()), pos};
            auto r = base.write(b);
            if (r) pos = 0;
            return r;
        }

        // Destructor flushes best-effort; errors are intentionally ignored.
        ~line_buffered_sink() { (void)flush(); }
    };

}
