#include "qspi_nor.h"

#include "power.h"
#include "quadspi.h"
#include "stm32h7xx_hal.h"

#include <cstdint>

namespace {

constexpr std::uint32_t kTimeoutMs = 1000U;
constexpr std::uint32_t kDefaultCapacityBytes = 16U * 1024U * 1024U;
constexpr std::uint32_t kMaxReadChunk = 256U;
constexpr std::uint32_t kPageSize = 256U;
constexpr std::uint32_t kEraseBlockSize = 4096U;

constexpr std::uint8_t kCmdReadJedecId = 0x9FU;
constexpr std::uint8_t kCmdReadData = 0x03U;
constexpr std::uint8_t kCmdReleasePowerDown = 0xABU;
constexpr std::uint8_t kCmdWriteEnable = 0x06U;
constexpr std::uint8_t kCmdReadStatus1 = 0x05U;
constexpr std::uint8_t kCmdPageProgram = 0x02U;
constexpr std::uint8_t kCmdSectorErase4K = 0x20U;
constexpr std::uint8_t kStatusBusy = 0x01U;
constexpr std::uint8_t kStatusWel = 0x02U;

h747_qspi_nor_state_t g_state{};
bool g_ready = false;
std::uint32_t g_capacity = 0;

void snapshot_regs() noexcept {
    if (hqspi.Instance != nullptr) {
        g_state.dcr = hqspi.Instance->DCR;
        g_state.sr = hqspi.Instance->SR;
        g_state.cr = hqspi.Instance->CR;
    }
    g_state.last_error = HAL_QSPI_GetError(&hqspi);
}

void remember_status(HAL_StatusTypeDef status) noexcept {
    g_state.last_hal_status = static_cast<std::uint32_t>(status);
    snapshot_regs();
}

bool qspi_command(QSPI_CommandTypeDef& command) noexcept {
    const auto status = HAL_QSPI_Command(&hqspi, &command, kTimeoutMs);
    remember_status(status);
    return status == HAL_OK;
}

bool qspi_receive(std::uint8_t* data) noexcept {
    const auto status = HAL_QSPI_Receive(&hqspi, data, kTimeoutMs);
    remember_status(status);
    return status == HAL_OK;
}

bool qspi_transmit(const std::uint8_t* data) noexcept {
    const auto status = HAL_QSPI_Transmit(&hqspi, const_cast<std::uint8_t*>(data), kTimeoutMs);
    remember_status(status);
    return status == HAL_OK;
}

bool release_power_down() noexcept {
    QSPI_CommandTypeDef command{};
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = kCmdReleasePowerDown;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_NONE;
    command.DummyCycles = 0;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    return qspi_command(command);
}

std::uint32_t capacity_from_jedec(std::uint32_t jedec) noexcept {
    const std::uint8_t capacity_log2 = static_cast<std::uint8_t>(jedec & 0xFFU);
    if (capacity_log2 >= 20U && capacity_log2 <= 31U) {
        return 1UL << capacity_log2;
    }
    return kDefaultCapacityBytes;
}

bool read_jedec() noexcept {
    std::uint8_t id[3]{};
    QSPI_CommandTypeDef command{};
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = kCmdReadJedecId;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = sizeof(id);
    command.DummyCycles = 0;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    if (!qspi_command(command) || !qspi_receive(id)) {
        return false;
    }

    const std::uint32_t jedec = (static_cast<std::uint32_t>(id[0]) << 16U)
        | (static_cast<std::uint32_t>(id[1]) << 8U)
        | static_cast<std::uint32_t>(id[2]);
    g_state.jedec_id = jedec;
    g_state.jedec_ok = (jedec != 0U && jedec != 0xFFFFFFU) ? 1U : 0U;
    g_capacity = capacity_from_jedec(jedec);
    g_state.capacity_bytes = g_capacity;
    return g_state.jedec_ok != 0U;
}

bool read_chunk(std::uint32_t offset, std::uint8_t* data, std::uint32_t bytes) noexcept {
    QSPI_CommandTypeDef command{};
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = kCmdReadData;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.Address = offset;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = bytes;
    command.DummyCycles = 0;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    return qspi_command(command) && qspi_receive(data);
}

bool read_status1(std::uint8_t& status) noexcept {
    QSPI_CommandTypeDef command{};
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = kCmdReadStatus1;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = 1U;
    command.DummyCycles = 0;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    if (!qspi_command(command) || !qspi_receive(&status)) {
        return false;
    }
    return true;
}

bool wait_ready() noexcept {
    const std::uint32_t start = HAL_GetTick();
    while (true) {
        std::uint8_t status = 0U;
        if (!read_status1(status)) {
            return false;
        }
        if ((status & kStatusBusy) == 0U) {
            return true;
        }
        if ((HAL_GetTick() - start) < kTimeoutMs) {
            HAL_Delay(1U);
            continue;
        }
        remember_status(HAL_TIMEOUT);
        return false;
    }
}

bool write_enable() noexcept {
    QSPI_CommandTypeDef command{};
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = kCmdWriteEnable;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_NONE;
    command.DummyCycles = 0;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    if (!qspi_command(command)) {
        return false;
    }
    std::uint8_t status = 0U;
    return read_status1(status) && ((status & kStatusWel) != 0U);
}

bool erase_block(std::uint32_t offset) noexcept {
    if (!write_enable()) {
        return false;
    }
    QSPI_CommandTypeDef command{};
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = kCmdSectorErase4K;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.Address = offset;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_NONE;
    command.DummyCycles = 0;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    return qspi_command(command) && wait_ready();
}

bool write_page(std::uint32_t offset, const std::uint8_t* data, std::uint32_t bytes) noexcept {
    if (!write_enable()) {
        return false;
    }
    QSPI_CommandTypeDef command{};
    command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command.Instruction = kCmdPageProgram;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.AddressSize = QSPI_ADDRESS_24_BITS;
    command.Address = offset;
    command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = bytes;
    command.DummyCycles = 0;
    command.DdrMode = QSPI_DDR_MODE_DISABLE;
    command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    if (!qspi_command(command) || !qspi_transmit(data)) {
        return false;
    }
    return wait_ready();
}

bool init_nor() noexcept {
    if (g_ready) {
        return true;
    }

    g_state.attempted = 1U;
    (void)power_apply_profile(POWER_PROFILE_STORAGE_STAGE_A);
    (void)power_pmic_set_rail_voltage_mv(POWER_PMIC_RAIL_DCDC1, 3300U);
    (void)power_pmic_set_rail_enabled(POWER_PMIC_RAIL_DCDC1, 1U);
    const auto p = power_pmic_snapshot();
    g_state.power_ok = (p.ready != 0U && p.dcdc1_enabled != 0U && p.dcdc1_mv >= 3000U) ? 1U : 0U;

    __HAL_RCC_QSPI_FORCE_RESET();
    __HAL_RCC_QSPI_RELEASE_RESET();
    HAL_Delay(2U);

    MX_QUADSPI_Init();
    if (hqspi.Instance == QUADSPI) {
        hqspi.Instance->DCR = (hqspi.Instance->DCR & ~QUADSPI_DCR_FSIZE)
            | ((23U << QUADSPI_DCR_FSIZE_Pos) & QUADSPI_DCR_FSIZE);
    }
    snapshot_regs();

    (void)release_power_down();
    HAL_Delay(1U);
    if (!read_jedec()) {
        g_ready = false;
        g_state.ready = 0U;
        g_state.initialized = 0U;
        return false;
    }

    g_ready = true;
    g_state.ready = 1U;
    g_state.initialized = 1U;
    return true;
}

} // namespace

