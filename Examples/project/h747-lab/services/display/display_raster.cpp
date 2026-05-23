#include "display_raster.h"

#include "display_min.h"
#include "memory_probe.h"
#include "stm32h7xx_hal.h"
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

display_raster_state_t g_raster{};
std::uint32_t g_front_buffer = kFramebufferBase;
std::uint32_t g_back_buffer = kFramebufferBase + kFramebufferBytes;

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

void clear_ltdc_frame_flags() noexcept {
    if (hltdc_display_min.Instance != nullptr) {
        hltdc_display_min.Instance->ICR = LTDC_ICR_CFUIF | LTDC_ICR_CTERRIF | LTDC_ICR_CRRIF;
    }
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

    std::memset(reinterpret_cast<void*>(kFramebufferBase), 0, kFramebufferPoolBytes);
    clean_dcache_range(reinterpret_cast<void*>(kFramebufferBase), kFramebufferPoolBytes);
    ++g_raster.cache_clean_count;
    g_raster.framebuffer_ready = 1U;

    if (configure_layer() != HAL_OK) {
        snapshot();
        return 0U;
    }

    clear_ltdc_frame_flags();
    if (hdsi_display_min.Instance != nullptr) {
        hdsi_display_min.Instance->WCR |= DSI_WCR_LTDCEN;
    }
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc_display_min);
    g_raster.init_ok = 1U;
    snapshot();
    return 1U;
}

uint8_t display_raster_present(const void* pixels, const uint32_t bytes) {
    auto* back = reinterpret_cast<void*>(g_back_buffer);
    if ((pixels != nullptr) && (pixels != back)) {
        if (bytes > kFramebufferBytes) {
            g_raster.present_ok = 0U;
            snapshot();
            return 0U;
        }
        std::memcpy(back, pixels, bytes);
    }

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
