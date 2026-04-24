#include <array>
#include <cstddef>
#include <cstdio>
#include <span>

import device.bus;
import device.desc;
import device.manager;
import device.registry;
import device.runtime_driver;
import device.types;
import io.channel;
import io.channel.slot_export;
import io.registry;
import util.core;
import util.error;

namespace {
    template <typename TransitionT, std::size_t MaxEvents>
    struct FixedTransitionLog {
        std::array<TransitionT, MaxEvents> events{};
        std::size_t count{0};

        static void on_event(void* ctx, const TransitionT& transition) noexcept {
            auto* self = static_cast<FixedTransitionLog*>(ctx);
            if (!self || self->count >= self->events.size()) {
                return;
            }
            self->events[self->count++] = transition;
        }
    };

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

    struct RuntimeContext {
        io::ChannelSlotExport<io::Registry<4>>* exported{nullptr};
        DummyChannel* backend{nullptr};
    };

    using RuntimeBinding = device::RuntimeDriverBinding<RuntimeContext>;

    struct RuntimeBusContext {
        RuntimeBinding* binding{nullptr};
        device::DeviceDesc desc{};
        bool enumerated{false};
    };

    bool expect(bool cond, const char* message) {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool expect_ok(const util::Result<void>& result, const char* message) {
        if (!result) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d\n",
                         message,
                         static_cast<int>(result.error()));
            return false;
        }
        return true;
    }

    bool expect_error(const io::result& r, io::errc want, const char* message) {
        if (r.error() != want) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d want=%d\n",
                         message,
                         static_cast<int>(r.error()),
                         static_cast<int>(want));
            return false;
        }
        return true;
    }

    util::Result<void> runtime_try_enumerate(void* ctx, device::RegistryBase& reg) noexcept {
        auto* self = static_cast<RuntimeBusContext*>(ctx);
        if (!self || !self->binding) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (self->enumerated) {
            return {};
        }
        auto added = reg.try_add_device(self->desc, self->binding);
        if (added) {
            self->enumerated = true;
        }
        return added;
    }

    bool runtime_enumerate(void* ctx, device::RegistryBase& reg) noexcept {
        return static_cast<bool>(runtime_try_enumerate(ctx, reg));
    }

    util::Result<void> runtime_try_probe(RuntimeContext& ctx, device::Device& dev) noexcept {
        if (ctx.exported != nullptr &&
            ctx.backend != nullptr &&
            dev.desc.class_id == 0x02 &&
            dev.desc.vendor_id == 0x1234 &&
            dev.desc.product_id == 0x5678 &&
            dev.desc.type.compare("usb.cdc") == 0) {
            return {};
        }
        return util::unexpected(util::Errc::bad_state);
    }

    bool runtime_probe(RuntimeContext& ctx, device::Device& dev) noexcept {
        return static_cast<bool>(runtime_try_probe(ctx, dev));
    }

    util::Result<void> runtime_try_init(RuntimeContext& ctx, device::Device&) noexcept {
        if (!ctx.exported || !ctx.backend) {
            return util::unexpected(util::Errc::bad_state);
        }
        return ctx.exported->attach(ctx.backend->channel);
    }

    bool runtime_init(RuntimeContext& ctx, device::Device& dev) noexcept {
        return static_cast<bool>(runtime_try_init(ctx, dev));
    }

    void runtime_remove(RuntimeContext& ctx, device::Device&) noexcept {
        if (!ctx.exported) {
            return;
        }
        ctx.exported->detach();
    }
}

