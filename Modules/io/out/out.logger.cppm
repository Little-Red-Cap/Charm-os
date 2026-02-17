module;
#include <array>
#include <cstdint>
#include <cstdlib>
#include <charconv>
#include <expected>
#include <string_view>
#include <type_traits>
#include <utility>
#include <cstring>
export module out.logger;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: out.core, out.sink, out.format, out.domain, out.port, out.ansi
// Forbidden out.* imports: out.api, out.print
// Rationale: logger is the single behavior owner (prefix/style/timestamp/newline/flush/error-policy).
// If you need functionality from a higher layer, add an extension point in this layer instead.

import out.ansi;
import out.core;
import out.domain;
import out.format;
import out.port;
import out.sink;

#if defined(OUT_ERROR_PROPAGATE)
#define OUT_LOGGER_NODISCARD [[nodiscard]]
#else
#define OUT_LOGGER_NODISCARD
#endif

#ifndef OUT_LOGGER_WRITE_BUFFER_SIZE
#define OUT_LOGGER_WRITE_BUFFER_SIZE 128
#endif

export namespace out {

    enum class error_policy : std::uint8_t { ignore, hook, assert_, propagate };

    inline constexpr error_policy build_error_policy =
#if defined(OUT_ERROR_PROPAGATE)
        error_policy::propagate;
#elif defined(OUT_ERROR_ASSERT)
        error_policy::assert_;
#elif defined(OUT_ERROR_HOOK)
        error_policy::hook;
#else
        error_policy::ignore;
#endif

    // error_hook is intended to be set during initialization only (not thread-safe).
    inline void (*error_hook)(errc) = nullptr;

    enum class newline : std::uint8_t { none, lf, crlf };

    struct style_cmd {
        enum class kind : std::uint8_t { seq, fg, bg };
        kind k{};
        std::string_view seq{};
        int code = 0;
    };

    constexpr style_cmd make_style(reset_t) noexcept { return {style_cmd::kind::seq, "\x1b[0m", 0}; }
    constexpr style_cmd make_style(bold_t) noexcept { return {style_cmd::kind::seq, "\x1b[1m", 0}; }
    constexpr style_cmd make_style(dim_t) noexcept { return {style_cmd::kind::seq, "\x1b[2m", 0}; }
    constexpr style_cmd make_style(italic_t) noexcept { return {style_cmd::kind::seq, "\x1b[3m", 0}; }
    constexpr style_cmd make_style(underline_t) noexcept { return {style_cmd::kind::seq, "\x1b[4m", 0}; }
    constexpr style_cmd make_style(ansi::fg_t v) noexcept {
        return {style_cmd::kind::fg, {}, ansi::fg_code(v.c)};
    }
    constexpr style_cmd make_style(ansi::bg_t v) noexcept {
        return {style_cmd::kind::bg, {}, ansi::bg_code(v.c)};
    }
    template <class T>
    constexpr style_cmd make_style(const T&) noexcept {
        static_assert(out::dependent_false_v<T>, "unsupported style token");
        return {};
    }

    namespace detail {
        template <class T>
        using public_return_t =
            std::conditional_t<build_error_policy == error_policy::propagate, result<T>, void>;

        inline void handle_error(errc e) noexcept {
            if constexpr (build_error_policy == error_policy::ignore) {
                (void)e;
            } else if constexpr (build_error_policy == error_policy::hook) {
                if (error_hook) error_hook(e);
            } else if constexpr (build_error_policy == error_policy::assert_) {
                (void)e;
                std::abort();
            } else {
                (void)e;
            }
        }

        template <class T>
        inline public_return_t<T> finalize(result<T> r) noexcept {
            if constexpr (build_error_policy == error_policy::propagate) {
                return r;
            } else {
                if (!r) handle_error(r.error());
                return;
            }
        }

        template <class S>
        struct sink_ref {
            S* base{};
            result<std::size_t> write(bytes b) const noexcept { return base->write(b); }
            result<std::size_t> write_ansi(std::string_view sv) const noexcept
              requires ansi::AnsiSink<S>
            {
                return base->write_ansi(sv);
            }
        };

#if defined(OUT_DEV)
        static_assert(ansi::AnsiSink<ansi::ansi_sink_ref<port::console_sink, true>>);
        using ansi_ref = sink_ref<ansi::ansi_sink_ref<port::console_sink, true>>;
        static_assert(ansi::AnsiSink<ansi_ref>);
#endif

        template <class S>
        constexpr S* base_ptr(S& s) noexcept { return std::addressof(s); }

        template <class S>
        constexpr S* base_ptr(sink_ref<S>& s) noexcept { return s.base; }

        template <class S>
        constexpr S* base_ptr(const sink_ref<S>& s) noexcept { return s.base; }

        template <class S, bool Enabled>
        constexpr S* base_ptr(ansi::ansi_sink_ref<S, Enabled>& s) noexcept { return s.base; }

