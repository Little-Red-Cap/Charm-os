module;
#include <cstddef>
#include <cstdint>
#include <span>
#include <expected>
#include <type_traits>
#include <utility>
export module out.core;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: (none)
// Forbidden out.* imports: out.*
// Rationale: foundation types only (errc/result/ok/bytes).
// If you need functionality from a higher layer, add an extension point in this layer instead.

// out.core intentionally contains only the minimal, cross-module primitives.
// Do NOT turn this into a general utilities header.

export namespace out {

    enum class errc : std::uint8_t {
        ok = 0,
        io_error,
        // io_fault,
        // would_block,
        buffer_overflow,
        invalid_format,
        not_supported,
    };

    template <class T>
    using result = std::expected<T, errc>;

    using bytes = std::span<const std::byte>;
    using cbytes = std::span<const char>;

    // Utility: view any trivially copyable object as a byte span.
    template <class T>
    constexpr bytes as_bytes(std::span<const T> s) noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        return {reinterpret_cast<const std::byte*>(s.data()), s.size_bytes()};
    }

    // Utility: explicit success construction to avoid implicit expected conversions.
    // Prefer ok(...) to keep diagnostics consistent across compilers.
    template <class T>
    constexpr result<std::remove_cvref_t<T>> ok(T&& v) noexcept {
        return result<std::remove_cvref_t<T>>{std::in_place, std::forward<T>(v)};
    }

    // Lazy wrapper for deferred evaluation.
    template <class F>
    struct lazy_t { F f; };

    template <class F>
    constexpr auto lazy(F&& f) { return lazy_t<std::decay_t<F>>{std::forward<F>(f)}; }

    template <class T>
    constexpr decltype(auto) eval(T&& v) { return std::forward<T>(v); }

    template <class F>
    constexpr decltype(auto) eval(lazy_t<F>& lz) { return lz.f(); }

    template <class F>
    constexpr decltype(auto) eval(const lazy_t<F>& lz) { return lz.f(); }

    template <class F>
    constexpr decltype(auto) eval(lazy_t<F>&& lz) { return lz.f(); }

    template <class T>
    constexpr void discard(result<T>&&) noexcept {}

    template <class T>
    constexpr void discard(const result<T>&) noexcept {}

    // Trait: whether ANSI sequences can be treated as plain bytes for a sink.
    template <class S>
    inline constexpr bool ansi_is_bytes_v = false;

    // Build-time override for ANSI-as-bytes behavior.
    template <class S>
    inline constexpr bool ansi_is_bytes_final_v =
#if defined(OUT_ANSI_IS_BYTES)
        (OUT_ANSI_IS_BYTES != 0);
#else
        ansi_is_bytes_v<S>;
#endif
}
