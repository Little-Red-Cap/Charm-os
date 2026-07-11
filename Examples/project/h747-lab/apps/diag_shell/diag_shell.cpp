#include "diag_shell.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "console_service.hpp"
#include "memory_service.hpp"
#include "port.h"
#include "power_service.hpp"
#include "storage.h"

import out.core;
import out.format;

namespace h747::apps::diag_shell {
namespace {

using namespace std::literals::string_view_literals;

template <charm::cap::ByteSink Sink>
class OutSinkAdapter {
public:
    explicit OutSinkAdapter(Sink& sink) : sink_(&sink) {}

    out::result<std::size_t> write(const out::bytes bytes) noexcept {
        if (sink_ == nullptr) {
            return out::ok<std::size_t>(0U);
        }
        const auto transfer = sink_->write(bytes);
        return out::ok(static_cast<std::size_t>(transfer.bytes));
    }

    out::result<std::size_t> flush() noexcept {
        if (sink_ != nullptr) {
            (void)sink_->flush();
        }
        return out::ok<std::size_t>(0U);
    }

private:
    Sink* sink_{nullptr};
};

h747::console::ConsoleStream& console_stream() noexcept {
    static h747::console::ConsoleStream stream{};
    return stream;
}

OutSinkAdapter<h747::console::ConsoleStream>& out_sink() noexcept {
    static OutSinkAdapter adapter{console_stream()};
    return adapter;
}

template <out::fixed_string Fmt, class... Args>
void emit(Args&&... args) noexcept {
    out::discard(out::vprint<Fmt>(out_sink(), std::forward<Args>(args)...));
}

console::ConsoleLineSource line_source;
power::Pmic pmic;
memory::StorageProbe storage;
std::uint32_t last_tick = 0U;

constexpr std::string_view trim_left(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ')) {
        sv.remove_prefix(1);
    }
    return sv;
}

constexpr std::pair<std::string_view, std::string_view> split_token(std::string_view sv) noexcept {
    sv = trim_left(sv);
    const auto pos = sv.find(' ');
    if (pos == std::string_view::npos) {
        return {sv, {}};
    }
    return {sv.substr(0, pos), trim_left(sv.substr(pos + 1))};
}

std::optional<std::uint32_t> parse_u32(std::string_view sv) noexcept {
    sv = trim_left(sv);
    if (sv.empty()) {
        return std::nullopt;
    }

    std::uint32_t value = 0U;
    int base = 10;
    if (sv.starts_with("0x"sv) || sv.starts_with("0X"sv)) {
        base = 16;
        sv.remove_prefix(2);
        if (sv.empty()) {
            return std::nullopt;
        }
    }

    bool parsed_any = false;
    for (const char c : sv) {
        std::uint32_t digit = 0U;
        if ((c >= '0') && (c <= '9')) {
            digit = static_cast<std::uint32_t>(c - '0');
        } else if ((base == 16) && (c >= 'a') && (c <= 'f')) {
            digit = static_cast<std::uint32_t>(c - 'a' + 10);
        } else if ((base == 16) && (c >= 'A') && (c <= 'F')) {
            digit = static_cast<std::uint32_t>(c - 'A' + 10);
        } else {
            return std::nullopt;
        }
        if (digit >= static_cast<std::uint32_t>(base)) {
            return std::nullopt;
        }
        parsed_any = true;
        value = (base == 16) ? ((value << 4U) | digit) : ((value * 10U) + digit);
    }
    return parsed_any ? std::optional<std::uint32_t>{value} : std::nullopt;
}

std::uint32_t parse_u32_or(std::string_view sv, const std::uint32_t fallback) noexcept {
    if (sv.empty()) {
        return fallback;
    }
    const auto parsed = parse_u32(sv);
    return parsed.value_or(fallback);
}

std::array<std::uint8_t, 16U * 512U>& storage_bench_buffer() noexcept {
    alignas(4) static std::array<std::uint8_t, 16U * 512U> buffer{};
    return buffer;
}

void fill_storage_bench_pattern(std::span<std::uint8_t> buffer,
                                const std::uint32_t width,
                                const std::uint32_t clock_div,
                                const std::uint32_t lba) noexcept {
    for (std::uint32_t index = 0U; index < buffer.size(); ++index) {
        buffer[index] = static_cast<std::uint8_t>(
            0x5AU ^ (index * 29U) ^ (width * 13U) ^ (clock_div * 7U) ^ (lba >> (index & 7U)));
    }
}

std::uint32_t sample_checksum(std::span<const std::uint8_t> buffer) noexcept {
    std::uint32_t checksum = 0x811C9DC5U;
    const std::uint32_t step = buffer.size() > 128U ? static_cast<std::uint32_t>(buffer.size() / 128U) : 1U;
    for (std::uint32_t index = 0U; index < buffer.size(); index += step) {
        checksum = (checksum ^ buffer[index]) * 16777619U;
    }
    return checksum;
}

void print_hex_bytes(const std::span<const std::uint8_t> data) noexcept {
    for (std::size_t index = 0; index < data.size(); ++index) {
        if (index != 0U) {
            emit<" ">();
        }
        emit<"{:02X}">(static_cast<unsigned>(data[index]));
    }
}

void prompt() {
    emit<"\r\nh747-lab> ">();
}

void print_help() {
    emit<"Commands:\n">();
    emit<"  help                  - Show help\n">();
    emit<"  status                - Print board/power/memory summary\n">();
    emit<"  power status          - Print PMIC transport and rail snapshot\n">();
    emit<"  pmic probe            - Refresh TPS65217 snapshot\n">();
    emit<"  pmic enable <rail> <0|1>\n">();
    emit<"  pmic set <rail> <mv>\n">();
    emit<"  memory status         - Print SDRAM/QSPI evidence\n">();
    emit<"  memory mpu normal     - Configure SDRAM MPU as normal non-cacheable\n">();
    emit<"  sdram1 probe          - Run SDRAM1 gated smoke\n">();
    emit<"  sdram1 probe force    - Run SDRAM1 smoke ignoring PMIC gate\n">();
    emit<"  sdram1 verify         - Run SDRAM1 segmented verify\n">();
    emit<"  sdram1 bus            - Run SDRAM1 data-bus pattern diag\n">();
    emit<"  sdram1 waitbus        - Run SDRAM1 bus diag with command busy waits\n">();
    emit<"  sdram1 spot           - Run SDRAM1 single-address spot diag\n">();
    emit<"  sdram1 alias          - Run SDRAM1 neighbor/alias read diag\n">();
    emit<"  sdram1 addr           - Run SDRAM1 address-line sentinel diag\n">();
    emit<"  sdram1 lane           - Run SDRAM1 byte-lane/NBL diag\n">();
    emit<"  sdram1 repeat         - Run SDRAM1 repeated-read diag\n">();
    emit<"  sdram1 locate         - Locate where a one-word write lands\n">();
    emit<"  sdram1 timing         - Sweep SDRAM1 CAS/pipe timing presets\n">();
    emit<"  sdram2 probe          - Run SDRAM2 gated smoke\n">();
    emit<"  sdram2 probe force    - Run SDRAM2 smoke ignoring PMIC gate\n">();
    emit<"  sdram2 verify         - Run SDRAM2 segmented verify\n">();
    emit<"  sdram2 bus            - Run SDRAM2 data-bus pattern diag\n">();
    emit<"  sdram2 waitbus        - Run SDRAM2 bus diag with command busy waits\n">();
    emit<"  sdram2 spot           - Run SDRAM2 single-address spot diag\n">();
    emit<"  sdram2 alias          - Run SDRAM2 neighbor/alias read diag\n">();
    emit<"  sdram2 addr           - Run SDRAM2 address-line sentinel diag\n">();
    emit<"  sdram2 lane           - Run SDRAM2 byte-lane/NBL diag\n">();
    emit<"  sdram2 repeat         - Run SDRAM2 repeated-read diag\n">();
    emit<"  sdram2 locate         - Locate where a one-word write lands\n">();
    emit<"  sdram2 timing         - Sweep SDRAM2 CAS/pipe timing presets\n">();
    emit<"  storage init          - Initialize eMMC storage service\n">();
    emit<"  storage status        - Print eMMC storage state\n">();
    emit<"  storage width <1|4|8> [lba] [count] [clock_div]\n">();
    emit<"  storage write <1|4|8> [lba] [count] [clock_div]\n">();
    emit<"  storage bench <read|write> <1|4|8> [lba] [blocks] [clock_div]\n">();
    emit<"  qspi probe            - Run QSPI JEDEC/status/read probe\n">();
    emit<"  qspi probe force      - Run QSPI probe ignoring PMIC gate\n">();
    emit<"  qspi read <addr> [len]\n">();
    emit<"  reboot                - Reboot\n">();
}

void print_power_status() {
    const auto snapshot = pmic.snapshot();
    const auto raw = snapshot.raw;
    emit<"power: profile={} bus={} ready={} ready_status={} transport={} ack={} rd={} wr={} irq={} chip=0x{:02X} status=0x{:02X} pgood=0x{:02X} en=0x{:02X}\n">(
        pmic.current_profile_name(),
        raw.bus_prepared,
        raw.ready,
        raw.ready_status,
        snapshot.transport_text(),
        raw.last_ack,
        raw.last_read_ok,
        raw.last_write_ok,
        raw.irq_pin,
        static_cast<unsigned>(raw.chipid_reg),
        static_cast<unsigned>(raw.status_reg),
        static_cast<unsigned>(raw.pgood_reg),
        static_cast<unsigned>(raw.enable_reg));
    emit<"rails: dcdc1={}/{} dcdc2={}/{} dcdc3={}/{} ldo4={}/{} wled={}/{} duty={}\n">(
        raw.dcdc1_enabled,
        raw.dcdc1_mv,
        raw.dcdc2_enabled,
        raw.dcdc2_mv,
        raw.dcdc3_enabled,
        raw.dcdc3_mv,
        raw.ldo4_enabled,
        raw.ldo4_mv,
        raw.wled_enabled,
        raw.wled_fdim_hz,
        raw.wled_duty_percent);
}

void print_sdram_status(const memory::SdramEvidence evidence) {
    emit<"{}: profile={} gate={} tried={} init={} smoke={} verify={} ready={} base=0x{:08X} size=0x{:08X} words={} vwords={} hal={} first=0x{:08X} expected=0x{:08X} actual=0x{:08X} vfirst=0x{:08X} vexpected=0x{:08X} vactual=0x{:08X}\n">(
        evidence.name(),
        evidence.profile_name(),
        evidence.power_good,
        evidence.attempted,
        evidence.init_ok,
        evidence.smoke_ok,
        evidence.verify_ok,
        evidence.ready,
        evidence.base,
        evidence.size_bytes,
        evidence.tested_words,
        evidence.verify_tested_words,
        evidence.last_hal_status,
        static_cast<std::uint32_t>(evidence.first_error_addr),
        evidence.first_expected,
        evidence.first_actual,
        static_cast<std::uint32_t>(evidence.verify_first_error_addr),
        evidence.verify_first_expected,
        evidence.verify_first_actual);
}

void print_memory_status() {
    const auto snapshot = storage.snapshot();
    const auto m = snapshot.raw;
    emit<"memory: profile_applied={} pmic={} ldo4_mv={} qspi_dcdc1_mv={} fmc_clk={} ccr=0x{:08X} shcsr=0x{:08X} sdsr=0x{:08X} sdcr1=0x{:08X} sdtr1=0x{:08X} sdcr2=0x{:08X} sdtr2=0x{:08X}\n">(
        m.storage_profile_applied,
        m.pmic_ready,
        m.ldo4_mv,
        m.qspi_dcdc1_mv,
        m.fmc_clock_hz,
        m.scb_ccr,
        m.scb_shcsr,
        m.fmc_sdsr,
        m.fmc_sdcr1,
        m.fmc_sdtr1,
        m.fmc_sdcr2,
        m.fmc_sdtr2);
    print_sdram_status(snapshot.sdram(memory::SdramBank::bank1));
    print_sdram_status(snapshot.sdram(memory::SdramBank::bank2));
    const auto qspi = snapshot.qspi();
    emit<"qspi1: gate={} tried={} init={} jedec={} status={} read={} hal={} err=0x{:08X} id={:02X}/{:02X}/{:02X} sr1=0x{:02X} sr2=0x{:02X} qcr=0x{:08X} qdcr=0x{:08X} qsr=0x{:08X}\n">(
        qspi.power_good,
        qspi.attempted,
        qspi.init_ok,
        qspi.jedec_ok,
        qspi.status_ok,
        qspi.read_ok,
        qspi.last_hal_status,
        qspi.last_error,
        static_cast<unsigned>(qspi.jedec_id[0]),
        static_cast<unsigned>(qspi.jedec_id[1]),
        static_cast<unsigned>(qspi.jedec_id[2]),
        static_cast<unsigned>(qspi.status1),
        static_cast<unsigned>(qspi.status2),
        qspi.cr,
        qspi.dcr,
        qspi.sr);
    if (qspi.last_read_len != 0U) {
        emit<"qspi1: last_cmd=0x{:02X} addr=0x{:08X} data=">(
            static_cast<unsigned>(qspi.last_cmd),
            qspi.last_addr);
        print_hex_bytes(qspi.last_read());
        emit<"\n">();
    }
}

void print_status() {
    emit<"status: board=h747_diy profile=diag_shell tick={}\n">(h747::port::tick_ms());
    print_power_status();
    print_memory_status();
}

void run_memory_mpu_normal() {
    const bool ok = storage.configure_sdram_mpu_normal();
    emit<"memory: mpu normal {}\n">(ok ? "ok" : "failed");
    print_memory_status();
}

void run_sdram_probe(const memory::SdramBank bank, const bool force) {
    const bool ok = storage.smoke(bank, force);
    emit<"{}: probe {}{}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        force ? " force" : "");
    print_memory_status();
}

void run_sdram_verify(const memory::SdramBank bank) {
    const bool ok = storage.verify(bank);
    emit<"{}: verify {}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed");
    print_memory_status();
}

