module;
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <string_view>
#include <utility>
export module out.api;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: (re-export only) out.core/out.sink/out.format/out.domain/out.channel/out.ansi/out.logger
// Forbidden out.* imports: (implementation should stay empty or thin wrappers only)
// Rationale: public facade; must not reintroduce a second behavior path.
// If you need functionality from a higher layer, add an extension point in this layer instead.

export import out.ansi;
export import out.channel;
export import out.core;
export import out.domain;
export import out.format;
export import out.logger;
export import out.sink;

#if defined(OUT_ERROR_PROPAGATE)
#define OUT_API_NODISCARD [[nodiscard]]
#else
#define OUT_API_NODISCARD
#endif

export namespace out {
    template <level L, class Domain, class S>
    inline auto log(S& s) noexcept;

    namespace ctx_detail {
        using write_fn = result<std::size_t> (*)(void*, bytes) noexcept;
        using flush_fn = result<std::size_t> (*)(void*) noexcept;

        template <class S>
        inline result<std::size_t> write_impl(void* ctx, bytes b) noexcept {
            return static_cast<S*>(ctx)->write(b);
        }

        template <class S>
        inline result<std::size_t> flush_impl(void* ctx) noexcept {
            return static_cast<S*>(ctx)->flush();
        }

        template <class S>
        inline flush_fn flush_or_null() noexcept {
            if constexpr (Flushable<S>) {
                return &flush_impl<S>;
            }
            return nullptr;
        }

        struct bound_sink {
            void* ctx{};
            write_fn write_cb{};
            flush_fn flush_cb{};

            result<std::size_t> write(bytes b) noexcept {
                if (!write_cb) return result<std::size_t>{std::unexpected(errc::bad_state)};
                return write_cb(ctx, b);
            }

            result<std::size_t> flush() noexcept {
                if (flush_cb) return flush_cb(ctx);
                return ok<std::size_t>(0u);
            }
        };

        struct context_state {
            bound_sink sink{};
            TimestampFn ts_fn = nullptr;
            void* ts_ctx = nullptr;
        };

        inline context_state g_ctx{};

        inline bool is_bound() noexcept { return g_ctx.sink.write_cb != nullptr; }

        inline bool bound_or_abort() noexcept {
#ifndef NDEBUG
            if (!is_bound()) std::abort();
#endif
            return is_bound();
        }

        template <class T>
        inline detail::public_return_t<T> unbound_return() noexcept {
            if constexpr (build_error_policy == error_policy::propagate) {
                return ok<T>(0u);
            } else {
                return;
            }
        }

        template <level L, class Domain>
        inline auto ctx_logger() noexcept {
            auto lg = log<L, Domain>(g_ctx.sink);
            if (g_ctx.ts_fn) lg.timestamp(g_ctx.ts_fn, g_ctx.ts_ctx);
            return lg;
        }
    }

    template <level L, class Domain = default_domain, class S>
    inline auto log(S& s) noexcept {
        return logger<L, Domain, detail::sink_ref<S>>{detail::sink_ref<S>{&s}};
    }

    template <class S>
    inline auto raw(S& s) noexcept {
        auto out = logger<level::info, raw_domain, detail::sink_ref<S>, true>{
            detail::sink_ref<S>{&s}
        };
        out.level_prefix(false);
        out.domain_prefix(false);
        out.no_flush();
        return out;
    }

    template <level L, class Domain = default_domain, class S>
    inline auto logc(S& s) noexcept {
        return log<L, Domain>(s).template ansi<true>();
    }

    template <Sink S>
    inline void bind(S& s, TimestampFn ts_fn = nullptr, void* ts_ctx = nullptr) noexcept {
        ctx_detail::g_ctx.sink = {
            &s,
            &ctx_detail::write_impl<S>,
            ctx_detail::flush_or_null<S>()
        };
        ctx_detail::g_ctx.ts_fn = ts_fn;
        ctx_detail::g_ctx.ts_ctx = ts_ctx;
    }

    inline void unbind() noexcept { ctx_detail::g_ctx = {}; }
    inline bool bound() noexcept { return ctx_detail::is_bound(); }

    inline void set_timestamp(TimestampFn ts_fn, void* ts_ctx = nullptr) noexcept {
        ctx_detail::g_ctx.ts_fn = ts_fn;
        ctx_detail::g_ctx.ts_ctx = ts_ctx;
    }

    inline void clear_timestamp() noexcept {
        ctx_detail::g_ctx.ts_fn = nullptr;
        ctx_detail::g_ctx.ts_ctx = nullptr;
    }

    struct Scope {
        ctx_detail::context_state prev{};

        template <Sink S>
        explicit Scope(S& s, TimestampFn ts_fn = nullptr, void* ts_ctx = nullptr) noexcept
            : prev(ctx_detail::g_ctx) {
            bind(s, ts_fn, ts_ctx);
        }

        ~Scope() { ctx_detail::g_ctx = prev; }
    };

