#include "memory_probe.h"

#include <array>
#include <cstring>
#include <span>

#include "fmc.h"
#include "power.h"
#include "quadspi.h"
#include "stm32h7xx_hal.h"

namespace {

enum class SdramBank : std::uint8_t {
    bank1,
    bank2,
};

enum class SdramWindowKind : std::uint8_t {
    smoke,
    verify,
};

struct SdramProfileRuntime {
    memory_probe_sdram_profile_t info;
    std::uint32_t sdclock_period;
    std::uint32_t cas_latency;
    std::uint32_t read_burst;
    std::uint32_t read_pipe_delay;
    std::uint32_t load_to_active_delay;
    std::uint32_t exit_self_refresh_delay;
    std::uint32_t self_refresh_time;
    std::uint32_t row_cycle_delay;
    std::uint32_t write_recovery_time;
    std::uint32_t rp_delay;
    std::uint32_t rcd_delay;
    std::uint32_t mode_reg;
};

struct SdramConfig {
    const char* name;
    std::uint32_t base;
    std::uint32_t bank;
    std::uint32_t command_target;
    const SdramProfileRuntime* profile;
};

constexpr std::uint32_t kSdram1Base = 0xC0000000U;
constexpr std::uint32_t kSdram2Base = 0xD0000000U;
constexpr std::uint32_t kSdramWindowWords = 64U;
constexpr std::uint32_t kSdramBusDiagWords = 4U;
constexpr std::uint32_t kSdramTimingSweepMaxSamples = 12U;
constexpr std::uint32_t kSdramModeReg = 0x0030U | 0x0200U;
constexpr std::uint32_t kSdramModeRegCas2 = 0x0020U | 0x0200U;
constexpr std::uint32_t kSdramModeRegBurstLength4 = 0x0002U | 0x0030U | 0x0200U;
constexpr std::uint32_t kSdramModeRegBurstLength8 = 0x0004U | 0x0030U | 0x0200U;
constexpr std::uint32_t kFmcSdsrBusyMask = 1U << 5U;
constexpr std::uint32_t kQspiTimeoutMs = 100U;
constexpr std::uint32_t kQspiClockPrescaler = 29U;
constexpr std::uint32_t kQspiFlashSize = 23U;
constexpr std::uint8_t kQspiCmdReadJedec = 0x9FU;
constexpr std::uint8_t kQspiCmdReadStatus1 = 0x05U;
constexpr std::uint8_t kQspiCmdReadStatus2 = 0x35U;
constexpr std::uint8_t kQspiCmdReadData = 0x03U;
constexpr std::uint32_t kQspiReadCaptureBytes = 16U;

memory_storage_state_t g_state{};

SdramProfileRuntime g_is42s32800g_32m{
    {"is42s32800g_32m", 0x02000000U, 9U, 12U, 4U, 32U, 0U},
    FMC_SDRAM_CLOCK_PERIOD_2,
    FMC_SDRAM_CAS_LATENCY_3,
    FMC_SDRAM_RBURST_ENABLE,
    FMC_SDRAM_RPIPE_DELAY_1,
    2U,
    7U,
    4U,
    7U,
    3U,
    2U,
    2U,
    kSdramModeReg,
};

SdramConfig g_sdram1{"sdram1", kSdram1Base, FMC_SDRAM_BANK1, FMC_SDRAM_CMD_TARGET_BANK1, &g_is42s32800g_32m};
SdramConfig g_sdram2{"sdram2", kSdram2Base, FMC_SDRAM_BANK2, FMC_SDRAM_CMD_TARGET_BANK2, &g_is42s32800g_32m};

struct SdramTimingPreset {
    std::uint32_t sdclock_period;
    std::uint32_t cas_latency;
    std::uint32_t read_burst;
    std::uint32_t read_pipe_delay;
    std::uint32_t load_to_active_delay;
    std::uint32_t exit_self_refresh_delay;
    std::uint32_t self_refresh_time;
    std::uint32_t row_cycle_delay;
    std::uint32_t write_recovery_time;
    std::uint32_t rp_delay;
    std::uint32_t rcd_delay;
    std::uint32_t mode_reg;
};

constexpr std::array<SdramTimingPreset, 12> kSdramTimingPresets{{
    // Current conservative baseline.
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_0,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeReg},
    // Same slow clock, but shift read capture by one FMC cycle.
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_1,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeReg},
    // Same slow clock, but shift read capture by two FMC cycles.
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_2,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeReg},
    // ST EVAL IS42S32800G shape at SDRAM clock HCLK/2.
    {FMC_SDRAM_CLOCK_PERIOD_2, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_ENABLE, FMC_SDRAM_RPIPE_DELAY_0,
     2U, 7U, 4U, 7U, 2U, 2U, 2U, kSdramModeReg},
    // CubeMX-style H7 SDRAM shape: HCLK/2, burst read, pipe delay 1.
    {FMC_SDRAM_CLOCK_PERIOD_2, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_ENABLE, FMC_SDRAM_RPIPE_DELAY_1,
     2U, 7U, 4U, 7U, 3U, 2U, 2U, kSdramModeReg},
    // Fast clock plus maximum read pipe.
    {FMC_SDRAM_CLOCK_PERIOD_2, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_ENABLE, FMC_SDRAM_RPIPE_DELAY_2,
     2U, 7U, 4U, 7U, 3U, 2U, 2U, kSdramModeReg},
    // Slow clock plus read burst/pipe, to separate rate issues from sampling issues.
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_ENABLE, FMC_SDRAM_RPIPE_DELAY_1,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeReg},
    // Mode-register burst length variants: if the issue is MRS burst semantics, these should move the signature.
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_0,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeRegBurstLength4},
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_3, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_0,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeRegBurstLength8},
    // CAS2 variants: if the failure is command/mode latency, these should change the signature.
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_2, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_0,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeRegCas2},
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_2, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_1,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeRegCas2},
    {FMC_SDRAM_CLOCK_PERIOD_3, FMC_SDRAM_CAS_LATENCY_2, FMC_SDRAM_RBURST_DISABLE, FMC_SDRAM_RPIPE_DELAY_2,
     4U, 10U, 6U, 10U, 4U, 4U, 4U, kSdramModeRegCas2},
}};

constexpr std::array<std::uint16_t, 64> kDcdcMv = {
    900, 925, 950, 975, 1000, 1025, 1050, 1075,
    1100, 1125, 1150, 1175, 1200, 1225, 1250, 1275,
    1300, 1325, 1350, 1375, 1400, 1425, 1450, 1475,
    1500, 1550, 1600, 1650, 1700, 1750, 1800, 1850,
    1900, 1950, 2000, 2050, 2100, 2150, 2200, 2250,
    2300, 2350, 2400, 2450, 2500, 2550, 2600, 2650,
    2700, 2750, 2800, 2850, 2900, 3000, 3100, 3200,
    3300, 3300, 3300, 3300, 3300, 3300, 3300, 3300,
};

constexpr std::array<std::uint16_t, 32> kLdo34Mv = {
    1500, 1550, 1600, 1650, 1700, 1750, 1800, 1850,
    1900, 1950, 2000, 2050, 2100, 2150, 2200, 2250,
    2300, 2350, 2400, 2450, 2500, 2550, 2600, 2650,
    2700, 2750, 2800, 2850, 2900, 3000, 3100, 3300,
};

std::uint16_t decode_dcdc_mv(const std::uint8_t code) {
    return kDcdcMv[code & 0x3FU];
}

std::uint16_t decode_ldo34_mv(const std::uint8_t code) {
    return kLdo34Mv[code & 0x1FU];
}

std::uint32_t fmc_column_bits_value(const std::uint32_t bits) {
    switch (bits) {
    case 8U: return FMC_SDRAM_COLUMN_BITS_NUM_8;
    case 9U: return FMC_SDRAM_COLUMN_BITS_NUM_9;
    case 10U: return FMC_SDRAM_COLUMN_BITS_NUM_10;
    case 11U: return FMC_SDRAM_COLUMN_BITS_NUM_11;
    default: return FMC_SDRAM_COLUMN_BITS_NUM_8;
    }
}

std::uint32_t fmc_row_bits_value(const std::uint32_t bits) {
    switch (bits) {
    case 11U: return FMC_SDRAM_ROW_BITS_NUM_11;
    case 12U: return FMC_SDRAM_ROW_BITS_NUM_12;
    case 13U: return FMC_SDRAM_ROW_BITS_NUM_13;
    default: return FMC_SDRAM_ROW_BITS_NUM_12;
    }
}

std::uint32_t fmc_internal_banks_value(const std::uint32_t banks) {
    return (banks == 2U) ? FMC_SDRAM_INTERN_BANKS_NUM_2 : FMC_SDRAM_INTERN_BANKS_NUM_4;
}

