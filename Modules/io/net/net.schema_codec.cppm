module;

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>

export module net.schema_codec;

export import net.common;
import util.core;
import util.error;
import util.expected;

export namespace net {
    enum class WireEndian : util::u8 {
        big,
        little,
    };
}

namespace net::schema_detail {
    template <class T>
    struct member_pointer_traits;

    template <class Owner, class Value>
    struct member_pointer_traits<Value Owner::*> {
        using owner = Owner;
        using value = Value;
    };

    template <class T>
    struct byte_blob_traits {
        static constexpr bool enabled = false;
    };

    template <class Element, std::size_t N>
    struct byte_blob_traits<Element[N]> {
        static constexpr bool enabled = sizeof(Element) == 1
            && std::is_trivially_copyable_v<Element>;
        static constexpr util::usize size = N;

        [[nodiscard]] static const Element* data(const Element (&value)[N]) noexcept {
            return value;
        }

        [[nodiscard]] static Element* data(Element (&value)[N]) noexcept {
            return value;
        }
    };

    template <class Element, std::size_t N>
    struct byte_blob_traits<std::array<Element, N>> {
        static constexpr bool enabled = sizeof(Element) == 1
            && std::is_trivially_copyable_v<Element>;
        static constexpr util::usize size = N;

        [[nodiscard]] static const Element* data(const std::array<Element, N>& value) noexcept {
            return value.data();
        }

        [[nodiscard]] static Element* data(std::array<Element, N>& value) noexcept {
            return value.data();
        }
    };

    template <class T>
    inline constexpr bool is_byte_blob_v = byte_blob_traits<std::remove_cv_t<T>>::enabled;

    template <class T, bool IsEnum = std::is_enum_v<T>>
    struct wire_scalar_storage {
        using type = T;
    };

    template <class T>
    struct wire_scalar_storage<T, true> {
        using type = std::underlying_type_t<T>;
    };

    template <class T>
    using wire_scalar_storage_t = typename wire_scalar_storage<T>::type;

    template <class T>
    inline constexpr bool is_wire_scalar_v =
        (std::is_integral_v<wire_scalar_storage_t<T>> || std::is_enum_v<T>)
        && !is_byte_blob_v<T>;

    template <class T>
    using wire_unsigned_t = std::make_unsigned_t<wire_scalar_storage_t<T>>;

    template <class T>
    [[nodiscard]] Result<void> decode_blob(T& value, ByteView payload) noexcept {
        using traits = byte_blob_traits<std::remove_cv_t<T>>;
        if (payload.size() != traits::size) {
            return util::unexpected(errc::format_error);
        }

        if constexpr (traits::size != 0) {
            std::memcpy(traits::data(value), payload.data(), traits::size);
        }
        return {};
    }

    template <class T>
    [[nodiscard]] Result<void> encode_blob(const T& value, MutByteView payload) noexcept {
        using traits = byte_blob_traits<std::remove_cv_t<T>>;
        if (payload.size() < traits::size) {
            return util::unexpected(errc::buffer_overflow);
        }

        if constexpr (traits::size != 0) {
            std::memcpy(payload.data(), traits::data(value), traits::size);
        }
        return {};
    }

    template <class T>
    [[nodiscard]] Result<void> decode_scalar(T& value,
                                             ByteView payload,
                                             net::WireEndian order) noexcept {
        using storage_type = wire_unsigned_t<T>;

        if (payload.size() != sizeof(T)) {
            return util::unexpected(errc::format_error);
        }

        storage_type raw = 0;
        if (order == net::WireEndian::big) {
            for (util::usize i = 0; i < payload.size(); ++i) {
                raw = static_cast<storage_type>(
                    (raw << 8) | static_cast<storage_type>(payload[i]));
            }
        } else {
            for (util::usize i = 0; i < payload.size(); ++i) {
                raw = static_cast<storage_type>(
                    raw | (static_cast<storage_type>(payload[i]) << (i * 8)));
            }
        }

        value = static_cast<T>(raw);
        return {};
    }

    template <class T>
    [[nodiscard]] Result<void> encode_scalar(const T& value,
                                             MutByteView payload,
                                             net::WireEndian order) noexcept {
        using storage_type = wire_unsigned_t<T>;

        if (payload.size() < sizeof(T)) {
            return util::unexpected(errc::buffer_overflow);
        }

        const storage_type raw = static_cast<storage_type>(value);
        if (order == net::WireEndian::big) {
            for (util::usize i = 0; i < sizeof(T); ++i) {
                const util::usize shift = (sizeof(T) - 1u - i) * 8u;
                payload[i] = static_cast<util::u8>((raw >> shift) & 0xffu);
            }
        } else {
            for (util::usize i = 0; i < sizeof(T); ++i) {
                const util::usize shift = i * 8u;
                payload[i] = static_cast<util::u8>((raw >> shift) & 0xffu);
            }
        }
        return {};
    }
}

export namespace net {
    struct EmptyMessage {};

    struct EmptyCodec {
        [[nodiscard]] static constexpr util::usize max_size() noexcept {
            return 0;
        }

        [[nodiscard]] static Result<EmptyMessage> decode(ByteView payload) noexcept {
            if (!payload.empty()) {
                return util::unexpected(errc::format_error);
            }
            return EmptyMessage{};
        }

        [[nodiscard]] static Result<util::usize> encode(const EmptyMessage&,
                                                        MutByteView payload) noexcept {
            (void)payload;
            return 0u;
        }
    };

