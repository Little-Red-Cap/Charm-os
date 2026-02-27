module;
#include <expected>
#include <array>
#include <utility>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <bit>  // std::bit_cast for NaN/Inf checks without <cmath>.
export module out.format;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: out.core, out.sink
// Forbidden out.* imports: out.ansi, out.logger, out.api, out.port, out.print, out.domain
// Rationale: formatting core; must not depend on ANSI/logger/policy/ports.
// If you need functionality from a higher layer, add an extension point in this layer instead.

import out.core;
import out.sink;


export namespace out {

  template <std::size_t N>
  struct fixed_string {
    char v[N]{};
    consteval fixed_string(const char (&s)[N]) {
      for (std::size_t i = 0; i < N; ++i) v[i] = s[i];
    }
    consteval std::size_t size() const { return N; }
    consteval const char* data() const { return v; }
    consteval std::string_view sv() const { return {v, N - 1}; } // drop '\0'
  };

  struct fmt_spec {
    char type = 0;          // 0 => default
    std::uint8_t width = 0; // 0 => none
    std::uint8_t precision = 0xFF; // 0xFF => none
    bool zero_pad = false;
    bool upper = false;
  };

  template <class T>
  struct formatter;

  enum class token_kind : std::uint8_t { lit, arg };

  struct token {
    token_kind kind{};
    std::uint16_t pos{};
    std::uint16_t len{};
    std::uint8_t  arg_index{}; // for arg
    fmt_spec spec{};
  };

  consteval bool is_digit(char c) { return c >= '0' && c <= '9'; }


  namespace detail {

    struct scan_result {
      std::size_t ntok  = 0;
      std::size_t nargs = 0;
      bool valid = true;
    };

    template <fixed_string F, class Emit>
    consteval scan_result scan_format(Emit emit) {
      scan_result res{};
      auto s = F.sv();

      std::size_t i = 0;
      std::size_t lit_start = 0;
      std::size_t next_arg = 0;

      auto push_lit = [&](std::size_t a, std::size_t b) {
        if (b > a) {
          if (a > 0xFFFFu || (b - a) > 0xFFFFu) { res.valid = false; return; }
          emit(token{
            .kind = token_kind::lit,
            .pos  = static_cast<std::uint16_t>(a),
            .len  = static_cast<std::uint16_t>(b - a),
            .arg_index = 0,
            .spec = fmt_spec{},
          });
          ++res.ntok;
        }
      };

      while (i < s.size()) {
        if (s[i] == '{') {
          if (i + 1 < s.size() && s[i + 1] == '{') { // escaped {
            push_lit(lit_start, i);
            if (!res.valid) break;
            push_lit(i, i + 1);
            if (!res.valid) break;
            i += 2;
            lit_start = i;
            continue;
          }

          push_lit(lit_start, i);
          if (!res.valid) break;

          ++i;
          if (i >= s.size()) { res.valid = false; break; }

          if (next_arg > 0xFFu) { res.valid = false; break; }
          std::size_t arg_index = next_arg++;

          fmt_spec spec{};

          if (s[i] == ':') {
            ++i;
            if (i < s.size() && s[i] == '0') { spec.zero_pad = true; ++i; }

            // width
            while (i < s.size() && is_digit(s[i])) {
              spec.width = static_cast<std::uint8_t>(spec.width * 10 + (s[i] - '0'));
              ++i;
            }

            // precision: .N
            if (i < s.size() && s[i] == '.') {
              ++i;
              std::uint8_t prec = 0;
              bool any = false;
              while (i < s.size() && is_digit(s[i])) {
                any = true;
                prec = static_cast<std::uint8_t>(prec * 10 + (s[i] - '0'));
                ++i;
              }
              if (!any) { res.valid = false; break; }
              spec.precision = prec;
            }

            // type
            if (i < s.size() && s[i] != '}') {
              spec.type = s[i];
              if (spec.type == 'X' || spec.type == 'B' ||
                  spec.type == 'E' || spec.type == 'F' || spec.type == 'G') {
                spec.upper = true;
              }
              ++i;
            }
          }

          if (i >= s.size() || s[i] != '}') { res.valid = false; break; }
          ++i;

          emit(token{
            .kind = token_kind::arg,
            .pos = 0,
            .len = 0,
            .arg_index = static_cast<std::uint8_t>(arg_index),
            .spec = spec,
          });
          ++res.ntok;

          lit_start = i;
        } else if (s[i] == '}') {
          if (i + 1 < s.size() && s[i + 1] == '}') { // escaped }
            push_lit(lit_start, i);
            if (!res.valid) break;
            push_lit(i, i + 1);
            if (!res.valid) break;
            i += 2;
            lit_start = i;
            continue;
          }
          res.valid = false;
          break;
        } else {
          ++i;
        }
      }

      if (res.valid) push_lit(lit_start, s.size());
      res.nargs = next_arg;
      return res;
    }

