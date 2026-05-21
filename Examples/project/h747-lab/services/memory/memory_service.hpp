#pragma once

#include "memory_probe.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace h747::memory {

using namespace std::literals::string_view_literals;

enum class SdramBank : std::uint8_t {
    bank1,
    bank2,
};

struct SdramEvidence {
    SdramBank bank{};
    const memory_probe_sdram_profile_t* profile{};
    bool power_good{};
    bool attempted{};
    bool init_ok{};
    bool smoke_ok{};
    bool ready{};
    bool verify_ok{};
    std::uint32_t base{};
    std::uint32_t size_bytes{};
    std::uint32_t tested_words{};
    std::uint32_t verify_tested_words{};
    std::uint32_t last_hal_status{};
    std::uintptr_t first_error_addr{};
    std::uint32_t first_expected{};
    std::uint32_t first_actual{};
    std::uintptr_t verify_first_error_addr{};
    std::uint32_t verify_first_expected{};
    std::uint32_t verify_first_actual{};

    [[nodiscard]] std::string_view name() const noexcept {
        return (bank == SdramBank::bank2) ? "sdram2"sv : "sdram1"sv;
    }

    [[nodiscard]] std::string_view profile_name() const noexcept {
        return (profile != nullptr) ? std::string_view{profile->name} : "none"sv;
    }
};

struct QspiEvidence {
    bool power_good{};
    bool attempted{};
    bool init_ok{};
    bool jedec_ok{};
    bool status_ok{};
    bool read_ok{};
    bool wip{};
    bool wel{};
    std::uint16_t dcdc1_mv{};
    std::uint8_t jedec_id[3]{};
    std::uint8_t status1{};
    std::uint8_t status2{};
    std::uint8_t last_read_len{};
    std::uint8_t last_read_data[16]{};
    std::uint8_t last_cmd{};
    std::uint32_t last_addr{};
    std::uint32_t last_hal_status{};
    std::uint32_t last_error{};
    std::uint32_t cr{};
    std::uint32_t dcr{};
    std::uint32_t sr{};

    [[nodiscard]] std::span<const std::uint8_t> last_read() const noexcept {
        return {last_read_data, last_read_len};
    }
};

struct StorageSnapshot {
    memory_storage_state_t raw{};

    [[nodiscard]] SdramEvidence sdram(const SdramBank bank) const noexcept {
        if (bank == SdramBank::bank2) {
            return {
                bank,
                memory_probe_sdram2_profile(),
                raw.sdram2_power_good != 0U,
                raw.sdram2_attempted != 0U,
                raw.sdram2_init_ok != 0U,
                raw.sdram2_smoke_ok != 0U,
                raw.sdram2_ready != 0U,
                raw.sdram2_verify_ok != 0U,
                raw.sdram2_base,
                raw.sdram2_size_bytes,
                raw.sdram2_tested_words,
                raw.sdram2_verify_tested_words,
                raw.sdram2_last_hal_status,
                raw.sdram2_first_error_addr,
                raw.sdram2_first_expected,
                raw.sdram2_first_actual,
                raw.sdram2_verify_first_error_addr,
                raw.sdram2_verify_first_expected,
                raw.sdram2_verify_first_actual,
            };
        }

        return {
            bank,
            memory_probe_sdram1_profile(),
            raw.sdram1_power_good != 0U,
            raw.sdram1_attempted != 0U,
            raw.sdram1_init_ok != 0U,
            raw.sdram1_smoke_ok != 0U,
            raw.sdram1_ready != 0U,
            raw.sdram1_verify_ok != 0U,
            raw.sdram1_base,
            raw.sdram1_size_bytes,
            raw.sdram1_tested_words,
            raw.sdram1_verify_tested_words,
            raw.sdram1_last_hal_status,
            raw.sdram1_first_error_addr,
            raw.sdram1_first_expected,
            raw.sdram1_first_actual,
            raw.sdram1_verify_first_error_addr,
            raw.sdram1_verify_first_expected,
            raw.sdram1_verify_first_actual,
        };
    }

