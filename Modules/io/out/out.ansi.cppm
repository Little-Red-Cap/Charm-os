module;
#include <charconv>
#include <cstdint>
#include <expected>
#include <string_view>

export module out.ansi;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: out.core, out.sink, out.format
// Forbidden out.* imports: out.logger, out.api, out.port, out.print, out.domain
// Rationale: ANSI tokens are just formattable values; keep logger/ports out.
// If you need functionality from a higher layer, add an extension point in this layer instead.

import out.core;
import out.sink;
import out.format; // fmt_spec + write_one protocol

export namespace out {
    struct reset_t {};
    struct bold_t {};
    struct dim_t {};
    struct italic_t {};
    struct underline_t {};
}

export namespace out::ansi {
    // Color/control tokens are values and can be composed via format/print.
    enum class color : std::uint8_t { default_, black, red, green, yellow, blue, magenta, cyan, white };

    inline constexpr reset_t reset{};
    inline constexpr bold_t  bold{};
    inline constexpr dim_t dim{};
    inline constexpr italic_t italic{};
    inline constexpr underline_t underline{};

    struct fg_t { color c; };
    struct bg_t { color c; };
    constexpr fg_t fg(color c) noexcept { return {c}; }
    constexpr bg_t bg(color c) noexcept { return {c}; }

    // Capability wrapper: ansi_sink_ref holds a pointer and does not copy Base.
    // When Enabled is false, ANSI writes compile away to zero (no runtime branch).
    template <class Base, bool Enabled = true>
    struct ansi_sink_ref {
        Base* base{};

        // Forward regular writes so the wrapper still satisfies Sink.
        result<std::size_t> write(bytes b) noexcept { return base->write(b); }
        result<std::size_t> write(bytes b) const noexcept { return base->write(b); }

        // ANSI write capability (only available on this wrapper).
        result<std::size_t> write_ansi(std::string_view sv) noexcept {
            if constexpr (Enabled) {
                if constexpr (requires(Base& b, std::string_view s) {
                    { b.write_ansi(s) } -> std::same_as<result<std::size_t>>;
                }) {
                    return base->write_ansi(sv);
                } else {
                    return out::write(*base, sv);
                }
            } else {
                return out::ok<std::size_t>(0u);
            }
        }
        result<std::size_t> write_ansi(std::string_view sv) const noexcept {
            if constexpr (Enabled) {
                if constexpr (requires(const Base& b, std::string_view s) {
                    { b.write_ansi(s) } -> std::same_as<result<std::size_t>>;
                }) {
                    return base->write_ansi(sv);
                } else {
                    return out::write(*base, sv);
                }
            } else {
                return out::ok<std::size_t>(0u);
            }
        }
    };

    template <class S>
    using sink_ref_t = ansi_sink_ref<std::remove_reference_t<S>>;

    // Sugar: ansi(sink) enables ANSI output by default.
    template <class S>
    constexpr auto enable(S& s) noexcept -> ansi_sink_ref<S, true> { return { &s }; }

    // Sugar: ansi<false>(sink) disables ANSI output (compile-time no-op).
    template <bool Enabled, class S>
    constexpr auto with(S& s) noexcept -> ansi_sink_ref<S, Enabled> { return { &s }; }

    // Internal helpers.
    namespace detail {
        // Concept: only sinks with write_ansi can consume ANSI tokens.
        template <class S>
        concept AnsiSink = requires(S& s, std::string_view sv) {
            { s.write_ansi(sv) } -> std::same_as<result<std::size_t>>;
        };

        // Build ESC[...]m code without heap allocation.
        constexpr int fg_code(color c) noexcept {
            switch (c) {
            case color::black: return 30;
            case color::red: return 31;
            case color::green: return 32;
            case color::yellow: return 33;
            case color::blue: return 34;
            case color::magenta: return 35;
            case color::cyan: return 36;
            case color::white: return 37;
            default: return 39;
            }
        }
        constexpr int bg_code(color c) noexcept { return (fg_code(c) - 30) + 40; }

        template <AnsiSink S>
        inline result<std::size_t> write_code(S& s, int code) noexcept {
            char  buf[16];
            char* p = buf;

            *p++ = '\x1b';
            *p++ = '[';

            auto [ptr, ec] = std::to_chars(p, buf + sizeof(buf), code);
            if (ec != std::errc{}) return std::unexpected(errc::buffer_overflow);

            *ptr++ = 'm';
            return s.write_ansi(std::string_view{buf, static_cast<std::size_t>(ptr - buf)});
        }
    } // namespace detail

    // Public helpers for ANSI-aware emitters.
    template <class S>
    concept AnsiSink = detail::AnsiSink<S>;

    constexpr int fg_code(color c) noexcept { return detail::fg_code(c); }
    constexpr int bg_code(color c) noexcept { return detail::bg_code(c); }

    template <AnsiSink S>
    inline result<std::size_t> write_ansi_code(S& s, int code) noexcept {
        return detail::write_code(s, code);
    }

    // write_one only applies to sinks that support write_ansi.
    template <AnsiSink S>
    inline result<std::size_t> write_one(S& s, reset_t, fmt_spec) noexcept { return s.write_ansi("\x1b[0m"); }

    template <AnsiSink S>
    inline result<std::size_t> write_one(S& s, bold_t, fmt_spec) noexcept { return s.write_ansi("\x1b[1m"); }

    template <AnsiSink S>
    inline result<std::size_t> write_one(S& s, dim_t, fmt_spec) noexcept { return s.write_ansi("\x1b[2m"); }