std::uint32_t fmc_data_width_value(const std::uint32_t bits) {
    switch (bits) {
    case 8U: return FMC_SDRAM_MEM_BUS_WIDTH_8;
    case 16U: return FMC_SDRAM_MEM_BUS_WIDTH_16;
    case 32U: return FMC_SDRAM_MEM_BUS_WIDTH_32;
    default: return FMC_SDRAM_MEM_BUS_WIDTH_32;
    }
}

std::uint32_t fmc_clock_hz() {
    std::uint32_t hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FMC);
    return (hz != 0U) ? hz : HAL_RCC_GetHCLKFreq();
}

std::uint32_t compute_sdram_refresh_rate(const std::uint32_t row_bits) {
    const std::uint64_t rows = 1ULL << row_bits;
    const std::uint64_t cycles = ((static_cast<std::uint64_t>(fmc_clock_hz()) * 64ULL) / 1000ULL) / rows;
    return (cycles > 20ULL) ? static_cast<std::uint32_t>(cycles - 20ULL) : 0U;
}

void refresh_profile_rates() {
    g_is42s32800g_32m.info.refresh_rate = compute_sdram_refresh_rate(g_is42s32800g_32m.info.row_bits);
}

SdramConfig& sdram_config(const SdramBank bank) {
    return (bank == SdramBank::bank2) ? g_sdram2 : g_sdram1;
}

SDRAM_HandleTypeDef* sdram_handle(const SdramBank bank) {
    return (bank == SdramBank::bank2) ? &hsdram2 : &hsdram1;
}

void snapshot_fmc_regs() {
    g_state.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    g_state.fmc_sdcr1 = FMC_Bank5_6_R->SDCR[0];
    g_state.fmc_sdtr1 = FMC_Bank5_6_R->SDTR[0];
    g_state.fmc_sdcr2 = FMC_Bank5_6_R->SDCR[1];
    g_state.fmc_sdtr2 = FMC_Bank5_6_R->SDTR[1];
    g_state.fmc_clock_hz = fmc_clock_hz();
    g_state.scb_ccr = SCB->CCR;
    g_state.scb_shcsr = SCB->SHCSR;
}

void snapshot_qspi_regs() {
    if (hqspi.Instance == nullptr) {
        g_state.qspi_cr = 0U;
        g_state.qspi_dcr = 0U;
        g_state.qspi_sr = 0U;
        g_state.qspi_last_error = 0U;
        return;
    }
    g_state.qspi_cr = hqspi.Instance->CR;
    g_state.qspi_dcr = hqspi.Instance->DCR;
    g_state.qspi_sr = hqspi.Instance->SR;
    g_state.qspi_last_error = HAL_QSPI_GetError(&hqspi);
}

void snapshot_power_state() {
    const power_state_t p = power_snapshot();
    g_state.power_ready = p.pmic_bus_prepared;
    g_state.pmic_ready = p.pmic_ready;
    g_state.storage_profile_applied =
        ((p.profile == POWER_PROFILE_STORAGE_STAGE_A) && (p.profile_applied != 0U)) ? 1U : 0U;

    const std::uint8_t ldo4_enabled = ((p.pmic_enable_reg & (1U << 5U)) != 0U) ? 1U : 0U;
    g_state.ldo4_mv = decode_ldo34_mv(p.pmic_defls2_reg);
    g_state.sdram1_power_good =
        ((p.pmic_ready != 0U) && (ldo4_enabled != 0U) && (g_state.ldo4_mv >= 3000U)) ? 1U : 0U;
    g_state.sdram2_power_good = g_state.sdram1_power_good;

    const std::uint8_t dcdc1_enabled = ((p.pmic_enable_reg & (1U << 4U)) != 0U) ? 1U : 0U;
    g_state.qspi_dcdc1_mv = decode_dcdc_mv(p.pmic_defdcdc1_reg);
    g_state.qspi_power_good =
        ((p.pmic_ready != 0U) && (dcdc1_enabled != 0U) && (g_state.qspi_dcdc1_mv >= 3000U)) ? 1U : 0U;
}

void sync_external_writes() {
    __DSB();
    __ISB();
}

std::uintptr_t cache_align_down(const std::uintptr_t address) {
    return address & ~static_cast<std::uintptr_t>(31U);
}

std::uint32_t cache_aligned_length(const std::uintptr_t address, const std::uint32_t length) {
    const std::uintptr_t start = cache_align_down(address);
    const std::uintptr_t end = (address + length + 31U) & ~static_cast<std::uintptr_t>(31U);
    return static_cast<std::uint32_t>(end - start);
}

void clean_invalidate_dcache_range(void* address, const std::uint32_t length) {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U) {
        return;
    }
    const auto addr = reinterpret_cast<std::uintptr_t>(address);
    auto* aligned = reinterpret_cast<std::uint32_t*>(cache_align_down(addr));
    const std::uint32_t bytes = cache_aligned_length(addr, length);
    SCB_CleanDCache_by_Addr(aligned, static_cast<std::int32_t>(bytes));
    SCB_InvalidateDCache_by_Addr(aligned, static_cast<std::int32_t>(bytes));
    __DSB();
    __ISB();
#else
    (void)address;
    (void)length;
#endif
}

void invalidate_dcache_range(void* address, const std::uint32_t length) {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U) {
        return;
    }
    const auto addr = reinterpret_cast<std::uintptr_t>(address);
    auto* aligned = reinterpret_cast<std::uint32_t*>(cache_align_down(addr));
    const std::uint32_t bytes = cache_aligned_length(addr, length);
    SCB_InvalidateDCache_by_Addr(aligned, static_cast<std::int32_t>(bytes));
    __DSB();
    __ISB();
#else
    (void)address;
    (void)length;
#endif
}

void sync_profile_state(const SdramBank bank) {
    const auto& cfg = sdram_config(bank);
    const auto& profile = cfg.profile->info;
    if (bank == SdramBank::bank2) {
        g_state.sdram2_base = cfg.base;
        g_state.sdram2_size_bytes = profile.size_bytes;
    } else {
        g_state.sdram1_base = cfg.base;
        g_state.sdram1_size_bytes = profile.size_bytes;
    }
}

void reset_sdram_result(const SdramBank bank) {
    if (bank == SdramBank::bank2) {
        g_state.sdram2_attempted = 1U;
        g_state.sdram2_init_ok = 0U;
        g_state.sdram2_smoke_ok = 0U;
        g_state.sdram2_ready = 0U;
        g_state.sdram2_verify_ok = 0U;
        g_state.sdram2_tested_words = 0U;
        g_state.sdram2_verify_tested_words = 0U;
        g_state.sdram2_first_error_addr = 0U;
        g_state.sdram2_first_expected = 0U;
        g_state.sdram2_first_actual = 0U;
        g_state.sdram2_verify_first_error_addr = 0U;
        g_state.sdram2_verify_first_expected = 0U;
        g_state.sdram2_verify_first_actual = 0U;
        g_state.sdram2_last_hal_status = 0U;
        return;
    }
    g_state.sdram1_attempted = 1U;
    g_state.sdram1_init_ok = 0U;
    g_state.sdram1_smoke_ok = 0U;
    g_state.sdram1_ready = 0U;
    g_state.sdram1_verify_ok = 0U;
    g_state.sdram1_tested_words = 0U;
    g_state.sdram1_verify_tested_words = 0U;
    g_state.sdram1_first_error_addr = 0U;
    g_state.sdram1_first_expected = 0U;
    g_state.sdram1_first_actual = 0U;
    g_state.sdram1_verify_first_error_addr = 0U;
    g_state.sdram1_verify_first_expected = 0U;
    g_state.sdram1_verify_first_actual = 0U;
    g_state.sdram1_last_hal_status = 0U;
}

void reset_sdram_verify_result(const SdramBank bank) {
    if (bank == SdramBank::bank2) {
        g_state.sdram2_attempted = 1U;
        g_state.sdram2_verify_ok = 0U;
        g_state.sdram2_verify_tested_words = 0U;
        g_state.sdram2_verify_first_error_addr = 0U;
        g_state.sdram2_verify_first_expected = 0U;
        g_state.sdram2_verify_first_actual = 0U;
        return;
    }
    g_state.sdram1_attempted = 1U;
    g_state.sdram1_verify_ok = 0U;
    g_state.sdram1_verify_tested_words = 0U;
    g_state.sdram1_verify_first_error_addr = 0U;
    g_state.sdram1_verify_first_expected = 0U;
    g_state.sdram1_verify_first_actual = 0U;
}