    [[nodiscard]] QspiEvidence qspi() const noexcept {
        QspiEvidence qspi{};
        qspi.power_good = raw.qspi_power_good != 0U;
        qspi.attempted = raw.qspi_attempted != 0U;
        qspi.init_ok = raw.qspi_init_ok != 0U;
        qspi.jedec_ok = raw.qspi_jedec_ok != 0U;
        qspi.status_ok = raw.qspi_status_ok != 0U;
        qspi.read_ok = raw.qspi_read_ok != 0U;
        qspi.wip = raw.qspi_wip != 0U;
        qspi.wel = raw.qspi_wel != 0U;
        qspi.dcdc1_mv = raw.qspi_dcdc1_mv;
        qspi.jedec_id[0] = raw.qspi_jedec_id[0];
        qspi.jedec_id[1] = raw.qspi_jedec_id[1];
        qspi.jedec_id[2] = raw.qspi_jedec_id[2];
        qspi.status1 = raw.qspi_status1;
        qspi.status2 = raw.qspi_status2;
        qspi.last_read_len = raw.qspi_last_read_len;
        for (std::uint32_t index = 0; index < sizeof(qspi.last_read_data); ++index) {
            qspi.last_read_data[index] = raw.qspi_last_read_data[index];
        }
        qspi.last_cmd = raw.qspi_last_cmd;
        qspi.last_addr = raw.qspi_last_addr;
        qspi.last_hal_status = raw.qspi_last_hal_status;
        qspi.last_error = raw.qspi_last_error;
        qspi.cr = raw.qspi_cr;
        qspi.dcr = raw.qspi_dcr;
        qspi.sr = raw.qspi_sr;
        return qspi;
    }
};

class StorageProbe {
public:
    void init() const noexcept {
        memory_probe_storage_init();
    }

    void poll() const noexcept {
        memory_probe_storage_poll();
    }

    [[nodiscard]] StorageSnapshot snapshot() const noexcept {
        return StorageSnapshot{memory_probe_storage_state()};
    }

    bool configure_sdram_mpu_normal() const noexcept {
        return memory_probe_configure_sdram_mpu_normal() != 0U;
    }

    bool smoke(const SdramBank bank, const bool force = false) const noexcept {
        if (bank == SdramBank::bank2) {
            return (force ? memory_probe_sdram2_smoke_force() : memory_probe_sdram2_smoke()) != 0U;
        }
        return (force ? memory_probe_sdram1_smoke_force() : memory_probe_sdram1_smoke()) != 0U;
    }

    bool verify(const SdramBank bank) const noexcept {
        if (bank == SdramBank::bank2) {
            return memory_probe_sdram2_verify() != 0U;
        }
        return memory_probe_sdram1_verify() != 0U;
    }

    bool bus_diag(const SdramBank bank, memory_probe_sdram_bus_diag_t& diag) const noexcept {
        if (bank == SdramBank::bank2) {
            return memory_probe_sdram2_bus_diag(&diag) != 0U;
        }
        return memory_probe_sdram1_bus_diag(&diag) != 0U;
    }

    bool spot_diag(const SdramBank bank, memory_probe_sdram_spot_diag_t& diag) const noexcept {
        if (bank == SdramBank::bank2) {
            return memory_probe_sdram2_spot_diag(&diag) != 0U;
        }
        return memory_probe_sdram1_spot_diag(&diag) != 0U;
    }

    bool alias_diag(const SdramBank bank, memory_probe_sdram_alias_diag_t& diag) const noexcept {
        if (bank == SdramBank::bank2) {
            return memory_probe_sdram2_alias_diag(&diag) != 0U;
        }
        return memory_probe_sdram1_alias_diag(&diag) != 0U;
    }

    bool wait_sequence_bus_diag(const SdramBank bank, memory_probe_sdram_bus_diag_t& diag) const noexcept {
        if (bank == SdramBank::bank2) {
            return memory_probe_sdram2_wait_sequence_bus_diag(&diag) != 0U;
        }
        return memory_probe_sdram1_wait_sequence_bus_diag(&diag) != 0U;
    }

    bool timing_sweep(const SdramBank bank, memory_probe_sdram_timing_diag_t& diag) const noexcept {
        if (bank == SdramBank::bank2) {
            return memory_probe_sdram2_timing_sweep(&diag) != 0U;
        }
        return memory_probe_sdram1_timing_sweep(&diag) != 0U;
    }

    bool probe_qspi(const bool force = false) const noexcept {
        return (force ? memory_probe_qspi_probe_force() : memory_probe_qspi_probe()) != 0U;
    }

    bool read_qspi(const std::uint32_t addr, const std::span<std::uint8_t> data) const noexcept {
        return memory_probe_qspi_read(addr, data.data(), static_cast<std::uint32_t>(data.size())) != 0U;
    }
};

} // namespace h747::memory
