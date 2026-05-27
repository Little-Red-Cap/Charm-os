#include "storage.h"

#include "power.h"
#include "sdmmc.h"
#include "stm32h7xx_hal.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>

namespace {

constexpr std::uint32_t kTimeoutMs = 1000U;
constexpr std::uint32_t kStorageClockDiv = 16U;
constexpr std::uint32_t kTransferRetries = 3U;
constexpr std::uint32_t kRetryDelayMs = 2U;
constexpr std::uint32_t kBusWidthUnknown = 0U;
constexpr std::uint32_t kBusWidth1 = 1U;
constexpr std::uint32_t kBusWidth4 = 4U;
constexpr std::uint32_t kBusWidth8 = 8U;

h747_storage_state_t g_state{};
std::uint32_t g_block_size{512U};
std::uint32_t g_block_count{0U};
std::uint32_t g_partition_lba{0U};
std::uint32_t g_exposed_block_count{0U};
bool g_ready{false};

void snapshot_regs() noexcept;

std::uint32_t bus_width_value(const std::uint32_t hal_width) noexcept {
    switch (hal_width) {
    case SDMMC_BUS_WIDE_8B:
        return kBusWidth8;
    case SDMMC_BUS_WIDE_4B:
        return kBusWidth4;
    case SDMMC_BUS_WIDE_1B:
        return kBusWidth1;
    default:
        return kBusWidthUnknown;
    }
}

HAL_StatusTypeDef select_bus_width(const std::uint32_t hal_width,
                                   std::uint32_t& status_slot) noexcept {
    const auto status = HAL_MMC_ConfigWideBusOperation(&hmmc1, hal_width);
    status_slot = static_cast<std::uint32_t>(status);
    if (status == HAL_OK) {
        g_state.selected_bus_width = bus_width_value(hal_width);
    }
    snapshot_regs();
    return status;
}

void snapshot_regs() noexcept {
    g_state.card_state = static_cast<std::uint32_t>(HAL_MMC_GetCardState(&hmmc1));
    if (hmmc1.Instance != nullptr) {
        g_state.clkcr = static_cast<std::uint32_t>(hmmc1.Instance->CLKCR);
        g_state.sta = static_cast<std::uint32_t>(hmmc1.Instance->STA);
        g_state.resp1 = static_cast<std::uint32_t>(hmmc1.Instance->RESP1);
    }
    g_state.last_error = static_cast<std::uint32_t>(HAL_MMC_GetError(&hmmc1));
}

bool wait_transfer_state(std::uint32_t lba, std::uint32_t count) noexcept {
    const auto start = HAL_GetTick();
    while (HAL_MMC_GetCardState(&hmmc1) != HAL_MMC_CARD_TRANSFER) {
        if ((HAL_GetTick() - start) > kTimeoutMs) {
            ++g_state.wait_timeout_count;
            g_state.last_lba = lba;
            g_state.last_count = count;
            g_state.last_hal_status = static_cast<std::uint32_t>(HAL_TIMEOUT);
            snapshot_regs();
            return false;
        }
    }
    return true;
}

std::uint32_t le32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0])
        | (static_cast<std::uint32_t>(p[1]) << 8)
        | (static_cast<std::uint32_t>(p[2]) << 16)
        | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t le64(const std::uint8_t* p) noexcept {
    const std::uint64_t lo = le32(p);
    const std::uint64_t hi = le32(p + 4);
    return lo | (hi << 32);
}

bool is_pow2(std::uint8_t value) noexcept {
    return value != 0U && ((value & (value - 1U)) == 0U);
}

