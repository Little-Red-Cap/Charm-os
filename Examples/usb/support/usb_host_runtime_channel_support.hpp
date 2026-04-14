#pragma once

#include <array>
#include <string_view>

namespace examples::usb::support {
    struct DummyChannel {
        std::array<util::u8, 8> rx_data{
            static_cast<util::u8>('O'),
            static_cast<util::u8>('K')
        };
        util::usize rx_size{2};
        util::usize rx_pos{0};
        std::array<util::u8, 16> tx_data{};
        util::usize tx_size{0};
        bool flushed{false};
        io::Channel channel{};

        DummyChannel() noexcept
            : channel{
                  this,
                  io::ChannelOps{
                      &DummyChannel::read_cb,
                      &DummyChannel::write_cb,
                      &DummyChannel::flush_cb
                  }
              } {
        }

        static io::result read_cb(void* ctx, io::MutByteView out) noexcept {
            auto* self = static_cast<DummyChannel*>(ctx);
            if (!self || out.empty()) {
                return io::fail(io::errc::invalid_arg);
            }
            if (self->rx_pos >= self->rx_size) {
                return io::fail(io::errc::end_of_stream);
            }

            const auto available = self->rx_size - self->rx_pos;
            const auto count = available < out.size() ? available : out.size();
            for (util::usize i = 0; i < count; ++i) {
                out[i] = self->rx_data[self->rx_pos + i];
            }
            self->rx_pos += count;
            return io::ok(count);
        }

        static io::result write_cb(void* ctx, io::ByteView in) noexcept {
            auto* self = static_cast<DummyChannel*>(ctx);
            if (!self || in.empty()) {
                return io::fail(io::errc::invalid_arg);
            }
            if (in.size() > self->tx_data.size()) {
                return io::fail(io::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < in.size(); ++i) {
                self->tx_data[i] = in[i];
            }
            self->tx_size = in.size();
            return io::ok(in.size());
        }

        static io::result flush_cb(void* ctx) noexcept {
            auto* self = static_cast<DummyChannel*>(ctx);
            if (!self) {
                return io::fail(io::errc::invalid_arg);
            }
            self->flushed = true;
            return io::ok(1);
        }
    };

    template <typename IoRegistryT>
    struct CdcRuntimeHarness {
        IoRegistryT* registry{nullptr};
        std::string_view cap_name{};
        DummyChannel backend{};
        ::usb::host::CdcChannelRuntimeBinding<IoRegistryT> binding;

        CdcRuntimeHarness(IoRegistryT& registry,
                          std::string_view cap_name,
                          util::u16 vendor_id,
                          util::u16 product_id,
                          std::string_view type = "usb.host.cdc",
                          io::EndpointCaps caps = io::EndpointCaps::duplex,
                          io::Reactor* reactor = nullptr,
                          const char* driver_name = "usb.host.cdc.runtime",
                          const char* bus_name = "usb.host",
                          util::u32 priority = 0) noexcept
            : registry(&registry),
              cap_name(cap_name),
              backend(),
              binding(registry,
                      cap_name,
                      backend.channel,
                      vendor_id,
                      product_id,
                      type,
                      caps,
                      reactor,
                      driver_name,
                      bus_name,
                      priority) {
        }

        io::Channel* stable() noexcept {
            return registry ? registry->open_channel(cap_name) : nullptr;
        }
    };
}