    template <fixed_string F>
    consteval std::size_t token_count() {
      auto r = scan_format<F>([](const token&) {});
      return r.ntok;
    }

  } // namespace detail

  template <fixed_string F, std::size_t NTok>
  struct parsed_format {
    static constexpr auto text = F.sv();
    std::array<token, NTok> toks{};
    std::size_t nargs = 0;
    bool valid = true;
  };

  template <fixed_string F>
  consteval auto parse_format() {
    constexpr std::size_t NTok = detail::token_count<F>();
    parsed_format<F, NTok> out{};

    std::size_t k = 0;
    auto r = detail::scan_format<F>([&](const token& tk) {
      // Should never overflow; if it does, token_count and scan_format disagree.
      if (k >= NTok) { out.valid = false; return; }
      out.toks[k++] = tk;
    });

    out.valid = out.valid && r.valid && (k == NTok);
    out.nargs = r.nargs;
    return out;
  }

  namespace detail {

    template <fixed_string Fmt>
    inline constexpr auto parsed_v = parse_format<Fmt>();

    template <class S, std::size_t N>
    struct buffered_writer {
      S& sink;
      std::array<char, N> buf{};
      std::size_t pos = 0;
      std::array<char, 64> ansi_buf{};
      std::size_t ansi_pos = 0;

      result<std::size_t> flush_ansi() noexcept {
        if constexpr (ansi_is_bytes_final_v<S>) {
          return ok<std::size_t>(0u);
        }
        if constexpr (requires(S& s, std::string_view v) { s.write_ansi(v); }) {
          if (ansi_pos == 0) return ok<std::size_t>(0u);
          const std::size_t n = ansi_pos;
          auto r = sink.write_ansi(std::string_view{ansi_buf.data(), ansi_pos});
          if (!r) return std::unexpected(r.error());
          ansi_pos = 0;
          return ok(n);
        } else {
          return ok<std::size_t>(0u);
        }
      }

      result<std::size_t> flush_bytes() noexcept {
        if (pos == 0) return ok<std::size_t>(0u);
        const std::size_t n = pos;
        auto r = out::write(sink, std::string_view{buf.data(), pos});
        if (!r) return std::unexpected(r.error());
        pos = 0;
        return ok(n);
      }

      result<std::size_t> flush() noexcept {
        auto ra = flush_ansi();
        if (!ra) return std::unexpected(ra.error());
        auto rb = flush_bytes();
        if (!rb) return std::unexpected(rb.error());
        return ok(*ra + *rb);
      }

      result<std::size_t> append(std::string_view sv) noexcept {
        if constexpr (!ansi_is_bytes_final_v<S>) {
          auto ra = flush_ansi();
          if (!ra) return std::unexpected(ra.error());
        }
        const std::size_t len = sv.size();
        while (!sv.empty()) {
          std::size_t space = N - pos;
          if (space == 0) {
            auto r = flush_bytes();
            if (!r) return std::unexpected(r.error());
            space = N;
          }
          const std::size_t n = (sv.size() < space) ? sv.size() : space;
          std::memcpy(buf.data() + pos, sv.data(), n);
          pos += n;
          sv.remove_prefix(n);
        }
        return ok(len);
      }