        template <class S, bool Enabled>
        constexpr S* base_ptr(const ansi::ansi_sink_ref<S, Enabled>& s) noexcept { return s.base; }
    }

    // BypassLevelGate is used by raw formatting paths (non-logging output).
    template <level L, class Domain, class Sink, bool BypassLevelGate = false>
    struct logger {
        Sink sink;
        std::array<style_cmd, 8> styles{};
        std::uint8_t style_count = 0;
        bool auto_reset_enabled = true;
        bool with_timestamp = false;
        bool with_level = true;
        bool with_domain = false;
        bool flush_enabled = true;
        newline nl = newline::crlf;

        explicit constexpr logger(Sink s) noexcept : sink(std::move(s)) {}

        template <class NewSink>
        constexpr auto with_sink(NewSink ns) const noexcept {
            logger<L, Domain, NewSink, BypassLevelGate> out{std::move(ns)};
            copy_opts_to(out);
            return out;
        }

        template <bool Enabled = true>
        constexpr auto ansi() const noexcept {
            auto* base = detail::base_ptr(sink);
            using base_t = std::remove_reference_t<decltype(*base)>;
            return with_sink(ansi::ansi_sink_ref<base_t, Enabled>{base});
        }

        template <class NewDomain>
        constexpr auto domain() const noexcept {
            logger<L, NewDomain, Sink, BypassLevelGate> out{sink};
            copy_opts_to(out);
            return out;
        }

        constexpr logger& auto_reset(bool on) noexcept { auto_reset_enabled = on; return *this; }
        constexpr logger& no_reset() noexcept { auto_reset_enabled = false; return *this; }
        constexpr logger& reset_on() noexcept { auto_reset_enabled = true; return *this; }
        constexpr logger& timestamp() noexcept { with_timestamp = true; return *this; }
        constexpr logger& level_prefix(bool on = true) noexcept { with_level = on; return *this; }
        constexpr logger& domain_prefix(bool on = true) noexcept { with_domain = on; return *this; }
        constexpr logger& set_newline(newline n) noexcept { nl = n; return *this; }
        constexpr logger& flush(bool on = true) noexcept { flush_enabled = on; return *this; }
        constexpr logger& no_flush() noexcept { flush_enabled = false; return *this; }

        template <class... Tokens>
        constexpr logger& style(Tokens&&... tokens) noexcept {
            (push_style(make_style(std::forward<Tokens>(tokens))), ...);
            return *this;
        }

        template <fixed_string Fmt, class... Args>
        inline result<std::size_t> try_print(Args&&... args) noexcept {
            return try_emit_impl<false, Fmt>(std::forward<Args>(args)...);
        }

        template <fixed_string Fmt, class... Args>
        inline result<std::size_t> try_println(Args&&... args) noexcept {
            return try_emit_impl<true, Fmt>(std::forward<Args>(args)...);
        }

        template <fixed_string Fmt, class... Args>
        OUT_LOGGER_NODISCARD inline detail::public_return_t<std::size_t> print(Args&&... args) noexcept {
            auto r = try_print<Fmt>(std::forward<Args>(args)...);
            return detail::finalize(r);
        }

        template <fixed_string Fmt, class... Args>
        OUT_LOGGER_NODISCARD inline detail::public_return_t<std::size_t> println(Args&&... args) noexcept {
            auto r = try_println<Fmt>(std::forward<Args>(args)...);
            return detail::finalize(r);
        }

    private:
        template <class Other>
        constexpr void copy_opts_to(Other& out) const noexcept {
            out.styles = styles;
            out.style_count = style_count;
            out.auto_reset_enabled = auto_reset_enabled;
            out.with_timestamp = with_timestamp;
            out.with_level = with_level;
            out.with_domain = with_domain;
            out.flush_enabled = flush_enabled;
            out.nl = nl;
        }

        static constexpr char level_tag() noexcept {
            if constexpr (L == level::error) return 'E';
            else if constexpr (L == level::warn) return 'W';
            else if constexpr (L == level::info) return 'I';
            else if constexpr (L == level::debug) return 'D';
            else if constexpr (L == level::trace) return 'T';
            else return ' ';
        }

        constexpr void push_style(style_cmd cmd) noexcept {
            if (style_count >= styles.size()) return;
            styles[style_count++] = cmd;
        }

        template <class S>
        inline result<std::size_t> write_style(S& s, const style_cmd& cmd) noexcept {
            if constexpr (ansi::AnsiSink<S>) {
                if (cmd.k == style_cmd::kind::seq) {
                    return s.write_ansi(cmd.seq);
                }
                return ansi::write_ansi_code(s, cmd.code);
            } else {
                return ok<std::size_t>(0u);
            }
        }

        template <class S>
        requires (!ansi::AnsiSink<S>)
        inline result<std::size_t> write_styles_combined(S&, bool) noexcept {
            return ok<std::size_t>(0u);
        }

