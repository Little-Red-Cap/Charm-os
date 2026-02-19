module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <utility>

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

    struct DeviceDescriptor {
        u8 length{18};
        DescriptorType type{DescriptorType::device};
        u16 bcd_usb{0x0200};
        u8 device_class{0};
        u8 device_subclass{0};
        u8 device_protocol{0};
        u8 max_packet_size0{64};
        u16 vendor_id{0};
        u16 product_id{0};
        u16 bcd_device{0x0100};
        u8 manufacturer{0};
        u8 product{0};
        u8 serial_number{0};
        u8 num_configurations{1};
    };

    struct ConfigDescriptor {
        u8 length{9};
        DescriptorType type{DescriptorType::configuration};
        u16 total_length{0};
        u8 num_interfaces{0};
        u8 configuration_value{1};
        u8 configuration{0};
        u8 attributes{0x80};
        u8 max_power{50}; // 100mA
    };

    struct InterfaceDescriptor {
        u8 length{9};
        DescriptorType type{DescriptorType::interface};
        u8 interface_number{0};
        u8 alternate_setting{0};
        u8 num_endpoints{0};
        u8 interface_class{0};
        u8 interface_subclass{0};
        u8 interface_protocol{0};
        u8 interface{0};
    };

    struct InterfaceAssociationDescriptor {
        u8 length{8};
        DescriptorType type{static_cast<DescriptorType>(0x0B)};
        u8 first_interface{0};
        u8 interface_count{0};
        u8 function_class{0};
        u8 function_subclass{0};
        u8 function_protocol{0};
        u8 function{0};
    };

    struct EndpointDescriptor {
        u8 length{7};
        DescriptorType type{DescriptorType::endpoint};
        u8 endpoint_address{0};
        u8 attributes{0};
        u16 max_packet_size{0};
        u8 interval{0};
    };

    struct DeviceQualifierDescriptor {
        u8 length{10};
        DescriptorType type{DescriptorType::device_qualifier};
        u16 bcd_usb{0x0200};
        u8 device_class{0};
        u8 device_subclass{0};
        u8 device_protocol{0};
        u8 max_packet_size0{64};
        u8 num_configurations{1};
        u8 reserved{0};
    };

    struct BosDescriptor {
        u8 length{5};
        DescriptorType type{DescriptorType::bos};
        u16 total_length{0};
        u8 num_caps{0};
    };

    struct DeviceCapabilityHeader {
        u8 length{0};
        u8 type{0x10};
        u8 capability{0};
    };

    struct StringDescriptorHeader {
        u8 length{2};
        DescriptorType type{DescriptorType::string};
    };

    struct DescriptorWriter {
        std::span<u8> buffer{};
        std::size_t offset{0};

        bool write_bytes(std::span<const u8> data) noexcept {
            if (offset + data.size() > buffer.size()) return false;
            std::memcpy(buffer.data() + offset, data.data(), data.size());
            offset += data.size();
            return true;
        }

        template <typename T>
        bool write_object(const T& obj) noexcept {
            return write_bytes(std::span<const u8>(
                reinterpret_cast<const u8*>(&obj), sizeof(T)));
        }
    };

    struct DescriptorBuilder {
        DescriptorWriter writer{};
        std::size_t config_offset{static_cast<std::size_t>(-1)};
        u8 interface_count{0};
        bool interface_count_override{false};
        u8 interface_count_value{0};

        explicit DescriptorBuilder(std::span<u8> buffer) noexcept { writer.buffer = buffer; }

        void set_interface_count(u8 value) noexcept {
            interface_count_override = true;
            interface_count_value = value;
        }

        bool begin_config(const ConfigDescriptor& cfg) noexcept {
            config_offset = writer.offset;
            interface_count = 0;
            return writer.write_object(cfg);
        }

        bool add_interface(const InterfaceDescriptor& desc) noexcept {
            interface_count = static_cast<u8>(interface_count + 1);
            return writer.write_object(desc);
        }

        bool add_endpoint(const EndpointDescriptor& desc) noexcept {
            return writer.write_object(desc);
        }

        template <typename T>
        bool add_class_descriptor(const T& desc) noexcept {
            return writer.write_object(desc);
        }

        bool end_config() noexcept {
            if (config_offset == static_cast<std::size_t>(-1)) return false;
            if (config_offset + sizeof(ConfigDescriptor) > writer.buffer.size()) return false;
            auto* cfg = reinterpret_cast<ConfigDescriptor*>(writer.buffer.data() + config_offset);
            cfg->total_length = static_cast<u16>(writer.offset - config_offset);
            cfg->num_interfaces = interface_count_override ? interface_count_value : interface_count;
            return true;
        }
    };

    inline bool write_lang_id_descriptor(DescriptorWriter& writer, u16 lang_id) noexcept {
        StringDescriptorHeader hdr{};
        hdr.length = static_cast<u8>(2 + sizeof(lang_id));
        if (!writer.write_object(hdr)) return false;
        return writer.write_object(lang_id);
    }

    inline bool write_ascii_string_descriptor(DescriptorWriter& writer, std::string_view text) noexcept {
        const auto utf16_bytes = text.size() * 2;
        if (utf16_bytes > 254) return false;
        StringDescriptorHeader hdr{};
        hdr.length = static_cast<u8>(2 + utf16_bytes);
        if (!writer.write_object(hdr)) return false;
        for (char ch : text) {
            const u16 le = static_cast<u8>(ch);
            if (!writer.write_object(le)) return false;
        }
        return true;
    }

    inline bool write_utf16_string_descriptor(DescriptorWriter& writer, std::u16string_view text) noexcept {
        const auto utf16_bytes = text.size() * 2;
        if (utf16_bytes > 254) return false;
        StringDescriptorHeader hdr{};
        hdr.length = static_cast<u8>(2 + utf16_bytes);
        if (!writer.write_object(hdr)) return false;
        for (char16_t ch : text) {
            const u16 le = static_cast<u16>(ch);
            if (!writer.write_object(le)) return false;
        }
        return true;
    }

    inline constexpr std::pair<u8, u16> utf16le_unit(char16_t ch) noexcept {
        return { static_cast<u8>(2), static_cast<u16>(ch) };
    }

    inline constexpr u8 utf16le_length(std::u16string_view text) noexcept {
        const auto bytes = text.size() * 2;
        return (bytes > 254) ? static_cast<u8>(0) : static_cast<u8>(bytes);
    }

    template <std::size_t N>
    consteval auto make_ascii_string_descriptor(const char (&text)[N]) {
        constexpr std::size_t len = (N > 0) ? (N - 1) : 0;
        static_assert(len <= 127, "USB string descriptor too long");
        std::array<u8, 2 + len * 2> out{};
        out[0] = static_cast<u8>(2 + len * 2);
        out[1] = static_cast<u8>(DescriptorType::string);
        for (std::size_t i = 0; i < len; ++i) {
            out[2 + i * 2] = static_cast<u8>(text[i]);
            out[2 + i * 2 + 1] = 0;
        }
        return out;
    }

    template <std::size_t N>
    consteval auto make_utf16_string_descriptor(const char16_t (&text)[N]) {
        constexpr std::size_t len = (N > 0) ? (N - 1) : 0;
        static_assert(len <= 127, "USB string descriptor too long");
        std::array<u8, 2 + len * 2> out{};
        out[0] = static_cast<u8>(2 + len * 2);
        out[1] = static_cast<u8>(DescriptorType::string);
        for (std::size_t i = 0; i < len; ++i) {
            const auto ch = static_cast<u16>(text[i]);
            out[2 + i * 2] = static_cast<u8>(ch & 0xFF);
            out[2 + i * 2 + 1] = static_cast<u8>((ch >> 8) & 0xFF);
        }
        return out;
    }

    template <std::size_t N>
    struct StringTable {
        std::array<std::span<const u8>, N> entries{};
        constexpr std::span<const u8> operator[](std::size_t idx) const noexcept {
            return (idx < N) ? entries[idx] : std::span<const u8>{};
        }
    };

    constexpr u8 make_request_type(RequestDirection dir, RequestType type, RequestRecipient recip) noexcept {
        return static_cast<u8>(static_cast<u8>(dir)
            | static_cast<u8>(type)
            | static_cast<u8>(recip));
    }

    constexpr RequestDirection request_direction(u8 bm_request_type) noexcept {
        return static_cast<RequestDirection>(bm_request_type & 0x80);
    }

    constexpr RequestType request_type(u8 bm_request_type) noexcept {
        return static_cast<RequestType>(bm_request_type & 0x60);
    }

    constexpr RequestRecipient request_recipient(u8 bm_request_type) noexcept {
        return static_cast<RequestRecipient>(bm_request_type & 0x1F);
    }
} // namespace usb