      result<std::size_t> write(bytes b) noexcept {
        return append(std::string_view{reinterpret_cast<const char*>(b.data()), b.size()});
      }

      template <class U = S>
      requires requires(U& s, std::string_view v) { s.write_ansi(v); }
      result<std::size_t> write_ansi(std::string_view sv) noexcept {
        if constexpr (ansi_is_bytes_final_v<S>) {
          return append(sv);
        }
        auto rb = flush_bytes();
        if (!rb) return std::unexpected(rb.error());
        if (sv.size() > ansi_buf.size()) {
          auto rf = flush_ansi();
          if (!rf) return std::unexpected(rf.error());
          return sink.write_ansi(sv);
        }
        if (ansi_pos + sv.size() > ansi_buf.size()) {
          auto rf = flush_ansi();
          if (!rf) return std::unexpected(rf.error());
        }
        std::memcpy(ansi_buf.data() + ansi_pos, sv.data(), sv.size());
        ansi_pos += sv.size();
        return ok(sv.size());
      }
    };

    template <class T>
    struct is_buffered_writer : std::false_type {};

    template <class S, std::size_t N>
    struct is_buffered_writer<buffered_writer<S, N>> : std::true_type {};

    template <class T>
    inline constexpr bool is_buffered_writer_v = is_buffered_writer<T>::value;

    template <auto& PF, std::size_t I, class S, class Tup>
    inline result<std::size_t> emit_token(S& sink, Tup& tup) noexcept {
      constexpr token tk = PF.toks[I];

      if constexpr (tk.kind == token_kind::lit) {
        auto sv = PF.text.substr(tk.pos, tk.len);
        return write(sink, sv);
      } else {
        constexpr std::size_t idx = static_cast<std::size_t>(tk.arg_index);
        return write_one(sink, std::get<idx>(tup), tk.spec);
      }
    }

    template <auto& PF, std::size_t I, class S, class Tup, std::size_t N>
    inline result<std::size_t> emit_token_buffered(buffered_writer<S, N>& bw, Tup& tup) noexcept {
      constexpr token tk = PF.toks[I];

      if constexpr (tk.kind == token_kind::lit) {
        auto sv = PF.text.substr(tk.pos, tk.len);
        return bw.append(sv);
      } else {
        constexpr std::size_t idx = static_cast<std::size_t>(tk.arg_index);
        return write_one(bw, std::get<idx>(tup), tk.spec);
      }
    }

    template <auto& PF, class S, class Tup, std::size_t N, std::size_t... Is>
    inline result<std::size_t> unroll_tokens_seq(
        buffered_writer<S, N>& bw, Tup& tup, std::index_sequence<Is...>) noexcept {
      std::size_t total = 0;
      errc first_err = errc::ok;
      bool ok_all = true;

      auto step = [&](auto r) {
        if (!ok_all) return;
        if (!r) { ok_all = false; first_err = r.error(); return; }
        total += *r;
      };

      (step(emit_token_buffered<PF, Is>(bw, tup)), ...);

      if (!ok_all) return std::unexpected(first_err);
      return ok(total);
    }

  } // namespace detail



  template<class T, bool = std::is_enum_v<T>>
  struct enum_underlying_or_self { using type = T; };

  template<class T>
  struct enum_underlying_or_self<T, true> { using type = std::underlying_type_t<T>; };

  template<class T>
  using enum_underlying_or_self_t = typename enum_underlying_or_self<T>::type;


  // ---------- compile-time probes for nicer diagnostics ----------
  template <class...>
  inline constexpr bool dependent_false_v = false;

  template <class PF>
  consteval bool needs_binary(const PF& pf) {
    for (std::size_t i = 0; i < pf.toks.size(); ++i) {
      if (pf.toks[i].kind == token_kind::arg) {
        char t = pf.toks[i].spec.type;
        if (t == 'b' || t == 'B') return true;
      }
    }
    return false;
  }