void record_sdram_error(const SdramBank bank,
                        const SdramWindowKind kind,
                        volatile std::uint32_t* addr,
                        const std::uint32_t expected,
                        const std::uint32_t actual) {
    if (kind == SdramWindowKind::verify) {
        if (bank == SdramBank::bank2) {
            if (g_state.sdram2_verify_first_error_addr == 0U) {
                g_state.sdram2_verify_first_error_addr = reinterpret_cast<std::uintptr_t>(addr);
                g_state.sdram2_verify_first_expected = expected;
                g_state.sdram2_verify_first_actual = actual;
            }
            return;
        }
        if (g_state.sdram1_verify_first_error_addr == 0U) {
            g_state.sdram1_verify_first_error_addr = reinterpret_cast<std::uintptr_t>(addr);
            g_state.sdram1_verify_first_expected = expected;
            g_state.sdram1_verify_first_actual = actual;
        }
        return;
    }

    if (bank == SdramBank::bank2) {
        if (g_state.sdram2_first_error_addr == 0U) {
            g_state.sdram2_first_error_addr = reinterpret_cast<std::uintptr_t>(addr);
            g_state.sdram2_first_expected = expected;
            g_state.sdram2_first_actual = actual;
        }
        return;
    }
    if (g_state.sdram1_first_error_addr == 0U) {
        g_state.sdram1_first_error_addr = reinterpret_cast<std::uintptr_t>(addr);
        g_state.sdram1_first_expected = expected;
        g_state.sdram1_first_actual = actual;
    }
}

void add_tested_words(const SdramBank bank, const SdramWindowKind kind, const std::uint32_t words) {
    if (kind == SdramWindowKind::verify) {
        if (bank == SdramBank::bank2) {
            g_state.sdram2_verify_tested_words += words;
        } else {
            g_state.sdram1_verify_tested_words += words;
        }
        return;
    }

    if (bank == SdramBank::bank2) {
        g_state.sdram2_tested_words += words;
    } else {
        g_state.sdram1_tested_words += words;
    }
}

void set_sdram_verify_status(const SdramBank bank, const bool ok) {
    if (bank == SdramBank::bank2) {
        g_state.sdram2_verify_ok = ok ? 1U : 0U;
        g_state.sdram2_ready = ok ? 1U : g_state.sdram2_ready;
    } else {
        g_state.sdram1_verify_ok = ok ? 1U : 0U;
        g_state.sdram1_ready = ok ? 1U : g_state.sdram1_ready;
    }
}

void set_sdram_init_status(const SdramBank bank, const HAL_StatusTypeDef status) {
    if (bank == SdramBank::bank2) {
        g_state.sdram2_last_hal_status = static_cast<std::uint32_t>(status);
        g_state.sdram2_init_ok = (status == HAL_OK) ? 1U : 0U;
    } else {
        g_state.sdram1_last_hal_status = static_cast<std::uint32_t>(status);
        g_state.sdram1_init_ok = (status == HAL_OK) ? 1U : 0U;
    }
}

void set_sdram_smoke_status(const SdramBank bank, const bool ok) {
    if (bank == SdramBank::bank2) {
        g_state.sdram2_smoke_ok = ok ? 1U : 0U;
        g_state.sdram2_ready = ok ? 1U : 0U;
    } else {
        g_state.sdram1_smoke_ok = ok ? 1U : 0U;
        g_state.sdram1_ready = ok ? 1U : 0U;
    }
}

void configure_sdram_handle(SDRAM_HandleTypeDef* handle, const SdramConfig& cfg) {
    const auto& profile = *cfg.profile;
    handle->Instance = FMC_SDRAM_DEVICE;
    handle->Init.SDBank = cfg.bank;
    handle->Init.ColumnBitsNumber = fmc_column_bits_value(profile.info.column_bits);
    handle->Init.RowBitsNumber = fmc_row_bits_value(profile.info.row_bits);
    handle->Init.MemoryDataWidth = fmc_data_width_value(profile.info.memory_data_width_bits);
    handle->Init.InternalBankNumber = fmc_internal_banks_value(profile.info.internal_banks);
    handle->Init.CASLatency = profile.cas_latency;
    handle->Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    handle->Init.SDClockPeriod = profile.sdclock_period;
    handle->Init.ReadBurst = profile.read_burst;
    handle->Init.ReadPipeDelay = profile.read_pipe_delay;
}

HAL_StatusTypeDef init_sdram_fmc(const SdramBank bank) {
    FMC_SDRAM_TimingTypeDef timing{};
    auto* handle = sdram_handle(bank);
    const auto& cfg = sdram_config(bank);
    const auto& profile = *cfg.profile;

    configure_sdram_handle(handle, cfg);
    timing.LoadToActiveDelay = profile.load_to_active_delay;
    timing.ExitSelfRefreshDelay = profile.exit_self_refresh_delay;
    timing.SelfRefreshTime = profile.self_refresh_time;
    timing.RowCycleDelay = profile.row_cycle_delay;
    timing.WriteRecoveryTime = profile.write_recovery_time;
    timing.RPDelay = profile.rp_delay;
    timing.RCDDelay = profile.rcd_delay;
    return HAL_SDRAM_Init(handle, &timing);
}

HAL_StatusTypeDef init_sdram_fmc_with_preset(const SdramBank bank, const SdramTimingPreset& preset) {
    FMC_SDRAM_TimingTypeDef timing{};
    auto* handle = sdram_handle(bank);
    const auto& cfg = sdram_config(bank);
    configure_sdram_handle(handle, cfg);
    handle->Init.SDClockPeriod = preset.sdclock_period;
    handle->Init.CASLatency = preset.cas_latency;
    handle->Init.ReadBurst = preset.read_burst;
    handle->Init.ReadPipeDelay = preset.read_pipe_delay;
    timing.LoadToActiveDelay = preset.load_to_active_delay;
    timing.ExitSelfRefreshDelay = preset.exit_self_refresh_delay;
    timing.SelfRefreshTime = preset.self_refresh_time;
    timing.RowCycleDelay = preset.row_cycle_delay;
    timing.WriteRecoveryTime = preset.write_recovery_time;
    timing.RPDelay = preset.rp_delay;
    timing.RCDDelay = preset.rcd_delay;
    return HAL_SDRAM_Init(handle, &timing);
}

HAL_StatusTypeDef init_sdram_sequence_with_mode_reg(const SdramBank bank, const std::uint32_t mode_reg) {
    FMC_SDRAM_CommandTypeDef command{};
    auto* handle = sdram_handle(bank);
    const auto& cfg = sdram_config(bank);

    command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    command.CommandTarget = cfg.command_target;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = 0U;
    if (HAL_SDRAM_SendCommand(handle, &command, 0xFFFFU) != HAL_OK) {
        return HAL_ERROR;
    }

    HAL_Delay(2U);

    command.CommandMode = FMC_SDRAM_CMD_PALL;
    if (HAL_SDRAM_SendCommand(handle, &command, 0xFFFFU) != HAL_OK) {
        return HAL_ERROR;
    }

    command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    command.AutoRefreshNumber = 8U;
    if (HAL_SDRAM_SendCommand(handle, &command, 0xFFFFU) != HAL_OK) {
        return HAL_ERROR;
    }

    command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = mode_reg;
    if (HAL_SDRAM_SendCommand(handle, &command, 0xFFFFU) != HAL_OK) {
        return HAL_ERROR;
    }

    const std::uint32_t refresh =
        (cfg.profile->info.refresh_rate != 0U)
            ? cfg.profile->info.refresh_rate
            : compute_sdram_refresh_rate(cfg.profile->info.row_bits);
    return HAL_SDRAM_ProgramRefreshRate(handle, refresh);
}

bool wait_sdram_not_busy(const std::uint32_t timeout_ms = 10U) {
    const std::uint32_t start = HAL_GetTick();
    while ((FMC_Bank5_6_R->SDSR & kFmcSdsrBusyMask) != 0U) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            return false;
        }
    }
    return true;
}

HAL_StatusTypeDef send_sdram_command_wait(SDRAM_HandleTypeDef* handle,
                                          FMC_SDRAM_CommandTypeDef& command,
                                          const std::uint32_t timeout_ms = 10U) {
    if (HAL_SDRAM_SendCommand(handle, &command, 0xFFFFU) != HAL_OK) {
        return HAL_ERROR;
    }
    return wait_sdram_not_busy(timeout_ms) ? HAL_OK : HAL_TIMEOUT;
}

HAL_StatusTypeDef init_sdram_sequence_wait(const SdramBank bank) {
    FMC_SDRAM_CommandTypeDef command{};
    auto* handle = sdram_handle(bank);
    const auto& cfg = sdram_config(bank);

    command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    command.CommandTarget = cfg.command_target;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = 0U;
    if (send_sdram_command_wait(handle, command) != HAL_OK) {
        return HAL_ERROR;
    }

    HAL_Delay(2U);

    command.CommandMode = FMC_SDRAM_CMD_PALL;
    if (send_sdram_command_wait(handle, command) != HAL_OK) {
        return HAL_ERROR;
    }

    command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    command.AutoRefreshNumber = 8U;
    if (send_sdram_command_wait(handle, command) != HAL_OK) {
        return HAL_ERROR;
    }

    command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    command.AutoRefreshNumber = 1U;
    command.ModeRegisterDefinition = cfg.profile->mode_reg;
    if (send_sdram_command_wait(handle, command) != HAL_OK) {
        return HAL_ERROR;
    }

    const std::uint32_t refresh =
        (cfg.profile->info.refresh_rate != 0U)
            ? cfg.profile->info.refresh_rate
            : compute_sdram_refresh_rate(cfg.profile->info.row_bits);
    return HAL_SDRAM_ProgramRefreshRate(handle, refresh);
}

