#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "port/stm32/daplink_backend_stm32_support.hpp"

namespace daplink::backend_target {
    struct Traits
        : daplink::backend_support::stm32::UsbPcdBackend<
              hpcd_USB_FS,
              daplink::backend_support::stm32::CubeMxUart12Backend<>> {};

    using Support = daplink::backend_support::BasicBackendOps<Traits>;
}

#include "port/daplink_backend_api.hpp"

#endif
