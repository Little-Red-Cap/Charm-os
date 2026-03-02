module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module io.proto.modem_xymodem;

import util.core;
import io.channel;

export namespace modem {
    enum class Status : util::u8 {
        ok,
        timeout,
        cancel,
        crc_error,
        format_error,
        io_error,
        retries_exhausted,
    };

    struct Result {
        Status status{Status::ok};
        util::u32 bytes{0};
        constexpr explicit operator bool() const noexcept { return status == Status::ok; }
    };

    struct Config {
        util::u32 timeout_ms{1000};
        util::u8 max_retries{10};
        bool use_1k{true};
    };

    constexpr util::u8 SOH = 0x01;
    constexpr util::u8 STX = 0x02;
    constexpr util::u8 EOT = 0x04;
    constexpr util::u8 ACK = 0x06;
    constexpr util::u8 NAK = 0x15;
    constexpr util::u8 CAN = 0x18;
    constexpr util::u8 C = 0x43;

    enum class ReadStatus : util::u8 {
        ok,
        timeout,
        io_error,
    };

    inline util::u16 crc16_ccitt(std::span<const util::u8> data) noexcept {
        util::u16 crc = 0;
        for (util::usize i = 0; i < data.size(); ++i) {
            crc ^= static_cast<util::u16>(data[i]) << 8;
            for (int b = 0; b < 8; ++b) {
                crc = (crc & 0x8000u) ? (crc << 1) ^ 0x1021u : (crc << 1);
            }
        }
        return crc;
    }

    inline ReadStatus read_byte(io::Channel& ch, util::u8& out, util::u32 timeout_ms) noexcept {
        util::u32 spins = 0;
        const auto start = io::now_ms();
        while (true) {
            auto r = ch.read(io::MutByteView{&out, 1});
            if (r) {
                if (*r == 1) return ReadStatus::ok;
            } else {
                const auto e = r.error();
                if (e == io::errc::io_error || e == io::errc::closed || e == io::errc::invalid) {
                    return ReadStatus::io_error;
                }
            }

            if (timeout_ms == 0) return ReadStatus::timeout;
            const auto now = io::now_ms();
            if (start != 0 && now != 0) {
                if (static_cast<util::u32>(now - start) >= timeout_ms) return ReadStatus::timeout;
            } else {
                if (++spins >= timeout_ms * 1000u) return ReadStatus::timeout;
            }
        }
    }

    inline bool write_byte(io::Channel& ch, util::u8 byte) noexcept {
        auto r = ch.write(io::ByteView{&byte, 1});
        return r && *r == 1;
    }

    inline bool write_data(io::Channel& ch, std::span<const util::u8> data) noexcept {
        util::usize sent = 0;
        util::u32 guard = 0;
        while (sent < data.size()) {
            auto r = ch.write(io::ByteView{data.data() + sent, data.size() - sent});
            if (!r) return false;
            if (*r == 0) {
                if (++guard > 4) return false;
                continue;
            }
            sent += *r;
        }
        return true;
    }

    using HeaderFn = void (*)(void* ctx, std::string_view name, util::u32 size) noexcept;