HAL_StatusTypeDef init_sdram_sequence(const SdramBank bank) {
    return init_sdram_sequence_with_mode_reg(bank, sdram_config(bank).profile->mode_reg);
}

bool test_sdram_window(const SdramBank bank, const SdramWindowKind kind, const std::uint32_t offset_bytes) {
    const auto& cfg = sdram_config(bank);
    auto* words = reinterpret_cast<volatile std::uint32_t*>(cfg.base + offset_bytes);
    constexpr std::uint32_t bytes = kSdramWindowWords * sizeof(std::uint32_t);
    add_tested_words(bank, kind, kSdramWindowWords);

    for (std::uint32_t index = 0; index < kSdramWindowWords; ++index) {
        words[index] = 0x13579BDFU ^ offset_bytes ^ index;
    }
    sync_external_writes();
    clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset_bytes), bytes);
    for (std::uint32_t index = 0; index < kSdramWindowWords; ++index) {
        const std::uint32_t expected = 0x13579BDFU ^ offset_bytes ^ index;
        const std::uint32_t actual = words[index];
        if (actual != expected) {
            record_sdram_error(bank, kind, &words[index], expected, actual);
            return false;
        }
    }

    for (std::uint32_t index = 0; index < kSdramWindowWords; ++index) {
        words[index] = 0xECA86420U ^ offset_bytes ^ index;
    }
    sync_external_writes();
    clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset_bytes), bytes);
    for (std::uint32_t index = 0; index < kSdramWindowWords; ++index) {
        const std::uint32_t expected = 0xECA86420U ^ offset_bytes ^ index;
        const std::uint32_t actual = words[index];
        if (actual != expected) {
            record_sdram_error(bank, kind, &words[index], expected, actual);
            return false;
        }
    }

    for (std::uint32_t index = 0; index < kSdramWindowWords; ++index) {
        words[index] = 0U;
    }
    sync_external_writes();
    clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset_bytes), bytes);
    return true;
}

std::array<std::uint32_t, 3> sdram_smoke_window_offsets(const SdramProfileRuntime& profile) {
    const std::uint32_t bytes = kSdramWindowWords * sizeof(std::uint32_t);
    const std::uint32_t size = profile.info.size_bytes;
    const std::uint32_t mid = (size > bytes) ? ((size / 2U) & ~0x3U) : 0U;
    const std::uint32_t tail =
        (size > (0x00100000U + bytes)) ? ((size - 0x00100000U) & ~0x3U) :
        ((size > bytes) ? ((size - bytes) & ~0x3U) : 0U);
    return {0U, mid, tail};
}

std::array<std::uint32_t, 5> sdram_verify_window_offsets(const SdramProfileRuntime& profile) {
    const std::uint32_t bytes = kSdramWindowWords * sizeof(std::uint32_t);
    const std::uint32_t size = profile.info.size_bytes;
    const std::uint32_t usable_tail = (size > bytes) ? (size - bytes) : 0U;
    return {
        0U,
        ((size / 4U) & ~0x3U),
        ((size / 2U) & ~0x3U),
        (((size * 3U) / 4U) & ~0x3U),
        (usable_tail & ~0x3U),
    };
}

bool init_sdram_bank_for_probe(const SdramBank bank) {
    const HAL_StatusTypeDef fmc_status = init_sdram_fmc(bank);
    set_sdram_init_status(bank, fmc_status);
    snapshot_fmc_regs();
    if (fmc_status != HAL_OK) {
        return false;
    }

    const HAL_StatusTypeDef seq_status = init_sdram_sequence(bank);
    set_sdram_init_status(bank, seq_status);
    snapshot_fmc_regs();
    return seq_status == HAL_OK;
}

bool init_sdram_bank_for_probe_wait(const SdramBank bank) {
    const HAL_StatusTypeDef fmc_status = init_sdram_fmc(bank);
    set_sdram_init_status(bank, fmc_status);
    snapshot_fmc_regs();
    if (fmc_status != HAL_OK) {
        return false;
    }

    const HAL_StatusTypeDef seq_status = init_sdram_sequence_wait(bank);
    set_sdram_init_status(bank, seq_status);
    snapshot_fmc_regs();
    return seq_status == HAL_OK;
}

void capture_bus_diag_regs(const SdramBank bank, memory_probe_sdram_bus_diag_t& diag) {
    diag.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

std::uint8_t run_sdram_bus_patterns(const SdramBank bank, memory_probe_sdram_bus_diag_t& diag) {
    const auto& cfg = sdram_config(bank);
    diag.init_ok = 1U;
    auto* words = reinterpret_cast<volatile std::uint32_t*>(cfg.base);
    constexpr std::array<std::uint32_t, 10> patterns = {
        0x00000000U,
        0xFFFFFFFFU,
        0x55555555U,
        0xAAAAAAAAU,
        0x13579BDFU,
        0xECA86420U,
        0x000000FFU,
        0x0000FF00U,
        0x00FF0000U,
        0xFF000000U,
    };

    bool ok = true;
    std::uint32_t sample_index = 0U;
    std::uint32_t mismatch_and = 0xFFFFFFFFU;
    std::uint32_t mismatch_or = 0U;
    for (const auto pattern : patterns) {
        for (std::uint32_t index = 0; index < kSdramBusDiagWords; ++index) {
            words[index] = pattern ^ index;
        }
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base),
                                      kSdramBusDiagWords * sizeof(std::uint32_t));

        for (std::uint32_t index = 0; index < kSdramBusDiagWords; ++index) {
            const std::uint32_t expected = pattern ^ index;
            const std::uint32_t actual = words[index];
            const std::uint32_t mismatch = expected ^ actual;
            if (mismatch != 0U) {
                ok = false;
                mismatch_or |= mismatch;
                mismatch_and &= mismatch;
            }
            if (sample_index < (sizeof(diag.samples) / sizeof(diag.samples[0]))) {
                diag.samples[sample_index].expected = expected;
                diag.samples[sample_index].actual = actual;
                ++sample_index;
            }
        }
    }

    for (std::uint32_t index = 0; index < kSdramBusDiagWords; ++index) {
        words[index] = 0U;
    }
    sync_external_writes();
    clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base),
                                  kSdramBusDiagWords * sizeof(std::uint32_t));

    diag.sample_count = static_cast<std::uint8_t>(sample_index);
    diag.mismatch_or = mismatch_or;
    diag.mismatch_and = ok ? 0U : mismatch_and;
    diag.ok = ok ? 1U : 0U;
    capture_bus_diag_regs(bank, diag);
    return diag.ok;
}

std::uint8_t run_sdram_bus_diag(const SdramBank bank, memory_probe_sdram_bus_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_bus_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        diag.init_ok = 0U;
        capture_bus_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    const std::uint8_t ok = run_sdram_bus_patterns(bank, diag);
    *out = diag;
    return ok;
}

std::uint8_t run_sdram_wait_sequence_bus_diag(const SdramBank bank, memory_probe_sdram_bus_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_bus_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe_wait(bank)) {
        diag.init_ok = 0U;
        capture_bus_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    const std::uint8_t ok = run_sdram_bus_patterns(bank, diag);
    *out = diag;
    return ok;
}

