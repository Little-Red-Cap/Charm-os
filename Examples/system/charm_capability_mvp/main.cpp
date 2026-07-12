#include "mvp_app.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace {
    using namespace charm::mvp;

    struct HostTextSink {
        std::array<char, 64> bytes{};
        std::size_t size{0};
        std::size_t flush_count{0};

        static Transfer write(void* context, std::string_view text) noexcept {
            auto& self = *static_cast<HostTextSink*>(context);
            if (text.size() > self.bytes.size() - self.size) {
                return {Status{StatusCode::out_of_range}, 0};
            }
            std::memcpy(self.bytes.data() + self.size, text.data(), text.size());
            self.size += text.size();
            return {Status{}, text.size()};
        }

        static Status flush(void* context) noexcept {
            auto& self = *static_cast<HostTextSink*>(context);
            ++self.flush_count;
            return {};
        }

        [[nodiscard]] TextSink endpoint() noexcept {
            return TextSink{this, &write, &flush};
        }
    };

    struct HostClock {
        std::uint64_t value_ms{424242};
        std::size_t read_count{0};

        static ClockSample now(void* context) noexcept {
            auto& self = *static_cast<HostClock*>(context);
            ++self.read_count;
            return {Status{}, self.value_ms};
        }

        [[nodiscard]] Clock endpoint() noexcept {
            return Clock{this, &now};
        }
    };

    struct HostBlockDevice {
        static constexpr std::size_t block_size_value = 512;
        static constexpr std::size_t block_count_value = 4;

        std::array<std::byte, block_size_value * block_count_value> storage{};
        std::size_t read_count{0};
        std::size_t write_count{0};
        std::size_t flush_count{0};

        static Status read(void* context,
                           std::uint64_t lba,
                           std::span<std::byte> out) noexcept {
            auto& self = *static_cast<HostBlockDevice*>(context);
            if (lba >= block_count_value || out.size() != block_size_value) {
                return {StatusCode::out_of_range};
            }
            std::memcpy(out.data(),
                        self.storage.data() + static_cast<std::size_t>(lba) * block_size_value,
                        out.size());
            ++self.read_count;
            return {};
        }

        static Status write(void* context,
                            std::uint64_t lba,
                            std::span<const std::byte> in) noexcept {
            auto& self = *static_cast<HostBlockDevice*>(context);
            if (lba >= block_count_value || in.size() != block_size_value) {
                return {StatusCode::out_of_range};
            }
            std::memcpy(self.storage.data() + static_cast<std::size_t>(lba) * block_size_value,
                        in.data(),
                        in.size());
            ++self.write_count;
            return {};
        }

        static Status flush(void* context) noexcept {
            auto& self = *static_cast<HostBlockDevice*>(context);
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

    bool expect(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    struct HostFixture {
        HostTextSink text_provider{};
        HostClock clock_provider{};
        HostBlockDevice block_provider{};
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

    bool positive_case() {
        HostFixture fixture{};
        const auto resolved = resolve(app::requirements, ProfileView{fixture.provisions, fixture.bindings});
        std::size_t app_start_count = 0;
        if (!expect(resolved.is_ok(), "complete host profile should resolve")) {
            return false;
        }

        ++app_start_count;
        const auto run = app::run(resolved.context);
        const std::string_view output{fixture.text_provider.bytes.data(), fixture.text_provider.size};

        bool ok = true;
        ok &= expect(run.is_ok(), "resolved app should run");
        ok &= expect(app_start_count == 1, "successful resolution should start app once");
        ok &= expect(run.evidence.timestamp_ms == 424242, "clock evidence should be stable");
        ok &= expect(run.evidence.stored && run.evidence.verified && run.evidence.reported,
                     "app evidence should report all semantic outcomes");
        ok &= expect(output == "charm-mvp: ok\n", "app should report through TextSink");
        ok &= expect(fixture.clock_provider.read_count == 1, "app should read clock once");
        ok &= expect(fixture.block_provider.write_count == 1 &&
                         fixture.block_provider.read_count == 1 &&
                         fixture.block_provider.flush_count == 1,
                     "app should write flush and read one record block");
        ok &= expect(fixture.text_provider.flush_count == 1, "app should flush TextSink once");
        if (ok) {
            std::printf("[charm-capability-mvp-host] positive=ok timestamp=%llu checksum=0x%08x\n",
                        static_cast<unsigned long long>(run.evidence.timestamp_ms),
                        static_cast<unsigned int>(run.evidence.record_checksum));
        }
        return ok;
    }

    bool missing_case() {
        HostFixture fixture{};
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
        const bool ok = expect(resolved.failure == ResolutionFailure::missing_binding,
                               "missing required binding should be classified") &&
                        expect(resolved.requirement_index == 2,
                               "missing failure should identify BlockDevice requirement") &&
                        expect(app_start_count == 0,
                               "missing binding must fail before app start");
        if (ok) {
            std::printf("[charm-capability-mvp-host] missing=%s start_count=%zu\n",
                        resolution_failure_name(resolved.failure),
                        app_start_count);
        }
        return ok;
    }

    bool duplicate_case() {
        HostFixture fixture{};
        const std::array bindings{
            fixture.bindings[0],
            fixture.bindings[1],
            fixture.bindings[1],
            fixture.bindings[2],
        };
        const auto resolved = resolve(app::requirements, ProfileView{fixture.provisions, bindings});
        std::size_t app_start_count = 0;
        if (resolved.is_ok()) {
            ++app_start_count;
            (void)app::run(resolved.context);
        }
        const bool ok = expect(resolved.failure == ResolutionFailure::duplicate_binding,
                               "duplicate required binding should be classified") &&
                        expect(resolved.requirement_index == 1,
                               "duplicate failure should identify Clock requirement") &&
                        expect(app_start_count == 0,
                               "duplicate binding must fail before app start");
        if (ok) {
            std::printf("[charm-capability-mvp-host] duplicate=%s start_count=%zu\n",
                        resolution_failure_name(resolved.failure),
                        app_start_count);
        }
        return ok;
    }

    bool mismatch_case() {
        HostFixture fixture{};
        auto bindings = fixture.bindings;
        bindings[2].provision_index = 1;
        const auto resolved = resolve(app::requirements, ProfileView{fixture.provisions, bindings});
        std::size_t app_start_count = 0;
        if (resolved.is_ok()) {
            ++app_start_count;
            (void)app::run(resolved.context);
        }
        const bool ok = expect(resolved.failure == ResolutionFailure::contract_mismatch,
                               "contract mismatch should be classified") &&
                        expect(resolved.requirement_index == 2,
                               "mismatch failure should identify BlockDevice requirement") &&
                        expect(app_start_count == 0,
                               "contract mismatch must fail before app start");
        if (ok) {
            std::printf("[charm-capability-mvp-host] mismatch=%s start_count=%zu\n",
                        resolution_failure_name(resolved.failure),
                        app_start_count);
        }
        return ok;
    }

    bool invalid_provision_case() {
        HostFixture fixture{};
        auto provisions = fixture.provisions;
        provisions[1] = Provision{ContractId::clock, nullptr, nullptr, nullptr};
        const auto resolved = resolve(app::requirements, ProfileView{provisions, fixture.bindings});
        std::size_t app_start_count = 0;
        if (resolved.is_ok()) {
            ++app_start_count;
            (void)app::run(resolved.context);
        }
        const bool ok = expect(resolved.failure == ResolutionFailure::invalid_provision,
                               "invalid provision should be classified") &&
                        expect(resolved.requirement_index == 1,
                               "invalid provision should identify Clock requirement") &&
                        expect(app_start_count == 0,
                               "invalid provision must fail before app start");
        if (ok) {
            std::printf("[charm-capability-mvp-host] invalid=%s start_count=%zu\n",
                        resolution_failure_name(resolved.failure),
                        app_start_count);
        }
        return ok;
    }
}

int main() {
    bool ok = true;
    ok &= positive_case();
    ok &= missing_case();
    ok &= duplicate_case();
    ok &= mismatch_case();
    ok &= invalid_provision_case();
    if (!ok) {
        return 1;
    }
    std::puts("[charm-capability-mvp-host] ok");
    return 0;
}
