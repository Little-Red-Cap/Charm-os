#include "capability_mvp.h"

#include "console.h"
#include "mvp_app.hpp"
#include "port.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace h747::apps::capability_mvp {
namespace {

using namespace charm::mvp;

struct BoardTextSink {
    std::size_t bytes_accepted{0};
    std::size_t flush_count{0};

    static Transfer write(void* context, const std::string_view text) noexcept {
        auto& self = *static_cast<BoardTextSink*>(context);
        for (const char value : text) {
            h747::console::write_char(value);
        }
        self.bytes_accepted += text.size();
        return {Status{}, text.size()};
    }

    static Status flush(void* context) noexcept {
        auto& self = *static_cast<BoardTextSink*>(context);
        ++self.flush_count;
        return {};
    }

    [[nodiscard]] TextSink endpoint() noexcept {
        return TextSink{this, &write, &flush};
    }
};

struct BoardClock {
    static constexpr std::uint64_t evidence_epoch_ms = 424242U;

    std::uint32_t first_tick_ms{0};
    std::size_t read_count{0};
    bool anchored{false};

    static ClockSample now(void* context) noexcept {
        auto& self = *static_cast<BoardClock*>(context);
        const auto tick = h747::port::tick_ms();
        if (!self.anchored) {
            self.first_tick_ms = tick;
            self.anchored = true;
        }
        ++self.read_count;
        return {Status{}, evidence_epoch_ms +
                              static_cast<std::uint32_t>(tick - self.first_tick_ms)};
    }

    [[nodiscard]] Clock endpoint() noexcept {
        return Clock{this, &now};
    }
};

struct BoardBlockDevice {
    static constexpr std::size_t block_size_value = 512;
    static constexpr std::size_t block_count_value = 4;

    std::array<std::byte, block_size_value * block_count_value> storage{};
    std::size_t read_count{0};
    std::size_t write_count{0};
    std::size_t flush_count{0};

    static Status read(void* context,
                       const std::uint64_t lba,
                       const std::span<std::byte> out) noexcept {
        auto& self = *static_cast<BoardBlockDevice*>(context);
        if (lba >= block_count_value || out.size() != block_size_value) {
            return {StatusCode::out_of_range};
        }
        const auto offset = static_cast<std::size_t>(lba) * block_size_value;
        for (std::size_t index = 0; index < out.size(); ++index) {
            out[index] = self.storage[offset + index];
        }
        ++self.read_count;
        return {};
    }

    static Status write(void* context,
                        const std::uint64_t lba,
                        const std::span<const std::byte> in) noexcept {
        auto& self = *static_cast<BoardBlockDevice*>(context);
        if (lba >= block_count_value || in.size() != block_size_value) {
            return {StatusCode::out_of_range};
        }
        const auto offset = static_cast<std::size_t>(lba) * block_size_value;
        for (std::size_t index = 0; index < in.size(); ++index) {
            self.storage[offset + index] = in[index];
        }
        ++self.write_count;
        return {};
    }

    static Status flush(void* context) noexcept {
        auto& self = *static_cast<BoardBlockDevice*>(context);
        ++self.flush_count;
        return {};
    }

    static std::uint64_t block_size(void*) noexcept {
        return block_size_value;
    }

    static std::uint64_t block_count(void*) noexcept {
        return block_count_value;
    }

    [[nodiscard]] BlockDevice endpoint() noexcept {
        return BlockDevice{this, &read, &write, &flush, &block_size, &block_count};
    }
};

struct BoardFixture {
    BoardTextSink text_provider{};
    BoardClock clock_provider{};
    BoardBlockDevice block_provider{};
    TextSink text{text_provider.endpoint()};
    Clock clock{clock_provider.endpoint()};
    BlockDevice block{block_provider.endpoint()};
    std::array<Provision, 3> provisions{
        Provision::for_text_sink(text),
        Provision::for_clock(clock),
        Provision::for_block_device(block),
    };
    std::array<Binding, 3> bindings{
        Binding{app::requirements[0], 0},
        Binding{app::requirements[1], 1},
        Binding{app::requirements[2], 2},
    };
};

void write_decimal(std::uint64_t value) noexcept {
    char digits[20]{};
    std::size_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    while (count != 0U) {
        h747::console::write_char(digits[--count]);
    }
}

void write_hex32(const std::uint32_t value) noexcept {
    constexpr char digits[] = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4) {
        h747::console::write_char(digits[(value >> shift) & 0x0fU]);
    }
}

void write_failure(const std::string_view stage) noexcept {
    h747::console::write("[charm-capability-mvp-h747] error=");
    h747::console::write(stage.data());
    h747::console::write_char('\n');
}

bool positive_case() noexcept {
    BoardFixture fixture{};
    const auto resolved = resolve(app::requirements,
                                  ProfileView{fixture.provisions, fixture.bindings});
    if (!resolved.is_ok()) {
        write_failure(resolution_failure_name(resolved.failure));
        return false;
    }

    const auto run = app::run(resolved.context);
    if (!run.is_ok() || run.evidence.timestamp_ms != BoardClock::evidence_epoch_ms ||
        !run.evidence.stored || !run.evidence.verified || !run.evidence.reported ||
        fixture.clock_provider.read_count != 1U ||
        fixture.block_provider.write_count != 1U ||
        fixture.block_provider.read_count != 1U ||
        fixture.block_provider.flush_count != 1U ||
        fixture.text_provider.flush_count != 1U) {
        write_failure("app_run");
        return false;
    }

    h747::console::write("[charm-capability-mvp-h747] positive=ok timestamp=");
    write_decimal(run.evidence.timestamp_ms);
    h747::console::write(" checksum=0x");
    write_hex32(run.evidence.record_checksum);
    h747::console::write_char('\n');
    return true;
}

bool missing_case() noexcept {
    BoardFixture fixture{};
    const std::array bindings{
        fixture.bindings[0],
        fixture.bindings[1],
    };
    const auto resolved = resolve(app::requirements,
                                  ProfileView{fixture.provisions, bindings});
    std::size_t app_start_count = 0;
    if (resolved.is_ok()) {
        ++app_start_count;
        (void)app::run(resolved.context);
    }
    if (resolved.failure != ResolutionFailure::missing_binding ||
        resolved.requirement_index != 2U || app_start_count != 0U) {
        write_failure("missing_case");
        return false;
    }

    h747::console::write("[charm-capability-mvp-h747] missing=");
    h747::console::write(resolution_failure_name(resolved.failure));
    h747::console::write(" start_count=");
    write_decimal(app_start_count);
    h747::console::write_char('\n');
    return true;
}

} // namespace

void init() noexcept {
    const bool positive_ok = positive_case();
    const bool missing_ok = missing_case();
    h747::console::write_line((positive_ok && missing_ok)
                                  ? "[charm-capability-mvp-h747] ok"
                                  : "[charm-capability-mvp-h747] failed");
}

void loop_once() noexcept {
}

} // namespace h747::apps::capability_mvp