void capture_spot_diag_regs(const SdramBank bank, memory_probe_sdram_spot_diag_t& diag) {
    diag.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

void capture_alias_diag_regs(const SdramBank bank, memory_probe_sdram_alias_diag_t& diag) {
    diag.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

void capture_addr_diag_regs(const SdramBank bank, memory_probe_sdram_addr_diag_t& diag) {
    diag.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

void capture_lane_diag_regs(const SdramBank bank, memory_probe_sdram_lane_diag_t& diag) {
    diag.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

void capture_repeat_diag_regs(const SdramBank bank, memory_probe_sdram_repeat_diag_t& diag) {
    diag.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

void capture_locate_diag_regs(const SdramBank bank, memory_probe_sdram_locate_diag_t& diag) {
    diag.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        diag.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        diag.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

std::uint8_t run_sdram_spot_diag(const SdramBank bank, memory_probe_sdram_spot_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_spot_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        diag.init_ok = 0U;
        capture_spot_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    constexpr std::array<std::uint32_t, 8> offsets = {
        0U,
        4U,
        8U,
        12U,
        16U,
        32U,
        64U,
        128U,
    };
    constexpr std::array<std::uint32_t, 4> patterns = {
        0x00000000U,
        0xFFFFFFFFU,
        0x13579BDFU,
        0xECA86420U,
    };

    diag.init_ok = 1U;
    bool ok = true;
    std::uint32_t sample_index = 0U;
    std::uint32_t mismatch_and = 0xFFFFFFFFU;
    std::uint32_t mismatch_or = 0U;
    for (const auto offset : offsets) {
        auto* const word = reinterpret_cast<volatile std::uint32_t*>(cfg.base + offset);
        for (const auto pattern : patterns) {
            const std::uint32_t expected = pattern ^ offset;
            *word = expected;
            sync_external_writes();
            const std::uint32_t immediate_actual = *word;
            invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset), sizeof(std::uint32_t));
            const std::uint32_t after_sync_actual = *word;

            const std::uint32_t mismatch = (expected ^ immediate_actual) | (expected ^ after_sync_actual);
            if (mismatch != 0U) {
                ok = false;
                mismatch_or |= mismatch;
                mismatch_and &= mismatch;
            }
            if (sample_index < (sizeof(diag.samples) / sizeof(diag.samples[0]))) {
                diag.samples[sample_index].offset = offset;
                diag.samples[sample_index].expected = expected;
                diag.samples[sample_index].immediate_actual = immediate_actual;
                diag.samples[sample_index].after_sync_actual = after_sync_actual;
                ++sample_index;
            }
        }
    }

    for (const auto offset : offsets) {
        *reinterpret_cast<volatile std::uint32_t*>(cfg.base + offset) = 0U;
    }
    sync_external_writes();
    clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base), 160U);

    diag.sample_count = static_cast<std::uint8_t>(sample_index);
    diag.mismatch_or = mismatch_or;
    diag.mismatch_and = ok ? 0U : mismatch_and;
    diag.ok = ok ? 1U : 0U;
    capture_spot_diag_regs(bank, diag);
    *out = diag;
    return diag.ok;
}

std::uint8_t run_sdram_alias_diag(const SdramBank bank, memory_probe_sdram_alias_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_alias_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        diag.init_ok = 0U;
        capture_alias_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    constexpr std::uint32_t kWindowWords = 16U;
    constexpr std::uint32_t kWindowBytes = kWindowWords * sizeof(std::uint32_t);
    constexpr std::array<std::uint32_t, 4> base_offsets = {
        0U,
        0x00000100U,
        0x00100000U,
        0x00F00000U,
    };

    diag.init_ok = 1U;
    bool ok = true;
    std::uint32_t sample_index = 0U;
    std::uint32_t mismatch_and = 0xFFFFFFFFU;
    std::uint32_t mismatch_or = 0U;

    for (const auto base_offset : base_offsets) {
        auto* const words = reinterpret_cast<volatile std::uint32_t*>(cfg.base + base_offset);
        for (std::uint32_t index = 0; index < kWindowWords; ++index) {
            words[index] = 0xA5000000U | (base_offset & 0x00FF0000U) | (index * 0x010101U);
        }
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + base_offset), kWindowBytes);

        for (std::uint32_t index = 2U; (index < (kWindowWords - 2U)) && (sample_index < (sizeof(diag.samples) / sizeof(diag.samples[0]))); index += 3U) {
            const std::uint32_t expected = 0xA5000000U | (base_offset & 0x00FF0000U) | (index * 0x010101U);
            const std::uint32_t actual = words[index];
            const std::uint32_t mismatch = expected ^ actual;
            if (mismatch != 0U) {
                ok = false;
                mismatch_or |= mismatch;
                mismatch_and &= mismatch;
            }

            auto& sample = diag.samples[sample_index];
            sample.offset = base_offset + (index * sizeof(std::uint32_t));
            sample.expected = expected;
            sample.actual_minus_2 = words[index - 2U];
            sample.actual_minus_1 = words[index - 1U];
            sample.actual_self = actual;
            sample.actual_plus_1 = words[index + 1U];
            sample.actual_plus_2 = words[index + 2U];
            ++sample_index;
        }
    }

    for (const auto base_offset : base_offsets) {
        auto* const words = reinterpret_cast<volatile std::uint32_t*>(cfg.base + base_offset);
        for (std::uint32_t index = 0; index < kWindowWords; ++index) {
            words[index] = 0U;
        }
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + base_offset), kWindowBytes);
    }

    diag.sample_count = static_cast<std::uint8_t>(sample_index);
    diag.mismatch_or = mismatch_or;
    diag.mismatch_and = ok ? 0U : mismatch_and;
    diag.ok = ok ? 1U : 0U;
    capture_alias_diag_regs(bank, diag);
    *out = diag;
    return diag.ok;
}

std::uint32_t sdram_addr_sentinel(const std::uint32_t offset) {
    return 0xC3A50000U ^ (offset * 2654435761UL) ^ (offset >> 7U);
}

std::uint32_t find_sdram_addr_source_offset(const std::span<const std::uint32_t> offsets,
                                            const std::uint32_t actual) {
    for (const auto offset : offsets) {
        if (sdram_addr_sentinel(offset) == actual) {
            return offset;
        }
    }
    return 0xFFFFFFFFU;
}

std::uint8_t run_sdram_addr_diag(const SdramBank bank, memory_probe_sdram_addr_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_addr_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        diag.init_ok = 0U;
        capture_addr_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    const std::uint32_t size = cfg.profile->info.size_bytes;
    const std::uint32_t tail = (size >= sizeof(std::uint32_t)) ? ((size - sizeof(std::uint32_t)) & ~0x3U) : 0U;
    std::array<std::uint32_t, 32> offsets{};
    std::uint32_t offset_count = 0U;
    const auto append_offset = [&](const std::uint32_t offset) {
        if ((offset > tail) || (offset_count >= offsets.size())) {
            return;
        }
        for (std::uint32_t index = 0; index < offset_count; ++index) {
            if (offsets[index] == offset) {
                return;
            }
        }
        offsets[offset_count++] = offset;
    };

    append_offset(0U);
    for (std::uint32_t offset = sizeof(std::uint32_t); (offset <= tail) && (offset != 0U); offset <<= 1U) {
        append_offset(offset);
        if (offset >= 0x01000000U) {
            break;
        }
    }
    append_offset(tail);

    diag.init_ok = 1U;
    bool ok = true;
    std::uint32_t mismatch_and = 0xFFFFFFFFU;
    std::uint32_t mismatch_or = 0U;
    const auto used_offsets = std::span<const std::uint32_t>{offsets.data(), offset_count};

    for (const auto offset : used_offsets) {
        auto* const word = reinterpret_cast<volatile std::uint32_t*>(cfg.base + offset);
        *word = sdram_addr_sentinel(offset);
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset), sizeof(std::uint32_t));
    }

    for (std::uint32_t index = 0; index < offset_count; ++index) {
        const std::uint32_t offset = offsets[index];
        auto* const word = reinterpret_cast<volatile std::uint32_t*>(cfg.base + offset);
        invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset), sizeof(std::uint32_t));
        const std::uint32_t expected = sdram_addr_sentinel(offset);
        const std::uint32_t actual = *word;
        const std::uint32_t mismatch = expected ^ actual;
        if (mismatch != 0U) {
            ok = false;
            mismatch_or |= mismatch;
            mismatch_and &= mismatch;
        }

        auto& sample = diag.samples[index];
        sample.offset = offset;
        sample.expected = expected;
        sample.actual = actual;
        sample.source_offset = (mismatch == 0U) ? offset : find_sdram_addr_source_offset(used_offsets, actual);
    }

    for (const auto offset : used_offsets) {
        auto* const word = reinterpret_cast<volatile std::uint32_t*>(cfg.base + offset);
        *word = 0U;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset), sizeof(std::uint32_t));
    }

    diag.sample_count = static_cast<std::uint8_t>(offset_count);
    diag.mismatch_or = mismatch_or;
    diag.mismatch_and = ok ? 0U : mismatch_and;
    diag.ok = ok ? 1U : 0U;
    capture_addr_diag_regs(bank, diag);
    *out = diag;
    return diag.ok;
}

void record_sdram_lane_sample(memory_probe_sdram_lane_diag_t& diag,
                              bool& ok,
                              std::uint32_t& mismatch_or,
                              std::uint32_t& mismatch_and,
                              const std::uint32_t access_bits,
                              const std::uint32_t offset,
                              const std::uint32_t write_value,
                              const std::uint32_t expected_word,
                              const std::uint32_t actual_word) {
    const std::uint32_t mismatch = expected_word ^ actual_word;
    if (mismatch != 0U) {
        ok = false;
        mismatch_or |= mismatch;
        mismatch_and &= mismatch;
    }
    if (diag.sample_count < (sizeof(diag.samples) / sizeof(diag.samples[0]))) {
        auto& sample = diag.samples[diag.sample_count++];
        sample.access_bits = access_bits;
        sample.offset = offset;
        sample.write_value = write_value;
        sample.expected_word = expected_word;
        sample.actual_word = actual_word;
    }
}

