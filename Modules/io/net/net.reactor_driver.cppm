module;

#include <concepts>
#include <array>

export module net.reactor_driver;

import io.channel;
import io.reactor;
import net.api;
import net.common;
import net.reactor;
import util.core;
import util.error;
import util.expected;

export namespace net {
    using StreamSendFn = util::Result<util::usize> (*)(void* ctx, ByteView data) noexcept;

    template <typename T>
    concept ReactorSession = requires(T& t, StreamSendFn fn, void* ctx, ByteView data) {
        t.set_sender(fn, ctx);
        t.feed(data);
        t.notify_writable();
    };

    template <util::usize Cap>
    class ByteRing {
    public:
        [[nodiscard]] util::usize size() const noexcept { return count_; }
        [[nodiscard]] util::usize capacity() const noexcept { return Cap; }
        [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
        [[nodiscard]] bool full() const noexcept { return count_ == Cap; }

        util::usize push(ByteView data) noexcept {
            util::usize pushed = 0;
            for (util::usize i = 0; i < data.size(); ++i) {
                if (full()) break;
                buf_[tail_] = data.data()[i];
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
                out.data()[i] = buf_[head_];
                head_ = (head_ + 1) % Cap;
                --count_;
                ++popped;
            }
            return popped;
        }

        void clear() noexcept {
            head_ = 0;
            tail_ = 0;
            count_ = 0;
        }

    private:
        std::array<util::u8, Cap> buf_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };

    template <ReactorSession SessionT,
              util::usize MaxWatches,
              util::usize RxCap = 256,
              util::usize TxCap = 256>
    class ReactorSocketDriver {
    public:
        ReactorSocketDriver(io::Reactor& reactor,
                            SocketPoller<MaxWatches>& poller,
                            SocketChannelBinding& binding,
                            SessionT& session) noexcept
            : reactor_(reactor),
              poller_(poller),
              binding_(binding),
              session_(session) {}

        [[nodiscard]] Result<void> start(util::u32 persistent_events = default_socket_reactor_events()) noexcept {
            if (started_) {
                return util::unexpected(errc::bad_state);
            }
            if (!binding_.bound() || binding_.socket() == nullptr || !binding_.socket()->valid()) {
                return util::unexpected(errc::invalid_arg);
            }

            const util::u32 subscribed_events = persistent_events | static_cast<util::u32>(io::Event::writable);
            auto sub = reactor_.subscribe(binding_.channel(), subscribed_events, &ReactorSocketDriver::on_event, this);
            if (!sub) {
                return util::unexpected(sub.error());
            }

            auto watch = poller_.watch(*binding_.socket(), binding_.channel(), persistent_events);
            if (!watch) {
                reactor_.unsubscribe(sub.value());
                return util::unexpected(watch.error());
            }

            session_.set_sender(&ReactorSocketDriver::send_trampoline, this);
            sub_ = sub.value();
            watch_ = watch.value();
            started_ = true;
            closed_ = false;
            last_error_ = errc::ok;
            return {};
        }

        void stop() noexcept {
            if (sub_) {
                reactor_.unsubscribe(sub_);
                sub_ = {};
            }
            if (watch_) {
                poller_.unwatch(watch_);
                watch_ = {};
            }
            started_ = false;
            pending_len_ = 0;
            pending_off_ = 0;
            tx_.clear();
        }

        void set_budgets(int rx, int tx) noexcept {
            rx_budget_ = rx;
            tx_budget_ = tx;
        }

        [[nodiscard]] bool started() const noexcept {
            return started_;
        }