extern "C" void h747_qspi_nor_init(void) {
    g_state = {};
    g_ready = false;
    g_capacity = 0;
    (void)init_nor();
}

extern "C" h747_qspi_nor_state_t h747_qspi_nor_state(void) {
    snapshot_regs();
    return g_state;
}

extern "C" std::uint32_t h747_qspi_nor_capacity(void) {
    return g_ready ? g_capacity : 0U;
}

extern "C" std::uint32_t h747_qspi_nor_erase_block_size(void) {
    return kEraseBlockSize;
}

extern "C" std::uint32_t h747_qspi_nor_write_align(void) {
    return 1U;
}

extern "C" std::uint8_t h747_qspi_nor_read(std::uint32_t offset,
                                            std::uint8_t* data,
                                            std::uint32_t bytes) {
    if (!g_ready && !init_nor()) {
        ++g_state.read_fail_count;
        return 0U;
    }
    if (data == nullptr || bytes == 0U || offset > g_capacity || bytes > (g_capacity - offset)) {
        ++g_state.read_fail_count;
        g_state.last_offset = offset;
        g_state.last_bytes = bytes;
        return 0U;
    }

    std::uint32_t copied = 0U;
    while (copied < bytes) {
        const std::uint32_t remaining = bytes - copied;
        const std::uint32_t chunk = (remaining < kMaxReadChunk) ? remaining : kMaxReadChunk;
        if (!read_chunk(offset + copied, data + copied, chunk)) {
            ++g_state.read_fail_count;
            g_state.last_offset = offset + copied;
            g_state.last_bytes = chunk;
            return 0U;
        }
        copied += chunk;
    }

    ++g_state.read_count;
    g_state.last_offset = offset;
    g_state.last_bytes = bytes;
    snapshot_regs();
    return 1U;
}

