module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module io.proto.modem_xymodem;

import util.core;

export namespace modem {
    using ReadByteFn = bool (*)(void* ctx, util::u8& out, util::u32 timeout_ms) noexcept;
    using WriteByteFn = void (*)(void* ctx, util::u8 byte) noexcept;
    using WriteDataFn = void (*)(void* ctx, std::span<const util::u8> data) noexcept;

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

    struct Callbacks {
        void* ctx{nullptr};
        ReadByteFn read{nullptr};
        WriteByteFn write{nullptr};
        WriteDataFn write_data{nullptr};
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

    using HeaderFn = void (*)(void* ctx, std::string_view name, util::u32 size) noexcept;

    template <util::usize MaxBlock>
    Result receive(const Callbacks& cb, const Config& cfg,
                   void (*on_block)(void* ctx, std::span<const util::u8> data, util::usize len) noexcept,
                   void* out_ctx,
                   HeaderFn on_header = nullptr) noexcept {
        if (!cb.read || !cb.write) return Result{Status::io_error, 0};
        const util::usize block_len = cfg.use_1k ? 1024 : 128;
        if (block_len > MaxBlock) return Result{Status::format_error, 0};

        util::u8 blk = 1;
        util::u32 total = 0;
        util::u8 retries = 0;

        cb.write(cb.ctx, C);

        std::array<util::u8, MaxBlock> block{};
        while (true) {
            util::u8 ch{};
            if (!cb.read(cb.ctx, ch, cfg.timeout_ms)) {
                if (++retries > cfg.max_retries) return Result{Status::timeout, total};
                cb.write(cb.ctx, C);
                continue;
            }

            if (ch == CAN) return Result{Status::cancel, total};
            if (ch == EOT) {
                cb.write(cb.ctx, ACK);
                return Result{Status::ok, total};
            }

            if (ch != SOH && ch != STX) {
                cb.write(cb.ctx, NAK);
                continue;
            }

            const util::usize expect_len = (ch == STX) ? 1024 : 128;
            if (expect_len > MaxBlock) return Result{Status::format_error, total};

            util::u8 seq = 0;
            util::u8 seq_inv = 0;
            if (!cb.read(cb.ctx, seq, cfg.timeout_ms)) return Result{Status::timeout, total};
            if (!cb.read(cb.ctx, seq_inv, cfg.timeout_ms)) return Result{Status::timeout, total};

            for (util::usize i = 0; i < expect_len; ++i) {
                if (!cb.read(cb.ctx, block[i], cfg.timeout_ms)) return Result{Status::timeout, total};
            }

            util::u8 crc_hi = 0;
            util::u8 crc_lo = 0;
            if (!cb.read(cb.ctx, crc_hi, cfg.timeout_ms)) return Result{Status::timeout, total};
            if (!cb.read(cb.ctx, crc_lo, cfg.timeout_ms)) return Result{Status::timeout, total};
            const util::u16 got_crc = (static_cast<util::u16>(crc_hi) << 8) | crc_lo;
            const util::u16 calc_crc = crc16_ccitt(std::span<const util::u8>(block.data(), expect_len));

            if (seq != static_cast<util::u8>(~seq_inv)) {
                cb.write(cb.ctx, NAK);
                continue;
            }
            if (got_crc != calc_crc) {
                cb.write(cb.ctx, NAK);
                continue;
            }

            if (seq == 0 && on_header) {
                const char* base = reinterpret_cast<const char*>(block.data());
                util::usize name_len = 0;
                while (name_len < expect_len && base[name_len] != '\0') {
                    ++name_len;
                }
                if (name_len == 0) {
                    cb.write(cb.ctx, ACK);
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
                cb.write(cb.ctx, ACK);
                cb.write(cb.ctx, C);
                retries = 0;
                continue;
            }
            if (seq == static_cast<util::u8>(blk - 1)) {
                cb.write(cb.ctx, ACK);
                continue;
            }
            if (seq == blk) {
                if (on_block) on_block(out_ctx, std::span<const util::u8>(block.data(), expect_len), expect_len);
                total += static_cast<util::u32>(expect_len);
                ++blk;
            }
            cb.write(cb.ctx, ACK);
            retries = 0;
        }
    }

    template <util::usize MaxBlock>
    Result send(const Callbacks& cb, const Config& cfg,
                bool (*next_block)(void* ctx, std::span<util::u8> out, util::usize& len) noexcept,
                void* in_ctx) noexcept {
        if (!cb.read || !cb.write || !cb.write_data) return Result{Status::io_error, 0};
        const util::usize block_len = cfg.use_1k ? 1024 : 128;
        if (block_len > MaxBlock) return Result{Status::format_error, 0};

        util::u32 total = 0;
        util::u8 blk = 1;
        util::u8 retries = 0;

        util::u8 ch{};
        while (true) {
            if (!cb.read(cb.ctx, ch, cfg.timeout_ms)) {
                if (++retries > cfg.max_retries) return Result{Status::timeout, total};
                continue;
            }
            if (ch == C || ch == NAK) break;
            if (ch == CAN) return Result{Status::cancel, total};
        }

        std::array<util::u8, MaxBlock> block{};
        while (true) {
            util::usize len = 0;
            if (!next_block(in_ctx, std::span<util::u8>(block.data(), block_len), len)) {
                cb.write(cb.ctx, EOT);
                if (!cb.read(cb.ctx, ch, cfg.timeout_ms)) return Result{Status::timeout, total};
                if (ch == ACK) return Result{Status::ok, total};
                return Result{Status::io_error, total};
            }
            if (len < block_len) {
                for (util::usize i = len; i < block_len; ++i) block[i] = 0x1A;
            }

            const util::u8 head = (block_len == 1024) ? STX : SOH;
            cb.write(cb.ctx, head);
            cb.write(cb.ctx, blk);
            cb.write(cb.ctx, static_cast<util::u8>(~blk));
            cb.write_data(cb.ctx, std::span<const util::u8>(block.data(), block_len));
            const util::u16 crc = crc16_ccitt(std::span<const util::u8>(block.data(), block_len));
            cb.write(cb.ctx, static_cast<util::u8>(crc >> 8));
            cb.write(cb.ctx, static_cast<util::u8>(crc & 0xFF));

            if (!cb.read(cb.ctx, ch, cfg.timeout_ms)) {
                if (++retries > cfg.max_retries) return Result{Status::timeout, total};
                continue;
            }
            if (ch == ACK) {
                total += static_cast<util::u32>(block_len);
                ++blk;
                retries = 0;
                continue;
            }
            if (ch == NAK) {
                if (++retries > cfg.max_retries) return Result{Status::retries_exhausted, total};
                continue;
            }
            if (ch == CAN) return Result{Status::cancel, total};
        }
    }
}
