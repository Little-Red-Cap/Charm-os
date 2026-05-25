#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <span>
#include <string_view>

import block.device;
import block.device.slot_export;
import block.registry;
import device.manager;
import io.channel;
import io.channel.slot_export;
import io.reactor;
import io.registry;
import usb.host.runtime_block;
import usb.host.runtime_channel;
import usb.host.runtime_manager;
import usb.host.runtime_observe;
import util.core;

#include "../support/usb_host_runtime_assert_support.hpp"
#include "../support/usb_host_runtime_block_support.hpp"
#include "../support/usb_host_runtime_channel_support.hpp"

namespace {
    using examples::usb::support::CdcRuntimeHarness;
    using examples::usb::support::expect;
    using examples::usb::support::expect_error;
    using examples::usb::support::expect_ok;
    using examples::usb::support::expect_status;
    using examples::usb::support::FixedTransitionLog;
    using examples::usb::support::MemoryDisk;
    using examples::usb::support::MscRuntimeHarness;
    using examples::usb::support::read_lba0;

    struct Options {
        const char* runtime_observe_path{nullptr};
        bool show_help{false};
    };

    void print_usage(const char* program) noexcept {
        std::printf("usage: %s [--runtime-observe PATH]\n", program ? program : "usb-host-runtime-multi-smoke");
    }

    bool parse_options(int argc, char** argv, Options& options) noexcept {
        for (int i = 1; i < argc; ++i) {
            const auto* arg = argv[i];
            if (!arg) {
                continue;
            }
            if (std::strcmp(arg, "--runtime-observe") == 0) {
                if (i + 1 >= argc || argv[i + 1] == nullptr) {
                    std::fprintf(stderr, "[ERR] --runtime-observe requires a path\n");
                    return false;
                }
                options.runtime_observe_path = argv[++i];
                continue;
            }
            if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
                options.show_help = true;
                return true;
            }
            std::fprintf(stderr, "[ERR] unknown argument: %s\n", arg);
            return false;
        }
        return true;
    }

    bool format_utc_timestamp(char* out, std::size_t size) noexcept {
        if (!out || size == 0) {
            return false;
        }
        const auto now = std::chrono::system_clock::now();
        const auto now_time = std::chrono::system_clock::to_time_t(now);
        std::tm utc{};
#if defined(_WIN32)
        if (gmtime_s(&utc, &now_time) != 0) {
            return false;
        }
#else
        if (gmtime_r(&now_time, &utc) == nullptr) {
            return false;
        }
#endif
        return std::strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", &utc) != 0;
    }

    template <typename TransitionT, std::size_t MaxEvents>
    void record_fixed_transition(FixedTransitionLog<TransitionT, MaxEvents>& log,
                                 const TransitionT& transition) noexcept {
        if (log.count >= log.events.size()) {
            return;
        }
        log.events[log.count++] = transition;
    }

    using ObserveCollector = usb::host::RuntimeObserveCollector<8, 16>;

    template <std::size_t MaxEvents>
    struct BlockObserverMux {
        FixedTransitionLog<block::ExportTransition, MaxEvents>* log{nullptr};
        ObserveCollector* collector{nullptr};
        std::string_view capability{};

        void on_transition(const block::ExportTransition& transition) noexcept {
            if (log) {
                record_fixed_transition(*log, transition);
            }
            if (collector) {
                collector->add_transition(capability, transition);
            }
        }
    };

    template <std::size_t MaxEvents>
    struct ChannelObserverMux {
        FixedTransitionLog<io::ExportTransition, MaxEvents>* log{nullptr};
        ObserveCollector* collector{nullptr};
        std::string_view capability{};

        void on_transition(const io::ExportTransition& transition) noexcept {
            if (log) {
                record_fixed_transition(*log, transition);
            }
            if (collector) {
                collector->add_transition(capability, transition);
            }
        }
    };

    bool write_runtime_observe_file(const char* path,
                                    const ObserveCollector& collector) noexcept {
        if (!path || path[0] == '\0') {
            return true;
        }

        std::array<char, 32> generated_at_utc{};
        if (!format_utc_timestamp(generated_at_utc.data(), generated_at_utc.size())) {
            std::fprintf(stderr, "[ERR] failed to format runtime observe timestamp\n");
            return false;
        }

        std::array<char, 8192> json{};
        const auto used = collector.format_json(
            std::string_view{generated_at_utc.data()},
            "examples.usb.usb_host_runtime_multi_smoke",
            json.data(),
            json.size());
        if (used == 0) {
            std::fprintf(stderr, "[ERR] failed to format runtime observe snapshot\n");
            return false;
        }

        std::FILE* file = nullptr;
#if defined(_WIN32)
        if (fopen_s(&file, path, "wb") != 0 || file == nullptr) {
            std::fprintf(stderr, "[ERR] failed to open runtime observe output: %s\n", path);
            return false;
        }
#else
        file = std::fopen(path, "wb");
        if (!file) {
            std::fprintf(stderr, "[ERR] failed to open runtime observe output: %s\n", path);
            return false;
        }
#endif

        const auto written = std::fwrite(json.data(), 1, used, file);
        const auto newline_written = std::fwrite("\n", 1, 1, file);
        const auto close_rc = std::fclose(file);
        if (written != used || newline_written != 1 || close_rc != 0) {
            std::fprintf(stderr, "[ERR] failed to write runtime observe output: %s\n", path);
            return false;
        }

        std::printf("[RUNTIME_OBSERVE] %s\n", path);
        return true;
    }
}