        template <class S>
        requires ansi::AnsiSink<S>
        inline result<std::size_t> write_styles_combined(S& s, bool include_reset) noexcept {
            if (style_count == 0) return ok<std::size_t>(0u);

            char buf[64];
            std::size_t pos = 0;

            auto append_sv = [&](std::string_view sv) -> bool {
                if (pos + sv.size() > sizeof(buf)) return false;
                std::memcpy(buf + pos, sv.data(), sv.size());
                pos += sv.size();
                return true;
            };

            auto append_code = [&](int code) -> bool {
                char tmp[16];
                char* p = tmp;
                *p++ = '\x1b';
                *p++ = '[';
                auto [ptr, ec] = std::to_chars(p, tmp + sizeof(tmp), code);
                if (ec != std::errc{}) return false;
                *ptr++ = 'm';
                return append_sv(std::string_view{tmp, static_cast<std::size_t>(ptr - tmp)});
            };

            bool ok_all = true;
            for (std::uint8_t i = 0; i < style_count; ++i) {
                const auto& cmd = styles[i];
                if (cmd.k == style_cmd::kind::seq) {
                    if (!append_sv(cmd.seq)) { ok_all = false; break; }
                } else {
                    if (!append_code(cmd.code)) { ok_all = false; break; }
                }
            }

            if (ok_all && include_reset) {
                if (!append_sv("\x1b[0m")) ok_all = false;
            }

            if (ok_all) {
                return s.write_ansi(std::string_view{buf, pos});
            }

            std::size_t total = 0;
            for (std::uint8_t i = 0; i < style_count; ++i) {
                auto rs = write_style(s, styles[i]);
                if (!rs) return std::unexpected(rs.error());
                total += *rs;
            }
            if (include_reset) {
                auto rr = write_style(s, make_style(reset_t{}));
                if (!rr) return std::unexpected(rr.error());
                total += *rr;
            }
            return ok(total);
        }

        template <bool WithNewline, fixed_string Fmt, class... Args>
        inline result<std::size_t> try_emit_impl(Args&&... args) noexcept {
            if constexpr (domain_enabled<Domain> &&
                          (BypassLevelGate || (L != level::off && build_level >= L))) {
                std::size_t total = 0;

                detail::buffered_writer<decltype(sink), OUT_LOGGER_WRITE_BUFFER_SIZE> bw{sink};

                if (with_timestamp) {
                    auto rts = vprint<"[{}] ", decltype(bw), false>(bw, port::now_ms());
                    if (!rts) return std::unexpected(rts.error());
                    total += *rts;
                }

                if (with_level) {
                    char buf[4] = {'[', level_tag(), ']', ' '};
                    auto rp = bw.append(std::string_view{buf, sizeof(buf)});
                    if (!rp) return std::unexpected(rp.error());
                    total += *rp;
                }

                if (with_domain) {
                    if constexpr (domain_name<Domain>.size() != 0) {
                        auto r1 = bw.append("[");
                        if (!r1) return std::unexpected(r1.error());
                        total += *r1;
                        auto r2 = bw.append(domain_name<Domain>);
                        if (!r2) return std::unexpected(r2.error());
                        total += *r2;
                        auto r3 = bw.append("] ");
                        if (!r3) return std::unexpected(r3.error());
                        total += *r3;
                    }
                }

                bool need_reset = auto_reset_enabled && style_count > 0;
                constexpr bool sink_is_ansi = ansi::AnsiSink<decltype(bw)>;
                auto rs = write_styles_combined(bw, sink_is_ansi && need_reset);
                if (!rs) return std::unexpected(rs.error());
                total += *rs;
                if constexpr (sink_is_ansi) {
                    if (need_reset) need_reset = false;
                }

                auto r = vprint<Fmt, decltype(bw), false>(bw, eval(std::forward<Args>(args))...);
                if (!r) return std::unexpected(r.error());
                total += *r;

                if (need_reset) {
                    auto rr = write_style(bw, make_style(reset_t{}));
                    if (!rr) return std::unexpected(rr.error());
                    total += *rr;
                }

                if constexpr (WithNewline) {
                    if (nl != newline::none) {
                        std::string_view nl_sv = (nl == newline::crlf) ? "\r\n" : "\n";
                        auto rn = bw.append(nl_sv);
                        if (!rn) return std::unexpected(rn.error());
                        total += *rn;
                    }
                }

                auto rwo = bw.flush();
                if (!rwo) return std::unexpected(rwo.error());
                total += *rwo;

                if constexpr (WithNewline) {
                    if (nl != newline::none) {
                        if (flush_enabled) {
                            auto* base = detail::base_ptr(sink);
                            using base_t = std::remove_reference_t<decltype(*base)>;
                            if constexpr (Flushable<base_t>) {
                                auto rf = base->flush();
                                if (!rf) return std::unexpected(rf.error());
                                total += *rf;
                            }
                        }
                    }
                }

                return ok(total);
            } else {
                return ok(0u);
            }
        }
    };

}

#undef OUT_LOGGER_NODISCARD
#undef OUT_LOGGER_WRITE_BUFFER_SIZE