bool is_fat_boot_sector(const std::uint8_t* buf) noexcept {
    if (buf == nullptr) return false;
    if (buf[510] != 0x55U || buf[511] != 0xAAU) return false;
    const auto bps = static_cast<std::uint16_t>(buf[11])
        | (static_cast<std::uint16_t>(buf[12]) << 8);
    if (!(bps == 512U || bps == 1024U || bps == 2048U || bps == 4096U)) return false;
    const auto spc = buf[13];
    if (spc == 0U || !is_pow2(spc)) return false;
    const auto reserved = static_cast<std::uint16_t>(buf[14])
        | (static_cast<std::uint16_t>(buf[15]) << 8);
    if (reserved == 0U) return false;
    const auto fats = buf[16];
    if (!(fats == 1U || fats == 2U)) return false;
    const auto total16 = static_cast<std::uint16_t>(buf[19])
        | (static_cast<std::uint16_t>(buf[20]) << 8);
    const auto total32 = le32(buf + 32);
    if (total16 == 0U && total32 == 0U) return false;
    const auto media = buf[21];
    if (media < 0xF0U) return false;
    const auto fatsz16 = static_cast<std::uint16_t>(buf[22])
        | (static_cast<std::uint16_t>(buf[23]) << 8);
    const auto fatsz32 = le32(buf + 36);
    if (fatsz16 == 0U && fatsz32 == 0U) return false;
    return std::memcmp(buf + 0x36, "FAT", 3) == 0
        || std::memcmp(buf + 0x52, "FAT", 3) == 0;
}

bool read_raw_block(std::uint32_t lba, std::span<std::uint8_t> out) noexcept {
    if (out.size() != 512U) return false;
    const auto status = HAL_MMC_ReadBlocks(&hmmc1, out.data(), lba, 1U, kTimeoutMs);
    g_state.last_hal_status = static_cast<std::uint32_t>(status);
    g_state.last_lba = lba;
    g_state.last_count = 1U;
    if (status != HAL_OK || !wait_transfer_state(lba, 1U)) {
        snapshot_regs();
        return false;
    }
    snapshot_regs();
    return true;
}

bool probe_lba0_readable() noexcept {
    alignas(4) std::array<std::uint8_t, 512> buf{};
    return read_raw_block(0U, std::span<std::uint8_t>(buf.data(), buf.size()));
}

bool ensure_readable_bus() noexcept {
    if (probe_lba0_readable()) {
        return true;
    }

    if (g_state.selected_bus_width == kBusWidth1) {
        return false;
    }

    const auto fallback_status = select_bus_width(SDMMC_BUS_WIDE_1B, g_state.wide_status_1);
    g_state.last_hal_status = static_cast<std::uint32_t>(fallback_status);
    if (fallback_status != HAL_OK) {
        return false;
    }

    return probe_lba0_readable();
}

bool probe_fat_lba(std::uint32_t lba, std::uint32_t& total_sectors) noexcept {
    alignas(4) std::array<std::uint8_t, 512> buf{};
    if (!read_raw_block(lba, std::span<std::uint8_t>(buf.data(), buf.size()))) return false;
    if (!is_fat_boot_sector(buf.data())) return false;
    const auto total16 = static_cast<std::uint16_t>(buf[19])
        | (static_cast<std::uint16_t>(buf[20]) << 8);
    const auto total32 = le32(buf.data() + 32);
    total_sectors = (total32 != 0U) ? total32 : total16;
    return total_sectors != 0U;
}

bool is_zero_guid(const std::uint8_t* p) noexcept {
    for (int i = 0; i < 16; ++i) {
        if (p[i] != 0U) return false;
    }
    return true;
}