        [[nodiscard]] bool closed() const noexcept {
            return closed_;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        [[nodiscard]] Result<void> arm_writable() noexcept {
            if (!started_ || !watch_) {
                return util::unexpected(errc::bad_state);
            }
            auto terminal = terminal_error();
            if (terminal != errc::ok) {
                return util::unexpected(terminal);
            }
            auto armed = poller_.arm(watch_, static_cast<util::u32>(io::Event::writable));
            if (!armed) {
                return util::unexpected(armed.error());
            }
            return {};
        }

    private:
        [[nodiscard]] static constexpr util::u32 all_reactor_events() noexcept {
            return static_cast<util::u32>(io::Event::readable)
                | static_cast<util::u32>(io::Event::writable)
                | static_cast<util::u32>(io::Event::closed)
                | static_cast<util::u32>(io::Event::error);
        }

        static void on_event(void* ctx, io::Channel& ch, util::u32 events) noexcept {
            auto* self = static_cast<ReactorSocketDriver*>(ctx);
            if (self) {
                self->handle(ch, events);
            }
        }

        static util::Result<util::usize> send_trampoline(void* ctx, ByteView data) noexcept {
            auto* self = static_cast<ReactorSocketDriver*>(ctx);
            if (!self || !self->started_) {
                return util::unexpected(errc::bad_state);
            }
            auto terminal = self->terminal_error();
            if (terminal != errc::ok) {
                return util::unexpected(terminal);
            }
            if (!self->binding_.bound() || self->binding_.socket() == nullptr) {
                return util::unexpected(errc::bad_state);
            }
            if (!self->binding_.socket()->valid()) {
                return util::unexpected(errc::closed);
            }
            const auto pushed = self->tx_.push(data);
            if (pushed == 0) {
                return util::unexpected(errc::would_block);
            }
            (void)self->arm_writable();
            return pushed;
        }

        void handle(io::Channel& ch, util::u32 events) noexcept {
            if ((events & static_cast<util::u32>(io::Event::readable)) != 0u) {
                auto read_ok = handle_readable(ch);
                if (!read_ok) {
                    return;
                }
            }

            if ((events & static_cast<util::u32>(io::Event::closed)) != 0u) {
                mark_closed();
                return;
            }

            if ((events & static_cast<util::u32>(io::Event::error)) != 0u) {
                mark_error(errc::io);
                return;
            }

            if ((events & static_cast<util::u32>(io::Event::writable)) != 0u) {
                flush_tx();
                if (terminal_error() != errc::ok) {
                    return;
                }
                session_.notify_writable();
            }
        }

        [[nodiscard]] bool handle_readable(io::Channel& ch) noexcept {
            for (int i = 0; i < rx_budget_; ++i) {
                auto r = ch.read(io::MutByteView{rx_buf_.data(), rx_buf_.size()});
                if (!r) {
                    if (r.error() == io::errc::would_block) {
                        break;
                    }
                    if (r.error() == io::errc::closed || r.error() == io::errc::end_of_stream) {
                        mark_closed();
                        return false;
                    }
                    mark_error(static_cast<errc>(r.error()));
                    return false;
                }
                session_.feed(ByteView{rx_buf_.data(), r.value()});
            }
            flush_tx();
            if (terminal_error() != errc::ok) {
                return false;
            }
            session_.notify_writable();
            return true;
        }

        void flush_tx() noexcept {
            if (!started_ || terminal_error() != errc::ok) return;

            for (int i = 0; i < tx_budget_; ++i) {
                if (pending_len_ == 0) {
                    if (tx_.empty()) return;
                    pending_len_ = tx_.pop(MutByteView{tx_buf_.data(), tx_buf_.size()});
                    pending_off_ = 0;
                    if (pending_len_ == 0) return;
                }

                auto view = io::ByteView{tx_buf_.data() + pending_off_, pending_len_ - pending_off_};
                auto w = binding_.channel().write(view);
                if (!w) {
                    if (w.error() == io::errc::would_block) {
                        (void)arm_writable();
                        return;
                    }
                    clear_tx_pending();
                    if (w.error() == io::errc::closed || w.error() == io::errc::end_of_stream) {
                        mark_closed();
                    } else {
                        mark_error(static_cast<errc>(w.error()));
                    }
                    return;
                }

                pending_off_ += w.value();
                if (pending_off_ < pending_len_) {
                    (void)arm_writable();
                    return;
                }

                pending_len_ = 0;
                pending_off_ = 0;
            }

            if (pending_len_ != 0 || !tx_.empty()) {
                (void)arm_writable();
            }
        }

        void clear_tx_pending() noexcept {
            pending_len_ = 0;
            pending_off_ = 0;
            tx_.clear();
        }

        void quiet_watch() noexcept {
            if (!watch_) {
                return;
            }
            (void)poller_.set_interest(watch_, 0);
            (void)poller_.disarm(watch_, all_reactor_events());
        }

        void mark_closed() noexcept {
            if (terminal_error() != errc::ok) {
                return;
            }
            closed_ = true;
            last_error_ = errc::closed;
            clear_tx_pending();
            quiet_watch();
            notify_transport_closed();
        }

        void mark_error(errc error) noexcept {
            if (terminal_error() != errc::ok) {
                return;
            }
            last_error_ = error;
            clear_tx_pending();
            quiet_watch();
            notify_transport_error();
        }

        [[nodiscard]] errc terminal_error() const noexcept {
            if (closed_) {
                return errc::closed;
            }
            if (last_error_ != errc::ok) {
                return last_error_;
            }
            return errc::ok;
        }

        void notify_transport_closed() noexcept {
            if constexpr (requires(SessionT& s) { s.on_transport_closed(); }) {
                session_.on_transport_closed();
            }
        }

        void notify_transport_error() noexcept {
            if constexpr (requires(SessionT& s, errc error) { s.on_transport_error(error); }) {
                session_.on_transport_error(last_error_);
            }
        }

        io::Reactor& reactor_;
        SocketPoller<MaxWatches>& poller_;
        SocketChannelBinding& binding_;
        SessionT& session_;
        io::Subscription sub_{};
        SocketWatch watch_{};
        ByteRing<TxCap> tx_{};
        std::array<util::u8, RxCap> rx_buf_{};
        std::array<util::u8, TxCap> tx_buf_{};
        util::usize pending_len_{0};
        util::usize pending_off_{0};
        int rx_budget_{4};
        int tx_budget_{4};
        bool started_{false};
        bool closed_{false};
        errc last_error_{errc::ok};
    };