    template <class T>
    struct TrivialCodec {
        static_assert(std::is_trivially_copyable_v<T>);

        [[nodiscard]] static constexpr util::usize max_size() noexcept {
            return sizeof(T);
        }

        [[nodiscard]] static Result<T> decode(ByteView payload) noexcept {
            if (payload.size() != sizeof(T)) {
                return util::unexpected(errc::format_error);
            }

            T value{};
            if constexpr (sizeof(T) != 0) {
                std::memcpy(&value, payload.data(), sizeof(T));
            }
            return value;
        }

        [[nodiscard]] static Result<util::usize> encode(const T& value,
                                                        MutByteView payload) noexcept {
            if (payload.size() < sizeof(T)) {
                return util::unexpected(errc::buffer_overflow);
            }

            if constexpr (sizeof(T) != 0) {
                std::memcpy(payload.data(), &value, sizeof(T));
            }
            return sizeof(T);
        }
    };

    template <auto Member, WireEndian Order = WireEndian::big>
    struct WireField {
        using owner_type = typename schema_detail::member_pointer_traits<decltype(Member)>::owner;
        using member_type = typename schema_detail::member_pointer_traits<decltype(Member)>::value;
        using raw_member_type = std::remove_cv_t<member_type>;

        static constexpr util::usize size = []() consteval {
            if constexpr (schema_detail::is_byte_blob_v<raw_member_type>) {
                return schema_detail::byte_blob_traits<raw_member_type>::size;
            } else {
                static_assert(schema_detail::is_wire_scalar_v<raw_member_type>,
                              "net::WireField only supports integral/enum members "
                              "or fixed-size byte arrays");
                return sizeof(raw_member_type);
            }
        }();

        [[nodiscard]] static Result<void> decode(owner_type& owner,
                                                 ByteView payload) noexcept {
            if (payload.size() != size) {
                return util::unexpected(errc::format_error);
            }

            if constexpr (schema_detail::is_byte_blob_v<raw_member_type>) {
                return schema_detail::decode_blob(owner.*Member, payload);
            } else {
                return schema_detail::decode_scalar(owner.*Member, payload, Order);
            }
        }

        [[nodiscard]] static Result<void> encode(const owner_type& owner,
                                                 MutByteView payload) noexcept {
            if (payload.size() < size) {
                return util::unexpected(errc::buffer_overflow);
            }

            auto field_payload = payload.subspan(0, size);
            if constexpr (schema_detail::is_byte_blob_v<raw_member_type>) {
                return schema_detail::encode_blob(owner.*Member, field_payload);
            } else {
                return schema_detail::encode_scalar(owner.*Member, field_payload, Order);
            }
        }
    };

    template <auto Member>
    using WireFieldBE = WireField<Member, WireEndian::big>;

    template <auto Member>
    using WireFieldLE = WireField<Member, WireEndian::little>;

    template <class T, class... Fields>
    struct SchemaCodec {
        static_assert(std::is_default_constructible_v<T>);
        static_assert((std::is_same_v<typename Fields::owner_type, T> && ... && true),
                      "net::SchemaCodec fields must all belong to T");

        [[nodiscard]] static constexpr util::usize max_size() noexcept {
            return (Fields::size + ... + 0u);
        }

        [[nodiscard]] static Result<T> decode(ByteView payload) noexcept {
            if (payload.size() != max_size()) {
                return util::unexpected(errc::format_error);
            }

            T value{};
            util::usize offset = 0;
            errc error = errc::ok;
            const bool ok = (decode_one<Fields>(value, payload, offset, error) && ... && true);
            if (!ok) {
                return util::unexpected(error);
            }
            return value;
        }

        [[nodiscard]] static Result<util::usize> encode(const T& value,
                                                        MutByteView payload) noexcept {
            if (payload.size() < max_size()) {
                return util::unexpected(errc::buffer_overflow);
            }

            util::usize offset = 0;
            errc error = errc::ok;
            const bool ok = (encode_one<Fields>(value, payload, offset, error) && ... && true);
            if (!ok) {
                return util::unexpected(error);
            }
            return max_size();
        }

    private:
        template <class Field>
        [[nodiscard]] static bool decode_one(T& value,
                                             ByteView payload,
                                             util::usize& offset,
                                             errc& error) noexcept {
            auto decoded = Field::decode(value, payload.subspan(offset, Field::size));
            if (!decoded) {
                error = decoded.error();
                return false;
            }
            offset += Field::size;
            return true;
        }

        template <class Field>
        [[nodiscard]] static bool encode_one(const T& value,
                                             MutByteView payload,
                                             util::usize& offset,
                                             errc& error) noexcept {
            auto encoded = Field::encode(value, payload.subspan(offset, Field::size));
            if (!encoded) {
                error = encoded.error();
                return false;
            }
            offset += Field::size;
            return true;
        }
    };

    template <class T, auto... Members>
    using WireSchemaCodec = SchemaCodec<T, WireField<Members>...>;

    template <class T, auto... Members>
    using WireSchemaCodecLE = SchemaCodec<T, WireFieldLE<Members>...>;

    template <class... Fields>
    struct WireFields {};

    template <class T, class Fields>
    struct SchemaFieldCodec;

    template <class T, class... Fields>
    struct SchemaFieldCodec<T, WireFields<Fields...>> : SchemaCodec<T, Fields...> {};

    template <auto... Members>
    using WireMembers = WireFields<WireField<Members>...>;

    template <auto... Members>
    using WireMembersLE = WireFields<WireFieldLE<Members>...>;
}
