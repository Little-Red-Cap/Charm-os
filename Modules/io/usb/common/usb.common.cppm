module;

#include <cstddef>
#include <cstdint>

export module usb.common;

export namespace usb {
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;

    enum class Speed : u8 {
        full,
        high,
        super_speed,
    };

    enum class DescriptorType : u8 {
        device = 1,
        configuration = 2,
        string = 3,
        interface = 4,
        endpoint = 5,
        device_qualifier = 6,
        other_speed_configuration = 7,
        interface_power = 8,
        bos = 0x0F,
    };

    enum class EndpointType : u8 {
        control = 0,
        isochronous = 1,
        bulk = 2,
        interrupt = 3,
    };

    enum class RequestDirection : u8 {
        out = 0x00,
        in = 0x80,
    };

    enum class RequestType : u8 {
        standard = 0x00,
        class_request = 0x20,
        vendor = 0x40,
    };

    enum class RequestRecipient : u8 {
        device = 0x00,
        interface = 0x01,
        endpoint = 0x02,
        other = 0x03,
    };

    enum class StandardRequest : u8 {
        get_status = 0,
        clear_feature = 1,
        set_feature = 3,
        set_address = 5,
        get_descriptor = 6,
        set_descriptor = 7,
        get_configuration = 8,
        set_configuration = 9,
        get_interface = 10,
        set_interface = 11,
        syn_frame = 12,
    };

    struct SetupPacket {
        u8 bm_request_type{0};
        u8 b_request{0};
        u16 w_value{0};
        u16 w_index{0};
        u16 w_length{0};
    };

    struct DescriptorHeader {
        u8 length{0};
        DescriptorType type{DescriptorType::device};
    };

    constexpr u8 make_request_type(RequestDirection dir, RequestType type, RequestRecipient recip) noexcept {
        return static_cast<u8>(static_cast<u8>(dir)
            | static_cast<u8>(type)
            | static_cast<u8>(recip));
    }
} // namespace usb
