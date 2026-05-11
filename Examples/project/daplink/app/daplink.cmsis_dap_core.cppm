module;

#include <cstddef>
#include <cstdint>

export module daplink.cmsis_dap:core;

import daplink.app_config;

export namespace daplink::cmsis_dap {
    constexpr std::size_t kPacketSize = 64;
    constexpr std::uint8_t kPacketCount = daplink::app_config::kConfig.dap.packet_count;

    struct InfoField {
        const char* data;
        std::uint8_t size;
    };

    struct DeviceInfo {
        InfoField vendor;
        InfoField product;
        InfoField serial;
        InfoField protocol_version;
        InfoField product_fw_version;
    };

    template <std::size_t N>
    constexpr InfoField make_info_field(const char (&text)[N]) noexcept {
        static_assert(N > 0);
        static_assert(N <= 255);
        return {text, static_cast<std::uint8_t>(N)};
    }

}