void run_sdram_bus_diag(const memory::SdramBank bank) {
    memory_probe_sdram_bus_diag_t diag{};
    const bool ok = storage.bus_diag(bank, diag);
    emit<"{}: bus {} init={} base=0x{:08X} samples={} mismatch_or=0x{:08X} mismatch_and=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        diag.mismatch_or,
        diag.mismatch_and,
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: bus[{}] expected=0x{:08X} actual=0x{:08X} xor=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.expected,
            sample.actual,
            sample.expected ^ sample.actual);
    }
}

void run_sdram_wait_sequence_bus_diag(const memory::SdramBank bank) {
    memory_probe_sdram_bus_diag_t diag{};
    const bool ok = storage.wait_sequence_bus_diag(bank, diag);
    emit<"{}: waitbus {} init={} base=0x{:08X} samples={} mismatch_or=0x{:08X} mismatch_and=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        diag.mismatch_or,
        diag.mismatch_and,
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: waitbus[{}] expected=0x{:08X} actual=0x{:08X} xor=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.expected,
            sample.actual,
            sample.expected ^ sample.actual);
    }
}

void run_sdram_spot_diag(const memory::SdramBank bank) {
    memory_probe_sdram_spot_diag_t diag{};
    const bool ok = storage.spot_diag(bank, diag);
    emit<"{}: spot {} init={} base=0x{:08X} samples={} mismatch_or=0x{:08X} mismatch_and=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        diag.mismatch_or,
        diag.mismatch_and,
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: spot[{}] off=0x{:08X} expected=0x{:08X} immediate=0x{:08X} synced=0x{:08X} xor=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.offset,
            sample.expected,
            sample.immediate_actual,
            sample.after_sync_actual,
            sample.expected ^ sample.after_sync_actual);
    }
}