  template <class PF>
  consteval bool needs_float_spec(const PF& pf) {
    for (std::size_t i = 0; i < pf.toks.size(); ++i) {
      if (pf.toks[i].kind == token_kind::arg) {
        char t = pf.toks[i].spec.type;
        if (t == 'f' || t == 'F' || t == 'e' || t == 'E' || t == 'g' || t == 'G') return true;
      }
    }
    return false;
  }

  template <class PF>
  consteval bool needs_float_non_fixed(const PF& pf) {
    for (std::size_t i = 0; i < pf.toks.size(); ++i) {
      if (pf.toks[i].kind == token_kind::arg) {
        char t = pf.toks[i].spec.type;
        if (t == 'e' || t == 'E' || t == 'g' || t == 'G') return true;
      }
    }
    return false;
  }

  // --------- Numeric formatting: no heap, no exceptions ----------
  // Note: ANSI tokens are handled via overloads in out.ansi.
  // Non-ANSI sinks get silent no-op behavior by default.
  inline constexpr std::size_t pad_chunk_size = 32;
#ifndef OUT_WRITE_BUFFER_SIZE
#define OUT_WRITE_BUFFER_SIZE 64
#endif

#if defined(OUT_UNROLL_TOKENS)
#ifndef OUT_UNROLL_TOKENS_MAX
#define OUT_UNROLL_TOKENS_MAX 32
#endif
#endif
  template <class UInt>
  inline result<std::size_t> write_uint_base(auto& sink, UInt v, unsigned base, fmt_spec spec) noexcept {
    char buf[80]; // enough for 64-bit in binary? 64 + maybe. binary needs 64, so enlarge if you enable b.
    char* first = buf;
    char* last  = buf + sizeof(buf);

    // Use std::to_chars for base-10/16 conversion (usually smaller than printf).
    auto ec = std::errc{};

    if (base == 10) {
      auto r = std::to_chars(first, last, v, 10);
      ec = r.ec;
      last = r.ptr;
    } else if (base == 16) {
      auto r = std::to_chars(first, last, v, 16);
      ec = r.ec;
      last = r.ptr;
      if (spec.upper) {
        for (char* p = first; p < last; ++p)
          if (*p >= 'a' && *p <= 'f') *p = static_cast<char>(*p - 'a' + 'A');
      }
    } else if (base == 2) {
#ifndef OUT_ENABLE_BINARY
      (void)v; (void)spec;
      return std::unexpected(errc::invalid_format);
#else
      char* p = last;
      do {
        *--p = char('0' + (v & 1u));
        v >>= 1u;
      } while (v != 0);
      first = p;
#endif
    } else {
      return std::unexpected(errc::invalid_format);
    }

    if (ec != std::errc{}) return std::unexpected(errc::buffer_overflow);

    std::size_t len = static_cast<std::size_t>(last - first);
    std::size_t total = 0;

    // padding (block write to reduce sink.write calls)
    if (spec.width > len) {
      std::size_t pad = spec.width - len;
      const char ch = spec.zero_pad ? '0' : ' ';
      char pad_buf[pad_chunk_size];
      for (auto& c : pad_buf) c = ch;
      while (pad != 0) {
        const std::size_t n = (pad > sizeof(pad_buf)) ? sizeof(pad_buf) : pad;
        auto r = write(sink, std::string_view{pad_buf, n});
        if (!r) return std::unexpected(r.error());
        total += *r;
        pad -= n;
      }
    }

    auto r = write(sink, std::string_view{first, len});
    if (!r) return std::unexpected(r.error());
    total += *r;
    return ok(total);
  }

#ifdef OUT_ENABLE_FLOAT
  template <class F>
  inline result<std::size_t> write_float(auto& sink, F v, fmt_spec spec) noexcept {
    char buf[128];
    char* first = buf;
    char* last  = buf + sizeof(buf);

    std::chars_format fmt = std::chars_format::general;
    switch (spec.type) {
      case 0:               fmt = std::chars_format::general; break;
      case 'g': case 'G':   fmt = std::chars_format::general; break;
      case 'f': case 'F':   fmt = std::chars_format::fixed; break;
      case 'e': case 'E':   fmt = std::chars_format::scientific; break;
      default:              fmt = std::chars_format::general; break;
    }

    // auto r0 = std::to_chars(first, last, v, fmt);
    auto r0 = (spec.precision == 0xFF)
    ? std::to_chars(first, last, v, fmt)
    : std::to_chars(first, last, v, fmt, static_cast<int>(spec.precision));

    if (r0.ec != std::errc{}) return std::unexpected(errc::buffer_overflow);
    last = r0.ptr;

    // Uppercase E: replace 'e' with 'E' in output.
    if (spec.type == 'E') {
      for (char* p = first; p < last; ++p) if (*p == 'e') *p = 'E';
    }

    std::size_t len = static_cast<std::size_t>(last - first);
    std::size_t total = 0;

    if (spec.width > len) {
      std::size_t pad = spec.width - len;
      const char ch = spec.zero_pad ? '0' : ' ';
      char pad_buf[pad_chunk_size];
      for (auto& c : pad_buf) c = ch;
      while (pad != 0) {
        const std::size_t n = (pad > sizeof(pad_buf)) ? sizeof(pad_buf) : pad;
        auto r = write(sink, std::string_view{pad_buf, n});
        if (!r) return std::unexpected(r.error());
        total += *r;
        pad -= n;
      }
    }

    auto r1 = write(sink, std::string_view{first, len});
    if (!r1) return std::unexpected(r1.error());
    total += *r1;
    return ok(total);
  }
#endif

#ifdef OUT_ENABLE_FLOAT

namespace detail {

