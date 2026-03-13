module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module driver.usart_channel;

import hal_uart;
import hal_core;
import io.channel;
import io.reactor;
import io.registry;
import init.node;
import service_ring_buffer;
import util.core;
import util.error;

export namespace driver::usart {
    template <util::usize RxCap, util::usize TxCap>
    class ChannelAdapter {
    public:
        explicit ChannelAdapter(hal::UartIoHandle uart, io::Reactor* reactor = nullptr) noexcept
            : uart_(uart), reactor_(reactor) {
            irq_supported_ = (uart_.ops && uart_.ops->enable_irq);
            channel_.ctx = this;
            channel_.ops = io::ChannelOps{
                .read = &ChannelAdapter::read_trampoline,
                .write = &ChannelAdapter::write_trampoline,
                .flush = &ChannelAdapter::flush_trampoline,
            };
        }

        io::Channel& channel() noexcept { return channel_; }
        const io::Channel& channel() const noexcept { return channel_; }

        void set_reactor(io::Reactor* r) noexcept { reactor_ = r; }
        hal::UartIoHandle uart_handle() const noexcept { return uart_; }

        void on_irq() noexcept {
            const bool rx_was_empty = rx_.empty();
            const bool tx_was_full = tx_full_;
            const bool overflow_before = rx_overflow_;

            drain_rx();
            drain_tx();

            if (!rx_was_empty && rx_.empty()) {
                // no event
            }
            if (rx_was_empty && !rx_.empty()) {
                notify(static_cast<util::u32>(io::Event::readable));
            }
            if (rx_overflow_ && !overflow_before) {
                notify(static_cast<util::u32>(io::Event::error));
            }
            if (tx_was_full && !tx_.full()) {
                tx_full_ = false;
                notify(static_cast<util::u32>(io::Event::writable));
            }

            if (tx_.empty() && !tx_pending_valid_) {
                hal::uart_disable_irq(uart_, static_cast<util::u32>(hal::UartIrq::tx));
            } else {
                hal::uart_enable_irq(uart_, static_cast<util::u32>(hal::UartIrq::tx));
            }
        }

        bool rx_overflowed() const noexcept { return rx_overflow_; }
        void clear_rx_overflow() noexcept { rx_overflow_ = false; }

    private:
        static io::result read_trampoline(void* ctx, io::MutByteView buf) noexcept {
            auto* self = static_cast<ChannelAdapter*>(ctx);
            if (!self) return io::fail(io::errc::invalid_arg);
            if (buf.empty()) return io::fail(io::errc::invalid_arg);
            util::usize read = 0;
            util::u8 byte = 0;
            while (read < buf.size() && self->rx_.pop(byte)) {
                buf.data()[read++] = byte;
            }
            if (read == 0) return io::fail(io::errc::would_block);
            return io::ok(read);
        }

        static io::result write_trampoline(void* ctx, io::ByteView buf) noexcept {
            auto* self = static_cast<ChannelAdapter*>(ctx);
            if (!self) return io::fail(io::errc::invalid_arg);
            if (buf.empty()) return io::fail(io::errc::invalid_arg);
            util::usize pushed = 0;
            for (util::usize i = 0; i < buf.size(); ++i) {
                if (self->tx_.full()) {
                    self->tx_full_ = true;
                    break;
                }
                if (!self->tx_.push(buf.data()[i])) {
                    self->tx_full_ = true;
                    break;
                }
                ++pushed;
            }
            if (pushed == 0) return io::fail(io::errc::would_block);
            hal::uart_enable_irq(self->uart_, static_cast<util::u32>(hal::UartIrq::tx));
            if (!self->irq_supported_) {
                self->drain_tx();
            }
            return io::ok(pushed);
        }

        static io::result flush_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ChannelAdapter*>(ctx);
            if (!self) return io::fail(io::errc::invalid_arg);
            if (!self->tx_.empty() || self->tx_pending_valid_) {
                return io::fail(io::errc::would_block);
            }
            return io::ok(0);
        }

        void notify(util::u32 events) noexcept {
            if (!reactor_) return;
            reactor_->notify(channel_, events);
        }

        void drain_rx() noexcept {
            while (true) {
                util::u8 byte = 0;
                auto r = hal::uart_try_read(uart_, byte);
                if (!r) {
                    if (r.status == hal::Status::busy) break;
                    rx_overflow_ = true;
                    break;
                }
                if (!rx_.push(byte)) {
                    rx_overflow_ = true;
                    break;
                }
            }
        }

        void drain_tx() noexcept {
            while (true) {
                if (!tx_pending_valid_) {
                    if (!tx_.pop(tx_pending_)) {
                        return;
                    }
                    tx_pending_valid_ = true;
                }
                auto r = hal::uart_try_write(uart_, tx_pending_);
                if (!r) {
                    if (r.status == hal::Status::busy) return;
                    tx_pending_valid_ = false;
                    return;
                }
                tx_pending_valid_ = false;
            }
        }

        hal::UartIoHandle uart_{};
        io::Reactor* reactor_{nullptr};
        io::Channel channel_{};
        bool irq_supported_{false};
        service::RingBuffer<util::u8, RxCap> rx_{};
        service::RingBuffer<util::u8, TxCap> tx_{};
        util::u8 tx_pending_{0};
        bool tx_pending_valid_{false};
        bool rx_overflow_{false};
        bool tx_full_{false};
    };

    template <typename RegistryT, util::usize RxCap, util::usize TxCap>
    struct ChannelBinding {
        ChannelAdapter<RxCap, TxCap> adapter;
        RegistryT* registry{nullptr};
        io::Reactor* reactor{nullptr};
        io::EndpointDesc desc{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 3> requires_caps{};
        init::Node node{};

        ChannelBinding(RegistryT& reg,
                       io::Reactor& r,
                       hal::UartIoHandle uart,
                       const char* endpoint_name,
                       const char* hal_cap_name = "hal.uart1") noexcept
            : adapter(uart, &r),
              registry(&reg),
              reactor(&r),
              desc{endpoint_name,
                   io::cap_id(endpoint_name),
                   io::EndpointKind::channel,
                   io::EndpointCaps::duplex} {
            provides[0] = init::cap_id(endpoint_name);
            requires_caps[0] = init::cap_id("io.registry");
            requires_caps[1] = init::cap_id("io.reactor");
            requires_caps[2] = init::cap_id(hal_cap_name);
            node = init::Node{
                endpoint_name,
                init::Phase::core,
                static_cast<util::u32>(init::Runlevel::all),
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &ChannelBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ChannelBinding*>(ctx);
            if (!self || !self->registry) return util::unexpected(util::Errc::invalid_arg);
            auto r = self->registry->register_channel(self->desc, self->adapter.channel(), self->reactor);
            if (!r) return r;
            hal::uart_enable_irq(self->adapter.uart_handle(),
                                 static_cast<util::u32>(hal::UartIrq::rx) |
                                 static_cast<util::u32>(hal::UartIrq::err));
            return {};
        }
    };
}