void run_sdram_alias_diag(const memory::SdramBank bank) {
    memory_probe_sdram_alias_diag_t diag{};
    const bool ok = storage.alias_diag(bank, diag);
    emit<"{}: alias {} init={} base=0x{:08X} samples={} mismatch_or=0x{:08X} mismatch_and=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        diag.mismatch_or,
        diag.mismatch_and,
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: alias[{}] off=0x{:08X} expected=0x{:08X} m2=0x{:08X} m1=0x{:08X} self=0x{:08X} p1=0x{:08X} p2=0x{:08X} xor=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.offset,
            sample.expected,
            sample.actual_minus_2,
            sample.actual_minus_1,
            sample.actual_self,
            sample.actual_plus_1,
            sample.actual_plus_2,
            sample.expected ^ sample.actual_self);
    }
}

void run_sdram_addr_diag(const memory::SdramBank bank) {
    memory_probe_sdram_addr_diag_t diag{};
    const bool ok = storage.addr_diag(bank, diag);
    emit<"{}: addr {} init={} base=0x{:08X} samples={} mismatch_or=0x{:08X} mismatch_and=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        diag.mismatch_or,
        diag.mismatch_and,
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: addr[{}] off=0x{:08X} expected=0x{:08X} actual=0x{:08X} source=0x{:08X} xor=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.offset,
            sample.expected,
            sample.actual,
            sample.source_offset,
            sample.expected ^ sample.actual);
    }
}