std::uint32_t detect_partition_lba() noexcept {
    alignas(4) std::array<std::uint8_t, 512> buf{};
    if (!read_raw_block(0U, std::span<std::uint8_t>(buf.data(), buf.size()))) return 0U;
    const bool lba0_fat = is_fat_boot_sector(buf.data());
    if (buf[510] != 0x55U || buf[511] != 0xAAU) return lba0_fat ? 0U : 0U;

    const std::uint8_t* entries = &buf[0x1BE];
    for (int i = 0; i < 4; ++i) {
        const std::uint8_t* ent = entries + (i * 16);
        const auto type = ent[4];
        const auto first_lba = le32(ent + 8);
        const auto sectors = le32(ent + 12);
        if (type == 0x00U) continue;
        if (type == 0xEEU) {
            if (!read_raw_block(1U, std::span<std::uint8_t>(buf.data(), buf.size()))) return first_lba;
            if (std::memcmp(buf.data(), "EFI PART", 8) != 0) return first_lba;
            const auto entries_lba = le64(buf.data() + 72);
            const auto entry_size = le32(buf.data() + 84);
            if (entries_lba > 0xFFFFFFFFULL || entry_size < 56U) return first_lba;
            if (!read_raw_block(static_cast<std::uint32_t>(entries_lba),
                                std::span<std::uint8_t>(buf.data(), buf.size()))) {
                return first_lba;
            }
            if (is_zero_guid(buf.data())) return first_lba;
            const auto gpt_first = le64(buf.data() + 32);
            return (gpt_first <= 0xFFFFFFFFULL) ? static_cast<std::uint32_t>(gpt_first) : first_lba;
        }
        if (first_lba != 0U && sectors != 0U) return first_lba;
    }
    return lba0_fat ? 0U : 0U;
}

bool init_emmc() noexcept {
    if (g_ready) return true;
    g_state.attempted = 1U;
    (void)power_apply_profile(POWER_PROFILE_STORAGE_STAGE_A);

    __HAL_RCC_SDMMC1_FORCE_RESET();
    __HAL_RCC_SDMMC1_RELEASE_RESET();
    HAL_Delay(2U);

    hmmc1.Instance = SDMMC1;
    hmmc1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hmmc1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    // Keep HAL_MMC_Init on the conservative 1-bit path. Wider bus modes are
    // negotiated explicitly below so a failed 8-bit switch can fall back cleanly.
    hmmc1.Init.BusWide = SDMMC_BUS_WIDE_1B;
    hmmc1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    // Polling writes can underrun the SDMMC FIFO at full speed during bring-up.
    // Keep the MSC/eMMC path conservative until DMA or a wider bus is stable.
    hmmc1.Init.ClockDiv = kStorageClockDiv;
    hmmc1.State = HAL_MMC_STATE_RESET;

    const auto status = HAL_MMC_Init(&hmmc1);
    g_state.init_status = static_cast<std::uint32_t>(status);
    g_state.last_hal_status = static_cast<std::uint32_t>(status);
    snapshot_regs();
    if (status != HAL_OK) return false;

    g_state.selected_bus_width = kBusWidth1;
    auto wide_status = HAL_OK;
    g_state.wide_status_8 = static_cast<std::uint32_t>(HAL_ERROR);
    g_state.wide_status_4 = static_cast<std::uint32_t>(HAL_ERROR);
    g_state.wide_status_1 = static_cast<std::uint32_t>(HAL_OK);
    g_state.last_hal_status = static_cast<std::uint32_t>(wide_status);
    snapshot_regs();
    if (wide_status != HAL_OK) return false;

    HAL_MMC_CardInfoTypeDef info{};
    const auto info_status = HAL_MMC_GetCardInfo(&hmmc1, &info);
    g_state.last_hal_status = static_cast<std::uint32_t>(info_status);
    snapshot_regs();
    if (info_status != HAL_OK) return false;

    if (!ensure_readable_bus()) return false;

    g_block_size = (info.LogBlockSize != 0U) ? info.LogBlockSize : 512U;
    g_block_count = info.LogBlockNbr;
    g_partition_lba = detect_partition_lba();
    std::uint32_t fat_total = 0U;
    g_state.fat_probe_ok = probe_fat_lba(g_partition_lba, fat_total) ? 1U : 0U;
    if (g_state.fat_probe_ok == 0U && g_partition_lba != 0U) {
        g_partition_lba = 0U;
        g_state.fat_probe_ok = probe_fat_lba(0U, fat_total) ? 1U : 0U;
    }
    g_state.partition_auto = (g_partition_lba != 0U) ? 1U : 0U;

    g_exposed_block_count = g_block_count;
    if (g_partition_lba < g_exposed_block_count) {
        g_exposed_block_count -= g_partition_lba;
    }
    if (fat_total != 0U && fat_total < g_exposed_block_count) {
        g_exposed_block_count = fat_total;
    }

    g_state.block_size = g_block_size;
    g_state.block_count = g_block_count;
    g_state.partition_lba = g_partition_lba;
    g_state.exposed_block_count = g_exposed_block_count;
    g_state.block_device_ready = 1U;
    g_state.ready = 1U;
    g_state.initialized = 1U;
    g_ready = true;
    return true;
}

} // namespace