    template <util::usize MaxWatches>
    class SocketWatchDriver {
    public:
        SocketWatchDriver(SocketPoller<MaxWatches>& poller,
                          SocketChannelBinding& binding,
                          SocketWatch& watch_out) noexcept
            : poller_(poller),
              binding_(binding),
              watch_out_(watch_out) {}

        [[nodiscard]] Result<void> start(
            util::u32 persistent_events = default_socket_reactor_events()) noexcept {
            if (started_ || watch_out_) {
                return util::unexpected(errc::bad_state);
            }
            if (!binding_.bound() || binding_.socket() == nullptr || !binding_.socket()->valid()) {
                return util::unexpected(errc::invalid_arg);
            }

            auto watch = poller_.watch(*binding_.socket(), binding_.channel(), persistent_events);
            if (!watch) {
                return util::unexpected(watch.error());
            }

            watch_ = watch.value();
            watch_out_ = watch_;
            started_ = true;
            return {};
        }

        void stop() noexcept {
            if (watch_) {
                poller_.unwatch(watch_);
                watch_ = {};
            }
            watch_out_ = {};
            started_ = false;
        }

        [[nodiscard]] bool started() const noexcept {
            return started_;
        }

        [[nodiscard]] SocketWatch watch() const noexcept {
            return watch_;
        }

    private:
        SocketPoller<MaxWatches>& poller_;
        SocketChannelBinding& binding_;
        SocketWatch& watch_out_;
        SocketWatch watch_{};
        bool started_{false};
    };

    template <typename DriverT>
    concept AcceptDriver = requires(DriverT& t) {
        { t.start(default_socket_reactor_events()) } -> std::same_as<Result<void>>;
        t.stop();
    };

