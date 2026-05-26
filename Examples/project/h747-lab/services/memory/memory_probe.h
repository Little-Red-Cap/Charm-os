#pragma once

#include <cstdint>
#include <string_view>

extern "C" {

struct memory_storage_state_t {
    std::uint8_t power_ready;
    std::uint8_t pmic_ready;
    std::uint8_t storage_profile_applied;
    std::uint8_t sdram1_power_good;
    std::uint8_t sdram2_power_good;
    std::uint8_t sdram1_attempted;
    std::uint8_t sdram1_init_ok;
    std::uint8_t sdram1_smoke_ok;
    std::uint8_t sdram1_ready;
    std::uint8_t sdram1_verify_ok;
    std::uint8_t sdram2_attempted;
    std::uint8_t sdram2_init_ok;
    std::uint8_t sdram2_smoke_ok;
    std::uint8_t sdram2_ready;
    std::uint8_t sdram2_verify_ok;
    std::uint8_t qspi_power_good;
    std::uint8_t qspi_attempted;
    std::uint8_t qspi_init_ok;
    std::uint8_t qspi_jedec_ok;
    std::uint8_t qspi_status_ok;
    std::uint8_t qspi_read_ok;
    std::uint8_t qspi_wip;
    std::uint8_t qspi_wel;
    std::uint16_t ldo4_mv;
    std::uint16_t qspi_dcdc1_mv;
    std::uint32_t sdram1_base;
    std::uint32_t sdram1_size_bytes;
    std::uint32_t sdram1_tested_words;
    std::uint32_t sdram1_verify_tested_words;
    std::uint32_t sdram2_base;
    std::uint32_t sdram2_size_bytes;
    std::uint32_t sdram2_tested_words;
    std::uint32_t sdram2_verify_tested_words;
    std::uintptr_t sdram1_first_error_addr;
    std::uint32_t sdram1_first_expected;
    std::uint32_t sdram1_first_actual;
    std::uintptr_t sdram1_verify_first_error_addr;
    std::uint32_t sdram1_verify_first_expected;
    std::uint32_t sdram1_verify_first_actual;
    std::uintptr_t sdram2_first_error_addr;
    std::uint32_t sdram2_first_expected;
    std::uint32_t sdram2_first_actual;
    std::uintptr_t sdram2_verify_first_error_addr;
    std::uint32_t sdram2_verify_first_expected;
    std::uint32_t sdram2_verify_first_actual;
    std::uint8_t qspi_jedec_id[3];
    std::uint8_t qspi_status1;
    std::uint8_t qspi_status2;
    std::uint8_t qspi_last_read_len;
    std::uint8_t qspi_last_read_data[16];
    std::uint8_t qspi_last_cmd;
    std::uint8_t reserved0[3];
    std::uint32_t qspi_last_addr;
    std::uint32_t sdram1_last_hal_status;
    std::uint32_t sdram2_last_hal_status;
    std::uint32_t qspi_last_hal_status;
    std::uint32_t qspi_last_error;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr1;
    std::uint32_t fmc_sdtr1;
    std::uint32_t fmc_sdcr2;
    std::uint32_t fmc_sdtr2;
    std::uint32_t fmc_clock_hz;
    std::uint32_t scb_ccr;
    std::uint32_t scb_shcsr;
    std::uint32_t qspi_cr;
    std::uint32_t qspi_dcr;
    std::uint32_t qspi_sr;
};

struct memory_probe_sdram_bus_sample_t {
    std::uint32_t expected;
    std::uint32_t actual;
};

struct memory_probe_sdram_bus_diag_t {
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t sample_count;
    std::uint8_t reserved0;
    std::uint32_t base;
    std::uint32_t mismatch_or;
    std::uint32_t mismatch_and;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
    memory_probe_sdram_bus_sample_t samples[40];
};

struct memory_probe_sdram_spot_sample_t {
    std::uint32_t offset;
    std::uint32_t expected;
    std::uint32_t immediate_actual;
    std::uint32_t after_sync_actual;
};

struct memory_probe_sdram_spot_diag_t {
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t sample_count;
    std::uint8_t reserved0;
    std::uint32_t base;
    std::uint32_t mismatch_or;
    std::uint32_t mismatch_and;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
    memory_probe_sdram_spot_sample_t samples[32];
};

struct memory_probe_sdram_timing_sample_t {
    std::uint8_t preset;
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t smoke_ok;
    std::uint32_t sdclock_period;
    std::uint32_t cas_latency;
    std::uint32_t read_burst;
    std::uint32_t read_pipe_delay;
    std::uint32_t mode_reg;
    std::uint32_t first_error_addr;
    std::uint32_t expected;
    std::uint32_t actual;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
};

struct memory_probe_sdram_timing_diag_t {
    std::uint8_t ok;
    std::uint8_t sample_count;
    std::uint8_t reserved0;
    std::uint8_t reserved1;
    std::uint32_t base;
    memory_probe_sdram_timing_sample_t samples[12];
};

struct memory_probe_sdram_alias_sample_t {
    std::uint32_t offset;
    std::uint32_t expected;
    std::uint32_t actual_minus_2;
    std::uint32_t actual_minus_1;
    std::uint32_t actual_self;
    std::uint32_t actual_plus_1;
    std::uint32_t actual_plus_2;
};

struct memory_probe_sdram_alias_diag_t {
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t sample_count;
    std::uint8_t reserved0;
    std::uint32_t base;
    std::uint32_t mismatch_or;
    std::uint32_t mismatch_and;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
    memory_probe_sdram_alias_sample_t samples[24];
};

struct memory_probe_sdram_addr_sample_t {
    std::uint32_t offset;
    std::uint32_t expected;
    std::uint32_t actual;
    std::uint32_t source_offset;
};

struct memory_probe_sdram_addr_diag_t {
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t sample_count;
    std::uint8_t reserved0;
    std::uint32_t base;
    std::uint32_t mismatch_or;
    std::uint32_t mismatch_and;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
    memory_probe_sdram_addr_sample_t samples[32];
};

struct memory_probe_sdram_lane_sample_t {
    std::uint32_t access_bits;
    std::uint32_t offset;
    std::uint32_t write_value;
    std::uint32_t expected_word;
    std::uint32_t actual_word;
};

struct memory_probe_sdram_lane_diag_t {
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t sample_count;
    std::uint8_t reserved0;
    std::uint32_t base;
    std::uint32_t mismatch_or;
    std::uint32_t mismatch_and;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
    memory_probe_sdram_lane_sample_t samples[12];
};

struct memory_probe_sdram_repeat_sample_t {
    std::uint32_t offset;
    std::uint32_t expected;
    std::uint32_t reads[8];
};

struct memory_probe_sdram_repeat_diag_t {
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t sample_count;
    std::uint8_t read_count;
    std::uint32_t base;
    std::uint32_t mismatch_or;
    std::uint32_t mismatch_and;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
    memory_probe_sdram_repeat_sample_t samples[8];
};

struct memory_probe_sdram_locate_sample_t {
    std::uint32_t write_offset;
    std::uint32_t expected;
    std::uint32_t hit_offset;
    std::uint32_t hit_count;
    std::uint32_t actual_minus_2;
    std::uint32_t actual_minus_1;
    std::uint32_t actual_self;
    std::uint32_t actual_plus_1;
    std::uint32_t actual_plus_2;
};

struct memory_probe_sdram_locate_diag_t {
    std::uint8_t ok;
    std::uint8_t init_ok;
    std::uint8_t sample_count;
    std::uint8_t reserved0;
    std::uint32_t base;
    std::uint32_t fmc_sdsr;
    std::uint32_t fmc_sdcr;
    std::uint32_t fmc_sdtr;
    memory_probe_sdram_locate_sample_t samples[8];
};

void memory_probe_storage_init();
void memory_probe_storage_poll();
memory_storage_state_t memory_probe_storage_state();
std::uint8_t memory_probe_configure_sdram_mpu_normal();
std::uint8_t memory_probe_sdram1_smoke();
std::uint8_t memory_probe_sdram1_smoke_force();
std::uint8_t memory_probe_sdram2_smoke();
std::uint8_t memory_probe_sdram2_smoke_force();
std::uint8_t memory_probe_sdram1_verify();
std::uint8_t memory_probe_sdram2_verify();
std::uint8_t memory_probe_sdram1_bus_diag(memory_probe_sdram_bus_diag_t* diag);
std::uint8_t memory_probe_sdram2_bus_diag(memory_probe_sdram_bus_diag_t* diag);
std::uint8_t memory_probe_sdram1_spot_diag(memory_probe_sdram_spot_diag_t* diag);
std::uint8_t memory_probe_sdram2_spot_diag(memory_probe_sdram_spot_diag_t* diag);
std::uint8_t memory_probe_sdram1_alias_diag(memory_probe_sdram_alias_diag_t* diag);
std::uint8_t memory_probe_sdram2_alias_diag(memory_probe_sdram_alias_diag_t* diag);
std::uint8_t memory_probe_sdram1_addr_diag(memory_probe_sdram_addr_diag_t* diag);
std::uint8_t memory_probe_sdram2_addr_diag(memory_probe_sdram_addr_diag_t* diag);
std::uint8_t memory_probe_sdram1_lane_diag(memory_probe_sdram_lane_diag_t* diag);
std::uint8_t memory_probe_sdram2_lane_diag(memory_probe_sdram_lane_diag_t* diag);
std::uint8_t memory_probe_sdram1_repeat_diag(memory_probe_sdram_repeat_diag_t* diag);
std::uint8_t memory_probe_sdram2_repeat_diag(memory_probe_sdram_repeat_diag_t* diag);
std::uint8_t memory_probe_sdram1_locate_diag(memory_probe_sdram_locate_diag_t* diag);
std::uint8_t memory_probe_sdram2_locate_diag(memory_probe_sdram_locate_diag_t* diag);
std::uint8_t memory_probe_sdram1_wait_sequence_bus_diag(memory_probe_sdram_bus_diag_t* diag);
std::uint8_t memory_probe_sdram2_wait_sequence_bus_diag(memory_probe_sdram_bus_diag_t* diag);
std::uint8_t memory_probe_sdram1_timing_sweep(memory_probe_sdram_timing_diag_t* diag);
std::uint8_t memory_probe_sdram2_timing_sweep(memory_probe_sdram_timing_diag_t* diag);
std::uint8_t memory_probe_qspi_probe();
std::uint8_t memory_probe_qspi_probe_force();
std::uint8_t memory_probe_qspi_read(std::uint32_t addr, std::uint8_t* data, std::uint32_t size);

}

struct memory_probe_sdram_profile_t {
    const char* name;
    std::uint32_t size_bytes;
    std::uint32_t column_bits;
    std::uint32_t row_bits;
    std::uint32_t internal_banks;
    std::uint32_t memory_data_width_bits;
    std::uint32_t refresh_rate;
};

const memory_probe_sdram_profile_t* memory_probe_sdram1_profile();
const memory_probe_sdram_profile_t* memory_probe_sdram2_profile();
