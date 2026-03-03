module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module io.proto.modem_xymodem;

import io.channel;
import io.reactor;
import util.core;
import util.error;

export namespace modem {
    using ByteView = std::span<const util::u8>;
    using MutByteView = std::span<util::u8>;

    enum class Status : util::u8 {
        ok,
        timeout,
        cancel,
        crc_error,
        format_error,
        io_error,
        overflow,
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

    template <util::usize Cap>
    class ByteRing {
    public:
        util::usize size() const noexcept { return count_; }
        bool empty() const noexcept { return count_ == 0; }
        bool full() const noexcept { return count_ == Cap; }

        util::usize push(ByteView data) noexcept {
            util::usize pushed = 0;
            for (util::usize i = 0; i < data.size(); ++i) {
                if (full()) break;
                buf_[tail_] = data[i];
                tail_ = (tail_ + 1) % Cap;
                ++count_;
                ++pushed;
            }
            return pushed;
        }

        util::usize pop(MutByteView out) noexcept {
            util::usize popped = 0;
            for (util::usize i = 0; i < out.size(); ++i) {
                if (empty()) break;
                out[i] = buf_[head_];
                head_ = (head_ + 1) % Cap;
                --count_;
                ++popped;
            }
            return popped;
        }

        bool pop_byte(util::u8& out) noexcept {
            if (empty()) return false;
            out = buf_[head_];
            head_ = (head_ + 1) % Cap;
            --count_;
            return true;
        }

    private:
        std::array<util::u8, Cap> buf_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };

    using HeaderFn = void (*)(void* ctx, std::string_view name, util::u32 size) noexcept;
    using BlockFn = void (*)(void* ctx, std::span<const util::u8> data, util::usize len) noexcept;

    template <util::usize MaxBlock>
    class XyModem {
    public:
        enum class State : util::u8 {
            idle,
            wait_header,
            recv_block,
            done,
            error,
        };

        void set_config(const Config& cfg) noexcept { cfg_ = cfg; }
        void set_handlers(BlockFn on_block, void* ctx, HeaderFn on_header = nullptr) noexcept {
            on_block_ = on_block;
            on_header_ = on_header;
            ctx_ = ctx;
        }

        void start() noexcept {
            reset();
            push_tx(C);
            state_ = State::wait_header;
        }

        void reset() noexcept {
            state_ = State::idle;
            total_ = 0;
            retries_ = 0;
            blk_ = 1;
            expect_len_ = 0;
            recv_pos_ = 0;
        }

        void on_rx(ByteView data) noexcept {
            if (state_ == State::error || state_ == State::done) return;
            const auto pushed = in_.push(data);
            if (pushed < data.size()) {
                state_ = State::error;
                last_status_ = Status::overflow;
                return;
            }
            step();
        }

        void on_timeout() noexcept {
            if (state_ == State::error || state_ == State::done) return;
            if (state_ == State::wait_header || state_ == State::recv_block) {
                if (++retries_ > cfg_.max_retries) {
                    state_ = State::error;
                    last_status_ = Status::retries_exhausted;
                    return;
                }
                push_tx(C);
            }
        }

        void on_writable() noexcept { step(); }

        util::usize take_tx(MutByteView out) noexcept { return out_.pop(out); }
        bool has_tx() const noexcept { return !out_.empty(); }
        State state() const noexcept { return state_; }
        Result result() const noexcept { return Result{last_status_, total_}; }

    private:
        void push_tx(util::u8 byte) noexcept {
            util::u8 b[1]{byte};
            out_.push(ByteView{b, 1});
        }

        void step() noexcept {
            while (true) {
                if (state_ == State::wait_header) {
                    util::u8 b{};
                    if (!in_.pop_byte(b)) return;
                    if (b == CAN) {
                        state_ = State::error;
                        last_status_ = Status::cancel;
                        return;
                    }
                    if (b == EOT) {
                        push_tx(ACK);
                        state_ = State::done;
                        last_status_ = Status::ok;
                        return;
                    }
                    if (b != SOH && b != STX) {
                        push_tx(NAK);
                        continue;
                    }
                    expect_len_ = (b == STX) ? 1024u : 128u;
                    if (expect_len_ > MaxBlock) {
                        state_ = State::error;
                        last_status_ = Status::format_error;
                        return;
                    }
                    recv_pos_ = 0;
                    state_ = State::recv_block;
                }

                if (state_ == State::recv_block) {
                    const util::usize need = 2 + expect_len_ + 2;
                    while (recv_pos_ < need) {
                        util::u8 b{};
                        if (!in_.pop_byte(b)) return;
                        if (recv_pos_ < 2) {
                            header_[recv_pos_] = b;
                        } else if (recv_pos_ < 2 + expect_len_) {
                            block_[recv_pos_ - 2] = b;
                        } else {
                            crc_[recv_pos_ - (2 + expect_len_)] = b;
                        }
                        ++recv_pos_;
                    }

                    const util::u8 seq = header_[0];
                    const util::u8 seq_inv = header_[1];
                    const util::u16 got_crc = (static_cast<util::u16>(crc_[0]) << 8) | crc_[1];
                    const util::u16 calc_crc = crc16_ccitt(std::span<const util::u8>(block_.data(), expect_len_));

                    if (seq != static_cast<util::u8>(~seq_inv)) {
                        push_tx(NAK);
                        state_ = State::wait_header;
                        continue;
                    }
                    if (got_crc != calc_crc) {
                        push_tx(NAK);
                        state_ = State::wait_header;
                        continue;
                    }

                    if (seq == 0 && on_header_) {
                        const char* base = reinterpret_cast<const char*>(block_.data());
                        util::usize name_len = 0;
                        while (name_len < expect_len_ && base[name_len] != '\0') ++name_len;
                        if (name_len == 0) {
                            push_tx(ACK);
                            state_ = State::done;
                            last_status_ = Status::ok;
                            return;
                        }
                        const std::string_view fname{base, name_len};
                        util::usize pos = name_len + 1;
                        util::u32 fsize = 0;
                        while (pos < expect_len_) {
                            const char ch = base[pos];
                            if (ch < '0' || ch > '9') break;
                            fsize = static_cast<util::u32>(fsize * 10u + static_cast<util::u32>(ch - '0'));
                            ++pos;
                        }
                        on_header_(ctx_, fname, fsize);
                        push_tx(ACK);
                        push_tx(C);
                        state_ = State::wait_header;
                        retries_ = 0;
                        continue;
                    }

                    if (seq == static_cast<util::u8>(blk_ - 1)) {
                        push_tx(ACK);
                        state_ = State::wait_header;
                        continue;
                    }

                    if (seq != blk_) {
                        push_tx(NAK);
                        state_ = State::wait_header;
                        continue;
                    }

                    if (on_block_) {
                        on_block_(ctx_, std::span<const util::u8>(block_.data(), expect_len_), expect_len_);
                    }
                    total_ += static_cast<util::u32>(expect_len_);
                    ++blk_;
                    push_tx(ACK);
                    state_ = State::wait_header;
                    retries_ = 0;
                }
            }
        }

