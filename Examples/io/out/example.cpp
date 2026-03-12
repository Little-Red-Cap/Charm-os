#include <string_view>

import out.api;
import out.channel;
import util.core;

// Custom type + formatter extension point.
struct vec2 {
    int x = 0;
    int y = 0;
};
template <>
struct out::formatter<vec2> {
    template <class S>
    static out::result<std::size_t> write(S& sink, const vec2& v, out::fmt_spec) noexcept {
        return out::try_print<"({}, {})">(sink, v.x, v.y);
    }
};

// Custom domain
struct network_domain {};
template <> inline constexpr auto out::domain_enabled<network_domain> = true;
template <> inline constexpr std::string_view out::domain_name<network_domain> = "net";
struct noisy_domain {};
template <> inline constexpr auto out::domain_enabled<noisy_domain> = false;

extern "C" void example(out::channel_sink& console, util::u64 now_ms)
{
    // ------------------------------------------------------------
    // Minimal: Hello world
    // ------------------------------------------------------------
    out::println<"Hello world!">(console);

    // ------------------------------------------------------------
    // Format basics: {}, escaping, width/zero-pad, hex
    // ------------------------------------------------------------
    out::info<"Enable output via LOG_LEVEL_INFO/DEBUG/... at build time.">(console);

    out::debug<"Value: {}\t Hex: {:04x}\t Hex: {:04X}">(console, 42, 0xABCD, 0xABCD);
    out::info<"Escaped braces: {{}}">(console);
    out::info<"Width pad: '0x{:08X}'">(console, 0x12AB);

    // ------------------------------------------------------------
    // Common types: zero/negative/char/string_view
    // ------------------------------------------------------------
    out::info<"Zero: '{}'">(console, 0);
    out::info<"Negative: '{}'">(console, -123);
    out::info<"Chars: '{}' '{}'">( console, 'A', 'z' );
    out::info<"String: '{}'">(console, std::string_view{"hello"});

    out::info<"Bool: '{}' and '{}'">(console, true, false);
    out::info<"Bool padded: '{:8}'">(console, true);

    // Custom formatter extension point.
    out::info<"Vec2: {}">(console, vec2{3, 4});

    out::info<"==========================">(console);

    // ------------------------------------------------------------
    // Optional features (pitfalls)
    //
    // Binary: {:b}/{:B}
        // Binary output requires compile option OUT_ENABLE_BINARY.
    //
    // Float: {:f}/{:e}/{:g} and precision {:.2f}
        // Floating-point output requires compile option OUT_ENABLE_FLOAT.
    //
    // If you forget to enable them, you should get a clear compile-time message.
    // ------------------------------------------------------------
    {
        // Binary output requires compile option OUT_ENABLE_BINARY.
        auto vb = 0b11001010;
        out::debug<"Binary demo: value={}, bin={:b}">(console, vb, vb);
        // Floating-point output requires compile option OUT_ENABLE_FLOAT.
        auto vf = 3.1415926f;
        out::debug<"Float demo: f={:f}">(console, vf);
        out::debug<"Float precision: f={:.2f}">(console, vf);
    }

    out::print<"==========================\r\n">(console);

    // ------------------------------------------------------------
    // ANSI: default (no color) -> injected -> disabled
    // ------------------------------------------------------------
    // ANSI tokens (compile-time enable/disable).
    out::warn<"{}WARN{} default color reset">(console, out::fg(out::color::yellow), out::reset);
    out::info<"{}INFO{} back to normal">(console, out::fg(out::color::green), out::reset);
    // enable
    auto console_ansi = out::ansi_with<true>(console);
    out::error<"{}{}{}{}">(console_ansi, out::fg(out::color::red), out::bold, "CRITICAL ERROR", out::reset);
    out::warn<"{}WARN{} default color reset">(console_ansi, out::fg(out::color::yellow), out::reset);
    out::info<"{}INFO{} back to normal">(console_ansi, out::fg(out::color::green), out::reset);
    // disable
    auto console_plain = out::ansi_with<false>(console);
    out::info<"{}INFO{} ansi off">(console_plain, out::fg(out::color::green), out::reset);

    // Logger chain: styles are outside format args.
    out::log<out::level::info>(console_ansi)
        .style(out::fg(out::color::green), out::bold)
        .println<"Status: {}">("OK");
    // Logger sugar: default ANSI + level prefix.
    out::logc<out::level::warn>(console)
        .level_prefix()
        .println<"Colored warning">();

    out::print<"==========================\r\n">(console_ansi);

    // Shorter names via using namespace out
    {
        using namespace out;

        auto text = "The text line";
        info<"{} is normal">(console_ansi, text);
        info<"{}{} add italic">(console_ansi, italic, text);
        info<"{}{} add bold">(console_ansi, bold, text);
        info<"{}{} add underline">(console_ansi, underline, text);
        info<"{}{} add Foreground">(console_ansi, !color::red, text);
        info<"{}{} add Background color">(console_ansi, ~color::blue, text);
        info<"{}{} add dim intensity">(console_ansi, dim, text);
        info<"{}{} cancel dim and bold">(console_ansi, "\x1b[22m", text);
        info<"{}{} is reset">(console_ansi, reset, text);

        print<"==========================\r\n">(console_ansi);
    }


    // ------------------------------------------------------------
    // Lazy evaluation: callable runs only when the level is enabled.
    // ------------------------------------------------------------
    out::trace<"Expensive: {}">(console, out::lazy([] { return 99; }));
    auto lazy_v = out::lazy([] { return 123; });
    out::trace<"Lazy lvalue: {}">(console, lazy_v);

    // ------------------------------------------------------------
    // Timestamp
    // ------------------------------------------------------------
    out::info<"Timestamp(ms): {} Event occurred">(console, now_ms);

    // ------------------------------------------------------------
    // Domain filtering
    // ------------------------------------------------------------
    out::buffer_sink<256> cap;
    // Filtered out; nothing is written into cap buffer.
    cap.clear();
    out::emit<out::level::info, noisy_domain, "[noisy] {}">(cap, "SHOULD NOT APPEAR");
    out::print<"{}">(console, cap.view());  // empty output

    cap.clear();
    out::emit<out::level::info, network_domain,   "[net] {}">(cap, "Connected");
    out::print<"{}">(console, cap.view());
    // Domain name (opt-in prefix)
    out::log<out::level::info, network_domain>(console)
        .domain_prefix()
        .println<"Domain prefix on">();

    // Newline policy per logger instance.
    out::log<out::level::info>(console)
        .set_newline(out::newline::lf)
        .println<"LF newline">();

    // ------------------------------------------------------------
    // Sinks: line-buffered + fixed buffer
    // ------------------------------------------------------------
    out::line_buffered_sink line_buf{console}; // flushes on '\n'.
    out::print<"Buffered line {}\r\n">(line_buf, 1);

    out::buffer_sink<256> buf;
    out::print<"Buf: {}\r\n">(buf, 123);
    out::print<"More: {}\r\n">(buf, "OK");
    out::print<"{}\r\n">(console, buf.view());

#if 0
    // ------------------------------------------------------------
    // Dev sink experiments: write/ansi/flush metrics
    // ------------------------------------------------------------
    out::dev_sink<1024> dev;
    auto dump_metrics = [&](std::string_view name) {
        const auto total_calls = dev.write_calls_total();
        const auto avg = total_calls ? (dev.bytes_total / total_calls) : 0u;
        out::info<"[dev] {} bytes_calls={} ansi_calls={} total_bytes={} flush_calls={} total_calls={} avg={}">(
            console, name, dev.bytes_calls, dev.ansi_calls, dev.bytes_total, dev.flush_calls, total_calls, avg);
    };

    // Case A: token unroll threshold (compile twice with/without OUT_UNROLL_TOKENS).
    dev.reset();
    out::raw(dev).println<"A{}B{}C{}D{}E{}F{}G{}H{}I{}J{}">(1,2,3,4,5,6,7,8,9,10);
    dump_metrics("A1 tokens<=max");
    dev.reset();
    out::raw(dev).println<
        "A{}B{}C{}D{}E{}F{}G{}H{}I{}J{}K{}L{}M{}N{}O{}P{}Q{}R{}S{}T{}U{}V{}W{}X{}Y{}Z{}a{}b{}c{}d{}e{}f{}g{}h{}i{}j{}k{}l{}m{}n{}"
    >(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40);
    dump_metrics("A2 tokens>max");

    // Case B: raw vs log flush policy.
    dev.reset();
    out::raw(dev).println<"Hello {}">(123);
    dump_metrics("B1 raw");
    dev.reset();
    out::log<out::level::info>(dev).println<"Hello {}">(123);
    dump_metrics("B2 log");
    dev.reset();
    out::log<out::level::info>(dev).no_flush().println<"Hello {}">(123);
    dump_metrics("B3 log no_flush");

    // Case C: ANSI path (style vs format tokens).
    dev.reset();
    out::logc<out::level::info>(dev)
        .style(out::fg(out::color::red), out::bold)
        .println<"X{}">(1);
    dump_metrics("C1 style");
    dev.reset();
    out::logc<out::level::info>(dev).println<"{}X{}">(out::fg(out::color::red), out::reset);
    dump_metrics("C2 format");

    // Case D: padding impact on write calls.
    dev.reset();
    out::raw(dev).println<"'{:08X}'">(0x12AB);
    dump_metrics("D1 padded");
    dev.reset();
    out::raw(dev).println<"'{}'">(0x12AB);
    dump_metrics("D2 plain");
#endif

    // TODO: add more error examples
    // out::debug<"{}\t{}">(uart, 42); // argument count mismatch
    // out::debug<"{">(uart, 42); // missing closing brace
}