  // Digits 0..9 are usually enough for MCU debug output.
  inline constexpr std::uint32_t pow10_u32[10] = {
    1u, 10u, 100u, 1000u, 10000u,
    100000u, 1000000u, 10000000u, 100000000u, 1000000000u
  };

  template <class S>
  inline result<std::size_t> write_pad(S& sink, char ch, std::size_t n) noexcept {
    std::size_t total = 0;
    char pad_buf[pad_chunk_size];
    for (auto& c : pad_buf) c = ch;
    while (n != 0) {
      const std::size_t chunk = (n > sizeof(pad_buf)) ? sizeof(pad_buf) : n;
      auto r = write(sink, std::string_view{pad_buf, chunk});
      if (!r) return std::unexpected(r.error());
      total += *r;
      n -= chunk;
    }
    return ok(total);
  }

  // MCU minimal mode: fixed only ({:f}/{:.Nf}); default {} uses fixed.
  template <class S>
  inline result<std::size_t> write_float_fixed_mcu(S& sink, float v, fmt_spec spec) noexcept {
    // Accept only 0 / f / F.
    if (spec.type != 0 && spec.type != 'f' && spec.type != 'F')
      return std::unexpected(errc::invalid_format);

    std::uint8_t prec = (spec.precision == 0xFF) ? 6 : spec.precision;
    if (prec > 9) prec = 9;

    const std::uint32_t bits = std::bit_cast<std::uint32_t>(v);
    const bool neg = (bits >> 31) != 0;
    const std::uint32_t exp  = (bits >> 23) & 0xFFu;
    const std::uint32_t mant = bits & 0x7FFFFFu;

    auto emit_with_width = [&](std::string_view core, bool with_sign) -> result<std::size_t> {
      const std::size_t len = core.size() + (with_sign ? 1u : 0u);
      const std::size_t pad = (spec.width > len) ? (spec.width - len) : 0u;
      const char pad_ch = spec.zero_pad ? '0' : ' ';

      std::size_t total = 0;

      if (spec.zero_pad && with_sign) {
        auto r0 = write(sink, std::string_view{"-", 1});
        if (!r0) return std::unexpected(r0.error());
        total += *r0;

        auto rp = write_pad(sink, pad_ch, pad);
        if (!rp) return std::unexpected(rp.error());
        total += *rp;
      } else {
        auto rp = write_pad(sink, pad_ch, pad);
        if (!rp) return std::unexpected(rp.error());
        total += *rp;

        if (with_sign) {
          auto r0 = write(sink, std::string_view{"-", 1});
          if (!r0) return std::unexpected(r0.error());
          total += *r0;
        }
      }

      auto r1 = write(sink, core);
      if (!r1) return std::unexpected(r1.error());
      total += *r1;
      return ok(total);
    };

    // NaN / Inf
    if (exp == 0xFFu) {
      const bool is_nan = (mant != 0);
      const bool is_inf = (mant == 0);

      const bool upper = spec.upper; // 'F' sets upper=true.
      const char* s = nullptr;
      if (is_nan) s = upper ? "NAN" : "nan";
      else if (is_inf) s = upper ? "INF" : "inf";
      else s = upper ? "NAN" : "nan";

      // nan usually has no sign; inf may have a sign (keep sign for inf).
      const bool sign = (!is_nan) && neg;
      return emit_with_width(std::string_view{s, 3}, sign);
    }

    // abs: clear sign bit without pulling in fabsf.
    const float av = std::bit_cast<float>(bits & 0x7FFFFFFFu);

    std::uint32_t ip = static_cast<std::uint32_t>(av);

    std::uint32_t frac_scaled = 0;
    std::uint32_t pow10 = 1u;

    if (prec == 0) {
      // Round to integer.
      const float rounded = av + 0.5f;
      ip = static_cast<std::uint32_t>(rounded);
    } else {
      pow10 = pow10_u32[prec];
      const float frac = av - static_cast<float>(ip);
      const float scaled = frac * static_cast<float>(pow10) + 0.5f;
      frac_scaled = static_cast<std::uint32_t>(scaled);

      // Handle 0.999999.. rounding carry.
      if (frac_scaled >= pow10) {
        frac_scaled = 0;
        ++ip;
      }
    }

    // Build core digits (without sign) for width calculation.
    char buf[32];
    char* p = buf;
    char* end = buf + sizeof(buf);

    auto [ptr, ec] = std::to_chars(p, end, ip, 10);
    if (ec != std::errc{}) return std::unexpected(errc::buffer_overflow);
    p = ptr;

    if (prec != 0) {
      if (p >= end) return std::unexpected(errc::buffer_overflow);
      *p++ = '.';

      // Write prec fractional digits (with leading zero).
      if (static_cast<std::size_t>(end - p) < prec)
        return std::unexpected(errc::buffer_overflow);

      char* q = p + prec;
      std::uint32_t x = frac_scaled;
      for (std::uint8_t i = 0; i < prec; ++i) {
        *--q = static_cast<char>('0' + (x % 10u));
        x /= 10u;
      }
      p += prec;
    }

    const std::string_view core{buf, static_cast<std::size_t>(p - buf)};
    return emit_with_width(core, neg);
  }

} // namespace detail

#endif // OUT_ENABLE_FLOAT

