#include "mvp_app.hpp"

#include "qemu_platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace {
    using namespace charm::mvp;

    struct QemuTextSink {
        std::size_t write_count{0};
        std::size_t flush_count{0};

        static Transfer write(void* context, const std::string_view text) noexcept {
            auto& self = *static_cast<QemuTextSink*>(context);
            qemu::write_text(text);
            ++self.write_count;
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
        static constexpr std::uint64_t value_ms = 424242;
        std::size_t read_count{0};

        static ClockSample now(void* context) noexcept {
            auto& self = *static_cast<QemuClock*>(context);
            ++self.read_count;
            return {Status{}, value_ms};
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
                           const std::uint64_t lba,
                           const std::span<std::byte> out) noexcept {
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
                            const std::uint64_t lba,
                            const std::span<const std::byte> in) noexcept {
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

    struct Fixture {
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
            Binding{app::requirements[0].key, ProvisionKey::text_sink},
            Binding{app::requirements[1].key, ProvisionKey::clock},
            Binding{app::requirements[2].key, ProvisionKey::block_device},
        };
    };

    bool positive_case() noexcept {
        Fixture fixture{};
        const auto resolved = resolve(
            app::requirements,
            ProfileView{fixture.provisions, fixture.bindings});
        if (!resolved.is_ok()) {
            return false;
        }
        const auto result = app::run(resolved.context);
        const bool ok = result.is_ok() &&
                        result.evidence.timestamp_ms == QemuClock::value_ms &&
                        result.evidence.record_checksum == 0x49b880f0U &&
                        result.evidence.stored &&
                        result.evidence.verified &&
                        result.evidence.reported &&
                        fixture.clock_provider.read_count == 1U &&
                        fixture.block_provider.write_count == 1U &&
                        fixture.block_provider.read_count == 1U &&
                        fixture.block_provider.flush_count == 1U &&
                        fixture.text_provider.write_count == 1U &&
                        fixture.text_provider.flush_count == 1U;
        if (!ok) {
            return false;
        }
        qemu::write_text("[charm-capability-mvp-qemu] positive=ok timestamp=");
        qemu::write_u64(result.evidence.timestamp_ms);
        qemu::write_text(" checksum=0x");
        qemu::write_hex_u32(result.evidence.record_checksum);
        qemu::write_char('\n');
        return true;
    }

    bool missing_case() noexcept {
        Fixture fixture{};
        const std::array bindings{
            fixture.bindings[0],
            fixture.bindings[1],
        };
        const auto resolved = resolve(
            app::requirements,
            ProfileView{fixture.provisions, bindings});
        std::size_t app_start_count = 0;
        if (resolved.is_ok()) {
            ++app_start_count;
            (void)app::run(resolved.context);
        }
        const bool ok = resolved.failure == ResolutionFailure::missing_binding &&
                        resolved.requirement_index == 2U &&
                        !resolved.context.valid() &&
                        app_start_count == 0U;
        if (!ok) {
            return false;
        }
        qemu::write_text("[charm-capability-mvp-qemu] missing=");
        qemu::write_text(resolution_failure_name(resolved.failure));
        qemu::write_text(" start_count=");
        qemu::write_u64(app_start_count);
        qemu::write_char('\n');
        return true;
    }
}

extern "C" int main() {
    charm::mvp::qemu::write_text("[charm-capability-mvp-qemu] boot\n");
    const bool ok = positive_case() && missing_case();
    if (ok) {
        charm::mvp::qemu::write_text("[charm-capability-mvp-qemu] ok\n");
        charm::mvp::qemu::exit(0);
    }
    charm::mvp::qemu::write_text("[charm-capability-mvp-qemu] failed\n");
    charm::mvp::qemu::exit(1);
}