    template <AnsiSink S>
    inline result<std::size_t> write_one(S& s, italic_t, fmt_spec) noexcept { return s.write_ansi("\x1b[3m"); }

    template <AnsiSink S>
    inline result<std::size_t> write_one(S& s, underline_t, fmt_spec) noexcept { return s.write_ansi("\x1b[4m"); }

    template <AnsiSink S>
    inline result<std::size_t> write_one(S& s, fg_t v, fmt_spec) noexcept { return detail::write_code(s, detail::fg_code(v.c)); }

    template <AnsiSink S>
    inline result<std::size_t> write_one(S& s, bg_t v, fmt_spec) noexcept { return detail::write_code(s, detail::bg_code(v.c)); }
} // namespace out::ansi

// Non-ANSI sinks: treat ANSI tokens as no-op (default "silent" behavior).
// Note: keep these overloads here to avoid pulling ANSI types into out.format
// and creating a module dependency cycle.
export namespace out {
    template <class S>
    requires (!ansi::AnsiSink<S>)
    inline result<std::size_t> write_one(S&, reset_t, fmt_spec) noexcept { return ok<std::size_t>(0u); }

    template <class S>
    requires (!ansi::AnsiSink<S>)
    inline result<std::size_t> write_one(S&, bold_t, fmt_spec) noexcept { return ok<std::size_t>(0u); }

    template <class S>
    requires (!ansi::AnsiSink<S>)
    inline result<std::size_t> write_one(S&, dim_t, fmt_spec) noexcept { return ok<std::size_t>(0u); }

    template <class S>
    requires (!ansi::AnsiSink<S>)
    inline result<std::size_t> write_one(S&, italic_t, fmt_spec) noexcept { return ok<std::size_t>(0u); }

    template <class S>
    requires (!ansi::AnsiSink<S>)
    inline result<std::size_t> write_one(S&, underline_t, fmt_spec) noexcept { return ok<std::size_t>(0u); }

    template <class S>
    requires (!ansi::AnsiSink<S>)
    inline result<std::size_t> write_one(S&, ansi::fg_t, fmt_spec) noexcept { return ok<std::size_t>(0u); }

    template <class S>
    requires (!ansi::AnsiSink<S>)
    inline result<std::size_t> write_one(S&, ansi::bg_t, fmt_spec) noexcept { return ok<std::size_t>(0u); }

    // Formatter specializations for ANSI tokens.
    template <>
    struct formatter<reset_t> {
        template <class S>
        static result<std::size_t> write(S& s, reset_t, fmt_spec) noexcept {
            if constexpr (ansi::AnsiSink<S>) return s.write_ansi("\x1b[0m");
            return ok<std::size_t>(0u);
        }
    };

    template <>
    struct formatter<bold_t> {
        template <class S>
        static result<std::size_t> write(S& s, bold_t, fmt_spec) noexcept {
            if constexpr (ansi::AnsiSink<S>) return s.write_ansi("\x1b[1m");
            return ok<std::size_t>(0u);
        }
    };

    template <>
    struct formatter<dim_t> {
        template <class S>
        static result<std::size_t> write(S& s, dim_t, fmt_spec) noexcept {
            if constexpr (ansi::AnsiSink<S>) return s.write_ansi("\x1b[2m");
            return ok<std::size_t>(0u);
        }
    };

    template <>
    struct formatter<italic_t> {
        template <class S>
        static result<std::size_t> write(S& s, italic_t, fmt_spec) noexcept {
            if constexpr (ansi::AnsiSink<S>) return s.write_ansi("\x1b[3m");
            return ok<std::size_t>(0u);
        }
    };

    template <>
    struct formatter<underline_t> {
        template <class S>
        static result<std::size_t> write(S& s, underline_t, fmt_spec) noexcept {
            if constexpr (ansi::AnsiSink<S>) return s.write_ansi("\x1b[4m");
            return ok<std::size_t>(0u);
        }
    };

    template <>
    struct formatter<ansi::fg_t> {
        template <class S>
        static result<std::size_t> write(S& s, ansi::fg_t v, fmt_spec) noexcept {
            if constexpr (ansi::AnsiSink<S>) return ansi::detail::write_code(s, ansi::detail::fg_code(v.c));
            return ok<std::size_t>(0u);
        }
    };

    template <>
    struct formatter<ansi::bg_t> {
        template <class S>
        static result<std::size_t> write(S& s, ansi::bg_t v, fmt_spec) noexcept {
            if constexpr (ansi::AnsiSink<S>) return ansi::detail::write_code(s, ansi::detail::bg_code(v.c));
            return ok<std::size_t>(0u);
        }
    };
}

export namespace out {
    // ansi_sink_ref emits byte sequences, so ANSI can be treated as bytes.
    template <class Base, bool Enabled>
    inline constexpr bool ansi_is_bytes_v<ansi::ansi_sink_ref<Base, Enabled>> = true;
}

// ===== Sugar: re-export common ANSI tokens into out namespace =====
export namespace out {
    using ansi::color;
    using ansi::reset;
    using ansi::bold;
    using ansi::dim;
    using ansi::italic;
    using ansi::underline;
    using ansi::fg;
    using ansi::bg;

    template <bool Enabled = true, class S>
    constexpr auto ansi_with(S& s) noexcept { return ansi::with<Enabled>(s); }

    constexpr ansi::fg_t operator!(color c) noexcept { return {c}; }  // !color::red => foreground
    constexpr ansi::bg_t operator~(color c) noexcept { return {c}; }  // ~color::red => background
}
