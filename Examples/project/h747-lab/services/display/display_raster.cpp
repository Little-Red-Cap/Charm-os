#include "display_raster.h"

#include "display_min.h"
#include "memory_probe.h"
#include "stm32h7xx_hal.h"
#if defined(HAL_DMA2D_MODULE_ENABLED)
#include "stm32h7xx_hal_dma2d.h"
#endif
#include "stm32h7xx_hal_ltdc.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern DSI_HandleTypeDef hdsi_display_min;
extern LTDC_HandleTypeDef hltdc_display_min;

namespace {

constexpr std::uint32_t kWidth = 720U;
constexpr std::uint32_t kHeight = 1280U;
constexpr std::uint32_t kBytesPerPixel = 4U;
constexpr std::uint32_t kFramebufferBase = 0xC0000000U;
constexpr std::uint32_t kFramebufferBytes = kWidth * kHeight * kBytesPerPixel;
constexpr std::uint32_t kFramebufferCount = 2U;
constexpr std::uint32_t kFramebufferPoolBytes = kFramebufferBytes * kFramebufferCount;
constexpr std::uint32_t kReloadWaitTimeoutMs = 50U;
constexpr std::uint32_t kDma2dTransferTimeoutMs = 100U;

display_raster_state_t g_raster{};
std::uint32_t g_front_buffer = kFramebufferBase;
std::uint32_t g_back_buffer = kFramebufferBase + kFramebufferBytes;
#if defined(HAL_DMA2D_MODULE_ENABLED)
DMA2D_HandleTypeDef g_dma2d{};
#endif

std::uintptr_t cache_align_down(const std::uintptr_t address) noexcept {
    return address & ~static_cast<std::uintptr_t>(31U);
}

std::uint32_t cache_aligned_length(const std::uintptr_t address, const std::uint32_t length) noexcept {
    const std::uintptr_t start = cache_align_down(address);
    const std::uintptr_t end = (address + length + 31U) & ~static_cast<std::uintptr_t>(31U);
    return static_cast<std::uint32_t>(end - start);
}

void clean_dcache_range(void* address, const std::uint32_t length) noexcept {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U) {
        return;
    }
    const auto addr = reinterpret_cast<std::uintptr_t>(address);
    auto* aligned = reinterpret_cast<std::uint32_t*>(cache_align_down(addr));
    const std::uint32_t bytes = cache_aligned_length(addr, length);
    SCB_CleanDCache_by_Addr(aligned, static_cast<std::int32_t>(bytes));
    __DSB();
    __ISB();
#else
    (void)address;
    (void)length;
#endif
}

void clean_dcache_range_const(const void* address, const std::uint32_t length) noexcept {
    clean_dcache_range(const_cast<void*>(address), length);
}

