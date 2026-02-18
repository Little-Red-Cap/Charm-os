// gui.perf.cppm
// Simple FPS counter based on real time (ms). Portable across platforms.

module;
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <expected>
#include <string_view>

export module gui.perf;

import out.core;
import out.format;
import trace_core;
import util.units;

namespace {
    struct trunc_sink {
        char* buf{nullptr};
        std::size_t cap{0};
        std::size_t pos{0};

        out::result<std::size_t> write(out::bytes b) noexcept
        {
            if (!buf || cap == 0) return std::unexpected(out::errc::buffer_overflow);
            const std::size_t avail = (pos < cap) ? (cap - pos) : 0;
            const std::size_t n = (b.size() < avail) ? b.size() : avail;
            if (n > 0) {
                std::memcpy(buf + pos, b.data(), n);
                pos += n;
            }
            if (n < b.size()) return std::unexpected(out::errc::buffer_overflow);
            return out::ok(b.size());
        }
    };

    template <out::fixed_string Fmt, class... Args>
    inline std::string_view format_to(char* buf, std::size_t size, Args&&... args) noexcept
    {
        if (!buf || size == 0) return {};
        trunc_sink sink{buf, size - 1u, 0u};
        (void)out::vprint<Fmt>(sink, std::forward<Args>(args)...);
        const std::size_t n = sink.pos;
        buf[n] = '\0';
        return {buf, n};
    }
}

export namespace gui::perf
{
    struct TickSource {
        void* ctx{nullptr};
        std::uint32_t (*now_ms)(void*){nullptr};

        [[nodiscard]] inline util::Milliseconds now() const noexcept
        {
            return util::ms(now_ms ? now_ms(ctx) : 0u);
        }
    };

    [[nodiscard]] inline TickSource make_tick_source(std::uint32_t (*fn)(void*), void* ctx) noexcept
    {
        TickSource out{};
        out.ctx = ctx;
        out.now_ms = fn;
        return out;
    }

    struct TraceHook {
        void* ctx{nullptr};
        void (*emit)(void*, trace::TraceKind, std::uint32_t, std::uint64_t) noexcept {nullptr};
        std::uint32_t fps_id{0};
        std::uint32_t frame_id{0};
    };

    struct FpsCounter {
        util::Milliseconds last_ms{};
        std::uint32_t frames{0};
        float fps{0.0f};
        TraceHook trace{};

        // Returns true when fps has been updated (about once per second).
        bool update(std::uint32_t now_ms) noexcept
        {
            return update(util::ms(now_ms));
        }

        bool update(util::Milliseconds now_ms) noexcept
        {
            ++frames;
            if (last_ms.value == 0) {
                last_ms = now_ms;
                return false;
            }
            const std::uint64_t span = now_ms.value - last_ms.value;
            if (span < 1000) return false;
            fps = span ? (static_cast<float>(frames) * 1000.0f / static_cast<float>(span)) : 0.0f;
            const std::uint32_t frames_last = frames;
            frames = 0;
            last_ms = now_ms;
            if (trace.emit) {
                if (trace.fps_id != 0) {
                    const std::uint64_t fps_x1000 = (std::uint64_t)(fps * 1000.0f + 0.5f);
                    trace.emit(trace.ctx, trace::TraceKind::counter, trace.fps_id, fps_x1000);
                }
                if (trace.frame_id != 0) {
                    trace.emit(trace.ctx, trace::TraceKind::counter_delta, trace.frame_id, frames_last);
                }
            }
            return true;
        }

        // Update using an injected time source (portable across platforms).
        bool update(const TickSource& clock) noexcept
        {
            return update(clock.now());
        }

        [[nodiscard]] inline float value() const noexcept { return fps; }

        void set_trace_hook(TraceHook hook) noexcept { trace = hook; }

        void format(char* out, std::size_t out_size, const char* label) const noexcept
        {
            if (!out || out_size == 0) return;
            const char* name = label ? label : "FPS";
            (void)format_to<"{}: {:.1f}">(out, out_size, name, fps);
        }
    };
} // namespace gui::perf