void run_sdram_lane_diag(const memory::SdramBank bank) {
    memory_probe_sdram_lane_diag_t diag{};
    const bool ok = storage.lane_diag(bank, diag);
    emit<"{}: lane {} init={} base=0x{:08X} samples={} mismatch_or=0x{:08X} mismatch_and=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        diag.mismatch_or,
        diag.mismatch_and,
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: lane[{}] access={} off=0x{:08X} write=0x{:08X} expected=0x{:08X} actual=0x{:08X} xor=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.access_bits,
            sample.offset,
            sample.write_value,
            sample.expected_word,
            sample.actual_word,
            sample.expected_word ^ sample.actual_word);
    }
}

void run_sdram_repeat_diag(const memory::SdramBank bank) {
    memory_probe_sdram_repeat_diag_t diag{};
    const bool ok = storage.repeat_diag(bank, diag);
    emit<"{}: repeat {} init={} base=0x{:08X} samples={} reads={} mismatch_or=0x{:08X} mismatch_and=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        static_cast<unsigned>(diag.read_count),
        diag.mismatch_or,
        diag.mismatch_and,
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: repeat[{}] off=0x{:08X} expected=0x{:08X} reads=">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.offset,
            sample.expected);
        for (std::uint32_t read = 0; read < diag.read_count; ++read) {
            if (read != 0U) {
                emit<",">();
            }
            emit<"0x{:08X}">(sample.reads[read]);
        }
        emit<"\n">();
    }
}

