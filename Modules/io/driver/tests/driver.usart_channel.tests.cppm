//
// Minimal smoke tests for driver.usart_channel (no framework).
//

module;
#include <array>
#include <cstdint>
#include <cstdlib>

export module driver.usart_channel.tests;

#if defined(IO_USART_SMOKE_TEST) && IO_USART_SMOKE_TEST

import driver.usart_channel;
import hal_uart;
import io.channel;
import io.reactor;
import service_ring_buffer;
import util.core;
import util.error;

namespace {
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void assert_true(bool v) noexcept { if (!v) fail(); }
    template <class A, class B>
    inline void assert_eq(const A& a, const B& b) noexcept { if (!(a == b)) fail(); }

    struct EventSpy {
        util::u32 events{0};
        util::u32 calls{0};
    };

    inline void on_event(void* ctx, io::Channel&, util::u32 events) noexcept {
        auto* spy = static_cast<EventSpy*>(ctx);
        if (!spy) return;
        spy->events |= events;
        ++spy->calls;
    }

    struct FakeUart {
        service::RingBuffer<util::u8, 32> rx_hw{};
        service::RingBuffer<util::u8, 32> tx_hw{};
        util::u32 irq_mask{0};

        void inject_rx(io::ByteView data) noexcept {
            for (auto b : data) {
                rx_hw.push(b);
            }
        }

        util::usize take_tx(io::MutByteView out) noexcept {
            util::usize n = 0;
            util::u8 b = 0;
            while (n < out.size() && tx_hw.pop(b)) {
                out.data()[n++] = b;
            }
            return n;
        }

        static hal::Result init(void*, const hal::UartConfig&) noexcept { return hal::ok(); }
        static hal::Result enable(void*) noexcept { return hal::ok(); }
        static hal::Result disable(void*) noexcept { return hal::ok(); }

        static hal::Result try_write(void* ctx, util::u8 byte) noexcept {
            auto* self = static_cast<FakeUart*>(ctx);
            if (!self->tx_hw.push(byte)) return hal::err(hal::Status::busy);
            return hal::ok();
        }

        static hal::Result try_read(void* ctx, util::u8& byte) noexcept {
            auto* self = static_cast<FakeUart*>(ctx);
            if (!self->rx_hw.pop(byte)) return hal::err(hal::Status::busy);
            return hal::ok();
        }

        static void enable_irq(void* ctx, util::u32 mask) noexcept {
            auto* self = static_cast<FakeUart*>(ctx);
            self->irq_mask |= mask;
        }

        static void disable_irq(void* ctx, util::u32 mask) noexcept {
            auto* self = static_cast<FakeUart*>(ctx);
            self->irq_mask &= ~mask;
        }

        static void clear_irq(void*, util::u32) noexcept {}
    };

    const hal::UartOps kFakeOps{
        &FakeUart::init,
        &FakeUart::enable,
        &FakeUart::disable,
        &FakeUart::try_write,
        &FakeUart::try_read,
        &FakeUart::enable_irq,
        &FakeUart::disable_irq,
        &FakeUart::clear_irq
    };

    inline hal::UartIoHandle make_handle(FakeUart& hw) noexcept {
        return hal::UartIoHandle{&hw, &kFakeOps};
    }

    void test_rx_chain() noexcept {
        FakeUart hw{};
        io::Reactor reactor{};
        driver::usart::ChannelAdapter<8, 8> adapter(make_handle(hw), &reactor);
        auto& ch = adapter.channel();

        EventSpy spy{};
        auto sub = reactor.subscribe(ch, static_cast<util::u32>(io::Event::readable), &on_event, &spy);
        assert_true(sub);

        std::array<util::u8, 4> in{1, 2, 3, 4};
        hw.inject_rx(io::ByteView{in.data(), in.size()});
        adapter.on_irq();
        reactor.drain();

        assert_eq(spy.calls, 1u);
        assert_true((spy.events & static_cast<util::u32>(io::Event::readable)) != 0u);

        std::array<util::u8, 4> out{};
        auto r = ch.read(io::MutByteView{out.data(), out.size()});
        assert_true(r);
        assert_eq(r.value(), in.size());
        for (util::usize i = 0; i < in.size(); ++i) {
            assert_eq(out[i], in[i]);
        }

        auto r2 = ch.read(io::MutByteView{out.data(), out.size()});
        assert_true(!r2);
        assert_eq(r2.error(), util::Errc::would_block);
    }

    void test_tx_chain() noexcept {
        FakeUart hw{};
        io::Reactor reactor{};
        driver::usart::ChannelAdapter<8, 4> adapter(make_handle(hw), &reactor);
        auto& ch = adapter.channel();

        std::array<util::u8, 4> in{10, 11, 12, 13};
        auto w1 = ch.write(io::ByteView{in.data(), in.size()});
        assert_true(w1);
        assert_eq(w1.value(), in.size());

        auto w2 = ch.write(io::ByteView{in.data(), 1});
        assert_true(!w2);
        assert_eq(w2.error(), util::Errc::would_block);

        adapter.on_irq();

        std::array<util::u8, 4> out{};
        const auto n = hw.take_tx(io::MutByteView{out.data(), out.size()});
        assert_eq(n, in.size());
        for (util::usize i = 0; i < in.size(); ++i) {
            assert_eq(out[i], in[i]);
        }

        auto w3 = ch.write(io::ByteView{in.data(), 1});
        assert_true(w3);
        assert_eq(w3.value(), (util::usize)1);
    }

    void test_event_merge() noexcept {
        FakeUart hw{};
        io::Reactor reactor{};
        driver::usart::ChannelAdapter<4, 4> adapter(make_handle(hw), &reactor);
        auto& ch = adapter.channel();

        EventSpy spy{};
        const auto want = static_cast<util::u32>(io::Event::readable) |
                          static_cast<util::u32>(io::Event::writable);
        auto sub = reactor.subscribe(ch, want, &on_event, &spy);
        assert_true(sub);

        reactor.notify(ch, static_cast<util::u32>(io::Event::readable));
        reactor.notify(ch, static_cast<util::u32>(io::Event::writable));
        reactor.notify(ch, static_cast<util::u32>(io::Event::readable));
        reactor.drain();

        assert_eq(spy.calls, 1u);
        assert_true((spy.events & static_cast<util::u32>(io::Event::readable)) != 0u);
        assert_true((spy.events & static_cast<util::u32>(io::Event::writable)) != 0u);
    }

    void test_overflow_sticky() noexcept {
        FakeUart hw{};
        io::Reactor reactor{};
        driver::usart::ChannelAdapter<4, 4> adapter(make_handle(hw), &reactor);
        auto& ch = adapter.channel();

        EventSpy spy{};
        auto sub = reactor.subscribe(ch, static_cast<util::u32>(io::Event::error), &on_event, &spy);
        assert_true(sub);

        std::array<util::u8, 8> in{1, 2, 3, 4, 5, 6, 7, 8};
        hw.inject_rx(io::ByteView{in.data(), in.size()});
        adapter.on_irq();
        reactor.drain();

        assert_true(adapter.rx_overflowed());
        assert_eq(spy.calls, 1u);

        adapter.on_irq();
        reactor.drain();
        assert_eq(spy.calls, 1u);
    }
} // namespace

export void run_usart_channel_smoke_tests() noexcept {
    test_rx_chain();
    test_tx_chain();
    test_event_merge();
    test_overflow_sticky();
}

#endif