extern "C" void h747_storage_init(void) {
    g_state = {};
    g_block_size = 512U;
    g_block_count = 0U;
    g_partition_lba = 0U;
    g_exposed_block_count = 0U;
    g_ready = false;
    (void)init_emmc();
}

extern "C" h747_storage_state_t h747_storage_state(void) {
    snapshot_regs();
    return g_state;
}

extern "C" std::uint32_t h747_storage_block_size(void) {
    return g_ready ? g_block_size : 0U;
}

extern "C" std::uint32_t h747_storage_raw_block_count(void) {
    return g_ready ? g_block_count : 0U;
}

extern "C" std::uint32_t h747_storage_block_count(void) {
    return g_ready ? g_exposed_block_count : 0U;
}

extern "C" std::uint32_t h747_storage_partition_lba(void) {
    return g_ready ? g_partition_lba : 0U;
}

extern "C" std::uint8_t h747_storage_read_blocks(std::uint32_t lba,
                                                  std::uint8_t* data,
                                                  std::uint32_t bytes) {
    if (!g_ready || data == nullptr || bytes == 0U || g_block_size == 0U
        || (bytes % g_block_size) != 0U) {
        ++g_state.read_fail_count;
        return 0U;
    }
    const auto count = bytes / g_block_size;
    if (lba > g_exposed_block_count || count > (g_exposed_block_count - lba)) {
        ++g_state.read_fail_count;
        return 0U;
    }
    const auto start_lba = lba + g_partition_lba;
    for (std::uint32_t attempt = 0U; attempt < kTransferRetries; ++attempt) {
        if (!wait_transfer_state(start_lba, count)) {
            HAL_Delay(kRetryDelayMs);
            continue;
        }
        __HAL_MMC_CLEAR_FLAG(&hmmc1, SDMMC_STATIC_FLAGS);
        const auto status = HAL_MMC_ReadBlocks(&hmmc1, data, start_lba, count, kTimeoutMs);
        g_state.last_hal_status = static_cast<std::uint32_t>(status);
        g_state.last_lba = start_lba;
        g_state.last_count = count;
        if (status == HAL_OK && wait_transfer_state(start_lba, count)) {
            ++g_state.read_count;
            snapshot_regs();
            return 1U;
        }
        snapshot_regs();
        HAL_Delay(kRetryDelayMs);
    }

    ++g_state.read_fail_count;
    snapshot_regs();
    return 0U;
}

extern "C" std::uint8_t h747_storage_read_raw_blocks(std::uint32_t lba,
                                                      std::uint8_t* data,
                                                      std::uint32_t bytes) {
    if (!g_ready || data == nullptr || bytes == 0U || g_block_size == 0U
        || (bytes % g_block_size) != 0U) {
        ++g_state.read_fail_count;
        return 0U;
    }
    const auto count = bytes / g_block_size;
    if (lba > g_block_count || count > (g_block_count - lba)) {
        ++g_state.read_fail_count;
        return 0U;
    }
    for (std::uint32_t attempt = 0U; attempt < kTransferRetries; ++attempt) {
        if (!wait_transfer_state(lba, count)) {
            HAL_Delay(kRetryDelayMs);
            continue;
        }
        __HAL_MMC_CLEAR_FLAG(&hmmc1, SDMMC_STATIC_FLAGS);
        const auto status = HAL_MMC_ReadBlocks(&hmmc1, data, lba, count, kTimeoutMs);
        g_state.last_hal_status = static_cast<std::uint32_t>(status);
        g_state.last_lba = lba;
        g_state.last_count = count;
        if (status == HAL_OK && wait_transfer_state(lba, count)) {
            ++g_state.read_count;
            snapshot_regs();
            return 1U;
        }
        snapshot_regs();
        HAL_Delay(kRetryDelayMs);
    }

    ++g_state.read_fail_count;
    snapshot_regs();
    return 0U;
}