int main(int argc, char** argv) {
    Options options{};
    if (!parse_options(argc, argv, options)) {
        return 1;
    }
    if (options.show_help) {
        print_usage(argc > 0 ? argv[0] : nullptr);
        return 0;
    }

    block::Registry<4> block_registry{};
    io::Registry<4> io_registry{};
    block_registry.init();
    io_registry.init();

    MscRuntimeHarness<block::Registry<4>> msc{
        block_registry,
        "block.usb0",
        0x1209,
        0x0010,
        "MULTIUSB"
    };

    CdcRuntimeHarness<io::Registry<4>> cdc{
        io_registry,
        "io.usb0",
        0x1209,
        0x0011
    };
    ObserveCollector runtime_observe{};
    FixedTransitionLog<block::ExportTransition, 8> msc_transitions{};
    FixedTransitionLog<io::ExportTransition, 8> cdc_transitions{};
    BlockObserverMux<8> msc_observer{&msc_transitions, &runtime_observe, msc.cap_name};
    ChannelObserverMux<8> cdc_observer{&cdc_transitions, &runtime_observe, cdc.cap_name};
    msc.set_observer(block::ExportObserverRef::bind(msc_observer));
    cdc.set_observer(io::ExportObserverRef::bind(cdc_observer));

    usb::host::RuntimeManager<8, 8> runtime{"usb.host.multi"};
    if (!expect_ok(msc.add_to(runtime), "failed to add MSC exported binding")) {
        return 1;
    }
    if (!expect_ok(cdc.add_to(runtime), "failed to add CDC exported binding")) {
        return 1;
    }
    const auto msc_initial = msc.state_in(runtime);
    const auto cdc_initial = cdc.state_in(runtime);
    if (!expect(msc_initial.tracked && cdc_initial.tracked,
                "new bindings should be tracked by runtime manager")) return 1;
    if (!expect(msc_initial.published() && cdc_initial.published(),
                "runtime state should report both capabilities as published")) return 1;
    if (!expect(msc_initial.export_state == block::ExportState::detached,
                "MSC binding should start exported but detached")) return 1;
    if (!expect(cdc_initial.export_state == io::ExportState::detached,
                "CDC binding should start exported but detached")) return 1;
    if (!expect(msc_transitions.count == 1 && cdc_transitions.count == 1,
                "add_exported should emit one publish transition per binding")) return 1;
    if (!expect(msc_transitions.events[0].action == block::ExportAction::ensure_exported &&
                cdc_transitions.events[0].action == io::ExportAction::ensure_exported,
                "first transition should be ensure_exported")) return 1;
    if (!expect(msc.export_state() == block::ExportState::detached,
                "MSC binding should agree with runtime state before enumeration")) return 1;
    if (!expect(cdc.export_state() == io::ExportState::detached,
                "CDC binding should agree with runtime state before enumeration")) return 1;

    auto* stable_block = msc.stable();
    auto* stable_channel = cdc.stable();
    if (!expect(stable_block == &msc.exported_slot().device(), "stable block slot mismatch")) return 1;
    if (!expect(stable_channel == &cdc.exported_slot().channel(), "stable channel slot mismatch")) return 1;

    std::array<util::u8, MemoryDisk::block_size> block_buf{};
    std::array<util::u8, 4> read_buf{};
    std::array<util::u8, 4> write_buf{
        static_cast<util::u8>('P'),
        static_cast<util::u8>('I'),
        static_cast<util::u8>('N'),
        static_cast<util::u8>('G')
    };

    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "detached block slot should read as noent")) return 1;
    if (!expect_error(stable_channel->read(read_buf), io::errc::noent,
                      "detached channel slot should read as noent")) return 1;

    device::BusManager<1> bus_manager{};
    const auto runtime_bus = runtime.bus().bus();
    if (!expect(runtime_bus.ops.try_enumerate != nullptr,
                "runtime host bus should expose try_enumerate")) return 1;
    if (!expect(bus_manager.add_bus(runtime_bus),
                "failed to add runtime host bus")) return 1;

    if (!expect_ok(bus_manager.try_enumerate_all(runtime.registry()),
                   "runtime host bus enumerate failed")) return 1;
    if (!expect_ok(runtime.registry().try_match_detected(),
                   "runtime registry match_detected failed")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should contain two devices")) return 1;
    if (!expect(msc.enumerated_in(runtime) && cdc.enumerated_in(runtime),
                "runtime manager did not enumerate all records")) return 1;

    if (!expect(msc.attached(), "MSC slot was not attached")) return 1;
    if (!expect(cdc.attached(), "CDC slot was not attached")) return 1;
    const auto msc_attached = msc.state_in(runtime);
    const auto cdc_attached = cdc.state_in(runtime);
    if (!expect(msc_attached.enumerated && cdc_attached.enumerated,
                "runtime state should mark both bindings as enumerated")) return 1;
    if (!expect(msc_attached.attached() && cdc_attached.attached(),
                "runtime state should report both exports as attached")) return 1;
    if (!expect(msc_transitions.count == 2 && cdc_transitions.count == 2,
                "enumeration should emit attach transitions for both bindings")) return 1;
    if (!expect(msc_transitions.events[1].action == block::ExportAction::attach &&
                cdc_transitions.events[1].action == io::ExportAction::attach,
                "second transition should be attach")) return 1;
    const auto cdc_generation_before = cdc.generation();

    auto block_read = read_lba0(*stable_block, block_buf);
    if (!expect(static_cast<bool>(block_read), "attached block slot should read successfully")) return 1;
    if (!expect(block_buf[0] == 0xEB && block_buf[510] == 0x55 && block_buf[511] == 0xAA,
                "attached block slot returned unexpected data")) return 1;

    auto channel_read = stable_channel->read(read_buf);
    if (!expect(static_cast<bool>(channel_read) && channel_read.value() == 2,
                "attached channel slot should read two bytes")) return 1;
    if (!expect(read_buf[0] == static_cast<util::u8>('O') &&
                read_buf[1] == static_cast<util::u8>('K'),
                "attached channel slot returned unexpected read data")) return 1;

    auto channel_write = stable_channel->write(write_buf);
    if (!expect(static_cast<bool>(channel_write) && channel_write.value() == write_buf.size(),
                "attached channel slot should write the full payload")) return 1;
    if (!expect(cdc.backend.tx_size == write_buf.size() &&
                cdc.backend.tx_data[0] == static_cast<util::u8>('P'),
                "backend channel did not observe the expected write payload")) return 1;
    if (!expect(static_cast<bool>(stable_channel->flush()) && cdc.backend.flushed,
                "attached channel slot should flush successfully")) return 1;

    if (!expect_ok(msc.try_remove_from(runtime), "failed to remove MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 1, "runtime registry should keep only CDC after MSC remove")) return 1;
    if (!expect(msc_transitions.count == 3, "MSC remove should emit a detach transition")) return 1;
    if (!expect(msc_transitions.events[2].action == block::ExportAction::detach,
                "MSC remove should report detach")) return 1;
    if (!expect(!msc.attached(), "MSC slot should detach after remove")) return 1;
    if (!expect(msc.export_state() == block::ExportState::detached,
                "removed MSC binding should remain exported but detached")) return 1;
    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "removed MSC slot should read as noent")) return 1;

    if (!expect_ok(msc.try_rediscover_in(runtime), "failed to rediscover MSC device")) return 1;
    if (!expect(runtime.registry().device_count() == 2, "runtime registry should restore MSC after re-enumeration")) return 1;
    if (!expect(msc_transitions.count == 4, "MSC rediscover should emit an attach transition")) return 1;
    if (!expect(msc_transitions.events[3].action == block::ExportAction::attach,
                "MSC rediscover should report attach")) return 1;
    if (!expect(msc.attached(), "MSC slot should reattach after re-enumeration")) return 1;
    if (!expect(static_cast<bool>(read_lba0(*stable_block, block_buf)),
                "re-enumerated MSC slot should read successfully")) return 1;
    if (!expect(cdc.generation() == cdc_generation_before,
                "rediscovering MSC should not reinitialize unchanged CDC")) return 1;

    if (!expect_ok(cdc.try_remove_from(runtime), "failed to remove CDC device")) return 1;
    if (!expect_ok(msc.try_remove_from(runtime), "failed to remove MSC device the second time")) return 1;
    if (!expect(cdc_transitions.count == 3 && msc_transitions.count == 5,
                "second remove round should emit detach transitions")) return 1;

    if (!expect(!msc.attached(), "MSC slot should be detached after remove")) return 1;
    if (!expect(!cdc.attached(), "CDC slot should be detached after remove")) return 1;
    if (!expect(msc.export_state() == block::ExportState::detached,
                "MSC should be detached before forget")) return 1;
    if (!expect(cdc.export_state() == io::ExportState::detached,
                "CDC should be detached before forget")) return 1;
    if (!expect(runtime.registry().device_count() == 0, "runtime registry should be empty after both removes")) return 1;
    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "detached block slot should return noent after remove")) return 1;
    if (!expect_error(stable_channel->read(read_buf), io::errc::noent,
                      "detached channel slot should return noent after remove")) return 1;
    if (!expect_error(stable_channel->write(write_buf), io::errc::noent,
                      "detached channel slot should reject writes after remove")) return 1;
    if (!expect_error(stable_channel->flush(), io::errc::noent,
                      "detached channel slot should reject flush after remove")) return 1;

    if (!expect_ok(cdc.try_forget_from(runtime), "failed to forget CDC binding")) return 1;
    if (!expect_ok(msc.try_forget_from(runtime), "failed to forget MSC binding")) return 1;
    const auto msc_forgotten = msc.state_in(runtime);
    const auto cdc_forgotten = cdc.state_in(runtime);
    if (!expect(!msc_forgotten.tracked && !cdc_forgotten.tracked,
                "forgotten bindings should be removed from runtime bus")) return 1;
    if (!expect(msc_transitions.count == 6 && cdc_transitions.count == 4,
                "forget should emit one unexport transition per binding")) return 1;
    if (!expect(msc_transitions.events[5].action == block::ExportAction::unexport &&
                cdc_transitions.events[3].action == io::ExportAction::unexport,
                "forget should report unexport")) return 1;
    if (!expect(msc_forgotten.publish_state == block::PublishState::missing,
                "forgotten MSC capability should become unpublished")) return 1;
    if (!expect(cdc_forgotten.publish_state == io::PublishState::missing,
                "forgotten CDC capability should become unpublished")) return 1;
    if (!expect(msc_forgotten.export_state == block::ExportState::missing,
                "forgotten MSC export should become missing")) return 1;
    if (!expect(cdc_forgotten.export_state == io::ExportState::missing,
                "forgotten CDC export should become missing")) return 1;
    if (!expect(block_registry.open_device("block.usb0") == nullptr,
                "forgotten MSC capability should be removed from block registry")) return 1;
    if (!expect(io_registry.open_channel("io.usb0") == nullptr,
                "forgotten CDC capability should be removed from io registry")) return 1;
    if (!expect_status(read_lba0(*stable_block, block_buf), block::Errc::noent,
                       "forgotten block slot pointer should remain revoked")) return 1;
    if (!expect_error(stable_channel->read(read_buf), io::errc::noent,
                      "forgotten channel slot pointer should remain revoked")) return 1;

    runtime_observe.add_binding_state(msc.cap_name, msc.state_in(runtime));
    runtime_observe.add_binding_state(cdc.cap_name, cdc.state_in(runtime));
    if (!write_runtime_observe_file(options.runtime_observe_path, runtime_observe)) {
        return 1;
    }

    std::puts("[OK] usb-host-runtime-multi-smoke passed");
    return 0;
}
