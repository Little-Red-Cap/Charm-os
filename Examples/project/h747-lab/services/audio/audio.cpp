#include "audio.h"

#include "i2s.h"
#include "stm32h7xx_hal.h"

#include <cstdint>

DMA_HandleTypeDef hdma_spi1_tx;

namespace {

h747_audio_state_t g_state{};

void snapshot_audio_regs() noexcept {
    g_state.i2s_error = static_cast<std::uint32_t>(HAL_I2S_GetError(&hi2s1));
    g_state.dma_error = static_cast<std::uint32_t>(HAL_DMA_GetError(&hdma_spi1_tx));
    if (hi2s1.Instance != nullptr) {
        g_state.spi_cr1 = static_cast<std::uint32_t>(hi2s1.Instance->CR1);
        g_state.spi_i2scfgr = static_cast<std::uint32_t>(hi2s1.Instance->I2SCFGR);
        g_state.spi_sr = static_cast<std::uint32_t>(hi2s1.Instance->SR);
    }
}

HAL_StatusTypeDef configure_i2s1_48k() noexcept {
    hi2s1.Instance = SPI1;
    hi2s1.Init.Mode = I2S_MODE_MASTER_TX;
    hi2s1.Init.Standard = I2S_STANDARD_PHILIPS;
    hi2s1.Init.DataFormat = I2S_DATAFORMAT_16B;
    hi2s1.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
    hi2s1.Init.AudioFreq = I2S_AUDIOFREQ_48K;
    hi2s1.Init.CPOL = I2S_CPOL_LOW;
    hi2s1.Init.FirstBit = I2S_FIRSTBIT_MSB;
    hi2s1.Init.WSInversion = I2S_WS_INVERSION_DISABLE;
    hi2s1.Init.Data24BitAlignment = I2S_DATA_24BIT_ALIGNMENT_RIGHT;
    hi2s1.Init.MasterKeepIOState = I2S_MASTER_KEEP_IO_STATE_DISABLE;
    return HAL_I2S_Init(&hi2s1);
}

HAL_StatusTypeDef configure_i2s1_dma() noexcept {
    __HAL_RCC_DMA1_CLK_ENABLE();
    hdma_spi1_tx = {};
    hdma_spi1_tx.Instance = DMA1_Stream0;
    hdma_spi1_tx.Init.Request = DMA_REQUEST_SPI1_TX;
    hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_spi1_tx.Init.Mode = DMA_CIRCULAR;
    hdma_spi1_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    const auto status = HAL_DMA_Init(&hdma_spi1_tx);
    if (status == HAL_OK) {
        __HAL_LINKDMA(&hi2s1, hdmatx, hdma_spi1_tx);
        HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0U, 0U);
        HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    }
    return status;
}

} // namespace

extern "C" void h747_audio_init(void) {
    g_state = {};
    g_state.attempted = 1U;
    const auto i2s_status = configure_i2s1_48k();
    g_state.i2s_status = static_cast<std::uint32_t>(i2s_status);
    g_state.i2s_ready = (i2s_status == HAL_OK) ? 1U : 0U;
    const auto dma_status = configure_i2s1_dma();
    g_state.dma_status = static_cast<std::uint32_t>(dma_status);
    g_state.dma_ready = (dma_status == HAL_OK) ? 1U : 0U;
    g_state.initialized = (g_state.i2s_ready != 0U && g_state.dma_ready != 0U) ? 1U : 0U;
    snapshot_audio_regs();
}

extern "C" h747_audio_state_t h747_audio_state(void) {
    snapshot_audio_regs();
    return g_state;
}

extern "C" void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_spi1_tx);
}

extern "C" void SPI1_IRQHandler(void) {
    HAL_I2S_IRQHandler(&hi2s1);
}

extern "C" void charm_audio_i2s_half_notify() {
    ++g_state.dma_half_count;
}

extern "C" void charm_audio_i2s_full_notify() {
    ++g_state.dma_full_count;
}

extern "C" void charm_audio_i2s_underrun_notify() {
    ++g_state.underrun_count;
}
