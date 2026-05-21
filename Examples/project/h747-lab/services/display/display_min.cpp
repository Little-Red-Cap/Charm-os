#include "display_min.h"

#include <cstddef>
#include <cstdint>

#include "power.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_ltdc_ex.h"

extern "C" void Error_Handler(void);

DSI_HandleTypeDef hdsi_display_min;
LTDC_HandleTypeDef hltdc_display_min;

namespace {

constexpr std::uint32_t kWidth = 720U;
constexpr std::uint32_t kHeight = 1280U;
constexpr std::uint32_t kVirtualChannel = 0U;
constexpr std::uint32_t kLaneByteClockKhz = 62500U;
constexpr std::uint32_t kPixelClockKhz = 26400U;
constexpr std::uint16_t kDisplayDcdc1TargetMv = 3300U;
constexpr std::uint16_t kDisplayWledFdimHz = 200U;
constexpr std::uint8_t kDisplayWledDutyPercent = 5U;
constexpr std::uint32_t kHsa = 16U;
constexpr std::uint32_t kHbp = 8U;
constexpr std::uint32_t kHfp = 8U;
constexpr std::uint32_t kVsa = 16U;
constexpr std::uint32_t kVbp = 8U;
constexpr std::uint32_t kVfp = 8U;
constexpr std::uint32_t kHorizontalTotalPixels = kWidth + kHsa + kHbp + kHfp;
constexpr std::uint16_t kRegFlagDelay = 0xFFFCU;
constexpr std::uint16_t kRegFlagEnd = 0xFFFDU;

struct PanelCmd {
    std::uint16_t reg;
    std::uint8_t len;
    std::uint8_t data[44];
};

display_min_state_t g_state{};

constexpr PanelCmd kHx8394dDts2LaneInit[] = {
    {0xB9, 3, {0xFF, 0x83, 0x94}},
    {0xBA, 2, {0x71, 0x83}},
    {0xB1, 15, {0x6C, 0x15, 0x15, 0x24, 0xE4, 0x11, 0xF1, 0x80, 0xE4, 0x97, 0x23, 0x80, 0xC0, 0xD2, 0x58}},
    {0xB2, 11, {0x00, 0x64, 0x10, 0x07, 0x22, 0x1C, 0x08, 0x08, 0x1C, 0x4D, 0x00}},
    {0xB4, 12, {0x00, 0xFF, 0x03, 0x5A, 0x03, 0x5A, 0x03, 0x5A, 0x01, 0x6A, 0x30, 0x6A}},
    {0xBC, 1, {0x07}},
    {0xBF, 3, {0x41, 0x0E, 0x01}},
    {0xD3, 30, {0x00, 0x06, 0x00, 0x40, 0x07, 0x08, 0x00, 0x32, 0x10, 0x07, 0x00, 0x07, 0x54, 0x15, 0x0F, 0x05, 0x04, 0x02, 0x12, 0x10, 0x05, 0x07, 0x33, 0x33, 0x0B, 0x0B, 0x37, 0x10, 0x07, 0x07}},
    {0xD5, 44, {0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x20, 0x21, 0x22, 0x23, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x18, 0x18, 0x1B, 0x1B, 0x1A, 0x1A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}},
    {0xD6, 44, {0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x23, 0x22, 0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x58, 0x58, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x1B, 0x1B, 0x1A, 0x1A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}},
    {0xCC, 1, {0x09}},
    {0xB6, 2, {0x51, 0x51}},
    {0xE0, 42, {0x00, 0x10, 0x16, 0x2D, 0x33, 0x3F, 0x23, 0x3E, 0x07, 0x0B, 0x0D, 0x17, 0x0E, 0x12, 0x14, 0x12, 0x13, 0x06, 0x11, 0x13, 0x18, 0x00, 0x0F, 0x16, 0x2E, 0x33, 0x3F, 0x23, 0x3D, 0x07, 0x0B, 0x0D, 0x18, 0x0F, 0x12, 0x14, 0x12, 0x14, 0x07, 0x11, 0x12, 0x17}},
    {0xC0, 2, {0x30, 0x14}},
    {0xC7, 4, {0x00, 0xC0, 0x40, 0xC0}},
    {0xDF, 1, {0x87}},
    {0xD2, 1, {0x66}},
    {0x3A, 1, {0x77}},
    {0x11, 0, {0x00}},
    {kRegFlagDelay, 120, {0x00}},
    {0x29, 0, {0x00}},
    {kRegFlagDelay, 40, {0x00}},
    {kRegFlagEnd, 0, {0x00}},
};

constexpr PanelCmd kHx8394dGithub4Lane2LaneInit[] = {
    {0xB9, 3, {0xFF, 0x83, 0x94}},
    {0xBA, 2, {0x31, 0x83}},
    {0xB1, 15, {0x6C, 0x12, 0x12, 0x26, 0x04, 0x11, 0xF1, 0x81, 0x3A, 0x54, 0x23, 0x80, 0xC0, 0xD2, 0x58}},
    {0xB2, 11, {0x00, 0x64, 0x0E, 0x0D, 0x22, 0x1C, 0x08, 0x08, 0x1C, 0x4D, 0x00}},
    {0xB4, 12, {0x00, 0xFF, 0x51, 0x5A, 0x59, 0x5A, 0x03, 0x5A, 0x01, 0x70, 0x01, 0x70}},
    {0xBC, 1, {0x07}},
    {0xBF, 3, {0x41, 0x0E, 0x01}},
    {0xD3, 37, {0x00, 0x0F, 0x00, 0x40, 0x07, 0x10, 0x00, 0x08, 0x10, 0x08, 0x00, 0x08, 0x54, 0x15, 0x0E, 0x05, 0x0E, 0x02, 0x15, 0x06, 0x05, 0x06, 0x47, 0x44, 0x0A, 0x0A, 0x4B, 0x10, 0x07, 0x07, 0x08, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x01}},
    {0xD5, 44, {0x1A, 0x1A, 0x1B, 0x1B, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x24, 0x25, 0x18, 0x18, 0x26, 0x27, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x20, 0x21, 0x18, 0x18, 0x18, 0x18}},
    {0xD6, 44, {0x1A, 0x1A, 0x1B, 0x1B, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x21, 0x20, 0x58, 0x58, 0x27, 0x26, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x25, 0x24, 0x18, 0x18, 0x18, 0x18}},
    {0xCC, 1, {0x09}},
    {0xC0, 2, {0x30, 0x14}},
    {0xC7, 4, {0x00, 0xC0, 0x40, 0xC0}},
    {0xB6, 2, {0x6B, 0x6B}},
    {0xE0, 42, {0x00, 0x0A, 0x0F, 0x24, 0x3A, 0x3F, 0x20, 0x3B, 0x08, 0x0D, 0x0E, 0x16, 0x0F, 0x12, 0x15, 0x13, 0x15, 0x09, 0x12, 0x12, 0x18, 0x00, 0x0A, 0x0F, 0x24, 0x3A, 0x3F, 0x20, 0x3B, 0x08, 0x0D, 0x0E, 0x16, 0x0F, 0x12, 0x15, 0x13, 0x15, 0x09, 0x12, 0x12, 0x18}},
    {0xBD, 1, {0x00}},
    {0xC1, 43, {0x01, 0x00, 0x06, 0x0C, 0x14, 0x1D, 0x27, 0x2F, 0x38, 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x71, 0x79, 0x81, 0x89, 0x91, 0x99, 0xA1, 0xA9, 0xB2, 0xB9, 0xC1, 0xCA, 0xD1, 0xD8, 0xE2, 0xEA, 0xF0, 0xF7, 0xFF, 0x38, 0xFC, 0x3F, 0x0B, 0xC1, 0x13, 0xF1, 0x0D, 0xC0}},
    {0xBD, 1, {0x01}},
    {0xC1, 42, {0x00, 0x06, 0x0C, 0x14, 0x1D, 0x27, 0x2F, 0x38, 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x71, 0x79, 0x81, 0x89, 0x91, 0x99, 0xA1, 0xA9, 0xB2, 0xB9, 0xC1, 0xCA, 0xD1, 0xD8, 0xE2, 0xEA, 0xF0, 0xF7, 0xFF, 0x38, 0xFC, 0x3F, 0x0B, 0xC1, 0x13, 0xF1, 0x0D, 0xC0}},
    {0xBD, 1, {0x02}},
    {0xC1, 42, {0x00, 0x06, 0x0C, 0x14, 0x1D, 0x27, 0x2F, 0x38, 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x71, 0x79, 0x81, 0x89, 0x91, 0x99, 0xA1, 0xA9, 0xB2, 0xB9, 0xC1, 0xCA, 0xD1, 0xD8, 0xE2, 0xEA, 0xF0, 0xF7, 0xFF, 0x38, 0xFC, 0x3F, 0x0B, 0xC1, 0x13, 0xF1, 0x0D, 0xC0}},
    {0x36, 1, {0x03}},
    {0x3A, 1, {0x77}},
    {0x11, 0, {0x00}},
    {kRegFlagDelay, 120, {0x00}},
    {0x29, 0, {0x00}},
    {kRegFlagDelay, 10, {0x00}},
    {kRegFlagEnd, 0, {0x00}},
};

const PanelCmd* selected_panel_init_table() {
#if defined(STM32H747_DISPLAY_MIN_PANEL_PROFILE_GITHUB4LANE_2LANE)
    return kHx8394dGithub4Lane2LaneInit;
#else
    return kHx8394dDts2LaneInit;
#endif
}

display_min_panel_profile_t selected_panel_profile() {
#if defined(STM32H747_DISPLAY_MIN_PANEL_PROFILE_GITHUB4LANE_2LANE)
    return DISPLAY_MIN_PANEL_PROFILE_GITHUB4LANE_2LANE;
#else
    return DISPLAY_MIN_PANEL_PROFILE_DTS_2LANE;
#endif
}

void update_static_state() {
    g_state.panel_profile = static_cast<std::uint8_t>(selected_panel_profile());
}

void set_phase(const display_min_phase_t phase) {
    g_state.phase = static_cast<std::uint8_t>(phase);
}

std::uint8_t ensure_display_power_ready() {
    const power_pmic_snapshot_t before = power_pmic_snapshot();
    g_state.power_ready = before.ready;
    if (before.ready == 0U) {
        return 0U;
    }

    const bool dcdc1_needs_fix = (before.dcdc1_enabled == 0U) || (before.dcdc1_mv != kDisplayDcdc1TargetMv);
    g_state.dcdc1_repair_needed = dcdc1_needs_fix ? 1U : 0U;
    g_state.dcdc1_repair_ok = dcdc1_needs_fix ? 0U : 1U;
    if (dcdc1_needs_fix) {
        const bool enable_ok = power_pmic_set_rail_enabled(POWER_PMIC_RAIL_DCDC1, 1U) != 0U;
        const bool voltage_ok = power_pmic_set_rail_voltage_mv(POWER_PMIC_RAIL_DCDC1, kDisplayDcdc1TargetMv) != 0U;
        const power_pmic_snapshot_t after = power_pmic_snapshot();
        g_state.dcdc1_repair_ok =
            (enable_ok && voltage_ok && (after.dcdc1_enabled != 0U) && (after.dcdc1_mv == kDisplayDcdc1TargetMv))
                ? 1U
                : 0U;
    }

    const power_pmic_snapshot_t dcdc1_state = power_pmic_snapshot();
    if ((dcdc1_state.dcdc1_enabled == 0U) || (dcdc1_state.dcdc1_mv != kDisplayDcdc1TargetMv)) {
        return 0U;
    }

    const bool wled_needs_fix = (dcdc1_state.wled_enabled == 0U) ||
                                (dcdc1_state.wled_duty_percent != kDisplayWledDutyPercent) ||
                                (dcdc1_state.wled_fdim_hz != kDisplayWledFdimHz);
    g_state.wled_repair_needed = wled_needs_fix ? 1U : 0U;
    g_state.wled_repair_ok = wled_needs_fix ? 0U : 1U;
    if (wled_needs_fix) {
        const bool current_ok = power_pmic_set_wled_current_profile(0U) != 0U;
        const bool fdim_ok = power_pmic_set_wled_fdim_hz(kDisplayWledFdimHz) != 0U;
        const bool duty_ok = power_pmic_set_wled_duty_percent(kDisplayWledDutyPercent) != 0U;
        const bool enable_ok = power_pmic_set_wled_enabled(1U) != 0U;
        const bool refresh_ok = power_pmic_refresh_wled() != 0U;
        const power_pmic_snapshot_t after = power_pmic_snapshot();
        g_state.wled_repair_ok =
            (current_ok && fdim_ok && duty_ok && enable_ok && refresh_ok && (after.wled_enabled != 0U) &&
             (after.wled_duty_percent == kDisplayWledDutyPercent) && (after.wled_fdim_hz == kDisplayWledFdimHz))
                ? 1U
                : 0U;
    }

    return (std::uint8_t)((g_state.dcdc1_repair_ok != 0U) && (g_state.wled_repair_ok != 0U));
}

HAL_StatusTypeDef record_status(const HAL_StatusTypeDef status) {
    g_state.last_hal_status = static_cast<std::uint32_t>(status);
    if (status != HAL_OK) {
        set_phase(DISPLAY_MIN_PHASE_ERROR);
    }
    return status;
}

void snapshot_regs() {
    g_state.last_dsi_error = HAL_DSI_GetError(&hdsi_display_min);
    g_state.wcr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->WCR : 0U;
    g_state.wisr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->WISR : 0U;
    g_state.vmcr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->VMCR : 0U;
    g_state.vpcr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->VPCR : 0U;
    g_state.pcr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->PCR : 0U;
    g_state.isr0 = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->ISR[0] : 0U;
    g_state.isr1 = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->ISR[1] : 0U;
    g_state.ltdc_isr = (hltdc_display_min.Instance != nullptr) ? hltdc_display_min.Instance->ISR : 0U;
    g_state.wrpcr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->WRPCR : 0U;
    g_state.psr = (hdsi_display_min.Instance != nullptr) ? hdsi_display_min.Instance->PSR : 0U;
    update_static_state();
}

void clear_sticky_flags() {
    if (hdsi_display_min.Instance != nullptr) {
        hdsi_display_min.Instance->WIFCR = DSI_WIFCR_CTEIF | DSI_WIFCR_CERIF | DSI_WIFCR_CPLLLIF |
                                           DSI_WIFCR_CPLLUIF | DSI_WIFCR_CRRIF;
    }
    if (hltdc_display_min.Instance != nullptr) {
        __HAL_LTDC_CLEAR_FLAG(&hltdc_display_min, LTDC_FLAG_LI | LTDC_FLAG_FU | LTDC_FLAG_TE | LTDC_FLAG_RR);
    }
}

HAL_StatusTypeDef configure_ltdc_pixel_clock() {
    RCC_PeriphCLKInitTypeDef clock{};
    clock.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    clock.PLL3.PLL3M = 5U;
    clock.PLL3.PLL3N = 96U;
    clock.PLL3.PLL3P = 2U;
    clock.PLL3.PLL3Q = 10U;
    clock.PLL3.PLL3R = 18U;
    clock.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    clock.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    clock.PLL3.PLL3FRACN = 0U;
    return HAL_RCCEx_PeriphCLKConfig(&clock);
}

HAL_StatusTypeDef configure_dsi_clock_source() {
    RCC_PeriphCLKInitTypeDef clock{};
    clock.PeriphClockSelection = RCC_PERIPHCLK_DSI;
    clock.DsiClockSelection = RCC_DSICLKSOURCE_PHY;
    return HAL_RCCEx_PeriphCLKConfig(&clock);
}

void configure_panel_reset_gpio() {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void pulse_panel_reset() {
    configure_panel_reset_gpio();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(200U);
}

DSI_VidCfgTypeDef make_video_config() {
    DSI_VidCfgTypeDef video{};
    video.VirtualChannelID = kVirtualChannel;
    video.ColorCoding = DSI_RGB888;
    video.LooselyPacked = DSI_LOOSELY_PACKED_DISABLE;
    video.Mode = DSI_VID_MODE_BURST;
    video.PacketSize = kWidth;
    video.NumberOfChunks = 0U;
    video.NullPacketSize = 0xFFFU;
    video.HSPolarity = DSI_HSYNC_ACTIVE_HIGH;
    video.VSPolarity = DSI_VSYNC_ACTIVE_HIGH;
    video.DEPolarity = DSI_DATA_ENABLE_ACTIVE_HIGH;
    video.HorizontalSyncActive = (kHsa * kLaneByteClockKhz) / kPixelClockKhz;
    video.HorizontalBackPorch = (kHbp * kLaneByteClockKhz) / kPixelClockKhz;
    video.HorizontalLine = (kHorizontalTotalPixels * kLaneByteClockKhz) / kPixelClockKhz;
    video.VerticalSyncActive = kVsa;
    video.VerticalBackPorch = kVbp;
    video.VerticalFrontPorch = kVfp;
    video.VerticalActive = kHeight;
    video.LPCommandEnable = DSI_LP_COMMAND_ENABLE;
    video.LPLargestPacketSize = 16U;
    video.LPVACTLargestPacketSize = 0U;
    video.LPHorizontalFrontPorchEnable = DSI_LP_HFP_ENABLE;
    video.LPHorizontalBackPorchEnable = DSI_LP_HBP_ENABLE;
    video.LPVerticalActiveEnable = DSI_LP_VACT_ENABLE;
    video.LPVerticalFrontPorchEnable = DSI_LP_VFP_ENABLE;
    video.LPVerticalBackPorchEnable = DSI_LP_VBP_ENABLE;
    video.LPVerticalSyncActiveEnable = DSI_LP_VSYNC_ENABLE;
    video.FrameBTAAcknowledgeEnable = DSI_FBTAA_DISABLE;
    return video;
}

HAL_StatusTypeDef configure_dsi_video() {
    set_phase(DISPLAY_MIN_PHASE_DSI_CONFIG);

    hdsi_display_min.Instance = DSI;
    (void)HAL_DSI_DeInit(&hdsi_display_min);

    DSI_PLLInitTypeDef pll{};
    pll.PLLNDIV = 100U;
    pll.PLLIDF = DSI_PLL_IN_DIV5;
    pll.PLLODF = DSI_PLL_OUT_DIV1;

    hdsi_display_min.Init.AutomaticClockLaneControl = DSI_AUTO_CLK_LANE_CTRL_DISABLE;
    hdsi_display_min.Init.NumberOfLanes = DSI_TWO_DATA_LANES;
    hdsi_display_min.Init.TXEscapeCkdiv = 4U;
    if (record_status(HAL_DSI_Init(&hdsi_display_min, &pll)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    DSI_HOST_TimeoutTypeDef timeouts{};
    timeouts.TimeoutCkdiv = 1U;
    timeouts.HighSpeedTransmissionTimeout = 0U;
    timeouts.LowPowerReceptionTimeout = 0U;
    timeouts.HighSpeedReadTimeout = 0U;
    timeouts.LowPowerReadTimeout = 0U;
    timeouts.HighSpeedWriteTimeout = 0U;
    timeouts.HighSpeedWritePrespMode = DSI_HS_PM_DISABLE;
    timeouts.LowPowerWriteTimeout = 0U;
    timeouts.BTATimeout = 0U;
    if (record_status(HAL_DSI_ConfigHostTimeouts(&hdsi_display_min, &timeouts)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    DSI_VidCfgTypeDef video = make_video_config();
    if (record_status(HAL_DSI_ConfigVideoMode(&hdsi_display_min, &video)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    DSI_PHY_TimerTypeDef phy{};
    phy.ClockLaneHS2LPTime = 35U;
    phy.ClockLaneLP2HSTime = 35U;
    phy.DataLaneHS2LPTime = 35U;
    phy.DataLaneLP2HSTime = 35U;
    phy.DataLaneMaxReadTime = 0U;
    phy.StopWaitTime = 10U;
    if (record_status(HAL_DSI_ConfigPhyTimer(&hdsi_display_min, &phy)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    if (record_status(HAL_DSI_ConfigErrorMonitor(&hdsi_display_min, HAL_DSI_ERROR_NONE)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }
    if (record_status(HAL_DSI_SetGenericVCID(&hdsi_display_min, kVirtualChannel)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    DSI_LPCmdTypeDef lp_cmd{};
    lp_cmd.LPGenShortWriteNoP = DSI_LP_GSW0P_ENABLE;
    lp_cmd.LPGenShortWriteOneP = DSI_LP_GSW1P_ENABLE;
    lp_cmd.LPGenShortWriteTwoP = DSI_LP_GSW2P_ENABLE;
    lp_cmd.LPGenLongWrite = DSI_LP_GLW_ENABLE;
    lp_cmd.LPDcsShortWriteNoP = DSI_LP_DSW0P_ENABLE;
    lp_cmd.LPDcsShortWriteOneP = DSI_LP_DSW1P_ENABLE;
    lp_cmd.LPDcsLongWrite = DSI_LP_DLW_ENABLE;
    lp_cmd.AcknowledgeRequest = DSI_ACKNOWLEDGE_DISABLE;
    if (record_status(HAL_DSI_ConfigCommand(&hdsi_display_min, &lp_cmd)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    snapshot_regs();
    return HAL_OK;
}

HAL_StatusTypeDef configure_ltdc_background_only() {
    set_phase(DISPLAY_MIN_PHASE_LTDC_CONFIG);
    if (configure_ltdc_pixel_clock() != HAL_OK) {
        return record_status(HAL_ERROR);
    }

    DSI_VidCfgTypeDef video = make_video_config();
    hltdc_display_min.Instance = LTDC;
    if (record_status(HAL_LTDCEx_StructInitFromVideoConfig(&hltdc_display_min, &video)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    hltdc_display_min.Init.HorizontalSync = kHsa - 1U;
    hltdc_display_min.Init.VerticalSync = kVsa - 1U;
    hltdc_display_min.Init.AccumulatedHBP = kHsa + kHbp - 1U;
    hltdc_display_min.Init.AccumulatedVBP = kVsa + kVbp - 1U;
    hltdc_display_min.Init.AccumulatedActiveW = kWidth + kHsa + kHbp - 1U;
    hltdc_display_min.Init.AccumulatedActiveH = kHeight + kVsa + kVbp - 1U;
    hltdc_display_min.Init.TotalWidth = kWidth + kHsa + kHbp + kHfp - 1U;
    hltdc_display_min.Init.TotalHeigh = kHeight + kVsa + kVbp + kVfp - 1U;
    hltdc_display_min.Init.Backcolor.Red = 0U;
    hltdc_display_min.Init.Backcolor.Green = 0U;
    hltdc_display_min.Init.Backcolor.Blue = 0U;
    hltdc_display_min.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

    if (record_status(HAL_LTDC_Init(&hltdc_display_min)) != HAL_OK) {
        return static_cast<HAL_StatusTypeDef>(g_state.last_hal_status);
    }

    snapshot_regs();
    return HAL_OK;
}

HAL_StatusTypeDef short_write_no_param(const std::uint8_t command) {
    return record_status(HAL_DSI_ShortWrite(&hdsi_display_min,
                                            kVirtualChannel,
                                            DSI_DCS_SHORT_PKT_WRITE_P0,
                                            command,
                                            0U));
}

HAL_StatusTypeDef short_write_one_param(const std::uint8_t command, const std::uint8_t param) {
    return record_status(HAL_DSI_ShortWrite(&hdsi_display_min,
                                            kVirtualChannel,
                                            DSI_DCS_SHORT_PKT_WRITE_P1,
                                            command,
                                            param));
}

HAL_StatusTypeDef long_write(const std::uint8_t command, const std::uint8_t* data, const std::uint32_t len) {
    return record_status(HAL_DSI_LongWrite(&hdsi_display_min,
                                           kVirtualChannel,
                                           DSI_DCS_LONG_PKT_WRITE,
                                           len,
                                           command,
                                           data));
}

HAL_StatusTypeDef send_panel_command(const PanelCmd& cmd) {
    g_state.last_cmd = static_cast<std::uint8_t>(cmd.reg & 0xFFU);
    if (cmd.len == 0U) {
        return short_write_no_param(static_cast<std::uint8_t>(cmd.reg));
    }
    if (cmd.len == 1U) {
        return short_write_one_param(static_cast<std::uint8_t>(cmd.reg), cmd.data[0]);
    }
    return long_write(static_cast<std::uint8_t>(cmd.reg), cmd.data, cmd.len);
}

HAL_StatusTypeDef run_panel_init_table() {
    set_phase(DISPLAY_MIN_PHASE_PANEL_INIT);
    g_state.panel_cmd_ok = 0U;
    g_state.panel_cmd_fail = 0U;
    g_state.last_cmd = 0U;
    g_state.fail_cmd = 0U;

    const PanelCmd* table = selected_panel_init_table();
    for (std::size_t i = 0U; table[i].reg != kRegFlagEnd; ++i) {
        const PanelCmd& cmd = table[i];
        if (cmd.reg == kRegFlagDelay) {
            HAL_Delay(cmd.len);
            continue;
        }

        const HAL_StatusTypeDef status = send_panel_command(cmd);
        if (status != HAL_OK) {
            ++g_state.panel_cmd_fail;
            g_state.fail_cmd = static_cast<std::uint8_t>(cmd.reg & 0xFFU);
            snapshot_regs();
            return status;
        }
        ++g_state.panel_cmd_ok;
        HAL_Delay(1U);
    }

    snapshot_regs();
    return HAL_OK;
}

void set_ltdc_background_register(const std::uint32_t argb8888) {
    const std::uint32_t red = (argb8888 >> 16U) & 0xFFU;
    const std::uint32_t green = (argb8888 >> 8U) & 0xFFU;
    const std::uint32_t blue = argb8888 & 0xFFU;

    if (hltdc_display_min.Instance != nullptr) {
        hltdc_display_min.Instance->BCCR = (red << 16U) | (green << 8U) | blue;
        __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc_display_min);
    }
}

void reset_peripheral_blocks() {
    __HAL_RCC_DSI_FORCE_RESET();
    __HAL_RCC_LTDC_FORCE_RESET();
    __DSB();
    HAL_Delay(1U);
    __HAL_RCC_DSI_RELEASE_RESET();
    __HAL_RCC_LTDC_RELEASE_RESET();
    __DSB();
    HAL_Delay(1U);
}

} // namespace

extern "C" void HAL_DSI_MspInit(DSI_HandleTypeDef* dsiHandle) {
    if ((dsiHandle == nullptr) || (dsiHandle->Instance != DSI)) {
        return;
    }

    if (configure_dsi_clock_source() != HAL_OK) {
        Error_Handler();
    }

    __HAL_RCC_DSI_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_15;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF13_DSI;
    HAL_GPIO_Init(GPIOA, &gpio);
}

extern "C" void HAL_DSI_MspDeInit(DSI_HandleTypeDef* dsiHandle) {
    if ((dsiHandle == nullptr) || (dsiHandle->Instance != DSI)) {
        return;
    }

    __HAL_RCC_DSI_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_15);
}

extern "C" void HAL_LTDC_MspInit(LTDC_HandleTypeDef* ltdcHandle) {
    if ((ltdcHandle == nullptr) || (ltdcHandle->Instance != LTDC)) {
        return;
    }

    __HAL_RCC_LTDC_CLK_ENABLE();
}

extern "C" void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef* ltdcHandle) {
    if ((ltdcHandle == nullptr) || (ltdcHandle->Instance != LTDC)) {
        return;
    }

    __HAL_RCC_LTDC_CLK_DISABLE();
}

uint8_t display_min_init(void) {
    g_state = {};
    update_static_state();
    if (ensure_display_power_ready() == 0U) {
        snapshot_regs();
        set_phase(DISPLAY_MIN_PHASE_ERROR);
        return 0U;
    }
    set_phase(DISPLAY_MIN_PHASE_RESET);
    clear_sticky_flags();
    pulse_panel_reset();
    reset_peripheral_blocks();
    hdsi_display_min = {};
    hltdc_display_min = {};

    if (configure_dsi_video() != HAL_OK) {
        snapshot_regs();
        return 0U;
    }

    if (configure_ltdc_background_only() != HAL_OK) {
        snapshot_regs();
        return 0U;
    }

    if (record_status(HAL_DSI_Start(&hdsi_display_min)) != HAL_OK) {
        snapshot_regs();
        return 0U;
    }
    set_phase(DISPLAY_MIN_PHASE_DSI_STARTED);

    if (run_panel_init_table() != HAL_OK) {
        snapshot_regs();
        return 0U;
    }

    g_state.init_ok = 1U;
    snapshot_regs();
    return 1U;
}

uint8_t display_min_start_pattern(void) {
    if (hdsi_display_min.Instance == nullptr) {
        snapshot_regs();
        return 0U;
    }

    hdsi_display_min.Instance->WCR |= DSI_WCR_LTDCEN;
    const HAL_StatusTypeDef status = record_status(HAL_DSI_PatternGeneratorStart(&hdsi_display_min, 0U, 0U));
    g_state.pattern_on = (status == HAL_OK) ? 1U : 0U;
    if (status == HAL_OK) {
        set_phase(DISPLAY_MIN_PHASE_PATTERN);
    }
    snapshot_regs();
    return (status == HAL_OK) ? 1U : 0U;
}

uint8_t display_min_stop_pattern(void) {
    if (hdsi_display_min.Instance == nullptr) {
        snapshot_regs();
        return 0U;
    }

    const HAL_StatusTypeDef status = record_status(HAL_DSI_PatternGeneratorStop(&hdsi_display_min));
    if (status == HAL_OK) {
        g_state.pattern_on = 0U;
    }
    snapshot_regs();
    return (status == HAL_OK) ? 1U : 0U;
}

void display_min_set_background(const std::uint32_t argb8888) {
    if (hdsi_display_min.Instance != nullptr) {
        hdsi_display_min.Instance->WCR |= DSI_WCR_LTDCEN;
    }
    set_ltdc_background_register(argb8888);
    set_phase(DISPLAY_MIN_PHASE_BACKGROUND);
    snapshot_regs();
}

void display_min_poll(void) {
    snapshot_regs();
}

display_min_state_t display_min_state(void) {
    snapshot_regs();
    return g_state;
}

const char* display_min_panel_profile_name(const uint8_t profile) {
    switch (static_cast<display_min_panel_profile_t>(profile)) {
    case DISPLAY_MIN_PANEL_PROFILE_DTS_2LANE:
        return "dts_2lane";
    case DISPLAY_MIN_PANEL_PROFILE_GITHUB4LANE_2LANE:
        return "github4lane_2lane";
    default:
        return "unknown";
    }
}
