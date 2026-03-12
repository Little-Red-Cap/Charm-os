#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <thread>

import charm.foundation;
import charm.runtime;
import charm.system.bringup;
import charm.system.bringup.win_stub;
import charm.system.app_host;
import out.api;
import platform.board.win_stub;
import platform.win.irq_guard;
import platform.win.time_source;
import platform.win.wakeup;
import util.expected;

namespace {
    struct StdoutSink {
        out::result<std::size_t> write(out::bytes b) noexcept {
            if (std::fwrite(b.data(), 1, b.size(), stdout) != b.size()) {
                return util::unexpected(out::errc::io_error);
            }
            return out::ok(b.size());
        }
        out::result<std::size_t> flush() noexcept {
            return std::fflush(stdout) == 0 ? out::ok<std::size_t>(0u)
                                            : util::unexpected(out::errc::io_error);
        }
    };

    struct ScriptedInput {
        util::u64 start_us{0};
        bool enabled{true};
    };

    bool is_down(void* ctx, input::Button b) noexcept {
        auto* s = static_cast<ScriptedInput*>(ctx);
        if (!s || !s->enabled) return false;
        if (b != input::Button::Enter) return false;
        const auto now = platform::win::SteadyClock::now();
        const auto ms = (now - s->start_us) / 1000u;
        return ((ms / 300u) % 2u) != 0u;
    }

    input::PointerRaw read_pointer(void* ctx) noexcept {
        (void)ctx;
        return input::PointerRaw{.down = false, .x = -1, .y = -1, .id = 0};
    }

    input::AxisRaw read_axis(void* ctx) noexcept {
        (void)ctx;
        return input::AxisRaw{};
    }

    std::optional<std::uint8_t> pop_encoder_ab(void* ctx) noexcept {
        (void)ctx;
        return std::nullopt;
    }

    struct RawPrintCtx {
        StdoutSink* sink{nullptr};
    };

    bool on_raw(void* ctx, const input::RawInputEvent& ev) noexcept {
        auto* c = static_cast<RawPrintCtx*>(ctx);
        if (!c || !c->sink) return true;
        int code = 0;
        int value = 0;
        if (ev.type == input::RawInputEventType::Button) {
            code = static_cast<int>(ev.button);
            value = ev.pressed ? 1 : 0;
        }
        (void)out::println<"[raw] t={} type={} code={} value={}">(
            *c->sink,
            ev.ms,
            static_cast<int>(ev.type),
            code,
            value);
        return true;
    }
}

int main(int argc, char** argv) {
    StdoutSink sink{};
    bool enable_input = true;
    if (argc > 1 && std::strcmp(argv[1], "--no-input") == 0) {
        enable_input = false;
    }
    auto caps = platform::board::win_stub::make_board_caps();
    charm::system::PumpCaps pump_caps{};
    charm::system::AppHost<charm::system::PumpCaps> host{pump_caps};
    ScriptedInput scripted{platform::win::SteadyClock::now(), enable_input};
    const hal::RawInputDriver kDriver{
        .ctx = &scripted,
        .is_down = &is_down,
        .read_pointer = &read_pointer,
        .read_axis = &read_axis,
        .pop_encoder_ab = &pop_encoder_ab
    };
    caps.input.driver = &kDriver;
    RawPrintCtx print_ctx{&sink};
    charm::system::BringupMinimal<8, 16, 8, 64, 64> bringup{
        caps,
        host,
        8,
        &on_raw,
        &print_ctx
    };

    auto r = bringup.start();
    if (!r) {
    (void)out::println<"[ERR] bringup failed err={}">(sink, static_cast<int>(r.error()));
        return 1;
    }
    (void)out::println<"[OK] bringup ok">(sink);

    const auto start = platform::win::SteadyClock::now();
    while ((platform::win::SteadyClock::now() - start) < 500000u) {
        (void)host.run_once();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}
