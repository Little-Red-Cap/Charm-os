module;

#include <array>
#include <span>
#include <string_view>

export module boot_xymodem;

import util.core;
import boot_core;
import boot_storage;
import boot_flash;
import io.proto.modem_xymodem;

export namespace boot {
    struct XyModemFlashConfig {
        Partition target{};
        FlashConfig flash{};
        bool require_header{false};
        bool trim_to_header_size{true};
        util::u32 max_size{0};
    };

    struct XyModemFlashState {
        util::u32 bytes_written{0};
        util::u32 expected_size{0};
        util::u32 payload_crc32{0};
        bool header_seen{false};
        bool write_error{false};
        bool size_error{false};
        std::array<char, 64> file_name{};
    };

    struct XyModemFlashResult {
        modem::Status transport_status{modem::Status::ok};
        util::u32 bytes_written{0};
        util::u32 expected_size{0};
        util::u32 payload_crc32{0};
        bool header_seen{false};
        bool header_missing{false};
        bool write_error{false};
        bool size_error{false};

        constexpr explicit operator bool() const noexcept {
            return transport_status == modem::Status::ok &&
                   !header_missing &&
                   !write_error &&
                   !size_error;
        }
    };

    template <util::usize MaxBlock = 1024>
    class XyModemFlashReceiver {
    public:
        XyModemFlashReceiver(const Storage& storage, XyModemFlashConfig cfg) noexcept
            : storage_(storage), cfg_(cfg) {
            modem_.set_handlers(&on_block_trampoline, this, &on_header_trampoline);
        }

        void start() noexcept {
            state_ = {};
            state_.size_error = size_limit() == 0;
            modem_.set_handlers(&on_block_trampoline, this, &on_header_trampoline);
            modem_.start();
        }

        modem::XyModem<MaxBlock>& modem() noexcept { return modem_; }
        const modem::XyModem<MaxBlock>& modem() const noexcept { return modem_; }

        const XyModemFlashState& state() const noexcept { return state_; }

        XyModemFlashResult result() const noexcept {
            const auto transport = modem_.result();
            return XyModemFlashResult{
                .transport_status = transport.status,
                .bytes_written = state_.bytes_written,
                .expected_size = state_.expected_size,
                .payload_crc32 = state_.payload_crc32,
                .header_seen = state_.header_seen,
                .header_missing = cfg_.require_header && !state_.header_seen,
                .write_error = state_.write_error,
                .size_error = state_.size_error
            };
        }

    private:
        static void on_header_trampoline(void* ctx, std::string_view name, util::u32 size) noexcept {
            auto* self = static_cast<XyModemFlashReceiver*>(ctx);
            if (self) self->on_header(name, size);
        }

        static void on_block_trampoline(void* ctx, std::span<const util::u8> data,
                                        util::usize len) noexcept {
            auto* self = static_cast<XyModemFlashReceiver*>(ctx);
            if (self) self->on_block(data, len);
        }

        util::u32 size_limit() const noexcept {
            if (cfg_.target.size == 0) return 0;
            if (cfg_.max_size == 0 || cfg_.max_size > cfg_.target.size) {
                return cfg_.target.size;
            }
            return cfg_.max_size;
        }

        void on_header(std::string_view name, util::u32 size) noexcept {
            state_.header_seen = true;
            state_.expected_size = size;
            state_.file_name.fill('\0');

            const auto copy_len = (name.size() < (state_.file_name.size() - 1))
                ? name.size()
                : (state_.file_name.size() - 1);
            for (util::usize i = 0; i < copy_len; ++i) {
                state_.file_name[i] = name[i];
            }

            if (size > size_limit()) {
                state_.size_error = true;
            }
        }

        void on_block(std::span<const util::u8> data, util::usize len) noexcept {
            if (state_.write_error || state_.size_error) return;

            util::u32 write_len = static_cast<util::u32>(len);
            if (cfg_.trim_to_header_size && state_.header_seen) {
                if (state_.bytes_written >= state_.expected_size) {
                    state_.size_error = true;
                    return;
                }
                const util::u32 remaining = state_.expected_size - state_.bytes_written;
                if (write_len > remaining) {
                    write_len = remaining;
                }
            }

            const util::u32 limit = size_limit();
            if (write_len > (limit - state_.bytes_written)) {
                state_.size_error = true;
                return;
            }
            if (write_len == 0) return;

            const auto chunk = std::span<const util::u8>(data.data(), write_len);
            if (!flash_write(storage_, cfg_.target.offset + state_.bytes_written, chunk, cfg_.flash)) {
                state_.write_error = true;
                return;
            }

            state_.payload_crc32 = crc32_update(state_.payload_crc32, chunk.data(), chunk.size());
            state_.bytes_written += write_len;
        }

        Storage storage_{};
        XyModemFlashConfig cfg_{};
        XyModemFlashState state_{};
        modem::XyModem<MaxBlock> modem_{};
    };
}
