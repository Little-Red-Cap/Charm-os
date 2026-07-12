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

    struct Suite {
        std::size_t checks{0};
        std::size_t failures{0};

        void expect(const bool condition, const char* message) noexcept {
            ++checks;
            if (!condition) {
                ++failures;
                std::fprintf(stderr, "[ERR] %s\n", message);
            }
        }
    };

    struct FaultFixture {
        static constexpr std::size_t storage_size = 512;

        Status clock_status{};
        Status block_write_status{};
        Status block_flush_status{};
        Status block_read_status{};
        Status report_write_status{};
        Status report_flush_status{};
        std::uint64_t timestamp_ms{424242};
        std::uint64_t block_size_value{512};
        std::uint64_t block_count_value{4};
        bool corrupt_read{false};
        bool partial_report{false};

        std::array<std::byte, storage_size> storage{};
        std::size_t clock_reads{0};
        std::size_t block_writes{0};
        std::size_t block_flushes{0};
        std::size_t block_reads{0};
        std::size_t report_writes{0};
        std::size_t report_flushes{0};

        TextSink text{this, &text_write, &text_flush};
        Clock clock{this, &clock_now};
        BlockDevice block{this,
                          &block_read,
                          &block_write,
                          &block_flush,
                          &block_size,
                          &block_count};
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

        static Transfer text_write(void* context, const std::string_view text_value) noexcept {
            auto& self = *static_cast<FaultFixture*>(context);
            ++self.report_writes;
            if (!self.report_write_status.is_ok()) {
                return {self.report_write_status, 0};
            }
            const auto bytes = self.partial_report && !text_value.empty()
                                   ? text_value.size() - 1U
                                   : text_value.size();
            return {Status{}, bytes};
        }

        static Status text_flush(void* context) noexcept {
            auto& self = *static_cast<FaultFixture*>(context);
            ++self.report_flushes;
            return self.report_flush_status;
        }

        static ClockSample clock_now(void* context) noexcept {
            auto& self = *static_cast<FaultFixture*>(context);
            ++self.clock_reads;
            return {self.clock_status, self.timestamp_ms};
        }

        static Status block_read(void* context,
                                 const std::uint64_t lba,
                                 const std::span<std::byte> out) noexcept {
            auto& self = *static_cast<FaultFixture*>(context);
            ++self.block_reads;
            if (!self.block_read_status.is_ok()) {
                return self.block_read_status;
            }
            if (lba != 0U || out.size() > self.storage.size()) {
                return {StatusCode::out_of_range};
            }
            std::memcpy(out.data(), self.storage.data(), out.size());
            if (self.corrupt_read && !out.empty()) {
                out[0] ^= std::byte{0xff};
            }
            return {};
        }

        static Status block_write(void* context,
                                  const std::uint64_t lba,
                                  const std::span<const std::byte> in) noexcept {
            auto& self = *static_cast<FaultFixture*>(context);
            ++self.block_writes;
            if (!self.block_write_status.is_ok()) {
                return self.block_write_status;
            }
            if (lba != 0U || in.size() > self.storage.size()) {
                return {StatusCode::out_of_range};
            }
            std::memcpy(self.storage.data(), in.data(), in.size());
            return {};
        }

        static Status block_flush(void* context) noexcept {
            auto& self = *static_cast<FaultFixture*>(context);
            ++self.block_flushes;
            return self.block_flush_status;
        }

        static std::uint64_t block_size(void* context) noexcept {
            return static_cast<FaultFixture*>(context)->block_size_value;
        }

        static std::uint64_t block_count(void* context) noexcept {
            return static_cast<FaultFixture*>(context)->block_count_value;
        }

        [[nodiscard]] ResolvedContext context() const noexcept {
            return ResolvedContext{&text, &clock, &block};
        }
    };

    void expect_resolution_failure(Suite& suite,
                                   const ResolutionResult& result,
                                   const ResolutionFailure expected,
                                   const std::size_t expected_index) noexcept {
        std::size_t app_start_count = 0;
        if (result.is_ok()) {
            ++app_start_count;
        }
        suite.expect(result.failure == expected, "resolution failure classification mismatch");
        suite.expect(result.requirement_index == expected_index,
                     "resolution failure requirement index mismatch");
        suite.expect(!result.context.valid(), "failed resolution must not expose a valid context");
        suite.expect(app_start_count == 0, "failed resolution must stop before app start");
    }

    std::size_t run_resolution_matrix(Suite& suite) {
        std::size_t cases = 0;
        constexpr std::array permutations{
            std::array<std::size_t, 3>{0, 1, 2},
            std::array<std::size_t, 3>{0, 2, 1},
            std::array<std::size_t, 3>{1, 0, 2},
            std::array<std::size_t, 3>{1, 2, 0},
            std::array<std::size_t, 3>{2, 0, 1},
            std::array<std::size_t, 3>{2, 1, 0},
        };

        for (const auto& order : permutations) {
            FaultFixture fixture{};
            std::array<Binding, 3> bindings{};
            for (std::size_t index = 0; index < bindings.size(); ++index) {
                bindings[index] = fixture.bindings[order[index]];
            }
            const auto result = resolve(app::requirements,
                                        ProfileView{fixture.provisions, bindings});
            suite.expect(result.is_ok(), "binding order must not change successful resolution");
            suite.expect(result.context.report == &fixture.text,
                         "resolved TextSink endpoint mismatch");
            suite.expect(result.context.monotonic_time == &fixture.clock,
                         "resolved Clock endpoint mismatch");
            suite.expect(result.context.record_store == &fixture.block,
                         "resolved BlockDevice endpoint mismatch");
            ++cases;
        }

        for (std::size_t failed_index = 0; failed_index < app::requirements.size(); ++failed_index) {
            FaultFixture fixture{};
            std::array<Binding, 2> bindings{};
            std::size_t output_index = 0;
            for (std::size_t index = 0; index < fixture.bindings.size(); ++index) {
                if (index != failed_index) {
                    bindings[output_index++] = fixture.bindings[index];
                }
            }
            expect_resolution_failure(
                suite,
                resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                ResolutionFailure::missing_binding,
                failed_index);
            ++cases;
        }

        for (std::size_t failed_index = 0; failed_index < app::requirements.size(); ++failed_index) {
            FaultFixture fixture{};
            std::array<Binding, 4> bindings{
                fixture.bindings[0],
                fixture.bindings[1],
                fixture.bindings[2],
                fixture.bindings[failed_index],
            };
            expect_resolution_failure(
                suite,
                resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                ResolutionFailure::duplicate_binding,
                failed_index);
            ++cases;
        }

        for (std::size_t failed_index = 0; failed_index < app::requirements.size(); ++failed_index) {
            FaultFixture fixture{};
            auto bindings = fixture.bindings;
            bindings[failed_index].provision_index = fixture.provisions.size();
            expect_resolution_failure(
                suite,
                resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                ResolutionFailure::invalid_provision_index,
                failed_index);
            ++cases;
        }

        for (std::size_t failed_index = 0; failed_index < app::requirements.size(); ++failed_index) {
            FaultFixture fixture{};
            auto bindings = fixture.bindings;
            bindings[failed_index].provision_index = (failed_index + 1U) % fixture.provisions.size();
            expect_resolution_failure(
                suite,
                resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                ResolutionFailure::contract_mismatch,
                failed_index);
            ++cases;
        }

        for (std::size_t failed_index = 0; failed_index < app::requirements.size(); ++failed_index) {
            FaultFixture fixture{};
            auto provisions = fixture.provisions;
            switch (failed_index) {
            case 0:
                provisions[0] = Provision{ContractId::text_sink, nullptr, nullptr, nullptr};
                break;
            case 1:
                provisions[1] = Provision{ContractId::clock, nullptr, nullptr, nullptr};
                break;
            default:
                provisions[2] = Provision{ContractId::block_device, nullptr, nullptr, nullptr};
                break;
            }
            expect_resolution_failure(
                suite,
                resolve(app::requirements, ProfileView{provisions, fixture.bindings}),
                ResolutionFailure::invalid_provision,
                failed_index);
            ++cases;
        }

        return cases;
    }

    void expect_record_evidence(Suite& suite,
                                const app::RunResult& result,
                                const bool stored,
                                const bool verified,
                                const bool reported) noexcept {
        suite.expect(result.evidence.timestamp_ms == 424242,
                     "record-stage timestamp evidence mismatch");
        suite.expect(result.evidence.record_checksum == 0x49b880f0U,
                     "record-stage checksum evidence mismatch");
        suite.expect(result.evidence.record_bytes == app::record_size,
                     "record-stage byte count evidence mismatch");
        suite.expect(result.evidence.stored == stored, "stored evidence flag mismatch");
        suite.expect(result.evidence.verified == verified, "verified evidence flag mismatch");
        suite.expect(result.evidence.reported == reported, "reported evidence flag mismatch");
    }

    std::size_t run_app_failure_matrix(Suite& suite) {
        std::size_t cases = 0;

        {
            const auto result = app::run({});
            suite.expect(result.code == app::RunCode::invalid_context,
                         "invalid context classification mismatch");
            suite.expect(result.evidence.timestamp_ms == 0,
                         "invalid context must not produce timestamp evidence");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.clock_status = {StatusCode::io_error};
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::clock_failed,
                         "clock failure classification mismatch");
            suite.expect(fixture.clock_reads == 1 && fixture.block_writes == 0 &&
                             fixture.report_writes == 0,
                         "clock failure must stop before storage and report");
            ++cases;
        }

        for (const auto geometry : std::array{
                 std::array<std::uint64_t, 2>{app::record_size - 1U, 4},
                 std::array<std::uint64_t, 2>{app::max_block_size + 1U, 4},
                 std::array<std::uint64_t, 2>{app::record_size, 0},
             }) {
            FaultFixture fixture{};
            fixture.block_size_value = geometry[0];
            fixture.block_count_value = geometry[1];
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::unsupported_geometry,
                         "unsupported geometry classification mismatch");
            suite.expect(result.evidence.timestamp_ms == 424242 &&
                             result.evidence.record_bytes == 0,
                         "geometry failure evidence boundary mismatch");
            suite.expect(fixture.block_writes == 0 && fixture.report_writes == 0,
                         "geometry failure must stop before write and report");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.block_write_status = {StatusCode::io_error};
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::write_failed,
                         "write failure classification mismatch");
            expect_record_evidence(suite, result, false, false, false);
            suite.expect(fixture.block_flushes == 0 && fixture.block_reads == 0 &&
                             fixture.report_writes == 0,
                         "write failure must stop before later stages");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.block_flush_status = {StatusCode::io_error};
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::flush_failed,
                         "storage flush failure classification mismatch");
            expect_record_evidence(suite, result, true, false, false);
            suite.expect(fixture.block_reads == 0 && fixture.report_writes == 0,
                         "storage flush failure must stop before read and report");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.block_read_status = {StatusCode::io_error};
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::read_failed,
                         "read failure classification mismatch");
            expect_record_evidence(suite, result, true, false, false);
            suite.expect(fixture.report_writes == 0,
                         "read failure must stop before report");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.corrupt_read = true;
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::verify_failed,
                         "verify failure classification mismatch");
            expect_record_evidence(suite, result, true, false, false);
            suite.expect(fixture.report_writes == 0,
                         "verify failure must stop before report");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.report_write_status = {StatusCode::io_error};
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::report_failed,
                         "report write failure classification mismatch");
            expect_record_evidence(suite, result, true, true, false);
            suite.expect(fixture.report_flushes == 0,
                         "failed report write must not flush");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.partial_report = true;
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::report_failed,
                         "partial report classification mismatch");
            expect_record_evidence(suite, result, true, true, false);
            suite.expect(fixture.report_flushes == 0,
                         "partial report must not flush");
            ++cases;
        }
        {
            FaultFixture fixture{};
            fixture.report_flush_status = {StatusCode::io_error};
            const auto result = app::run(fixture.context());
            suite.expect(result.code == app::RunCode::report_failed,
                         "report flush failure classification mismatch");
            expect_record_evidence(suite, result, true, true, false);
            suite.expect(fixture.report_flushes == 1,
                         "report flush failure must attempt one flush");
            ++cases;
        }

        return cases;
    }
}

int main() {
    Suite suite{};
    const auto resolution_cases = run_resolution_matrix(suite);
    const auto app_cases = run_app_failure_matrix(suite);

    std::printf("[charm-capability-mvp-host-matrix] resolution_cases=%zu app_cases=%zu checks=%zu failures=%zu\n",
                resolution_cases,
                app_cases,
                suite.checks,
                suite.failures);
    if (suite.failures != 0U) {
        return 1;
    }
    std::puts("[charm-capability-mvp-host-matrix] ok");
    return 0;
}