int main() {
    constexpr auto kCapName = "io.usb0";

    io::Registry<4> io_registry{};
    io_registry.init();

    io::ChannelSlotExport<io::Registry<4>> exported{
        io_registry,
        kCapName,
        io::EndpointCaps::duplex
    };
    DummyChannel backend{};
    FixedTransitionLog<io::ExportTransition, 4> transitions{};
    exported.set_observer(&FixedTransitionLog<io::ExportTransition, 4>::on_event, &transitions);

    auto register_r = exported.ensure_exported();
    if (!register_r) {
        std::fprintf(stderr, "[ERR] io registry register failed err=%d\n", static_cast<int>(register_r.error()));
        return 1;
    }
    if (!expect(transitions.count == 1, "ensure_exported should emit one channel transition")) return 1;
    if (!expect(transitions.events[0].action == io::ExportAction::ensure_exported,
                "first channel transition should be ensure_exported")) return 1;
    if (!expect(transitions.events[0].before == io::ExportState::missing &&
                transitions.events[0].after == io::ExportState::detached,
                "ensure_exported should move channel slot from missing to detached")) return 1;

    auto* stable = io_registry.open_channel(kCapName);
    if (!expect(stable == &exported.channel(), "registry did not expose the stable channel slot")) return 1;
    if (!expect(exported.exported(), "channel export should stay published")) return 1;
    if (!expect(!exported.attached(), "channel slot should start detached")) return 1;
    if (!expect(exported.generation() == 0, "channel slot generation should start at 0")) return 1;

    std::array<util::u8, 4> read_buf{};
    std::array<util::u8, 4> write_buf{
        static_cast<util::u8>('P'),
        static_cast<util::u8>('I'),
        static_cast<util::u8>('N'),
        static_cast<util::u8>('G')
    };

    if (!expect_error(stable->read(read_buf), io::errc::noent, "detached slot should read as noent")) return 1;
    if (!expect_error(stable->write(write_buf), io::errc::noent, "detached slot should write as noent")) return 1;
    if (!expect_error(stable->flush(), io::errc::noent, "detached slot should flush as noent")) return 1;

    RuntimeContext runtime_ctx{&exported, &backend};
    RuntimeBinding binding{
        &runtime_ctx,
        device::RuntimeDriverHook<RuntimeContext>{
            .probe = runtime_probe,
            .init = runtime_init,
            .remove = runtime_remove,
            .try_probe = runtime_try_probe,
            .try_init = runtime_try_init
        }
    };

    RuntimeBusContext bus_ctx{
        .binding = &binding,
        .desc = device::DeviceDesc{
            .class_id = 0x02,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.cdc"
        }
    };

    auto driver = device::make_runtime_driver<RuntimeContext>(
        bus_ctx.desc,
        "demo.runtime.channel.slot");

    device::Bus bus{
        .name = "demo.usb.host",
        .ctx = &bus_ctx,
        .ops = device::BusOps{
            .enumerate = runtime_enumerate,
            .try_enumerate = runtime_try_enumerate
        }
    };

    device::Registry<4, 4> registry{};
    device::BusManager<1> bus_manager{};

    if (!expect_ok(registry.try_add_driver(driver), "failed to add runtime channel driver")) return 1;
    if (!expect_ok(bus_manager.try_add_bus(bus), "failed to add runtime channel bus")) return 1;

    if (!expect_ok(bus_manager.try_enumerate_all(registry), "runtime bus enumerate failed")) return 1;
    if (!expect(bus_ctx.enumerated, "runtime bus did not enumerate the channel device")) return 1;
    if (!expect(registry.device_count() == 1, "runtime registry should contain one channel device")) return 1;

    if (!expect_ok(registry.try_match_all(), "runtime registry match failed")) return 1;
    if (!expect(transitions.count == 2, "attach should emit a second channel transition")) return 1;
    if (!expect(transitions.events[1].action == io::ExportAction::attach,
                "second channel transition should be attach")) return 1;
    if (!expect(transitions.events[1].before == io::ExportState::detached &&
                transitions.events[1].after == io::ExportState::attached,
                "attach should move channel slot from detached to attached")) return 1;
    if (!expect(exported.attached(), "runtime init did not attach the channel slot")) return 1;
    if (!expect(exported.generation() == 1, "channel slot generation should advance after attach")) return 1;
    if (!expect(registry.device_at(0).state == device::DeviceState::running,
                "runtime channel device should be running after match_all")) {
        return 1;
    }

    auto* stable_after_attach = io_registry.open_channel(io::cap_id(kCapName));
    if (!expect(stable_after_attach == stable, "registry pointer should stay stable after attach")) return 1;

    auto read_r = stable->read(read_buf);
    if (!expect(static_cast<bool>(read_r) && read_r.value() == 2, "attached slot should read two bytes")) return 1;
    if (!expect(read_buf[0] == static_cast<util::u8>('O') && read_buf[1] == static_cast<util::u8>('K'),
                "attached slot returned unexpected read payload")) {
        return 1;
    }

    auto write_r = stable->write(write_buf);
    if (!expect(static_cast<bool>(write_r) && write_r.value() == write_buf.size(),
                "attached slot should write the full payload")) {
        return 1;
    }
    if (!expect(backend.tx_size == write_buf.size(),
                "backend did not observe the expected write size")) {
        return 1;
    }
    if (!expect(backend.tx_data[0] == static_cast<util::u8>('P') &&
                backend.tx_data[1] == static_cast<util::u8>('I') &&
                backend.tx_data[2] == static_cast<util::u8>('N') &&
                backend.tx_data[3] == static_cast<util::u8>('G'),
                "backend did not observe the expected write payload")) {
        return 1;
    }

    auto flush_r = stable->flush();
    if (!expect(static_cast<bool>(flush_r), "attached slot should flush successfully")) return 1;
    if (!expect(backend.flushed, "backend flush callback was not observed")) return 1;

    if (!expect_ok(registry.try_dispatch(registry.device_at(0), device::DeviceEvent::remove),
                   "runtime remove did not complete")) {
        return 1;
    }
    if (!expect(transitions.count == 3, "remove should emit a detach transition")) return 1;
    if (!expect(transitions.events[2].action == io::ExportAction::detach,
                "third channel transition should be detach")) return 1;
    if (!expect(transitions.events[2].before == io::ExportState::attached &&
                transitions.events[2].after == io::ExportState::detached,
                "detach should move channel slot from attached to detached")) return 1;
    if (!expect(!exported.attached(), "runtime remove did not detach the channel slot")) return 1;
    if (!expect(exported.generation() == 2, "channel slot generation should advance after detach")) return 1;
    if (!expect(io_registry.open_channel(kCapName) == stable,
                "registry pointer should remain stable after detach")) {
        return 1;
    }

    if (!expect_error(stable->read(read_buf), io::errc::noent, "detached slot should read as noent after remove")) return 1;
    if (!expect_error(stable->write(write_buf), io::errc::noent, "detached slot should write as noent after remove")) return 1;
    if (!expect_error(stable->flush(), io::errc::noent, "detached slot should flush as noent after remove")) return 1;

    std::puts("[OK] runtime channel slot demo passed");
    return 0;
}