std::uint8_t run_sdram_lane_diag(const SdramBank bank, memory_probe_sdram_lane_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_lane_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        diag.init_ok = 0U;
        capture_lane_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    diag.init_ok = 1U;
    bool ok = true;
    std::uint32_t mismatch_and = 0xFFFFFFFFU;
    std::uint32_t mismatch_or = 0U;
    auto* const word = reinterpret_cast<volatile std::uint32_t*>(cfg.base);
    auto* const bytes = reinterpret_cast<volatile std::uint8_t*>(cfg.base);
    auto* const halfwords = reinterpret_cast<volatile std::uint16_t*>(cfg.base);

    for (std::uint32_t lane = 0U; lane < 4U; ++lane) {
        *word = 0U;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base), sizeof(std::uint32_t));
        bytes[lane] = static_cast<std::uint8_t>(0xA5U ^ (lane * 0x11U));
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base), sizeof(std::uint32_t));
        const std::uint32_t expected = static_cast<std::uint32_t>(0xA5U ^ (lane * 0x11U)) << (lane * 8U);
        record_sdram_lane_sample(diag, ok, mismatch_or, mismatch_and, 8U, lane, 0xA5U ^ (lane * 0x11U), expected, *word);
    }

    for (std::uint32_t lane = 0U; lane < 2U; ++lane) {
        *word = 0U;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base), sizeof(std::uint32_t));
        const std::uint16_t value = static_cast<std::uint16_t>(0x5AA5U ^ (lane * 0x1111U));
        halfwords[lane] = value;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base), sizeof(std::uint32_t));
        const std::uint32_t expected = static_cast<std::uint32_t>(value) << (lane * 16U);
        record_sdram_lane_sample(diag, ok, mismatch_or, mismatch_and, 16U, lane * 2U, value, expected, *word);
    }

    constexpr std::array<std::uint32_t, 4> word_patterns = {
        0x000000FFU,
        0x0000FF00U,
        0x00FF0000U,
        0xFF000000U,
    };
    for (const auto pattern : word_patterns) {
        *word = pattern;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base), sizeof(std::uint32_t));
        record_sdram_lane_sample(diag, ok, mismatch_or, mismatch_and, 32U, 0U, pattern, pattern, *word);
    }

    *word = 0U;
    sync_external_writes();
    clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base), sizeof(std::uint32_t));

    diag.mismatch_or = mismatch_or;
    diag.mismatch_and = ok ? 0U : mismatch_and;
    diag.ok = ok ? 1U : 0U;
    capture_lane_diag_regs(bank, diag);
    *out = diag;
    return diag.ok;
}

std::uint8_t run_sdram_repeat_diag(const SdramBank bank, memory_probe_sdram_repeat_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_repeat_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;
    diag.read_count = 8U;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        diag.init_ok = 0U;
        capture_repeat_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    const std::uint32_t size = cfg.profile->info.size_bytes;
    const std::uint32_t tail = (size >= sizeof(std::uint32_t)) ? ((size - sizeof(std::uint32_t)) & ~0x3U) : 0U;
    const std::array<std::uint32_t, 8> offsets = {
        0U,
        4U,
        8U,
        0x00000100U,
        0x00010000U,
        0x00100000U,
        0x01000000U,
        tail,
    };

    diag.init_ok = 1U;
    bool ok = true;
    std::uint32_t mismatch_and = 0xFFFFFFFFU;
    std::uint32_t mismatch_or = 0U;
    std::uint32_t sample_count = 0U;

    for (const auto offset : offsets) {
        if ((offset > tail) || (sample_count >= (sizeof(diag.samples) / sizeof(diag.samples[0])))) {
            continue;
        }
        auto* const word = reinterpret_cast<volatile std::uint32_t*>(cfg.base + offset);
        auto& sample = diag.samples[sample_count++];
        sample.offset = offset;
        sample.expected = 0x6D390000U ^ offset ^ (sample_count * 0x01010101U);
        *word = sample.expected;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset), sizeof(std::uint32_t));

        for (std::uint32_t read = 0U; read < diag.read_count; ++read) {
            invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + offset), sizeof(std::uint32_t));
            sample.reads[read] = *word;
            const std::uint32_t mismatch = sample.expected ^ sample.reads[read];
            if (mismatch != 0U) {
                ok = false;
                mismatch_or |= mismatch;
                mismatch_and &= mismatch;
            }
        }
    }

    for (std::uint32_t index = 0U; index < sample_count; ++index) {
        auto* const word = reinterpret_cast<volatile std::uint32_t*>(cfg.base + diag.samples[index].offset);
        *word = 0U;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + diag.samples[index].offset),
                                      sizeof(std::uint32_t));
    }

    diag.sample_count = static_cast<std::uint8_t>(sample_count);
    diag.mismatch_or = mismatch_or;
    diag.mismatch_and = ok ? 0U : mismatch_and;
    diag.ok = ok ? 1U : 0U;
    capture_repeat_diag_regs(bank, diag);
    *out = diag;
    return diag.ok;
}

std::uint8_t run_sdram_locate_diag(const SdramBank bank, memory_probe_sdram_locate_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_locate_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;

    memory_probe_storage_poll();
    reset_sdram_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        diag.init_ok = 0U;
        capture_locate_diag_regs(bank, diag);
        *out = diag;
        return 0U;
    }

    constexpr std::uint32_t kSearchWords = 33U;
    constexpr std::uint32_t kCenterWord = kSearchWords / 2U;
    constexpr std::array<std::uint32_t, 8> offsets = {
        0U,
        4U,
        8U,
        0x00000100U,
        0x00010000U,
        0x00100000U,
        0x01000000U,
        0x01FFFFFCU,
    };

    diag.init_ok = 1U;
    bool ok = true;
    std::uint32_t sample_count = 0U;
    const std::uint32_t size = cfg.profile->info.size_bytes;
    const std::uint32_t window_bytes = kSearchWords * sizeof(std::uint32_t);
    const std::uint32_t tail = (size >= sizeof(std::uint32_t)) ? ((size - sizeof(std::uint32_t)) & ~0x3U) : 0U;
    const std::uint32_t last_window_start =
        (size > window_bytes) ? ((size - window_bytes) & ~0x3U) : 0U;

    for (const auto write_offset : offsets) {
        if ((write_offset > tail) || (sample_count >= (sizeof(diag.samples) / sizeof(diag.samples[0])))) {
            break;
        }
        std::uint32_t start_offset = (write_offset >= (kCenterWord * sizeof(std::uint32_t)))
            ? (write_offset - (kCenterWord * sizeof(std::uint32_t)))
            : 0U;
        if (start_offset > last_window_start) {
            start_offset = last_window_start;
        }
        auto* const window = reinterpret_cast<volatile std::uint32_t*>(cfg.base + start_offset);
        for (std::uint32_t index = 0U; index < kSearchWords; ++index) {
            window[index] = 0U;
        }
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + start_offset),
                                      kSearchWords * sizeof(std::uint32_t));

        const std::uint32_t expected = 0x9B5D0000U ^ write_offset ^ (sample_count * 0x01010101U);
        auto* const write_word = reinterpret_cast<volatile std::uint32_t*>(cfg.base + write_offset);
        *write_word = expected;
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + start_offset),
                                      kSearchWords * sizeof(std::uint32_t));

        auto& sample = diag.samples[sample_count++];
        sample.write_offset = write_offset;
        sample.expected = expected;
        sample.hit_offset = 0xFFFFFFFFU;
        for (std::uint32_t index = 0U; index < kSearchWords; ++index) {
            invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + start_offset + (index * sizeof(std::uint32_t))),
                                    sizeof(std::uint32_t));
            const std::uint32_t actual = window[index];
            if (actual == expected) {
                if (sample.hit_count == 0U) {
                    sample.hit_offset = start_offset + (index * sizeof(std::uint32_t));
                }
                ++sample.hit_count;
            }
        }

        const std::uint32_t self_word_index = (write_offset - start_offset) / sizeof(std::uint32_t);
        const auto read_neighbor = [&](const std::int32_t delta) -> std::uint32_t {
            const std::int32_t index = static_cast<std::int32_t>(self_word_index) + delta;
            if ((index < 0) || (index >= static_cast<std::int32_t>(kSearchWords))) {
                return 0xFFFFFFFFU;
            }
            return window[static_cast<std::uint32_t>(index)];
        };
        sample.actual_minus_2 = read_neighbor(-2);
        sample.actual_minus_1 = read_neighbor(-1);
        sample.actual_self = read_neighbor(0);
        sample.actual_plus_1 = read_neighbor(1);
        sample.actual_plus_2 = read_neighbor(2);

        if ((sample.hit_count != 1U) || (sample.hit_offset != write_offset) || (sample.actual_self != expected)) {
            ok = false;
        }

        for (std::uint32_t index = 0U; index < kSearchWords; ++index) {
            window[index] = 0U;
        }
        sync_external_writes();
        clean_invalidate_dcache_range(reinterpret_cast<void*>(cfg.base + start_offset),
                                      kSearchWords * sizeof(std::uint32_t));
    }

    diag.sample_count = static_cast<std::uint8_t>(sample_count);
    diag.ok = ok ? 1U : 0U;
    capture_locate_diag_regs(bank, diag);
    *out = diag;
    return diag.ok;
}

