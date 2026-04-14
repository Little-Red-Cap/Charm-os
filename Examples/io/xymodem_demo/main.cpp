#include <array>
#include <cstdio>
#include <span>
#include <string_view>
#include <vector>

import io.proto.modem_xymodem;
import util.core;

namespace {
    constexpr util::u8 kPad = 0x1Au;

    std::vector<util::u8> drain_tx(modem::XyModem<1024>& receiver) {
        std::array<util::u8, 16> buf{};
        std::vector<util::u8> out;
        while (receiver.has_tx()) {
            const auto n = receiver.take_tx(std::span<util::u8>(buf.data(), buf.size()));
            out.insert(out.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
        }
        return out;
    }

    std::vector<util::u8> make_header_frame(std::string_view file_name, util::u32 file_size) {
        std::array<util::u8, 128> payload{};
        util::usize pos = 0;
        for (; pos < file_name.size() && pos < payload.size(); ++pos) {
            payload[pos] = static_cast<util::u8>(file_name[pos]);
        }
        if (pos < payload.size()) {
            payload[pos++] = 0;
        }

        char digits[16]{};
        int digit_count = std::snprintf(digits, sizeof(digits), "%u", file_size);
        if (digit_count < 0) digit_count = 0;
        for (int i = 0; i < digit_count && pos < payload.size(); ++i) {
            payload[pos++] = static_cast<util::u8>(digits[i]);
        }

        std::vector<util::u8> frame;
        frame.reserve(3 + payload.size() + 2);
        frame.push_back(modem::SOH);
        frame.push_back(0x00);
        frame.push_back(0xFF);
        frame.insert(frame.end(), payload.begin(), payload.end());
        const auto crc = modem::crc16_ccitt(std::span<const util::u8>(payload.data(), payload.size()));
        frame.push_back(static_cast<util::u8>((crc >> 8) & 0xFFu));
        frame.push_back(static_cast<util::u8>(crc & 0xFFu));
        return frame;
    }

    std::vector<util::u8> make_data_frame(util::u8 seq, std::span<const util::u8> data) {
        std::array<util::u8, 1024> payload{};
        for (auto& byte : payload) byte = kPad;
        for (util::usize i = 0; i < data.size(); ++i) {
            payload[i] = data[i];
        }

        std::vector<util::u8> frame;
        frame.reserve(3 + payload.size() + 2);
        frame.push_back(modem::STX);
        frame.push_back(seq);
        frame.push_back(static_cast<util::u8>(~seq));
        frame.insert(frame.end(), payload.begin(), payload.end());
        const auto crc = modem::crc16_ccitt(std::span<const util::u8>(payload.data(), payload.size()));
        frame.push_back(static_cast<util::u8>((crc >> 8) & 0xFFu));
        frame.push_back(static_cast<util::u8>(crc & 0xFFu));
        return frame;
    }

    bool expect_bytes(std::span<const util::u8> actual, std::span<const util::u8> expected) {
        if (actual.size() != expected.size()) return false;
        for (util::usize i = 0; i < actual.size(); ++i) {
            if (actual[i] != expected[i]) return false;
        }
        return true;
    }
}

int main() {
    std::vector<util::u8> logical_payload(1500);
    for (util::usize i = 0; i < logical_payload.size(); ++i) {
        logical_payload[i] = static_cast<util::u8>((i * 13u + 7u) & 0xFFu);
    }

    std::vector<util::u8> protocol_payload;
    protocol_payload.reserve(2048);

    std::array<char, 64> file_name{};
    util::u32 file_size = 0;
    bool header_seen = false;
    struct ReceiverState {
        std::vector<util::u8>* payload;
        std::array<char, 64>* file_name;
        util::u32* file_size;
    } receiver_state{&protocol_payload, &file_name, &file_size};

    modem::XyModem<1024> receiver{};
    receiver.set_config(modem::Config{
        .timeout_ms = 200,
        .max_retries = 10,
        .use_1k = true
    });
    receiver.set_handlers(
        +[](void* ctx, std::span<const util::u8> data, util::usize len) noexcept {
            auto* state = static_cast<ReceiverState*>(ctx);
            state->payload->insert(state->payload->end(),
                                   data.begin(),
                                   data.begin() + static_cast<std::ptrdiff_t>(len));
        },
        &receiver_state,
        +[](void* ctx, std::string_view name, util::u32 size) noexcept {
            auto* state = static_cast<ReceiverState*>(ctx);
            auto& out_name = *state->file_name;
            out_name.fill('\0');
            const auto copy_len = (name.size() < (out_name.size() - 1)) ? name.size() : (out_name.size() - 1);
            for (util::usize i = 0; i < copy_len; ++i) {
                out_name[i] = name[i];
            }
            *state->file_size = size;
        });
    receiver.start();

    auto tx = drain_tx(receiver);
    const util::u8 want_crc[] = {modem::C};
    bool ok = expect_bytes(tx, std::span<const util::u8>(want_crc, 1));

    const auto header = make_header_frame("demo.bin", static_cast<util::u32>(logical_payload.size()));
    receiver.on_rx(std::span<const util::u8>(header.data(), header.size()));
    tx = drain_tx(receiver);
    const util::u8 want_header_ack[] = {modem::ACK, modem::C};
    ok = ok && expect_bytes(tx, std::span<const util::u8>(want_header_ack, 2));
    header_seen = true;

    util::u8 seq = 1;
    for (util::usize off = 0; off < logical_payload.size(); off += 1024) {
        const auto chunk = (logical_payload.size() - off > 1024)
            ? 1024
            : (logical_payload.size() - off);
        const auto frame = make_data_frame(
            seq++,
            std::span<const util::u8>(logical_payload.data() + off, chunk));
        receiver.on_rx(std::span<const util::u8>(frame.data(), frame.size()));
        tx = drain_tx(receiver);
        const util::u8 want_ack[] = {modem::ACK};
        ok = ok && expect_bytes(tx, std::span<const util::u8>(want_ack, 1));
    }

    const util::u8 eot[] = {modem::EOT};
    receiver.on_rx(std::span<const util::u8>(eot, 1));
    tx = drain_tx(receiver);
    const util::u8 want_eot_ack[] = {modem::ACK};
    ok = ok && expect_bytes(tx, std::span<const util::u8>(want_eot_ack, 1));

    const auto res = receiver.result();
    protocol_payload.resize(file_size);
    ok = ok &&
         header_seen &&
         file_size == logical_payload.size() &&
         protocol_payload == logical_payload &&
         static_cast<bool>(res) &&
         res.status == modem::Status::ok;

    std::printf("[xymodem] header=%s size=%u protocol_bytes=%u logical_bytes=%zu status=%u\n",
                file_name.data(),
                file_size,
                res.bytes,
                protocol_payload.size(),
                static_cast<unsigned>(res.status));
    std::printf("[xymodem] ok=%d\n", ok ? 1 : 0);
    return ok ? 0 : 1;
}