  // Unified entry: one writer per type.
  // TODO: compile-time string concat.
  template <class S, class T>
  inline result<std::size_t> write_one(S& sink, const T& value, fmt_spec spec) noexcept {
    if constexpr (std::is_same_v<T, char>) {
      char c = value;
      return write(sink, std::string_view{&c, 1});
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
      return write(sink, std::string_view(value));
    } else if constexpr (std::is_same_v<T, bool>) {
      const std::string_view sv = value ? std::string_view{"true"} : std::string_view{"false"};
      std::size_t total = 0;

      if (spec.width > sv.size()) {
        std::size_t pad = spec.width - sv.size();
        const char ch = spec.zero_pad ? '0' : ' ';
        char pad_buf[pad_chunk_size];
        for (auto& c : pad_buf) c = ch;
        while (pad != 0) {
          const std::size_t n = (pad > sizeof(pad_buf)) ? sizeof(pad_buf) : pad;
          auto r = write(sink, std::string_view{pad_buf, n});
          if (!r) return std::unexpected(r.error());
          total += *r;
          pad -= n;
        }
      }

      auto r = write(sink, sv);
      if (!r) return std::unexpected(r.error());
      total += *r;
      return ok(total);
    } else if constexpr (std::is_floating_point_v<T>) {
#ifdef OUT_ENABLE_FLOAT
      // return write_float(sink, value, spec);
      // Ban double; use float.
      if constexpr (std::is_same_v<T, double>) {
        static_assert(dependent_false_v<T>,
          "double formatting is disabled on MCU. "
          "Use float (e.g. 3.14159f) to avoid huge code size.");
        return std::unexpected(errc::invalid_format);
      } else {
        return detail::write_float_fixed_mcu(sink, static_cast<float>(value), spec);
      }
#else
      static_assert(dependent_false_v<T>,
        "floating-point formatting requested but OUT_ENABLE_FLOAT is not defined");
      return std::unexpected(errc::invalid_format);
#endif
    } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
      // using Raw = std::conditional_t<std::is_enum_v<T>, std::underlying_type_t<T>, T>;
      using Raw = enum_underlying_or_self_t<T>;
      using U   = std::make_unsigned_t<Raw>;

      auto pick_base = [&](char t) -> unsigned {
        if (t == 'x' || t == 'X') return 16;
        if (t == 'b' || t == 'B') return 2;
        return 10;
      };

      Raw rv = static_cast<Raw>(value);

      if constexpr (std::is_signed_v<Raw>) {
        if (rv < 0) {
          auto r = write(sink, std::string_view{"-", 1});
          if (!r) return std::unexpected(r.error());
          if (spec.width > 0) --spec.width;

          // Make abs value (handle INT_MIN).
          U uv = static_cast<U>(rv);
          uv = U(0) - uv;

          unsigned base = pick_base(spec.type);
#ifndef OUT_ENABLE_BINARY
          if (base == 2) return std::unexpected(errc::invalid_format);
#endif
          auto rr = write_uint_base(sink, uv, base, spec);
          if (!rr) return std::unexpected(rr.error());
          return ok(*r + *rr);
        }
      }

      U uv = static_cast<U>(rv);
      unsigned base = pick_base(spec.type);
#ifndef OUT_ENABLE_BINARY
      if (base == 2) return std::unexpected(errc::invalid_format);
#endif
      return write_uint_base(sink, uv, base, spec);
    } else if constexpr (requires {
      formatter<T>::write(sink, value, spec);
    }) {
      return formatter<T>::write(sink, value, spec);
    } else {
      static_assert(dependent_false_v<T>,
        "Type is not formattable. "
        "Provide formatter<T>::write(sink, value, spec).");
      return std::unexpected(errc::invalid_format);
    }
  }

  // format output: compile-time parse + runtime expansion
  // TODO: optionally unroll tokens at compile time for MCU builds.
  template <fixed_string Fmt, Sink S, bool FinalFlush = true, class... Args>
  inline result<std::size_t> vprint(S& sink, Args&&... args) noexcept {
    constexpr auto& pf = detail::parsed_v<Fmt>;
#if defined(OUT_ENABLE_FLOAT)
    static_assert(!needs_float_non_fixed(pf),
      "MCU float backend only supports fixed format {:f}. "
      "Please use {:f} / {:.Nf} (or implement another backend).");
#endif
    static_assert(pf.valid, "format string invalid");
    static_assert(pf.nargs == sizeof...(Args), "format args count mismatch");

#ifndef OUT_ENABLE_BINARY
    static_assert(!needs_binary(pf),
      "binary formatting requested (use {:b}) but OUT_ENABLE_BINARY is not defined");
#endif

#ifndef OUT_ENABLE_FLOAT
    static_assert(!needs_float_spec(pf),
      "float formatting spec requested (use {:f}/{:e}/{:g}) but OUT_ENABLE_FLOAT is not defined");
#endif

    // Pack args into tuple for index-based access.
    auto tup = std::forward_as_tuple(std::forward<Args>(args)...);

    if constexpr (detail::is_buffered_writer_v<S>) {
      std::size_t total = 0;

#if defined(OUT_UNROLL_TOKENS)
      if constexpr (pf.toks.size() <= OUT_UNROLL_TOKENS_MAX) {
        auto r = detail::unroll_tokens_seq<detail::parsed_v<Fmt>>(
          sink, tup, std::make_index_sequence<pf.toks.size()>{});
        if (!r) return std::unexpected(r.error());
        total += *r;
      } else {
        for (std::size_t i = 0; i < pf.toks.size(); ++i) {
          const auto& tk = pf.toks[i];
          if (tk.kind == token_kind::lit) {
            auto sv = pf.text.substr(tk.pos, tk.len);
            auto r = sink.append(sv);
            if (!r) return std::unexpected(r.error());
            total += *r;
          } else {
            // ?????????
            auto idx = tk.arg_index;
            result<std::size_t> r = std::unexpected(errc::invalid_format);

            // ????? lambda + index_sequence ? compile-time ??
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
              ((idx == Is ? r = write_one(sink, std::get<Is>(tup), tk.spec) : r), ...);
            }(std::make_index_sequence<sizeof...(Args)>{});

            if (!r) return std::unexpected(r.error());
            total += *r;
          }
        }
      }
#else
      for (std::size_t i = 0; i < pf.toks.size(); ++i) {
        const auto& tk = pf.toks[i];
        if (tk.kind == token_kind::lit) {
          auto sv = pf.text.substr(tk.pos, tk.len);
          auto r = sink.append(sv);
          if (!r) return std::unexpected(r.error());
          total += *r;
        } else {
          // ?????????
          auto idx = tk.arg_index;
          result<std::size_t> r = std::unexpected(errc::invalid_format);

          // ????? lambda + index_sequence ? compile-time ??
          [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((idx == Is ? r = write_one(sink, std::get<Is>(tup), tk.spec) : r), ...);
          }(std::make_index_sequence<sizeof...(Args)>{});

          if (!r) return std::unexpected(r.error());
          total += *r;
        }
      }
#endif
      if constexpr (FinalFlush) {
        auto rf = sink.flush();
        if (!rf) return std::unexpected(rf.error());
        total += *rf;
      }
      return ok(total);
    } else {
#if defined(OUT_UNROLL_TOKENS)
    if constexpr (pf.toks.size() <= OUT_UNROLL_TOKENS_MAX) {
      detail::buffered_writer<S, OUT_WRITE_BUFFER_SIZE> bw{sink};
      auto r = detail::unroll_tokens_seq<detail::parsed_v<Fmt>>(
        bw, tup, std::make_index_sequence<pf.toks.size()>{});
      if (!r) return std::unexpected(r.error());
      std::size_t total = *r;
      if constexpr (FinalFlush) {
        auto rf = bw.flush();
        if (!rf) return std::unexpected(rf.error());
        total += *rf;
      }
      return ok(total);
    }
#endif
    detail::buffered_writer<S, OUT_WRITE_BUFFER_SIZE> bw{sink};
    std::size_t total = 0;
    for (std::size_t i = 0; i < pf.toks.size(); ++i) {
      const auto& tk = pf.toks[i];
      if (tk.kind == token_kind::lit) {
        auto sv = pf.text.substr(tk.pos, tk.len);
        auto r = bw.append(sv);
        if (!r) return std::unexpected(r.error());
        total += *r;
      } else {
        // ?????????
        auto idx = tk.arg_index;
        result<std::size_t> r = std::unexpected(errc::invalid_format);

        // ????? lambda + index_sequence ? compile-time ??
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
          ((idx == Is ? r = write_one(bw, std::get<Is>(tup), tk.spec) : r), ...);
        }(std::make_index_sequence<sizeof...(Args)>{});

        if (!r) return std::unexpected(r.error());
        total += *r;
      }
    }
    if constexpr (FinalFlush) {
      auto rf = bw.flush();
      if (!rf) return std::unexpected(rf.error());
      total += *rf;
    }
    return ok(total);
  }
  }

}
