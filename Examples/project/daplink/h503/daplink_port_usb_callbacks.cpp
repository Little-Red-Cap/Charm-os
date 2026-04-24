#include "daplink_port_api.hpp"

import daplink.usb_minimal;

extern "C" void HAL_PCD_ResetCallback(daplink::port::UsbPcdHandle* hpcd) {
    daplink::usb_minimal::on_reset(*hpcd);
}

extern "C" void HAL_PCD_SetupStageCallback(daplink::port::UsbPcdHandle* hpcd) {
    daplink::usb_minimal::on_setup_stage(*hpcd);
}

extern "C" void HAL_PCD_DataOutStageCallback(daplink::port::UsbPcdHandle* hpcd, std::uint8_t epnum) {
    daplink::usb_minimal::on_data_out_stage(*hpcd, epnum);
}

extern "C" void HAL_PCD_DataInStageCallback(daplink::port::UsbPcdHandle* hpcd, std::uint8_t epnum) {
    daplink::usb_minimal::on_data_in_stage(*hpcd, epnum);
}