extern "C" std::uint8_t h747_storage_write_blocks(std::uint32_t lba,
                                                   const std::uint8_t* data,
                                                   std::uint32_t bytes) {
    if (!g_ready || data == nullptr || bytes == 0U || g_block_size == 0U
        || (bytes % g_block_size) != 0U) {
        ++g_state.write_fail_count;
        return 0U;
    }
    const auto count = bytes / g_block_size;
    if (lba > g_exposed_block_count || count > (g_exposed_block_count - lba)) {
        ++g_state.write_fail_count;
        return 0U;
    }
    const auto start_lba = lba + g_partition_lba;
    for (std::uint32_t attempt = 0U; attempt < kTransferRetries; ++attempt) {
        if (!wait_transfer_state(start_lba, count)) {
            HAL_Delay(kRetryDelayMs);
            continue;
        }
        __HAL_MMC_CLEAR_FLAG(&hmmc1, SDMMC_STATIC_FLAGS);
        const auto status = HAL_MMC_WriteBlocks(&hmmc1, const_cast<std::uint8_t*>(data), start_lba, count, kTimeoutMs);
        g_state.last_hal_status = static_cast<std::uint32_t>(status);
        g_state.last_lba = start_lba;
        g_state.last_count = count;
        if (status == HAL_OK && wait_transfer_state(start_lba, count)) {
            ++g_state.write_count;
            snapshot_regs();
            return 1U;
        }
        snapshot_regs();
        HAL_Delay(kRetryDelayMs);
    }

    ++g_state.write_fail_count;
    snapshot_regs();
    return 0U;
}

extern "C" std::uint8_t h747_storage_write_raw_blocks(std::uint32_t lba,
                                                       const std::uint8_t* data,
                                                       std::uint32_t bytes) {
    if (!g_ready || data == nullptr || bytes == 0U || g_block_size == 0U
        || (bytes % g_block_size) != 0U) {
        ++g_state.write_fail_count;
        return 0U;
    }
    const auto count = bytes / g_block_size;
    if (lba > g_block_count || count > (g_block_count - lba)) {
        ++g_state.write_fail_count;
        return 0U;
    }
    for (std::uint32_t attempt = 0U; attempt < kTransferRetries; ++attempt) {
        if (!wait_transfer_state(lba, count)) {
            HAL_Delay(kRetryDelayMs);
            continue;
        }
        __HAL_MMC_CLEAR_FLAG(&hmmc1, SDMMC_STATIC_FLAGS);
        const auto status = HAL_MMC_WriteBlocks(&hmmc1, const_cast<std::uint8_t*>(data), lba, count, kTimeoutMs);
        g_state.last_hal_status = static_cast<std::uint32_t>(status);
        g_state.last_lba = lba;
        g_state.last_count = count;
        if (status == HAL_OK && wait_transfer_state(lba, count)) {
            ++g_state.write_count;
            snapshot_regs();
            return 1U;
        }
        snapshot_regs();
        HAL_Delay(kRetryDelayMs);
    }

    ++g_state.write_fail_count;
    snapshot_regs();
    return 0U;
}

extern "C" std::uint8_t h747_storage_flush(void) {
    snapshot_regs();
    return g_ready ? 1U : 0U;
}

extern "C" void SDMMC1_IRQHandler(void) {
    HAL_MMC_IRQHandler(&hmmc1);
}
