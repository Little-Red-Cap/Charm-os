module;

#include <array>
#include <span>

export module io.device_i2c_mock;

import io.device_i2c;
import util.core;
import util.error;
import util.expected;

export namespace io::device::mock {
    enum class I2cTransactionKind : util::u8 {
        write,
        read,
        write_read,
    };

    template <util::usize MaxTx, util::usize MaxRx>
    struct I2cTransaction {
        I2cTransactionKind kind{I2cTransactionKind::write};
        I2cAddress address{0};
        std::array<util::u8, MaxTx> tx{};
        util::usize tx_size{0};
        std::array<util::u8, MaxRx> rx{};
        util::usize rx_size{0};
        I2cErrorKind result{I2cErrorKind::ok};

        [[nodiscard]] constexpr ByteView tx_view() const noexcept {
            return ByteView{tx.data(), tx_size};
        }

        [[nodiscard]] constexpr ByteView rx_view() const noexcept {
            return ByteView{rx.data(), rx_size};
        }
    };

    template <util::usize MaxOps, util::usize MaxTx, util::usize MaxRx>
    class I2cScriptBus {
    public:
        using Transaction = I2cTransaction<MaxTx, MaxRx>;

        [[nodiscard]] constexpr util::usize expected_count() const noexcept {
            return expected_count_;
        }

        [[nodiscard]] constexpr util::usize consumed_count() const noexcept {
            return consumed_count_;
        }

        [[nodiscard]] constexpr bool all_satisfied() const noexcept {
            return consumed_count_ == expected_count_ && first_script_error_ == util::Errc::ok;
        }

        [[nodiscard]] constexpr util::Errc first_script_error() const noexcept {
            return first_script_error_;
        }

        constexpr void reset() noexcept {
            expected_count_ = 0;
            consumed_count_ = 0;
            first_script_error_ = util::Errc::ok;
            for (auto& tx : expected_) {
                tx = {};
            }
        }

        [[nodiscard]] constexpr util::Result<void> expect_write(
            I2cAddress address,
            ByteView tx,
            I2cErrorKind result = I2cErrorKind::ok) noexcept {
            return append(Transaction{
                .kind = I2cTransactionKind::write,
                .address = address,
                .tx_size = tx.size(),
                .result = result,
            }, tx, {});
        }

        [[nodiscard]] constexpr util::Result<void> expect_read(
            I2cAddress address,
            ByteView rx,
            I2cErrorKind result = I2cErrorKind::ok) noexcept {
            return append(Transaction{
                .kind = I2cTransactionKind::read,
                .address = address,
                .rx_size = rx.size(),
                .result = result,
            }, {}, rx);
        }

        [[nodiscard]] constexpr util::Result<void> expect_write_read(
            I2cAddress address,
            ByteView tx,
            ByteView rx,
            I2cErrorKind result = I2cErrorKind::ok) noexcept {
            return append(Transaction{
                .kind = I2cTransactionKind::write_read,
                .address = address,
                .tx_size = tx.size(),
                .rx_size = rx.size(),
                .result = result,
            }, tx, rx);
        }

        [[nodiscard]] I2cResult write(I2cAddress address, ByteView tx) noexcept {
            return consume(I2cTransactionKind::write, address, tx, {});
        }

        [[nodiscard]] I2cResult read(I2cAddress address, MutByteView rx) noexcept {
            return consume(I2cTransactionKind::read, address, {}, rx);
        }

        [[nodiscard]] I2cResult write_read(I2cAddress address,
                                           ByteView tx,
                                           MutByteView rx) noexcept {
            return consume(I2cTransactionKind::write_read, address, tx, rx);
        }

    private:
        [[nodiscard]] constexpr util::Result<void> append(Transaction op,
                                                          ByteView tx,
                                                          ByteView rx) noexcept {
            if (expected_count_ >= MaxOps) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            if (tx.size() > MaxTx || rx.size() > MaxRx) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            copy_into(op.tx, tx);
            copy_into(op.rx, rx);
            expected_[expected_count_++] = op;
            return {};
        }

        [[nodiscard]] I2cResult consume(I2cTransactionKind kind,
                                        I2cAddress address,
                                        ByteView tx,
                                        MutByteView rx) noexcept {
            if (consumed_count_ >= expected_count_) {
                return fail(util::Errc::noent);
            }

            const auto& expected = expected_[consumed_count_];
            if (expected.kind != kind || expected.address != address) {
                return fail(util::Errc::bad_state);
            }
            if (!bytes_equal(expected.tx_view(), tx)) {
                return fail(util::Errc::bad_state);
            }
            if (rx.size() != expected.rx_size) {
                return fail(util::Errc::invalid_arg);
            }

            ++consumed_count_;
            if (expected.result != I2cErrorKind::ok) {
                return util::unexpected(to_errc(expected.result));
            }
            for (util::usize i = 0; i < rx.size(); ++i) {
                rx[i] = expected.rx[i];
            }
            return {};
        }

        [[nodiscard]] I2cResult fail(util::Errc err) noexcept {
            if (first_script_error_ == util::Errc::ok) {
                first_script_error_ = err;
            }
            return util::unexpected(err);
        }

        template <util::usize N>
        static constexpr void copy_into(std::array<util::u8, N>& dst, ByteView src) noexcept {
            for (util::usize i = 0; i < src.size(); ++i) {
                dst[i] = src[i];
            }
        }

        [[nodiscard]] static constexpr bool bytes_equal(ByteView lhs, ByteView rhs) noexcept {
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

        std::array<Transaction, MaxOps> expected_{};
        util::usize expected_count_{0};
        util::usize consumed_count_{0};
        util::Errc first_script_error_{util::Errc::ok};
    };
}
