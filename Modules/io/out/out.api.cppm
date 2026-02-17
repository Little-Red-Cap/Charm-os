module;
#include <cstdint>
#include <expected>
#include <string_view>
#include <utility>
export module out.api;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: (re-export only) out.core/out.sink/out.format/out.domain/out.port/out.ansi/out.logger
// Forbidden out.* imports: (implementation should stay empty or thin wrappers only)
// Rationale: public facade; must not reintroduce a second behavior path.
// If you need functionality from a higher layer, add an extension point in this layer instead.

export import out.ansi;
export import out.core;
export import out.domain;
export import out.format;
export import out.logger;
export import out.port;
export import out.sink;

#if defined(OUT_ERROR_PROPAGATE)
#define OUT_API_NODISCARD [[nodiscard]]
#else
#define OUT_API_NODISCARD
#endif

export namespace out {
    template <level L, class Domain = default_domain>
    inline auto log() noexcept {
        return logger<L, Domain, detail::sink_ref<port::console_sink>>{
            detail::sink_ref<port::console_sink>{&port::default_console()}
        };
    }

    template <level L, class Domain = default_domain, class S>
    inline auto log(S& s) noexcept {
        return logger<L, Domain, detail::sink_ref<S>>{detail::sink_ref<S>{&s}};
    }

    inline auto raw() noexcept {
        auto out = logger<level::info, default_domain, detail::sink_ref<port::console_sink>, true>{
            detail::sink_ref<port::console_sink>{&port::default_console()}
        };
        out.level_prefix(false);
        out.domain_prefix(false);
        out.no_flush();
        return out;
    }

    template <class S>
    inline auto raw(S& s) noexcept {
        auto out = logger<level::info, default_domain, detail::sink_ref<S>, true>{
            detail::sink_ref<S>{&s}
        };
        out.level_prefix(false);
        out.domain_prefix(false);
        out.no_flush();
        return out;
    }

    template <level L, class Domain = default_domain>
    inline auto logc() noexcept {
        return log<L, Domain>().template ansi<true>();
    }

    template <level L, class Domain = default_domain, class S>
    inline auto logc(S& s) noexcept {
        return log<L, Domain>(s).template ansi<true>();
    }

    // Raw formatter entry (no prefixes).
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_print(S& s, Args&&... a) noexcept {
        return raw(s).template try_print<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_print(Args&&... a) noexcept {
        return raw().template try_print<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_println(S& s, Args&&... a) noexcept {
        return raw(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_println(Args&&... a) noexcept {
        return raw().template try_println<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> print(S& s, Args&&... a) noexcept {
        auto r = raw(s).template try_print<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> print(Args&&... a) noexcept {
        auto r = raw().template try_print<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> println(S& s, Args&&... a) noexcept {
        auto r = raw(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> println(Args&&... a) noexcept {
        auto r = raw().template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    // Unified entry: level + domain.
    template <level L, class Domain, fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_emit(S& sink, Args&&... args) noexcept {
        return log<L, Domain>(sink).template try_print<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, class... Args>
    inline result<std::size_t> try_emit(Args&&... args) noexcept {
        return log<L, Domain>().template try_print<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_emitln(S& sink, Args&&... args) noexcept {
        return log<L, Domain>(sink).template try_println<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, class... Args>
    inline result<std::size_t> try_emitln(Args&&... args) noexcept {
        return log<L, Domain>().template try_println<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> emit(S& sink, Args&&... args) noexcept {
        auto r = log<L, Domain>(sink).template try_println<Fmt>(std::forward<Args>(args)...);
        return detail::finalize(r);
    }

    template <level L, class Domain, fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> emit(Args&&... args) noexcept {
        auto r = log<L, Domain>().template try_println<Fmt>(std::forward<Args>(args)...);
        return detail::finalize(r);
    }

    // Convenience overloads: default_domain.
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_error(S& s, Args&&... a) noexcept {
        return log<level::error>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_error(Args&&... a) noexcept {
        return log<level::error>().template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> error(S& s, Args&&... a) noexcept {
        auto r = log<level::error>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> error(Args&&... a) noexcept {
        auto r = log<level::error>().template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_warn(S& s, Args&&... a) noexcept {
        return log<level::warn>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_warn(Args&&... a) noexcept {
        return log<level::warn>().template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> warn(S& s, Args&&... a) noexcept {
        auto r = log<level::warn>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> warn(Args&&... a) noexcept {
        auto r = log<level::warn>().template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_info(S& s, Args&&... a) noexcept {
        return log<level::info>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_info(Args&&... a) noexcept {
        return log<level::info>().template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> info(S& s, Args&&... a) noexcept {
        auto r = log<level::info>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> info(Args&&... a) noexcept {
        auto r = log<level::info>().template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_debug(S& s, Args&&... a) noexcept {
        return log<level::debug>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_debug(Args&&... a) noexcept {
        return log<level::debug>().template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> debug(S& s, Args&&... a) noexcept {
        auto r = log<level::debug>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> debug(Args&&... a) noexcept {
        auto r = log<level::debug>().template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_trace(S& s, Args&&... a) noexcept {
        return log<level::trace>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_trace(Args&&... a) noexcept {
        return log<level::trace>().template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> trace(S& s, Args&&... a) noexcept {
        auto r = log<level::trace>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> trace(Args&&... a) noexcept {
        auto r = log<level::trace>().template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

}

#undef OUT_API_NODISCARD