extern "C" std::uint8_t h747_qspi_nor_write(std::uint32_t offset,
                                             const std::uint8_t* data,
                                             std::uint32_t bytes) {
    if (!g_ready && !init_nor()) {
        ++g_state.write_fail_count;
        return 0U;
    }
    if (data == nullptr || bytes == 0U || offset > g_capacity || bytes > (g_capacity - offset)) {
        ++g_state.write_fail_count;
        g_state.last_write_offset = offset;
        g_state.last_write_bytes = bytes;
        return 0U;
    }

    std::uint32_t written = 0U;
    while (written < bytes) {
        const std::uint32_t page_offset = (offset + written) % kPageSize;
        std::uint32_t chunk = kPageSize - page_offset;
        const std::uint32_t remaining = bytes - written;
        if (chunk > remaining) {
            chunk = remaining;
        }
        if (!write_page(offset + written, data + written, chunk)) {
            ++g_state.write_fail_count;
            g_state.last_write_offset = offset + written;
            g_state.last_write_bytes = chunk;
            return 0U;
        }
        written += chunk;
    }

    ++g_state.write_count;
    g_state.last_write_offset = offset;
    g_state.last_write_bytes = bytes;
    snapshot_regs();
    return 1U;
}

extern "C" std::uint8_t h747_qspi_nor_erase(std::uint32_t offset, std::uint32_t bytes) {
    if (!g_ready && !init_nor()) {
        ++g_state.erase_fail_count;
        return 0U;
    }
    if (bytes == 0U || (offset % kEraseBlockSize) != 0U || (bytes % kEraseBlockSize) != 0U ||
        offset > g_capacity || bytes > (g_capacity - offset)) {
        ++g_state.erase_fail_count;
        g_state.last_erase_offset = offset;
        g_state.last_erase_bytes = bytes;
        return 0U;
    }

    std::uint32_t erased = 0U;
    while (erased < bytes) {
        if (!erase_block(offset + erased)) {
            ++g_state.erase_fail_count;
            g_state.last_erase_offset = offset + erased;
            g_state.last_erase_bytes = kEraseBlockSize;
            return 0U;
        }
        erased += kEraseBlockSize;
    }

    ++g_state.erase_count;
    g_state.last_erase_offset = offset;
    g_state.last_erase_bytes = bytes;
    snapshot_regs();
    return 1U;
}