void capture_timing_diag_regs(const SdramBank bank, memory_probe_sdram_timing_sample_t& sample) {
    sample.fmc_sdsr = FMC_Bank5_6_R->SDSR;
    if (bank == SdramBank::bank2) {
        sample.fmc_sdcr = FMC_Bank5_6_R->SDCR[1];
        sample.fmc_sdtr = FMC_Bank5_6_R->SDTR[1];
    } else {
        sample.fmc_sdcr = FMC_Bank5_6_R->SDCR[0];
        sample.fmc_sdtr = FMC_Bank5_6_R->SDTR[0];
    }
}

std::uint8_t run_sdram_timing_sweep(const SdramBank bank, memory_probe_sdram_timing_diag_t* out) {
    if (out == nullptr) {
        return 0U;
    }
    memory_probe_sdram_timing_diag_t diag{};
    const auto& cfg = sdram_config(bank);
    diag.base = cfg.base;
    bool any_ok = false;
    std::uint32_t sample_count = 0U;

    for (std::uint32_t index = 0U;
         (index < kSdramTimingPresets.size()) && (sample_count < kSdramTimingSweepMaxSamples);
         ++index) {
        const auto& preset = kSdramTimingPresets[index];
        auto& sample = diag.samples[sample_count];
        sample.preset = static_cast<std::uint8_t>(index);
        sample.sdclock_period = preset.sdclock_period;
        sample.cas_latency = preset.cas_latency;
        sample.read_burst = preset.read_burst;
        sample.read_pipe_delay = preset.read_pipe_delay;
        sample.mode_reg = preset.mode_reg;

        memory_probe_storage_poll();
        reset_sdram_result(bank);
        const HAL_StatusTypeDef fmc_status = init_sdram_fmc_with_preset(bank, preset);
        set_sdram_init_status(bank, fmc_status);
        snapshot_fmc_regs();
        if (fmc_status != HAL_OK) {
            sample.init_ok = 0U;
            capture_timing_diag_regs(bank, sample);
            ++sample_count;
            continue;
        }

        const HAL_StatusTypeDef seq_status = init_sdram_sequence_with_mode_reg(bank, preset.mode_reg);
        set_sdram_init_status(bank, seq_status);
        snapshot_fmc_regs();
        if (seq_status != HAL_OK) {
            sample.init_ok = 0U;
            capture_timing_diag_regs(bank, sample);
            ++sample_count;
            continue;
        }

        sample.init_ok = 1U;
        const bool ok = test_sdram_window(bank, SdramWindowKind::smoke, 0U);
        sample.smoke_ok = ok ? 1U : 0U;
        sample.ok = ok ? 1U : 0U;
        any_ok = ok || any_ok;

        const memory_storage_state_t state = g_state;
        if (bank == SdramBank::bank2) {
            sample.first_error_addr = static_cast<std::uint32_t>(state.sdram2_first_error_addr);
            sample.expected = state.sdram2_first_expected;
            sample.actual = state.sdram2_first_actual;
        } else {
            sample.first_error_addr = static_cast<std::uint32_t>(state.sdram1_first_error_addr);
            sample.expected = state.sdram1_first_expected;
            sample.actual = state.sdram1_first_actual;
        }
        capture_timing_diag_regs(bank, sample);
        ++sample_count;
    }

    diag.sample_count = static_cast<std::uint8_t>(sample_count);
    diag.ok = any_ok ? 1U : 0U;
    *out = diag;
    return diag.ok;
}

std::uint8_t run_sdram_smoke_force(const SdramBank bank) {
    memory_probe_storage_poll();
    reset_sdram_result(bank);

    if (!init_sdram_bank_for_probe(bank)) {
        return 0U;
    }

    bool ok = true;
    for (const auto offset : sdram_smoke_window_offsets(*sdram_config(bank).profile)) {
        ok = test_sdram_window(bank, SdramWindowKind::smoke, offset) && ok;
    }

    set_sdram_smoke_status(bank, ok);
    snapshot_fmc_regs();
    return ok ? 1U : 0U;
}

std::uint8_t run_sdram_smoke(const SdramBank bank) {
    memory_probe_storage_poll();
    const std::uint8_t power_good =
        (bank == SdramBank::bank2) ? g_state.sdram2_power_good : g_state.sdram1_power_good;
    if (power_good == 0U) {
        reset_sdram_result(bank);
        set_sdram_init_status(bank, HAL_ERROR);
        snapshot_fmc_regs();
        return 0U;
    }
    return run_sdram_smoke_force(bank);
}

std::uint8_t run_sdram_verify(const SdramBank bank) {
    memory_probe_storage_poll();
    const std::uint8_t power_good =
        (bank == SdramBank::bank2) ? g_state.sdram2_power_good : g_state.sdram1_power_good;
    if (power_good == 0U) {
        reset_sdram_verify_result(bank);
        set_sdram_verify_status(bank, false);
        set_sdram_init_status(bank, HAL_ERROR);
        snapshot_fmc_regs();
        return 0U;
    }

    reset_sdram_verify_result(bank);
    if (!init_sdram_bank_for_probe(bank)) {
        set_sdram_verify_status(bank, false);
        return 0U;
    }

    bool ok = true;
    for (const auto offset : sdram_verify_window_offsets(*sdram_config(bank).profile)) {
        ok = test_sdram_window(bank, SdramWindowKind::verify, offset) && ok;
    }
    set_sdram_verify_status(bank, ok);
    snapshot_fmc_regs();
    return ok ? 1U : 0U;
}

HAL_StatusTypeDef remember_qspi_status(const HAL_StatusTypeDef status) {
    g_state.qspi_last_hal_status = static_cast<std::uint32_t>(status);
    snapshot_qspi_regs();
    return status;
}

void clear_qspi_results() {
    g_state.qspi_attempted = 1U;
    g_state.qspi_init_ok = 0U;
    g_state.qspi_jedec_ok = 0U;
    g_state.qspi_status_ok = 0U;
    g_state.qspi_read_ok = 0U;
    g_state.qspi_wip = 0U;
    g_state.qspi_wel = 0U;
    g_state.qspi_last_hal_status = 0U;
    g_state.qspi_jedec_id[0] = 0U;
    g_state.qspi_jedec_id[1] = 0U;
    g_state.qspi_jedec_id[2] = 0U;
    g_state.qspi_status1 = 0U;
    g_state.qspi_status2 = 0U;
    g_state.qspi_last_read_len = 0U;
    std::memset(g_state.qspi_last_read_data, 0, sizeof(g_state.qspi_last_read_data));
    g_state.qspi_last_cmd = 0U;
    g_state.qspi_last_addr = 0U;
}

bool ensure_qspi_ready() {
    (void)HAL_QSPI_DeInit(&hqspi);
    hqspi.Instance = QUADSPI;
    hqspi.Init.ClockPrescaler = kQspiClockPrescaler;
    hqspi.Init.FifoThreshold = 4U;
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi.Init.FlashSize = kQspiFlashSize;
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_4_CYCLE;
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;

    const HAL_StatusTypeDef status = remember_qspi_status(HAL_QSPI_Init(&hqspi));
    g_state.qspi_init_ok = (status == HAL_OK) ? 1U : 0U;
    return g_state.qspi_init_ok != 0U;
}

bool qspi_read_register(const std::uint8_t command, std::uint8_t* data, const std::uint32_t size) {
    QSPI_CommandTypeDef cmd{};
    if ((data == nullptr) || (size == 0U)) {
        return false;
    }

    cmd.Instruction = command;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode = QSPI_DATA_1_LINE;
    cmd.NbData = size;
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    g_state.qspi_last_cmd = command;
    g_state.qspi_last_addr = 0U;

    if (remember_qspi_status(HAL_QSPI_Command(&hqspi, &cmd, kQspiTimeoutMs)) != HAL_OK) {
        return false;
    }
    return remember_qspi_status(HAL_QSPI_Receive(&hqspi, data, kQspiTimeoutMs)) == HAL_OK;
}