        Config cfg_{};
        State state_{State::idle};
        Status last_status_{Status::ok};
        BlockFn on_block_{nullptr};
        HeaderFn on_header_{nullptr};
        void* ctx_{nullptr};

        util::u32 total_{0};
        util::u8 retries_{0};
        util::u8 blk_{1};
        util::usize expect_len_{0};
        util::usize recv_pos_{0};

        std::array<util::u8, 2> header_{};
        std::array<util::u8, 2> crc_{};
        std::array<util::u8, MaxBlock> block_{};

        ByteRing<2048> in_{};
        ByteRing<512> out_{};
    };

    template <util::usize MaxBlock>
    class XyModemDriver {
    public:
        XyModemDriver(io::Reactor& r, io::Channel& ch, XyModem<MaxBlock>& modem) noexcept
            : reactor_(r), channel_(ch), modem_(modem) {}

        util::Result<void> start() noexcept {
            const util::u32 events =
                static_cast<util::u32>(io::Event::readable) |
                static_cast<util::u32>(io::Event::writable) |
                static_cast<util::u32>(io::Event::closed);
            auto sub = reactor_.subscribe(channel_, events, &on_event, this);
            if (!sub) return util::unexpected(sub.error());
            sub_ = sub.value();
            modem_.start();
            flush_tx();
            return {};
        }

        void stop() noexcept {
            reactor_.unsubscribe(sub_);
            sub_ = {};
        }

        void set_budgets(int rx, int tx) noexcept {
            rx_budget_ = rx;
            tx_budget_ = tx;
        }

        void on_timeout() noexcept {
            modem_.on_timeout();
            flush_tx();
        }

    private:
        static void on_event(void* ctx, io::Channel& ch, util::u32 ev) noexcept {
            auto* self = static_cast<XyModemDriver*>(ctx);
            if (self) self->handle(ch, ev);
        }

        void handle(io::Channel& ch, util::u32 ev) noexcept {
            if (ev & static_cast<util::u32>(io::Event::readable)) {
                for (int i = 0; i < rx_budget_; ++i) {
                    auto r = ch.read(io::MutByteView{rx_buf_.data(), rx_buf_.size()});
                    if (!r) {
                        if (r.error() == util::Errc::would_block) break;
                        return;
                    }
                    if (r.value() == 0) {
                        util::halt();
                        return;
                    }
                    modem_.on_rx(ByteView{rx_buf_.data(), r.value()});
                }
                flush_tx();
            }
            if (ev & static_cast<util::u32>(io::Event::writable)) {
                flush_tx();
            }
        }

        void flush_tx() noexcept {
            for (int i = 0; i < tx_budget_; ++i) {
                if (pending_len_ == 0) {
                    if (!modem_.has_tx()) return;
                    pending_len_ = modem_.take_tx(io::MutByteView{tx_buf_.data(), tx_buf_.size()});
                    pending_off_ = 0;
                    if (pending_len_ == 0) return;
                }
                auto view = io::ByteView{
                    tx_buf_.data() + pending_off_,
                    pending_len_ - pending_off_
                };
                auto w = channel_.write(view);
                if (!w) {
                    if (w.error() == util::Errc::would_block) return;
                    pending_len_ = 0;
                    pending_off_ = 0;
                    return;
                }
                if (w.value() == 0) {
                    util::halt();
                    return;
                }
                pending_off_ += w.value();
                if (pending_off_ < pending_len_) return;
                pending_len_ = 0;
                pending_off_ = 0;
            }
        }

        io::Reactor& reactor_;
        io::Channel& channel_;
        XyModem<MaxBlock>& modem_;
        io::Subscription sub_{};
        std::array<util::u8, 256> rx_buf_{};
        std::array<util::u8, 256> tx_buf_{};
        util::usize pending_len_{0};
        util::usize pending_off_{0};
        int rx_budget_{4};
        int tx_budget_{4};
    };
}