void invalidate_dcache_range(void* address, const std::uint32_t length) noexcept {
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

void record_dma2d_status(const HAL_StatusTypeDef status) noexcept {
    g_raster.dma2d_last_hal_status = static_cast<std::uint32_t>(status);
#if defined(HAL_DMA2D_MODULE_ENABLED)
    g_raster.dma2d_last_error = (g_dma2d.Instance != nullptr) ? HAL_DMA2D_GetError(&g_dma2d) : 0U;
#else
    g_raster.dma2d_last_error = 0U;
#endif
}

bool init_dma2d() noexcept {
#if defined(HAL_DMA2D_MODULE_ENABLED)
    __HAL_RCC_DMA2D_CLK_ENABLE();

    g_dma2d = {};
    g_dma2d.Instance = DMA2D;
    g_dma2d.Init.Mode = DMA2D_M2M;
    g_dma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
    g_dma2d.Init.OutputOffset = 0U;
    g_dma2d.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    g_dma2d.Init.RedBlueSwap = DMA2D_RB_REGULAR;

    HAL_StatusTypeDef status = HAL_DMA2D_Init(&g_dma2d);
    record_dma2d_status(status);
    if (status != HAL_OK) {
        ++g_raster.dma2d_error_count;
        g_raster.dma2d_ready = 0U;
        return false;
    }

    g_dma2d.LayerCfg[1].InputOffset = 0U;
    g_dma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
    g_dma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    g_dma2d.LayerCfg[1].InputAlpha = 0xFFU;
    g_dma2d.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;
    g_dma2d.LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR;

    status = HAL_DMA2D_ConfigLayer(&g_dma2d, 1U);
    record_dma2d_status(status);
    if (status != HAL_OK) {
        ++g_raster.dma2d_error_count;
        g_raster.dma2d_ready = 0U;
        return false;
    }

    g_raster.dma2d_ready = 1U;
    return true;
#else
    g_raster.dma2d_ready = 0U;
    g_raster.dma2d_last_hal_status = static_cast<std::uint32_t>(HAL_ERROR);
    g_raster.dma2d_last_error = 0U;
    return false;
#endif
}

bool copy_frame_dma2d(const void* source, void* destination, const std::uint32_t bytes) noexcept {
#if defined(HAL_DMA2D_MODULE_ENABLED)
    if ((g_raster.dma2d_ready == 0U) || (source == nullptr) || (destination == nullptr) || (bytes != kFramebufferBytes)) {
        return false;
    }

    clean_dcache_range_const(source, bytes);
    ++g_raster.cache_clean_count;

    HAL_StatusTypeDef status = HAL_DMA2D_Start(&g_dma2d,
                                               reinterpret_cast<std::uint32_t>(source),
                                               reinterpret_cast<std::uint32_t>(destination),
                                               kWidth,
                                               kHeight);
    record_dma2d_status(status);
    if (status != HAL_OK) {
        ++g_raster.dma2d_error_count;
        return false;
    }

    status = HAL_DMA2D_PollForTransfer(&g_dma2d, kDma2dTransferTimeoutMs);
    record_dma2d_status(status);
    if (status != HAL_OK) {
        ++g_raster.dma2d_error_count;
        return false;
    }

    invalidate_dcache_range(destination, bytes);
    ++g_raster.dma2d_used_count;
    return true;
#else
    (void)source;
    (void)destination;
    (void)bytes;
    return false;
#endif
}

void copy_frame_cpu(const void* source, void* destination, const std::uint32_t bytes) noexcept {
    std::memcpy(destination, source, bytes);
    ++g_raster.dma2d_fallback_count;
}

bool wait_reload_complete() noexcept {
    if (hltdc_display_min.Instance == nullptr) {
        return false;
    }

    const std::uint32_t start = HAL_GetTick();
    do {
        if ((hltdc_display_min.Instance->ISR & LTDC_ISR_RRIF) != 0U) {
            hltdc_display_min.Instance->ICR = LTDC_ICR_CRRIF;
            return true;
        }
        if ((hltdc_display_min.Instance->SRCR & (LTDC_SRCR_IMR | LTDC_SRCR_VBR)) == 0U) {
            return true;
        }
    } while ((HAL_GetTick() - start) < kReloadWaitTimeoutMs);
    return false;
}

std::uint32_t sample_argb8888(const void* pixels, const std::uint32_t byte_offset) noexcept {
    if (pixels == nullptr) {
        return 0U;
    }
    std::uint32_t value{};
    std::memcpy(&value, static_cast<const std::byte*>(pixels) + byte_offset, sizeof(value));
    return value;
}

std::uint32_t sample_center_offset() noexcept {
    constexpr std::uint32_t center_x = kWidth / 2U;
    constexpr std::uint32_t center_y = kHeight / 2U;
    return ((center_y * kWidth) + center_x) * kBytesPerPixel;
}

void sample_surface(const void* pixels,
                    const std::uint32_t bytes,
                    std::uint32_t& sample0,
                    std::uint32_t& sample_center,
                    std::uint32_t& sample_last) noexcept {
    if ((pixels == nullptr) || (bytes < sizeof(std::uint32_t))) {
        sample0 = 0U;
        sample_center = 0U;
        sample_last = 0U;
        return;
    }

    sample0 = sample_argb8888(pixels, 0U);
    const std::uint32_t center = sample_center_offset();
    sample_center = (center + sizeof(std::uint32_t) <= bytes)
        ? sample_argb8888(pixels, center)
        : 0U;
    sample_last = sample_argb8888(pixels, bytes - sizeof(std::uint32_t));
}

void clear_ltdc_frame_flags() noexcept {
    if (hltdc_display_min.Instance != nullptr) {
        hltdc_display_min.Instance->ICR = LTDC_ICR_CFUIF | LTDC_ICR_CTERRIF | LTDC_ICR_CRRIF;
    }
}

HAL_StatusTypeDef refresh_dsi_from_ltdc() noexcept {
    if (hdsi_display_min.Instance == nullptr) {
        return HAL_ERROR;
    }

    const HAL_StatusTypeDef status = HAL_DSI_Refresh(&hdsi_display_min);
    g_raster.dsi_refresh_hal_status = static_cast<std::uint32_t>(status);
    if (status == HAL_OK) {
        ++g_raster.dsi_refresh_count;
    }
    return status;
}

void snapshot() noexcept {
    g_raster.framebuffer_base = g_back_buffer;
    g_raster.framebuffer_bytes = kFramebufferBytes;
    g_raster.front_buffer_base = g_front_buffer;
    g_raster.back_buffer_base = g_back_buffer;
    const memory_storage_state_t memory = memory_probe_storage_state();
    g_raster.sdram_ready = memory.sdram1_ready;
    g_raster.sdram_smoke_ok = memory.sdram1_smoke_ok;
    g_raster.sdram_tested_words = memory.sdram1_tested_words;
    g_raster.sdram_first_error_addr = memory.sdram1_first_error_addr;
    g_raster.sdram_last_hal_status = memory.sdram1_last_hal_status;
    g_raster.dsi_error = HAL_DSI_GetError(&hdsi_display_min);
    g_raster.dsi_wcr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->WCR : 0U;
    g_raster.dsi_wisr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->WISR : 0U;
    g_raster.ltdc_isr = (hltdc_display_min.Instance != nullptr) ? hltdc_display_min.Instance->ISR : 0U;
    if (LTDC_Layer1 != nullptr) {
        g_raster.ltdc_layer_cfb_addr = LTDC_Layer1->CFBAR;
        g_raster.ltdc_layer_cr = LTDC_Layer1->CR;
        g_raster.ltdc_layer_pfcr = LTDC_Layer1->PFCR;
        g_raster.ltdc_layer_cfblr = LTDC_Layer1->CFBLR;
        g_raster.ltdc_layer_cfblnr = LTDC_Layer1->CFBLNR;
    } else {
        g_raster.ltdc_layer_cfb_addr = 0U;
        g_raster.ltdc_layer_cr = 0U;
        g_raster.ltdc_layer_pfcr = 0U;
        g_raster.ltdc_layer_cfblr = 0U;
        g_raster.ltdc_layer_cfblnr = 0U;
    }
    sample_surface(reinterpret_cast<const void*>(g_front_buffer),
                   kFramebufferBytes,
                   g_raster.front_sample0,
                   g_raster.front_sample_center,
                   g_raster.front_sample_last);
    sample_surface(reinterpret_cast<const void*>(g_back_buffer),
                   kFramebufferBytes,
                   g_raster.back_sample0,
                   g_raster.back_sample_center,
                   g_raster.back_sample_last);
}

HAL_StatusTypeDef configure_layer() noexcept {
    LTDC_LayerCfgTypeDef layer{};
    layer.WindowX0 = 0U;
    layer.WindowX1 = kWidth;
    layer.WindowY0 = 0U;
    layer.WindowY1 = kHeight;
    layer.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
    layer.FBStartAdress = g_front_buffer;
    layer.Alpha = 255U;
    layer.Alpha0 = 0U;
    layer.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    layer.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    layer.ImageWidth = kWidth;
    layer.ImageHeight = kHeight;
    layer.Backcolor.Red = 0U;
    layer.Backcolor.Green = 0U;
    layer.Backcolor.Blue = 0U;

    const HAL_StatusTypeDef status = HAL_LTDC_ConfigLayer(&hltdc_display_min, &layer, 0U);
    g_raster.last_hal_status = static_cast<std::uint32_t>(status);
    if (status == HAL_OK) {
        g_raster.ltdc_layer_ready = 1U;
    }
    return status;
}

} // namespace

