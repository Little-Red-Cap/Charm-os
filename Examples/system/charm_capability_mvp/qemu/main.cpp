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
        Status write_status{};
        Status flush_status{};
        bool partial_write{false};
        bool emit_to_uart{true};
        std::size_t bytes_accepted{0};
        std::size_t write_count{0};
        std::size_t flush_count{0};

        static Transfer write(void* context, std::string_view text) noexcept {
            auto& self = *static_cast<QemuTextSink*>(context);
            ++self.write_count;
            if (!self.write_status.is_ok()) {
                return {self.write_status, 0};
            }
            const auto accepted = self.partial_write && !text.empty()
                                      ? text.size() - 1U
                                      : text.size();
            if (self.emit_to_uart) {
                CmsdkUart::write(text);
            }
            self.bytes_accepted += accepted;
            return {Status{}, accepted};
        }

        static Status flush(void* context) noexcept {
            auto& self = *static_cast<QemuTextSink*>(context);
            ++self.flush_count;
            return self.flush_status;
        }

        [[nodiscard]] TextSink endpoint() noexcept {
            return TextSink{this, &write, &flush};
        }
    };

    struct QemuClock {
        Status status{};
        std::uint64_t value_ms{424242};
        std::size_t read_count{0};

        static ClockSample now(void* context) noexcept {
            auto& self = *static_cast<QemuClock*>(context);
            ++self.read_count;
            return {self.status, self.value_ms};
        }

        [[nodiscard]] Clock endpoint() noexcept {
            return Clock{this, &now};
        }
    };

    struct QemuBlockDevice {
        static constexpr std::size_t block_size_value = 512;
        static constexpr std::size_t block_count_value = 4;

        Status read_status{};
        Status write_status{};
        Status flush_status{};
        std::uint64_t reported_block_size{block_size_value};
        std::uint64_t reported_block_count{block_count_value};
        bool corrupt_read{false};
        std::array<std::byte, block_size_value * block_count_value> storage{};
        std::size_t read_count{0};
        std::size_t write_count{0};
        std::size_t flush_count{0};

        static Status read(void* context,
                           std::uint64_t lba,
                           std::span<std::byte> out) noexcept {
            auto& self = *static_cast<QemuBlockDevice*>(context);
            ++self.read_count;
            if (!self.read_status.is_ok()) {
                return self.read_status;
            }
            if (lba >= self.reported_block_count ||
                out.size() != self.reported_block_size || out.size() > block_size_value) {
                return {StatusCode::out_of_range};
            }
            const auto offset = static_cast<std::size_t>(lba) * block_size_value;
            for (std::size_t index = 0; index < out.size(); ++index) {
                out[index] = self.storage[offset + index];
            }
            if (self.corrupt_read && !out.empty()) {
                out[0] ^= std::byte{0xff};
            }
            return {};
        }

        static Status write(void* context,
                            std::uint64_t lba,
                             std::span<const std::byte> in) noexcept {
            auto& self = *static_cast<QemuBlockDevice*>(context);
            ++self.write_count;
            if (!self.write_status.is_ok()) {
                return self.write_status;
            }
            if (lba >= self.reported_block_count ||
                in.size() != self.reported_block_size || in.size() > block_size_value) {
                return {StatusCode::out_of_range};
            }
            const auto offset = static_cast<std::size_t>(lba) * block_size_value;
            for (std::size_t index = 0; index < in.size(); ++index) {
                self.storage[offset + index] = in[index];
            }
            return {};
        }

        static Status flush(void* context) noexcept {
            auto& self = *static_cast<QemuBlockDevice*>(context);
            ++self.flush_count;
            return self.flush_status;
        }

        static std::uint64_t block_size(void* context) noexcept {
            return static_cast<QemuBlockDevice*>(context)->reported_block_size;
        }

        static std::uint64_t block_count(void* context) noexcept {
            return static_cast<QemuBlockDevice*>(context)->reported_block_count;
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

        [[nodiscard]] ResolvedContext context() const noexcept {
            return ResolvedContext{&text, &clock, &block};
        }
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

    bool record_evidence_matches(const app::RunResult& result,
                                 const bool stored,
                                 const bool verified,
                                 const bool reported) noexcept {
        return result.evidence.timestamp_ms == 424242U &&
               result.evidence.record_checksum == 0x49b880f0U &&
               result.evidence.record_bytes == app::record_size &&
               result.evidence.stored == stored && result.evidence.verified == verified &&
               result.evidence.reported == reported;
    }

    void record_app_case(const std::string_view label,
                         const bool condition,
                         std::size_t& cases,
                         std::size_t& failures) noexcept {
        ++cases;
        if (!condition) {
            ++failures;
            write_failure(label);
        }
    }

    bool app_failure_matrix() noexcept {
        std::size_t cases = 0;
        std::size_t failures = 0;

        {
            const auto run = app::run({});
            record_app_case(
                "app_invalid_context",
                run.code == app::RunCode::invalid_context &&
                    run.evidence.timestamp_ms == 0U && run.evidence.record_checksum == 0U &&
                    run.evidence.record_bytes == 0U && !run.evidence.stored &&
                    !run.evidence.verified && !run.evidence.reported,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.clock_provider.status = {StatusCode::io_error};
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_clock_failed",
                run.code == app::RunCode::clock_failed &&
                    fixture.clock_provider.read_count == 1U &&
                    fixture.block_provider.write_count == 0U &&
                    fixture.block_provider.flush_count == 0U &&
                    fixture.block_provider.read_count == 0U &&
                    fixture.text_provider.write_count == 0U,
                cases,
                failures);
        }

        constexpr std::array geometries{
            std::array<std::uint64_t, 2>{app::record_size - 1U, 4U},
            std::array<std::uint64_t, 2>{app::max_block_size + 1U, 4U},
            std::array<std::uint64_t, 2>{app::record_size, 0U},
        };
        constexpr std::array<std::string_view, 3> geometry_labels{
            "app_geometry_small",
            "app_geometry_large",
            "app_geometry_empty",
        };
        for (std::size_t index = 0; index < geometries.size(); ++index) {
            QemuFixture fixture{};
            fixture.block_provider.reported_block_size = geometries[index][0];
            fixture.block_provider.reported_block_count = geometries[index][1];
            const auto run = app::run(fixture.context());
            record_app_case(
                geometry_labels[index],
                run.code == app::RunCode::unsupported_geometry &&
                    run.evidence.timestamp_ms == 424242U &&
                    run.evidence.record_bytes == 0U &&
                    fixture.block_provider.write_count == 0U &&
                    fixture.text_provider.write_count == 0U,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.block_provider.write_status = {StatusCode::io_error};
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_write_failed",
                run.code == app::RunCode::write_failed &&
                    record_evidence_matches(run, false, false, false) &&
                    fixture.block_provider.write_count == 1U &&
                    fixture.block_provider.flush_count == 0U &&
                    fixture.block_provider.read_count == 0U &&
                    fixture.text_provider.write_count == 0U,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.block_provider.flush_status = {StatusCode::io_error};
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_flush_failed",
                run.code == app::RunCode::flush_failed &&
                    record_evidence_matches(run, true, false, false) &&
                    fixture.block_provider.flush_count == 1U &&
                    fixture.block_provider.read_count == 0U &&
                    fixture.text_provider.write_count == 0U,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.block_provider.read_status = {StatusCode::io_error};
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_read_failed",
                run.code == app::RunCode::read_failed &&
                    record_evidence_matches(run, true, false, false) &&
                    fixture.block_provider.read_count == 1U &&
                    fixture.text_provider.write_count == 0U,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.block_provider.corrupt_read = true;
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_verify_failed",
                run.code == app::RunCode::verify_failed &&
                    record_evidence_matches(run, true, false, false) &&
                    fixture.block_provider.read_count == 1U &&
                    fixture.text_provider.write_count == 0U,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.text_provider.emit_to_uart = false;
            fixture.text_provider.write_status = {StatusCode::io_error};
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_report_write_failed",
                run.code == app::RunCode::report_failed &&
                    record_evidence_matches(run, true, true, false) &&
                    fixture.text_provider.write_count == 1U &&
                    fixture.text_provider.flush_count == 0U,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.text_provider.emit_to_uart = false;
            fixture.text_provider.partial_write = true;
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_report_partial",
                run.code == app::RunCode::report_failed &&
                    record_evidence_matches(run, true, true, false) &&
                    fixture.text_provider.write_count == 1U &&
                    fixture.text_provider.flush_count == 0U,
                cases,
                failures);
        }
        {
            QemuFixture fixture{};
            fixture.text_provider.emit_to_uart = false;
            fixture.text_provider.flush_status = {StatusCode::io_error};
            const auto run = app::run(fixture.context());
            record_app_case(
                "app_report_flush_failed",
                run.code == app::RunCode::report_failed &&
                    record_evidence_matches(run, true, true, false) &&
                    fixture.text_provider.write_count == 1U &&
                    fixture.text_provider.flush_count == 1U,
                cases,
                failures);
        }

        CmsdkUart::write("[charm-capability-mvp-qemu] app_failure_cases=");
        CmsdkUart::write_decimal(cases);
        CmsdkUart::write(" failures=");
        CmsdkUart::write_decimal(failures);
        CmsdkUart::write("\n");
        return cases == 12U && failures == 0U;
    }

    bool expect_prestart_failure(const std::string_view label,
                                 const ResolutionResult& resolved,
                                 const ResolutionFailure expected,
                                 const std::size_t expected_index,
                                 const bool emit_success) noexcept {
        std::size_t app_start_count = 0;
        if (resolved.is_ok()) {
            ++app_start_count;
            (void)app::run(resolved.context);
        }
        if (resolved.failure != expected ||
            resolved.requirement_index != expected_index ||
            resolved.context.valid() || app_start_count != 0U) {
            write_failure(label);
            return false;
        }

        if (emit_success) {
            CmsdkUart::write("[charm-capability-mvp-qemu] ");
            CmsdkUart::write(label);
            CmsdkUart::write("=");
            CmsdkUart::write(resolution_failure_name(resolved.failure));
            CmsdkUart::write(" start_count=");
            CmsdkUart::write_decimal(app_start_count);
            CmsdkUart::write("\n");
        }
        return true;
    }

    bool resolution_matrix() noexcept {
        std::size_t cases = 0;
        std::size_t failures = 0;
        constexpr std::array permutations{
            std::array<std::size_t, 3>{0, 1, 2},
            std::array<std::size_t, 3>{0, 2, 1},
            std::array<std::size_t, 3>{1, 0, 2},
            std::array<std::size_t, 3>{1, 2, 0},
            std::array<std::size_t, 3>{2, 0, 1},
            std::array<std::size_t, 3>{2, 1, 0},
        };

        for (const auto& order : permutations) {
            QemuFixture fixture{};
            std::array<Binding, 3> bindings{};
            for (std::size_t index = 0; index < bindings.size(); ++index) {
                bindings[index] = fixture.bindings[order[index]];
            }
            const auto resolved = resolve(
                app::requirements, ProfileView{fixture.provisions, bindings});
            ++cases;
            if (!resolved.is_ok() || resolved.context.report != &fixture.text ||
                resolved.context.monotonic_time != &fixture.clock ||
                resolved.context.record_store != &fixture.block) {
                ++failures;
                write_failure("resolution_order");
            }
        }

        for (std::size_t failed_index = 0;
             failed_index < app::requirements.size();
             ++failed_index) {
            QemuFixture fixture{};
            std::array<Binding, 2> bindings{};
            std::size_t output_index = 0;
            for (std::size_t index = 0; index < fixture.bindings.size(); ++index) {
                if (index != failed_index) {
                    bindings[output_index++] = fixture.bindings[index];
                }
            }
            ++cases;
            if (!expect_prestart_failure(
                    "missing",
                    resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                    ResolutionFailure::missing_binding,
                    failed_index,
                    failed_index == 2U)) {
                ++failures;
            }
        }

        for (std::size_t failed_index = 0;
             failed_index < app::requirements.size();
             ++failed_index) {
            QemuFixture fixture{};
            const std::array bindings{
                fixture.bindings[0],
                fixture.bindings[1],
                fixture.bindings[2],
                fixture.bindings[failed_index],
            };
            ++cases;
            if (!expect_prestart_failure(
                    "duplicate",
                    resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                    ResolutionFailure::duplicate_binding,
                    failed_index,
                    failed_index == 1U)) {
                ++failures;
            }
        }

        for (std::size_t failed_index = 0;
             failed_index < app::requirements.size();
             ++failed_index) {
            QemuFixture fixture{};
            auto bindings = fixture.bindings;
            bindings[failed_index].provision_index = fixture.provisions.size();
            ++cases;
            if (!expect_prestart_failure(
                    "invalid_index",
                    resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                    ResolutionFailure::invalid_provision_index,
                    failed_index,
                    failed_index == 2U)) {
                ++failures;
            }
        }

        for (std::size_t failed_index = 0;
             failed_index < app::requirements.size();
             ++failed_index) {
            QemuFixture fixture{};
            auto bindings = fixture.bindings;
            bindings[failed_index].provision_index =
                (failed_index + 1U) % fixture.provisions.size();
            ++cases;
            if (!expect_prestart_failure(
                    "mismatch",
                    resolve(app::requirements, ProfileView{fixture.provisions, bindings}),
                    ResolutionFailure::contract_mismatch,
                    failed_index,
                    failed_index == 2U)) {
                ++failures;
            }
        }

        for (std::size_t failed_index = 0;
             failed_index < app::requirements.size();
             ++failed_index) {
            QemuFixture fixture{};
            auto provisions = fixture.provisions;
            switch (failed_index) {
            case 0:
                provisions[0] = Provision{ContractId::text_sink, nullptr, nullptr, nullptr};
                break;
            case 1:
                provisions[1] = Provision{ContractId::clock, nullptr, nullptr, nullptr};
                break;
            default:
                provisions[2] =
                    Provision{ContractId::block_device, nullptr, nullptr, nullptr};
                break;
            }
            ++cases;
            if (!expect_prestart_failure(
                    "invalid",
                    resolve(app::requirements, ProfileView{provisions, fixture.bindings}),
                    ResolutionFailure::invalid_provision,
                    failed_index,
                    failed_index == 1U)) {
                ++failures;
            }
        }

        CmsdkUart::write("[charm-capability-mvp-qemu] resolution_cases=");
        CmsdkUart::write_decimal(cases);
        CmsdkUart::write(" failures=");
        CmsdkUart::write_decimal(failures);
        CmsdkUart::write("\n");
        return cases == 21U && failures == 0U;
    }
}

extern "C" int charm_capability_mvp_qemu_main() noexcept {
    const bool positive_ok = positive_case();
    const bool resolution_ok = resolution_matrix();
    const bool app_failures_ok = app_failure_matrix();
    if (positive_ok && resolution_ok && app_failures_ok) {
        CmsdkUart::write("[charm-capability-mvp-qemu] ok\n");
        return 0;
    }
    CmsdkUart::write("[charm-capability-mvp-qemu] failed\n");
    return 1;
}
