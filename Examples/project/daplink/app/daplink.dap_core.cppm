module;

export module daplink.dap_core;

import daplink.cmsis_dap;

export namespace daplink::dap_core {
    using daplink::cmsis_dap::kPacketSize;
    using daplink::cmsis_dap::kPacketCount;
    using daplink::cmsis_dap::InfoField;
    using daplink::cmsis_dap::DeviceInfo;
    using daplink::cmsis_dap::State;
    using daplink::cmsis_dap::make_info_field;
    using daplink::cmsis_dap::process_packet;
}