    // Raw formatter entry (no prefixes).
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_print(S& s, Args&&... a) noexcept {
        return raw(s).template try_print<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_println(S& s, Args&&... a) noexcept {
        return raw(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> print(S& s, Args&&... a) noexcept {
        auto r = raw(s).template try_print<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> println(S& s, Args&&... a) noexcept {
        auto r = raw(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    // Raw formatter entry (bound sink).
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_print(Args&&... a) noexcept {
        if (!ctx_detail::is_bound()) return result<std::size_t>{std::unexpected(errc::bad_state)};
        return raw(ctx_detail::g_ctx.sink).template try_print<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_println(Args&&... a) noexcept {
        if (!ctx_detail::is_bound()) return result<std::size_t>{std::unexpected(errc::bad_state)};
        return raw(ctx_detail::g_ctx.sink).template try_println<Fmt>(std::forward<Args>(a)...);
    }

    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> print(Args&&... a) noexcept {
        if (!ctx_detail::bound_or_abort()) return ctx_detail::unbound_return<std::size_t>();
        auto r = raw(ctx_detail::g_ctx.sink).template try_print<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> println(Args&&... a) noexcept {
        if (!ctx_detail::bound_or_abort()) return ctx_detail::unbound_return<std::size_t>();
        auto r = raw(ctx_detail::g_ctx.sink).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    // Unified entry: level + domain.
    template <level L, class Domain, fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_emit(S& sink, Args&&... args) noexcept {
        return log<L, Domain>(sink).template try_print<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_emitln(S& sink, Args&&... args) noexcept {
        return log<L, Domain>(sink).template try_println<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> emit(S& sink, Args&&... args) noexcept {
        auto r = log<L, Domain>(sink).template try_println<Fmt>(std::forward<Args>(args)...);
        return detail::finalize(r);
    }

    template <level L, class Domain, fixed_string Fmt, class... Args>
    inline result<std::size_t> try_emit(Args&&... args) noexcept {
        if (!ctx_detail::is_bound()) return result<std::size_t>{std::unexpected(errc::bad_state)};
        return ctx_detail::ctx_logger<L, Domain>().template try_print<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, class... Args>
    inline result<std::size_t> try_emitln(Args&&... args) noexcept {
        if (!ctx_detail::is_bound()) return result<std::size_t>{std::unexpected(errc::bad_state)};
        return ctx_detail::ctx_logger<L, Domain>().template try_println<Fmt>(std::forward<Args>(args)...);
    }

    template <level L, class Domain, fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> emit(Args&&... args) noexcept {
        if (!ctx_detail::bound_or_abort()) return ctx_detail::unbound_return<std::size_t>();
        auto r = ctx_detail::ctx_logger<L, Domain>().template try_println<Fmt>(std::forward<Args>(args)...);
        return detail::finalize(r);
    }

    // Convenience overloads: default_domain.
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_error(S& s, Args&&... a) noexcept {
        return log<level::error>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> error(S& s, Args&&... a) noexcept {
        auto r = log<level::error>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_warn(S& s, Args&&... a) noexcept {
        return log<level::warn>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> warn(S& s, Args&&... a) noexcept {
        auto r = log<level::warn>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_info(S& s, Args&&... a) noexcept {
        return log<level::info>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> info(S& s, Args&&... a) noexcept {
        auto r = log<level::info>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_debug(S& s, Args&&... a) noexcept {
        return log<level::debug>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> debug(S& s, Args&&... a) noexcept {
        auto r = log<level::debug>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    inline result<std::size_t> try_trace(S& s, Args&&... a) noexcept {
        return log<level::trace>(s).template try_println<Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, Sink S, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> trace(S& s, Args&&... a) noexcept {
        auto r = log<level::trace>(s).template try_println<Fmt>(std::forward<Args>(a)...);
        return detail::finalize(r);
    }

    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_error(Args&&... a) noexcept {
        return try_emitln<level::error, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> error(Args&&... a) noexcept {
        return emit<level::error, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_warn(Args&&... a) noexcept {
        return try_emitln<level::warn, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> warn(Args&&... a) noexcept {
        return emit<level::warn, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_info(Args&&... a) noexcept {
        return try_emitln<level::info, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> info(Args&&... a) noexcept {
        return emit<level::info, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_debug(Args&&... a) noexcept {
        return try_emitln<level::debug, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> debug(Args&&... a) noexcept {
        return emit<level::debug, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    inline result<std::size_t> try_trace(Args&&... a) noexcept {
        return try_emitln<level::trace, default_domain, Fmt>(std::forward<Args>(a)...);
    }
    template <fixed_string Fmt, class... Args>
    OUT_API_NODISCARD inline detail::public_return_t<std::size_t> trace(Args&&... a) noexcept {
        return emit<level::trace, default_domain, Fmt>(std::forward<Args>(a)...);
    }
}

#undef OUT_API_NODISCARD