    template <AcceptDriver DriverT, util::usize MaxWatches>
    class TcpSingleAcceptDriver {
    public:
        TcpSingleAcceptDriver(io::Reactor& reactor,
                              SocketPoller<MaxWatches>& poller,
                              TcpListener& listener,
                              TcpClient& accepted_socket,
                              DriverT& accepted_driver) noexcept
            : reactor_(reactor),
              poller_(poller),
              listener_(listener),
              accepted_socket_(accepted_socket),
              accepted_driver_(accepted_driver),
              listener_binding_(listener.raw()) {}

        [[nodiscard]] Result<void> start(
            util::u32 persistent_events = default_socket_reactor_events()) noexcept {
            if (started_) {
                return util::unexpected(errc::bad_state);
            }
            if (!listener_.valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            if (accepted_socket_.valid()) {
                return util::unexpected(errc::bad_state);
            }

            listener_binding_.bind(listener_.raw());
            peer_ = {};
            accepted_ = false;
            failed_ = false;
            last_error_ = errc::ok;
            accepted_events_ = persistent_events;

            auto sub = reactor_.subscribe(listener_binding_.channel(),
                                          persistent_events,
                                          &TcpSingleAcceptDriver::on_event,
                                          this);
            if (!sub) {
                return util::unexpected(sub.error());
            }

            auto watch = poller_.watch(listener_.raw(),
                                       listener_binding_.channel(),
                                       persistent_events);
            if (!watch) {
                reactor_.unsubscribe(sub.value());
                return util::unexpected(watch.error());
            }

            sub_ = sub.value();
            watch_ = watch.value();
            started_ = true;
            return {};
        }

        void stop() noexcept {
            if (sub_) {
                reactor_.unsubscribe(sub_);
                sub_ = {};
            }
            if (watch_) {
                poller_.unwatch(watch_);
                watch_ = {};
            }
            started_ = false;
        }

        [[nodiscard]] bool started() const noexcept {
            return started_;
        }

        [[nodiscard]] bool accepted() const noexcept {
            return accepted_;
        }

        [[nodiscard]] bool failed() const noexcept {
            return failed_;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        [[nodiscard]] const Endpoint& peer() const noexcept {
            return peer_;
        }

    private:
        static void on_event(void* ctx, io::Channel&, util::u32 events) noexcept {
            auto* self = static_cast<TcpSingleAcceptDriver*>(ctx);
            if (self) {
                self->handle(events);
            }
        }

        void handle(util::u32 events) noexcept {
            if ((events & static_cast<util::u32>(io::Event::error)) != 0u) {
                mark_failed(errc::io);
                return;
            }
            if ((events & static_cast<util::u32>(io::Event::closed)) != 0u) {
                mark_failed(errc::closed);
                return;
            }
            if ((events & static_cast<util::u32>(io::Event::readable)) == 0u || accepted_) {
                return;
            }

            auto accepted = listener_.accept(accepted_socket_, &peer_);
            if (!accepted) {
                if (accepted.error() == errc::would_block) {
                    return;
                }
                mark_failed(accepted.error());
                return;
            }

            auto started = accepted_driver_.start(accepted_events_);
            if (!started) {
                (void)accepted_socket_.close();
                mark_failed(started.error());
                return;
            }

            accepted_ = true;
            stop();
        }

        void mark_failed(errc error) noexcept {
            failed_ = true;
            last_error_ = error;
            stop();
        }

        io::Reactor& reactor_;
        SocketPoller<MaxWatches>& poller_;
        TcpListener& listener_;
        TcpClient& accepted_socket_;
        DriverT& accepted_driver_;
        SocketEventChannelBinding listener_binding_{};
        io::Subscription sub_{};
        SocketWatch watch_{};
        Endpoint peer_{};
        util::u32 accepted_events_{default_socket_reactor_events()};
        bool started_{false};
        bool accepted_{false};
        bool failed_{false};
        errc last_error_{errc::ok};
    };
}
