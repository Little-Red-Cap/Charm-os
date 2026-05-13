#ifndef DAPLINK_BACKEND_HPP
#define DAPLINK_BACKEND_HPP

#include "platform/stm32/daplink_platform_stm32_backend_support.hpp"

namespace daplink::backend_target {
    struct Traits
        : daplink::platform::stm32::backend_support::UsbPcdBackend<
              hpcd_USB_FS,
              daplink::platform::stm32::backend_support::CubeMxUart12Backend<>> {};

    using Support = daplink::backend_support::BasicBackendOps<Traits>;
}

#include "port/daplink_backend_api.hpp"

#endif