uint8_t display_raster_init(void) {
    if (g_raster.init_ok != 0U) {
        snapshot();
        return 1U;
    }

    g_raster = {};
    g_front_buffer = kFramebufferBase;
    g_back_buffer = kFramebufferBase + kFramebufferBytes;
    g_raster.framebuffer_base = g_back_buffer;
    g_raster.framebuffer_bytes = kFramebufferBytes;
    g_raster.front_buffer_base = g_front_buffer;
    g_raster.back_buffer_base = g_back_buffer;

    memory_probe_storage_init();
    if (memory_probe_sdram1_smoke_force() == 0U) {
        snapshot();
        return 0U;
    }

    if (display_min_init() == 0U) {
        snapshot();
        return 0U;
    }

    init_dma2d();

    std::memset(reinterpret_cast<void*>(kFramebufferBase), 0, kFramebufferPoolBytes);
    clean_dcache_range(reinterpret_cast<void*>(kFramebufferBase), kFramebufferPoolBytes);
    ++g_raster.cache_clean_count;
    g_raster.framebuffer_ready = 1U;

    if (configure_layer() != HAL_OK) {
        snapshot();
        return 0U;
    }

    clear_ltdc_frame_flags();
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc_display_min);
    if (refresh_dsi_from_ltdc() != HAL_OK) {
        snapshot();
        return 0U;
    }
    g_raster.init_ok = 1U;
    snapshot();
    return 1U;
}

