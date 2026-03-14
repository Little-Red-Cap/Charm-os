#include "main.h"

#include <cstdint>

import daplink.board;
import daplink.usb_minimal;
import daplink.cmsis_dap;

extern "C" void SystemClock_Config(void);
extern "C" void MPU_Config(void);

extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_reset(*hpcd);
}

extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    daplink::usb_minimal::on_setup_stage(*hpcd);
}

extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    daplink::usb_minimal::on_data_out_stage(*hpcd, epnum);
}

extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    daplink::usb_minimal::on_data_in_stage(*hpcd, epnum);
}

int main()
{
    HAL_Init();
    SystemClock_Config();

    if (!daplink::board::init_peripherals()) {
        Error_Handler();
    }
    daplink::board::configure_debug_pins_hi_z();

    static_assert(daplink::usb_minimal::hid_packet_size == daplink::cmsis_dap::kPacketSize);
    daplink::cmsis_dap::State dap_state{};
    constexpr char kVendor[] = "Charm";
    constexpr char kProduct[] = "Charm CMSIS-DAP";
    constexpr char kSerial[] = "0001";
    constexpr char kFw[] = "0.1.0";
    const daplink::cmsis_dap::DeviceInfo kInfo{
        daplink::cmsis_dap::make_info_field(kVendor),
        daplink::cmsis_dap::make_info_field(kProduct),
        daplink::cmsis_dap::make_info_field(kSerial),
        daplink::cmsis_dap::make_info_field(kFw)
    };

    while (true) {
        if (daplink::usb_minimal::take_reset()) {
            dap_state = {};
        }
#if (CHARM_DAP_USB_PROFILE != 1)
        if (daplink::usb_minimal::out_ready()) {
            auto in = daplink::usb_minimal::out_packet();
            auto out = daplink::usb_minimal::in_packet();
            daplink::cmsis_dap::process_packet<daplink::board::SwdBackend>(dap_state, kInfo, in, out);
            daplink::usb_minimal::send_in_packet(static_cast<std::uint16_t>(daplink::cmsis_dap::kPacketSize));
            daplink::usb_minimal::consume_out();
        }
#endif
#if (CHARM_DAP_USB_PROFILE != 0)
        if (daplink::usb_minimal::cdc_out_ready()) {
            const auto payload = daplink::usb_minimal::cdc_out_packet();
            if (!payload.empty()) {
                (void)daplink::usb_minimal::cdc_send_in(payload.data(),
                                                       static_cast<std::uint16_t>(payload.size()));
            }
            daplink::usb_minimal::cdc_consume_out();
        }
#endif
    }
}