    template <util::usize MaxBlock>
    Result receive(io::Channel& ch, const Config& cfg,
                   void (*on_block)(void* ctx, std::span<const util::u8> data, util::usize len) noexcept,
                   void* out_ctx,
                   HeaderFn on_header = nullptr) noexcept {
        const util::usize block_len = cfg.use_1k ? 1024 : 128;
        if (block_len > MaxBlock) return Result{Status::format_error, 0};

        util::u8 blk = 1;
        util::u32 total = 0;
        util::u8 retries = 0;

        if (!write_byte(ch, C)) return Result{Status::io_error, 0};

        std::array<util::u8, MaxBlock> block{};
        while (true) {
            util::u8 byte{};
            const auto r = read_byte(ch, byte, cfg.timeout_ms);
            if (r == ReadStatus::timeout) {
                if (++retries > cfg.max_retries) return Result{Status::timeout, total};
                (void)write_byte(ch, C);
                continue;
            }
            if (r == ReadStatus::io_error) return Result{Status::io_error, total};

            if (byte == CAN) return Result{Status::cancel, total};
            if (byte == EOT) {
                (void)write_byte(ch, ACK);
                return Result{Status::ok, total};
            }

            if (byte != SOH && byte != STX) {
                (void)write_byte(ch, NAK);
                continue;
            }

            const util::usize expect_len = (byte == STX) ? 1024 : 128;
            if (expect_len > MaxBlock) return Result{Status::format_error, total};

            util::u8 seq = 0;
            util::u8 seq_inv = 0;
            if (read_byte(ch, seq, cfg.timeout_ms) != ReadStatus::ok) return Result{Status::timeout, total};
            if (read_byte(ch, seq_inv, cfg.timeout_ms) != ReadStatus::ok) return Result{Status::timeout, total};

            for (util::usize i = 0; i < expect_len; ++i) {
                if (read_byte(ch, block[i], cfg.timeout_ms) != ReadStatus::ok) return Result{Status::timeout, total};
            }

            util::u8 crc_hi = 0;
            util::u8 crc_lo = 0;
            if (read_byte(ch, crc_hi, cfg.timeout_ms) != ReadStatus::ok) return Result{Status::timeout, total};
            if (read_byte(ch, crc_lo, cfg.timeout_ms) != ReadStatus::ok) return Result{Status::timeout, total};
            const util::u16 got_crc = (static_cast<util::u16>(crc_hi) << 8) | crc_lo;
            const util::u16 calc_crc = crc16_ccitt(std::span<const util::u8>(block.data(), expect_len));

            if (seq != static_cast<util::u8>(~seq_inv)) {
                (void)write_byte(ch, NAK);
                continue;
            }
            if (got_crc != calc_crc) {
                (void)write_byte(ch, NAK);
                continue;
            }

            if (seq == 0 && on_header) {
                const char* base = reinterpret_cast<const char*>(block.data());
                util::usize name_len = 0;
                while (name_len < expect_len && base[name_len] != '\0') {
                    ++name_len;
                }
                if (name_len == 0) {
                    (void)write_byte(ch, ACK);
                    return Result{Status::ok, total};
                }
                const std::string_view fname{base, name_len};
                util::usize pos = name_len + 1;
                util::u32 fsize = 0;
                while (pos < expect_len) {
                    const char chp = base[pos];
                    if (chp < '0' || chp > '9') break;
                    fsize = static_cast<util::u32>(fsize * 10u + static_cast<util::u32>(chp - '0'));
                    ++pos;
                }
                on_header(out_ctx, fname, fsize);
                (void)write_byte(ch, ACK);
                (void)write_byte(ch, C);
                retries = 0;
                continue;
            }
            if (seq == static_cast<util::u8>(blk - 1)) {
                (void)write_byte(ch, ACK);
                continue;
            }
            if (seq == blk) {
                if (on_block) on_block(out_ctx, std::span<const util::u8>(block.data(), expect_len), expect_len);
                total += static_cast<util::u32>(expect_len);
                ++blk;
            }
            (void)write_byte(ch, ACK);
            retries = 0;
        }
    }

    template <util::usize MaxBlock>
    Result send(io::Channel& ch, const Config& cfg,
                bool (*next_block)(void* ctx, std::span<util::u8> out, util::usize& len) noexcept,
                void* in_ctx) noexcept {
        const util::usize block_len = cfg.use_1k ? 1024 : 128;
        if (block_len > MaxBlock) return Result{Status::format_error, 0};

        util::u32 total = 0;
        util::u8 blk = 1;
        util::u8 retries = 0;

        util::u8 byte{};
        while (true) {
            const auto r = read_byte(ch, byte, cfg.timeout_ms);
            if (r == ReadStatus::timeout) {
                if (++retries > cfg.max_retries) return Result{Status::timeout, total};
                continue;
            }
            if (r == ReadStatus::io_error) return Result{Status::io_error, total};
            if (byte == C || byte == NAK) break;
            if (byte == CAN) return Result{Status::cancel, total};
        }

        std::array<util::u8, MaxBlock> block{};
        while (true) {
            util::usize len = 0;
            if (!next_block(in_ctx, std::span<util::u8>(block.data(), block_len), len)) {
                (void)write_byte(ch, EOT);
                if (read_byte(ch, byte, cfg.timeout_ms) != ReadStatus::ok) return Result{Status::timeout, total};
                if (byte == ACK) return Result{Status::ok, total};
                return Result{Status::io_error, total};
            }
            if (len < block_len) {
                for (util::usize i = len; i < block_len; ++i) block[i] = 0x1A;
            }

            const util::u8 head = (block_len == 1024) ? STX : SOH;
            (void)write_byte(ch, head);
            (void)write_byte(ch, blk);
            (void)write_byte(ch, static_cast<util::u8>(~blk));
            if (!write_data(ch, std::span<const util::u8>(block.data(), block_len))) {
                return Result{Status::io_error, total};
            }
            const util::u16 crc = crc16_ccitt(std::span<const util::u8>(block.data(), block_len));
            (void)write_byte(ch, static_cast<util::u8>(crc >> 8));
            (void)write_byte(ch, static_cast<util::u8>(crc & 0xFF));

            if (read_byte(ch, byte, cfg.timeout_ms) != ReadStatus::ok) {
                if (++retries > cfg.max_retries) return Result{Status::timeout, total};
                continue;
            }
            if (byte == ACK) {
                total += static_cast<util::u32>(block_len);
                ++blk;
                retries = 0;
                continue;
            }
            if (byte == NAK) {
                if (++retries > cfg.max_retries) return Result{Status::retries_exhausted, total};
                continue;
            }
            if (byte == CAN) return Result{Status::cancel, total};
        }
    }
}
