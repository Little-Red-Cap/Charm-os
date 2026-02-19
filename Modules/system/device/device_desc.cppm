module;

#include <cstdint>
#include <string_view>

export module device.desc;

import util.core;

export namespace device {
    struct DeviceDesc {
        util::u16 class_id{0};
        util::u16 vendor_id{0};
        util::u16 product_id{0};
        std::string_view type{};
    };
}
