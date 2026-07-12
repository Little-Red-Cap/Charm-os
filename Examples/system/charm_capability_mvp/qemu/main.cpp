#include "mvp_app.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace {
    using namespace charm::mvp;

    class CmsdkUart {
    public:
        static void write_byte(char value) noexcept {
            while ((*state() & tx_full) != 0U) {
            }
            *data() = static_cast<std::uint32_t>(static_cast<unsigned char>(value));
        }

        static void write(std::string_view text) noexcept {
            ensure_initialized();
            for (const auto value : text) {
                write_byte(value);
            }
        }

        static void write_decimal(std::uint64_t value) noexcept {
            char digits[20]{};
            std::size_t count = 0;
            do {
                digits[count++] = static_cast<char>('0' + (value % 10U));
                value /= 10U;
            } while (value != 0U);
            while (count != 0U) {
                write_byte(digits[--count]);
            }
        }

        static void write_hex32(std::uint32_t value) noexcept {
            constexpr char digits[] = "0123456789abcdef";
            for (int shift = 28; shift >= 0; shift -= 4) {
                write_byte(digits[(value >> shift) & 0x0fU]);
            }
        }

    private:
        static constexpr std::uintptr_t base = 0x40004000U;
        static constexpr std::uint32_t tx_full = 1U << 1U;
        static constexpr std::uint32_t tx_enable = 1U << 0U;
        static constexpr std::uint32_t rx_enable = 1U << 1U;

        static volatile std::uint32_t* data() noexcept {
            return reinterpret_cast<volatile std::uint32_t*>(base + 0x00U);
        }

        static volatile std::uint32_t* state() noexcept {
            return reinterpret_cast<volatile std::uint32_t*>(base + 0x04U);
        }

        static volatile std::uint32_t* control() noexcept {
            return reinterpret_cast<volatile std::uint32_t*>(base + 0x08U);
        }

        static void ensure_initialized() noexcept {
            static bool initialized = false;
            if (!initialized) {
                *control() = tx_enable | rx_enable;
                initialized = true;
            }
        }
    };

    struct QemuTextSink {
        std::size_t bytes_accepted{0};
        std::size_t flush_count{0};

        static Transfer write(void* context, std::string_view text) noexcept {
            auto& self = *static_cast<QemuTextSink*>(context);
            CmsdkUart::write(text);
            self.bytes_accepted += text.size();
            return {Status{}, text.size()};
        }

        static Status flush(void* context) noexcept {
            auto& self = *static_cast<QemuTextSink*>(context);
            ++self.flush_count;
            return {};
        }

        [[nodiscard]] TextSink endpoint() noexcept {
            return TextSink{this, &write, &flush};
        }
    };

    struct QemuClock {
        std::uint64_t value_ms{424242};
        std::size_t read_count{0};

        static ClockSample now(void* context) noexcept {
            auto& self = *static_cast<QemuClock*>(context);
            ++self.read_count;
            return {Status{}, self.value_ms};
        }

        [[nodiscard]] Clock endpoint() noexcept {
            return Clock{this, &now};
        }
    };

    struct QemuBlockDevice {
        static constexpr std::size_t block_size_value = 512;
        static constexpr std::size_t block_count_value = 4;

        std::array<std::byte, block_size_value * block_count_value> storage{};
        std::size_t read_count{0};
        std::size_t write_count{0};
        std::size_t flush_count{0};

        static Status read(void* context,
                           std::uint64_t lba,
                           std::span<std::byte> out) noexcept {
            auto& self = *static_cast<QemuBlockDevice*>(context);
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
                            std::uint64_t lba,
                            std::span<const std::byte> in) noexcept {
            auto& self = *static_cast<QemuBlockDevice*>(context);
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
            auto& self = *static_cast<QemuBlockDevice*>(context);
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

    struct QemuFixture {
        QemuTextSink text_provider{};
        QemuClock clock_provider{};
        QemuBlockDevice block_provider{};
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

    void write_failure(std::string_view stage) noexcept {
        CmsdkUart::write("[charm-capability-mvp-qemu] error=");
        CmsdkUart::write(stage);
        CmsdkUart::write("\n");
    }

    bool positive_case() noexcept {
        QemuFixture fixture{};
        const auto resolved = resolve(app::requirements, ProfileView{fixture.provisions, fixture.bindings});
        if (!resolved.is_ok()) {
            write_failure(resolution_failure_name(resolved.failure));
            return false;
        }

        const auto run = app::run(resolved.context);
        if (!run.is_ok() || run.evidence.timestamp_ms != 424242U ||
            !run.evidence.stored || !run.evidence.verified || !run.evidence.reported ||
            fixture.clock_provider.read_count != 1U ||
            fixture.block_provider.write_count != 1U ||
            fixture.block_provider.read_count != 1U ||
            fixture.block_provider.flush_count != 1U ||
            fixture.text_provider.flush_count != 1U) {
            write_failure("app_run");
            return false;
        }

        CmsdkUart::write("[charm-capability-mvp-qemu] positive=ok timestamp=");
        CmsdkUart::write_decimal(run.evidence.timestamp_ms);
        CmsdkUart::write(" checksum=0x");
        CmsdkUart::write_hex32(run.evidence.record_checksum);
        CmsdkUart::write("\n");
        return true;
    }

    bool missing_case() noexcept {
        QemuFixture fixture{};
        const std::array bindings{
            fixture.bindings[0],
            fixture.bindings[1],
        };
        const auto resolved = resolve(app::requirements, ProfileView{fixture.provisions, bindings});
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

        CmsdkUart::write("[charm-capability-mvp-qemu] missing=");
        CmsdkUart::write(resolution_failure_name(resolved.failure));
        CmsdkUart::write(" start_count=");
        CmsdkUart::write_decimal(app_start_count);
        CmsdkUart::write("\n");
        return true;
    }
}

extern "C" int charm_capability_mvp_qemu_main() noexcept {
    const bool positive_ok = positive_case();
    const bool missing_ok = missing_case();
    if (positive_ok && missing_ok) {
        CmsdkUart::write("[charm-capability-mvp-qemu] ok\n");
        return 0;
    }
    CmsdkUart::write("[charm-capability-mvp-qemu] failed\n");
    return 1;
}