bool qspi_read_data_internal(const std::uint32_t address, std::uint8_t* data, const std::uint32_t size) {
    QSPI_CommandTypeDef cmd{};
    if ((data == nullptr) || (size == 0U)) {
        return false;
    }

    cmd.Instruction = kQspiCmdReadData;
    cmd.Address = address;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode = QSPI_ADDRESS_1_LINE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode = QSPI_DATA_1_LINE;
    cmd.NbData = size;
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    g_state.qspi_last_cmd = kQspiCmdReadData;
    g_state.qspi_last_addr = address;

    if (remember_qspi_status(HAL_QSPI_Command(&hqspi, &cmd, kQspiTimeoutMs)) != HAL_OK) {
        return false;
    }
    return remember_qspi_status(HAL_QSPI_Receive(&hqspi, data, kQspiTimeoutMs)) == HAL_OK;
}

void capture_qspi_read_bytes(const std::uint8_t* data, const std::uint32_t size) {
    const std::uint32_t capture = (size < kQspiReadCaptureBytes) ? size : kQspiReadCaptureBytes;
    std::memset(g_state.qspi_last_read_data, 0, sizeof(g_state.qspi_last_read_data));
    if ((data != nullptr) && (capture > 0U)) {
        std::memcpy(g_state.qspi_last_read_data, data, capture);
    }
    g_state.qspi_last_read_len = static_cast<std::uint8_t>(capture);
}

bool qspi_jedec_valid(const std::uint8_t* jedec) {
    if (jedec == nullptr) {
        return false;
    }
    const bool all_zero = (jedec[0] == 0x00U) && (jedec[1] == 0x00U) && (jedec[2] == 0x00U);
    const bool all_ff = (jedec[0] == 0xFFU) && (jedec[1] == 0xFFU) && (jedec[2] == 0xFFU);
    return !(all_zero || all_ff);
}

} // namespace

void memory_probe_storage_init() {
    std::memset(&g_state, 0, sizeof(g_state));
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
    refresh_profile_rates();
    sync_profile_state(SdramBank::bank1);
    sync_profile_state(SdramBank::bank2);
    snapshot_power_state();
    snapshot_fmc_regs();
    snapshot_qspi_regs();
}

void memory_probe_storage_poll() {
    refresh_profile_rates();
    sync_profile_state(SdramBank::bank1);
    sync_profile_state(SdramBank::bank2);
    snapshot_power_state();
    snapshot_fmc_regs();
    snapshot_qspi_regs();
}

std::uint8_t memory_probe_configure_sdram_mpu_normal() {
    MPU_Region_InitTypeDef region{};

    HAL_MPU_Disable();

    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER5;
    region.BaseAddress = kSdram1Base;
    region.Size = MPU_REGION_SIZE_32MB;
    region.SubRegionDisable = 0x00U;
    region.TypeExtField = MPU_TEX_LEVEL1;
    region.AccessPermission = MPU_REGION_FULL_ACCESS;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    region.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    region.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    region.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&region);

    region.Number = MPU_REGION_NUMBER6;
    region.BaseAddress = kSdram2Base;
    HAL_MPU_ConfigRegion(&region);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    __DSB();
    __ISB();
    memory_probe_storage_poll();
    return 1U;
}

memory_storage_state_t memory_probe_storage_state() {
    memory_probe_storage_poll();
    return g_state;
}

std::uint8_t memory_probe_sdram1_smoke() {
    return run_sdram_smoke(SdramBank::bank1);
}

std::uint8_t memory_probe_sdram1_smoke_force() {
    return run_sdram_smoke_force(SdramBank::bank1);
}

std::uint8_t memory_probe_sdram2_smoke() {
    return run_sdram_smoke(SdramBank::bank2);
}

std::uint8_t memory_probe_sdram2_smoke_force() {
    return run_sdram_smoke_force(SdramBank::bank2);
}

std::uint8_t memory_probe_sdram1_verify() {
    return run_sdram_verify(SdramBank::bank1);
}

std::uint8_t memory_probe_sdram2_verify() {
    return run_sdram_verify(SdramBank::bank2);
}

std::uint8_t memory_probe_sdram1_bus_diag(memory_probe_sdram_bus_diag_t* diag) {
    return run_sdram_bus_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_bus_diag(memory_probe_sdram_bus_diag_t* diag) {
    return run_sdram_bus_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_spot_diag(memory_probe_sdram_spot_diag_t* diag) {
    return run_sdram_spot_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_spot_diag(memory_probe_sdram_spot_diag_t* diag) {
    return run_sdram_spot_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_alias_diag(memory_probe_sdram_alias_diag_t* diag) {
    return run_sdram_alias_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_alias_diag(memory_probe_sdram_alias_diag_t* diag) {
    return run_sdram_alias_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_addr_diag(memory_probe_sdram_addr_diag_t* diag) {
    return run_sdram_addr_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_addr_diag(memory_probe_sdram_addr_diag_t* diag) {
    return run_sdram_addr_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_lane_diag(memory_probe_sdram_lane_diag_t* diag) {
    return run_sdram_lane_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_lane_diag(memory_probe_sdram_lane_diag_t* diag) {
    return run_sdram_lane_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_repeat_diag(memory_probe_sdram_repeat_diag_t* diag) {
    return run_sdram_repeat_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_repeat_diag(memory_probe_sdram_repeat_diag_t* diag) {
    return run_sdram_repeat_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_locate_diag(memory_probe_sdram_locate_diag_t* diag) {
    return run_sdram_locate_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_locate_diag(memory_probe_sdram_locate_diag_t* diag) {
    return run_sdram_locate_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_wait_sequence_bus_diag(memory_probe_sdram_bus_diag_t* diag) {
    return run_sdram_wait_sequence_bus_diag(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_wait_sequence_bus_diag(memory_probe_sdram_bus_diag_t* diag) {
    return run_sdram_wait_sequence_bus_diag(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_sdram1_timing_sweep(memory_probe_sdram_timing_diag_t* diag) {
    return run_sdram_timing_sweep(SdramBank::bank1, diag);
}

std::uint8_t memory_probe_sdram2_timing_sweep(memory_probe_sdram_timing_diag_t* diag) {
    return run_sdram_timing_sweep(SdramBank::bank2, diag);
}

std::uint8_t memory_probe_qspi_probe() {
    memory_probe_storage_poll();
    if (g_state.qspi_power_good == 0U) {
        clear_qspi_results();
        g_state.qspi_last_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        snapshot_qspi_regs();
        return 0U;
    }
    return memory_probe_qspi_probe_force();
}

std::uint8_t memory_probe_qspi_probe_force() {
    std::uint8_t jedec[3] = {};
    std::uint8_t status1 = 0U;
    std::uint8_t status2 = 0U;
    std::uint8_t data[kQspiReadCaptureBytes] = {};

    memory_probe_storage_poll();
    clear_qspi_results();
    if (!ensure_qspi_ready()) {
        return 0U;
    }

    if (qspi_read_register(kQspiCmdReadJedec, jedec, sizeof(jedec))) {
        g_state.qspi_jedec_id[0] = jedec[0];
        g_state.qspi_jedec_id[1] = jedec[1];
        g_state.qspi_jedec_id[2] = jedec[2];
        g_state.qspi_jedec_ok = qspi_jedec_valid(jedec) ? 1U : 0U;
    }
    if (qspi_read_register(kQspiCmdReadStatus1, &status1, 1U)) {
        g_state.qspi_status1 = status1;
        g_state.qspi_wip = (status1 & 0x01U) != 0U ? 1U : 0U;
        g_state.qspi_wel = (status1 & 0x02U) != 0U ? 1U : 0U;
        g_state.qspi_status_ok = 1U;
    }
    if (qspi_read_register(kQspiCmdReadStatus2, &status2, 1U)) {
        g_state.qspi_status2 = status2;
    }
    if (qspi_read_data_internal(0U, data, sizeof(data))) {
        capture_qspi_read_bytes(data, sizeof(data));
        g_state.qspi_read_ok = 1U;
    }
    snapshot_qspi_regs();
    return g_state.qspi_jedec_ok;
}

std::uint8_t memory_probe_qspi_read(const std::uint32_t addr, std::uint8_t* data, const std::uint32_t size) {
    memory_probe_storage_poll();
    if ((data == nullptr) || (size == 0U) || (g_state.qspi_power_good == 0U)) {
        g_state.qspi_last_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
        snapshot_qspi_regs();
        return 0U;
    }
    if ((g_state.qspi_init_ok == 0U) && !ensure_qspi_ready()) {
        return 0U;
    }
    if (!qspi_read_data_internal(addr, data, size)) {
        return 0U;
    }
    capture_qspi_read_bytes(data, size);
    g_state.qspi_read_ok = 1U;
    snapshot_qspi_regs();
    return 1U;
}

const memory_probe_sdram_profile_t* memory_probe_sdram1_profile() {
    return &g_sdram1.profile->info;
}

const memory_probe_sdram_profile_t* memory_probe_sdram2_profile() {
    return &g_sdram2.profile->info;
}