void run_sdram_locate_diag(const memory::SdramBank bank) {
    memory_probe_sdram_locate_diag_t diag{};
    const bool ok = storage.locate_diag(bank, diag);
    emit<"{}: locate {} init={} base=0x{:08X} samples={} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.init_ok,
        diag.base,
        static_cast<unsigned>(diag.sample_count),
        diag.fmc_sdsr,
        diag.fmc_sdcr,
        diag.fmc_sdtr);
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: locate[{}] write=0x{:08X} expected=0x{:08X} hit=0x{:08X} hits={} m2=0x{:08X} m1=0x{:08X} self=0x{:08X} p1=0x{:08X} p2=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            index,
            sample.write_offset,
            sample.expected,
            sample.hit_offset,
            sample.hit_count,
            sample.actual_minus_2,
            sample.actual_minus_1,
            sample.actual_self,
            sample.actual_plus_1,
            sample.actual_plus_2);
    }
}

void run_sdram_timing_sweep(const memory::SdramBank bank) {
    memory_probe_sdram_timing_diag_t diag{};
    const bool ok = storage.timing_sweep(bank, diag);
    emit<"{}: timing {} base=0x{:08X} samples={}\n">(
        (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
        ok ? "ok" : "failed",
        diag.base,
        static_cast<unsigned>(diag.sample_count));
    for (std::uint32_t index = 0; index < diag.sample_count; ++index) {
        const auto& sample = diag.samples[index];
        emit<"{}: timing[{}] ok={} init={} smoke={} sdclk=0x{:08X} cas=0x{:08X} burst=0x{:08X} pipe=0x{:08X} mode=0x{:08X} first=0x{:08X} expected=0x{:08X} actual=0x{:08X} sdsr=0x{:08X} sdcr=0x{:08X} sdtr=0x{:08X}\n">(
            (bank == memory::SdramBank::bank2) ? "sdram2"sv : "sdram1"sv,
            static_cast<unsigned>(sample.preset),
            sample.ok,
            sample.init_ok,
            sample.smoke_ok,
            sample.sdclock_period,
            sample.cas_latency,
            sample.read_burst,
            sample.read_pipe_delay,
            sample.mode_reg,
            sample.first_error_addr,
            sample.expected,
            sample.actual,
            sample.fmc_sdsr,
            sample.fmc_sdcr,
            sample.fmc_sdtr);
    }
    print_memory_status();
}

void print_storage_status() {
    const auto s = h747_storage_state();
    emit<"storage: attempted={} init={} ready={} block_device={} fat={} part_auto={} init_status={} hal={} err=0x{:08X} card={} block_size={} blocks={} part_lba={} exposed={} rd={} rdfail={} wr={} wrfail={} wait_timeout={} last_lba={} last_count={} clkcr=0x{:08X} sta=0x{:08X} resp1=0x{:08X} bus={} wide8={} wide4={} wide1={}\n">(
        s.attempted,
        s.initialized,
        s.ready,
        s.block_device_ready,
        s.fat_probe_ok,
        s.partition_auto,
        s.init_status,
        s.last_hal_status,
        s.last_error,
        s.card_state,
        s.block_size,
        s.block_count,
        s.partition_lba,
        s.exposed_block_count,
        s.read_count,
        s.read_fail_count,
        s.write_count,
        s.write_fail_count,
        s.wait_timeout_count,
        s.last_lba,
        s.last_count,
        s.clkcr,
        s.sta,
        s.resp1,
        s.selected_bus_width,
        s.wide_status_8,
        s.wide_status_4,
        s.wide_status_1);
}

void run_storage_init() {
    h747_storage_init();
    print_storage_status();
}

void run_storage_width(const std::string_view args) {
    const auto [width_token, after_width] = split_token(args);
    const auto [lba_token, count_token] = split_token(after_width);
    const auto [count_value_token, clock_div_token] = split_token(count_token);
    const auto width = parse_u32(width_token);
    if (!width.has_value() || !(*width == 1U || *width == 4U || *width == 8U)) {
        emit<"storage: usage storage width <1|4|8> [lba] [count] [clock_div]\n">();
        return;
    }

    std::uint32_t lba = 0U;
    std::uint32_t count = 1U;
    std::uint32_t clock_div = 16U;
    if (!lba_token.empty()) {
        const auto parsed = parse_u32(lba_token);
        if (!parsed.has_value()) {
            emit<"storage: invalid lba\n">();
            return;
        }
        lba = *parsed;
    }
    if (!count_value_token.empty()) {
        const auto parsed = parse_u32(count_value_token);
        if (!parsed.has_value()) {
            emit<"storage: invalid count\n">();
            return;
        }
        count = *parsed;
    }
    if (!clock_div_token.empty()) {
        const auto parsed = parse_u32(clock_div_token);
        if (!parsed.has_value()) {
            emit<"storage: invalid clock_div\n">();
            return;
        }
        clock_div = *parsed;
    }

    h747_storage_bus_probe_t probe{};
    const bool ok = h747_storage_probe_bus_width(*width, lba, count, clock_div, &probe) != 0U;
    emit<"storage: width request={} selected={} ok={} init={} read={} lba={} count={} bytes={} clock_div={} init_status={} wide_status={} read_status={} err=0x{:08X} card={} clkcr=0x{:08X} sta=0x{:08X} resp1=0x{:08X} crc=0x{:08X} sample=">(
        probe.requested_bus_width,
        probe.selected_bus_width,
        ok ? 1U : 0U,
        probe.initialized,
        probe.read_ok,
        probe.lba,
        probe.block_count,
        probe.bytes,
        probe.clock_div,
        probe.init_status,
        probe.wide_status,
        probe.read_status,
        probe.last_error,
        probe.card_state,
        probe.clkcr,
        probe.sta,
        probe.resp1,
        probe.crc32);
    print_hex_bytes(std::span<const std::uint8_t>{probe.sample, probe.sample_len});
    emit<"\n">();
    print_storage_status();
}

void run_storage_write(const std::string_view args) {
    constexpr std::uint32_t kDefaultScratchLba = 4194304U; // 2 GiB / 512 B
    const auto [width_token, after_width] = split_token(args);
    const auto [lba_token, after_lba] = split_token(after_width);
    const auto [count_token, clock_div_token] = split_token(after_lba);
    const auto width = parse_u32(width_token);
    if (!width.has_value() || !(*width == 1U || *width == 4U || *width == 8U)) {
        emit<"storage: usage storage write <1|4|8> [lba] [count] [clock_div]\n">();
        return;
    }

    const std::uint32_t lba = parse_u32_or(lba_token, kDefaultScratchLba);
    const std::uint32_t count = parse_u32_or(count_token, 1U);
    const std::uint32_t clock_div = parse_u32_or(clock_div_token, 16U);
    if (count != 1U) {
        emit<"storage: write probe count is limited to 1 block\n">();
        return;
    }

    h747_storage_write_probe_t probe{};
    const bool ok = h747_storage_probe_bus_width_write(*width, lba, count, clock_div, &probe) != 0U;
    emit<"storage: write request={} selected={} ok={} init={} lba={} count={} bytes={} clock_div={} wide_status={} write={} write_status={} read={} read_status={} verify={} err=0x{:08X} card={} clkcr=0x{:08X} sta=0x{:08X} resp1=0x{:08X} test_crc=0x{:08X} read_crc=0x{:08X} pattern=">(
        probe.requested_bus_width,
        probe.selected_bus_width,
        ok ? 1U : 0U,
        probe.initialized,
        probe.lba,
        probe.block_count,
        probe.bytes,
        probe.clock_div,
        probe.test_wide_status,
        probe.test_write_ok,
        probe.test_write_status,
        probe.test_read_ok,
        probe.test_read_status,
        probe.verify_ok,
        probe.last_error,
        probe.card_state,
        probe.clkcr,
        probe.sta,
        probe.resp1,
        probe.test_crc32,
        probe.readback_crc32);
    print_hex_bytes(std::span<const std::uint8_t>{probe.test_sample, probe.sample_len});
    emit<" readback=">();
    print_hex_bytes(std::span<const std::uint8_t>{probe.readback_sample, probe.sample_len});
    emit<"\n">();
    print_storage_status();
}

void run_qspi_probe(const bool force) {
    const bool ok = storage.probe_qspi(force);
    emit<"qspi1: probe {}{}\n">(ok ? "ok" : "failed", force ? " force" : "");
    print_memory_status();
}

void run_qspi_read(const std::string_view args) {
    std::array<std::uint8_t, 16> data{};
    const auto [addr_token, rest] = split_token(args);
    const auto len_token = trim_left(rest);
    const auto addr = parse_u32(addr_token);
    if (!addr.has_value()) {
        emit<"qspi1: invalid address\n">();
        return;
    }

    std::uint32_t len = data.size();
    if (!len_token.empty()) {
        const auto parsed = parse_u32(len_token);
        if (!parsed.has_value()) {
            emit<"qspi1: invalid length\n">();
            return;
        }
        len = *parsed;
    }
    if (len == 0U) {
        len = data.size();
    }
    if (len > data.size()) {
        len = data.size();
    }

    const bool ok = storage.read_qspi(*addr, std::span<std::uint8_t>{data.data(), len});
    const auto qspi = storage.snapshot().qspi();
    emit<"qspi1: read addr=0x{:08X} len={} ok={} hal={} err=0x{:08X}">(
        *addr,
        len,
        ok,
        qspi.last_hal_status,
        qspi.last_error);
    if (ok) {
        emit<" data=">();
        print_hex_bytes(std::span<const std::uint8_t>{data.data(), len});
    }
    emit<"\n">();
}

void run_pmic_probe() {
    const bool ok = pmic.probe();
    storage.poll();
    emit<"pmic: probe {}\n">(ok ? "ok" : "failed");
    print_power_status();
}

void run_pmic_enable(const std::string_view args) {
    const auto [rail_token, state_token] = split_token(args);
    const auto rail = power::parse_rail(rail_token);
    const auto enabled = parse_u32(state_token);
    if (!rail.has_value() || !enabled.has_value()) {
        emit<"pmic: usage pmic enable <rail> <0|1>\n">();
        return;
    }
    const bool ok = pmic.set_enabled(*rail, *enabled != 0U);
    storage.poll();
    emit<"pmic: enable rail={} value={} {}\n">(
        power::rail_name(*rail),
        static_cast<std::uint8_t>(*enabled != 0U),
        ok ? "ok" : "failed");
    print_power_status();
}

void run_pmic_set(const std::string_view args) {
    const auto [rail_token, mv_token] = split_token(args);
    const auto rail = power::parse_rail(rail_token);
    const auto millivolts = parse_u32(mv_token);
    if (!rail.has_value() || !millivolts.has_value()) {
        emit<"pmic: usage pmic set <rail> <mv>\n">();
        return;
    }
    const bool ok = pmic.set_voltage_mv(*rail, static_cast<std::uint16_t>(*millivolts));
    storage.poll();
    emit<"pmic: set rail={} mv={} {}\n">(
        power::rail_name(*rail),
        *millivolts,
        ok ? "ok" : "failed");
    print_power_status();
}

void handle_command(const std::string_view line) {
    const std::string_view cmd = trim_left(line);
    if (cmd.empty()) {
        return;
    }
    if (cmd == "help"sv) {
        print_help();
    } else if (cmd == "status"sv) {
        print_status();
    } else if (cmd == "power status"sv) {
        print_power_status();
    } else if (cmd == "memory status"sv) {
        print_memory_status();
    } else if (cmd == "memory mpu normal"sv) {
        run_memory_mpu_normal();
    } else if (cmd == "pmic probe"sv) {
        run_pmic_probe();
    } else if (cmd.starts_with("pmic enable "sv)) {
        run_pmic_enable(cmd.substr(12));
    } else if (cmd.starts_with("pmic set "sv)) {
        run_pmic_set(cmd.substr(9));
    } else if (cmd == "sdram1 probe"sv) {
        run_sdram_probe(memory::SdramBank::bank1, false);
    } else if (cmd == "sdram1 probe force"sv) {
        run_sdram_probe(memory::SdramBank::bank1, true);
    } else if (cmd == "sdram1 verify"sv) {
        run_sdram_verify(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 bus"sv) {
        run_sdram_bus_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 waitbus"sv) {
        run_sdram_wait_sequence_bus_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 spot"sv) {
        run_sdram_spot_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 alias"sv) {
        run_sdram_alias_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 addr"sv) {
        run_sdram_addr_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 lane"sv) {
        run_sdram_lane_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 repeat"sv) {
        run_sdram_repeat_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 locate"sv) {
        run_sdram_locate_diag(memory::SdramBank::bank1);
    } else if (cmd == "sdram1 timing"sv) {
        run_sdram_timing_sweep(memory::SdramBank::bank1);
    } else if (cmd == "sdram2 probe"sv) {
        run_sdram_probe(memory::SdramBank::bank2, false);
    } else if (cmd == "sdram2 probe force"sv) {
        run_sdram_probe(memory::SdramBank::bank2, true);
    } else if (cmd == "sdram2 verify"sv) {
        run_sdram_verify(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 bus"sv) {
        run_sdram_bus_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 waitbus"sv) {
        run_sdram_wait_sequence_bus_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 spot"sv) {
        run_sdram_spot_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 alias"sv) {
        run_sdram_alias_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 addr"sv) {
        run_sdram_addr_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 lane"sv) {
        run_sdram_lane_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 repeat"sv) {
        run_sdram_repeat_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 locate"sv) {
        run_sdram_locate_diag(memory::SdramBank::bank2);
    } else if (cmd == "sdram2 timing"sv) {
        run_sdram_timing_sweep(memory::SdramBank::bank2);
    } else if (cmd == "storage init"sv) {
        run_storage_init();
    } else if (cmd == "storage status"sv) {
        print_storage_status();
    } else if (cmd.starts_with("storage width "sv)) {
        run_storage_width(cmd.substr(14));
    } else if (cmd.starts_with("storage write "sv)) {
        run_storage_write(cmd.substr(14));
    } else if (cmd == "qspi probe"sv) {
        run_qspi_probe(false);
    } else if (cmd == "qspi probe force"sv) {
        run_qspi_probe(true);
    } else if (cmd.starts_with("qspi read "sv)) {
        run_qspi_read(cmd.substr(10));
    } else if (cmd == "reboot"sv) {
        emit<"rebooting...\n">();
        HAL_Delay(20U);
        NVIC_SystemReset();
    } else {
        emit<"unknown command\n">();
    }
}

} // namespace

void init() {
    pmic.init();
    (void)pmic.apply(power::Profile::storage_stage_a);
    storage.init();
    emit<"diag_shell: init ok\n">();
    print_help();
    print_status();
    prompt();
    last_tick = h747::port::tick_ms();
}

void loop_once() noexcept {
    if (const auto line = line_source.poll_line()) {
        handle_command(*line);
        prompt();
    }

    const std::uint32_t now = h747::port::tick_ms();
    if ((now - last_tick) >= 2000U) {
        last_tick = now;
        const auto memory = storage.snapshot();
        const auto power = pmic.snapshot();
        emit<"diag_shell: alive tick={} sdram1={} sdram2={} qspi={} pmic={} transport={}\n">(
            now,
            memory.sdram(memory::SdramBank::bank1).ready,
            memory.sdram(memory::SdramBank::bank2).ready,
            memory.qspi().jedec_ok,
            power.ready(),
            power.transport_text());
        prompt();
    }
}

} // namespace h747::apps::diag_shell