uint8_t display_raster_present(const void* pixels, const uint32_t bytes) {
    auto* back = reinterpret_cast<void*>(g_back_buffer);
    sample_surface(pixels,
                   bytes,
                   g_raster.present_src_sample0,
                   g_raster.present_src_sample_center,
                   g_raster.present_src_sample_last);
    if ((pixels != nullptr) && (pixels != back)) {
        if (bytes > kFramebufferBytes) {
            g_raster.present_ok = 0U;
            snapshot();
            return 0U;
        }
        if (!copy_frame_dma2d(pixels, back, bytes)) {
            copy_frame_cpu(pixels, back, bytes);
        }
    }
    sample_surface(back,
                   kFramebufferBytes,
                   g_raster.presented_sample0,
                   g_raster.presented_sample_center,
                   g_raster.presented_sample_last);

    clean_dcache_range(back, kFramebufferBytes);
    ++g_raster.cache_clean_count;

    const HAL_StatusTypeDef address_status = HAL_LTDC_SetAddress_NoReload(&hltdc_display_min, g_back_buffer, 0U);
    g_raster.last_hal_status = static_cast<std::uint32_t>(address_status);
    if (address_status != HAL_OK) {
        g_raster.present_ok = 0U;
        snapshot();
        return 0U;
    }

    clear_ltdc_frame_flags();
    const HAL_StatusTypeDef reload_status = HAL_LTDC_Reload(&hltdc_display_min, LTDC_RELOAD_VERTICAL_BLANKING);
    g_raster.last_hal_status = static_cast<std::uint32_t>(reload_status);
    if (reload_status != HAL_OK) {
        g_raster.present_ok = 0U;
        snapshot();
        return 0U;
    }

    if (!wait_reload_complete()) {
        g_raster.present_ok = 0U;
        snapshot();
        return 0U;
    }

    if (refresh_dsi_from_ltdc() != HAL_OK) {
        g_raster.present_ok = 0U;
        snapshot();
        return 0U;
    }

    const std::uint32_t old_front = g_front_buffer;
    g_front_buffer = g_back_buffer;
    g_back_buffer = old_front;
    ++g_raster.present_count;
    g_raster.present_ok = 1U;
    snapshot();
    return 1U;
}

display_raster_state_t display_raster_state(void) {
    snapshot();
    return g_raster;
}

void* display_raster_framebuffer(void) {
    return reinterpret_cast<void*>(g_back_buffer);
}

uint32_t display_raster_framebuffer_bytes(void) {
    return kFramebufferBytes;
}
