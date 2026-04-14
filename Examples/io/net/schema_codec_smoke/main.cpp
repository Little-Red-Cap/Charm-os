#include <array>
#include <cstdio>

import charm.net;
import util.core;

namespace {
    struct MixedMessage {
        util::u16 request_id{0};
        util::u8 flags{0};
        std::array<util::u8, 2> tag{};
    };

    struct LittleMessage {
        util::u16 value{0};
    };

    using MixedCodec = net::SchemaFieldCodec<
        MixedMessage,
        net::WireMembers<
            &MixedMessage::request_id,
            &MixedMessage::flags,
            &MixedMessage::tag>>;
    using LittleCodec = net::SchemaFieldCodec<
        LittleMessage,
        net::WireMembersLE<&LittleMessage::value>>;

    bool bytes_eq(net::ByteView lhs, net::ByteView rhs) noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (util::usize i = 0; i < lhs.size(); ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }
}

int main() {
    std::array<util::u8, MixedCodec::max_size()> mixed_buf{};
    const MixedMessage mixed_in{0x1234u, 0x5au, {'o', 'k'}};

    auto mixed_encoded = MixedCodec::encode(
        mixed_in,
        net::MutByteView{mixed_buf.data(), mixed_buf.size()});
    if (!mixed_encoded || mixed_encoded.value() != mixed_buf.size()) {
        std::fputs("schema codec mixed encode failed\n", stderr);
        return 1;
    }

    static constexpr util::u8 mixed_wire[]{0x12u, 0x34u, 0x5au, 'o', 'k'};
    if (!bytes_eq(net::ByteView{mixed_buf.data(), mixed_buf.size()},
                  net::ByteView{mixed_wire, sizeof(mixed_wire)})) {
        std::fputs("schema codec mixed wire mismatch\n", stderr);
        return 2;
    }

    auto mixed_decoded = MixedCodec::decode(net::ByteView{mixed_buf.data(), mixed_buf.size()});
    if (!mixed_decoded
        || mixed_decoded.value().request_id != mixed_in.request_id
        || mixed_decoded.value().flags != mixed_in.flags
        || mixed_decoded.value().tag != mixed_in.tag) {
        std::fputs("schema codec mixed decode failed\n", stderr);
        return 3;
    }

    std::array<util::u8, LittleCodec::max_size()> little_buf{};
    auto little_encoded = LittleCodec::encode(
        LittleMessage{0x1234u},
        net::MutByteView{little_buf.data(), little_buf.size()});
    if (!little_encoded) {
        std::fputs("schema codec little encode failed\n", stderr);
        return 4;
    }

    static constexpr util::u8 little_wire[]{0x34u, 0x12u};
    if (!bytes_eq(net::ByteView{little_buf.data(), little_buf.size()},
                  net::ByteView{little_wire, sizeof(little_wire)})) {
        std::fputs("schema codec little wire mismatch\n", stderr);
        return 5;
    }

    auto little_decoded = LittleCodec::decode(net::ByteView{little_buf.data(), little_buf.size()});
    if (!little_decoded || little_decoded.value().value != 0x1234u) {
        std::fputs("schema codec little decode failed\n", stderr);
        return 6;
    }

    auto bad_length = MixedCodec::decode(net::ByteView{mixed_buf.data(), mixed_buf.size() - 1u});
    if (bad_length || bad_length.error() != net::errc::format_error) {
        std::fputs("schema codec bad length check failed\n", stderr);
        return 7;
    }

    std::array<util::u8, 1> small{};
    auto overflow = MixedCodec::encode(mixed_in, net::MutByteView{small.data(), small.size()});
    if (overflow || overflow.error() != net::errc::buffer_overflow) {
        std::fputs("schema codec overflow check failed\n", stderr);
        return 8;
    }

    auto empty_decoded = net::WireSchemaCodec<net::EmptyMessage>::decode(net::ByteView{});
    if (!empty_decoded) {
        std::fputs("schema codec empty decode failed\n", stderr);
        return 9;
    }

    std::array<util::u8, 1> scratch{};
    auto empty_encoded = net::WireSchemaCodec<net::EmptyMessage>::encode(
        net::EmptyMessage{},
        net::MutByteView{scratch.data(), 0});
    if (!empty_encoded || empty_encoded.value() != 0u) {
        std::fputs("schema codec empty encode failed\n", stderr);
        return 10;
    }

    std::puts("net schema codec smoke: ok");
    return 0;
}
