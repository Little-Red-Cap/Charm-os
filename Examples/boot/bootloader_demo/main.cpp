#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

import charm.foundation;
import charm.runtime;
import platform.board;

namespace {
    constexpr util::u8 kPad = 0x1Au;

    struct MockFlash {
        std::array<util::u8, 2048> data{};

        MockFlash() noexcept {
            data.fill(0xFFu);
        }

        bool read(util::u32 offset, std::span<util::u8> out) noexcept {
            if (offset + out.size() > data.size()) return false;
            std::memcpy(out.data(), data.data() + offset, out.size());
            return true;
        }

        bool write(util::u32 offset, std::span<const util::u8> in) noexcept {
            if (offset + in.size() > data.size()) return false;
            std::memcpy(data.data() + offset, in.data(), in.size());
            return true;
        }

        bool erase(util::u32 offset, util::u32 size) noexcept {
            if (offset + size > data.size()) return false;
            std::memset(data.data() + offset, 0xFF, size);
            return true;
        }
    };

    boot::Storage make_storage(MockFlash& flash) noexcept {
        return boot::Storage{
            &flash,
            +[](void* ctx, util::u32 off, std::span<util::u8> out) noexcept {
                return static_cast<MockFlash*>(ctx)->read(off, out);
            },
            +[](void* ctx, util::u32 off, std::span<const util::u8> in) noexcept {
                return static_cast<MockFlash*>(ctx)->write(off, in);
            },
            +[](void* ctx, util::u32 off, util::u32 size) noexcept {
                return static_cast<MockFlash*>(ctx)->erase(off, size);
            }
        };
    }

    std::vector<util::u8> build_image(std::string_view payload, bool valid,
                                      util::u32 version, util::u32 key,
                                      util::u32 entry_offset = 0,
                                      util::u16 extra_flags = 0) {
        boot::ImageHeader h{};
        h.payload_size = static_cast<util::u32>(payload.size());
        h.image_size = h.payload_size + sizeof(boot::ImageHeader);
        h.payload_crc32 = valid
            ? boot::crc32_update(0, reinterpret_cast<const util::u8*>(payload.data()), payload.size())
            : 0x12345678u;
        h.entry_offset = entry_offset;
        h.image_version = version;
        h.min_version = 1;
        h.flags = static_cast<util::u16>(boot::ImageFlags::signed_image) | extra_flags;
        h.signature = boot::calc_signature(key,
                                           reinterpret_cast<const util::u8*>(&h.payload_crc32),
                                           sizeof(h.payload_crc32));

        std::vector<util::u8> image(sizeof(h) + payload.size());
        std::memcpy(image.data(), &h, sizeof(h));
        std::memcpy(image.data() + sizeof(h), payload.data(), payload.size());
        return image;
    }

    template <class Transport>
    std::vector<util::u8> drain_tx(Transport& receiver) {
        std::array<util::u8, 16> buf{};
        std::vector<util::u8> out;
        while (receiver.has_tx()) {
            const auto n = receiver.take_tx(std::span<util::u8>(buf.data(), buf.size()));
            out.insert(out.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
        }
        return out;
    }

    bool expect_bytes(std::span<const util::u8> actual, std::span<const util::u8> expected) {
        if (actual.size() != expected.size()) return false;
        for (util::usize i = 0; i < actual.size(); ++i) {
            if (actual[i] != expected[i]) return false;
        }
        return true;
    }

    const char* selection_reason_name(boot::BootSelectionReason reason) noexcept {
        switch (reason) {
        case boot::BootSelectionReason::pending_trial:
            return "trial";
        case boot::BootSelectionReason::active:
            return "active";
        case boot::BootSelectionReason::pending:
            return "pending";
        case boot::BootSelectionReason::fallback:
            return "fallback";
        default:
            return "none";
        }
    }

    const char* load_kind_name(boot::BootLoadKind kind) noexcept {
        switch (kind) {
        case boot::BootLoadKind::xip:
            return "xip";
        case boot::BootLoadKind::copy_to_ram:
            return "copy";
        default:
            return "unknown";
        }
    }

    struct MockLaunchContext {
        util::u32 expected_payload_offset{0};
        util::u32 expected_storage_entry_offset{0};
        platform::board::BootLoadKind expected_load_kind{
            platform::board::BootLoadKind::copy_to_ram};
        bool resolve_called{false};
        bool load_called{false};
        bool prepare_called{false};
        bool jump_called{false};
        bool entry_called{false};
    };

    void mock_boot_entry(void* ctx) noexcept {
        static_cast<MockLaunchContext*>(ctx)->entry_called = true;
    }

    util::usize resolve_mock_payload_base(void* ctx,
                                          const platform::board::BootLoadResolveRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->resolve_called = true;
        if (request.kind != launch->expected_load_kind) {
            return 0;
        }
        if (request.storage_payload_offset != launch->expected_payload_offset) {
            return 0;
        }
        if (request.storage_entry_offset != launch->expected_storage_entry_offset) {
            return 0;
        }
        return reinterpret_cast<util::usize>(&mock_boot_entry) - request.entry_offset;
    }

    bool load_mock_payload(void* ctx,
                           const platform::board::BootLoadTransferRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->load_called = true;
        if (request.kind != launch->expected_load_kind) {
            return false;
        }
        if (request.storage_payload_offset != launch->expected_payload_offset) {
            return false;
        }
        return request.payload_base != 0;
    }

    bool prepare_mock_execution(void* ctx,
                                const platform::board::BootExecRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->prepare_called = true;
        return request.entry_addr == reinterpret_cast<util::usize>(&mock_boot_entry);
    }

    bool jump_mock_execution(void* ctx,
                             const platform::board::BootExecRequest& request) noexcept {
        auto* launch = static_cast<MockLaunchContext*>(ctx);
        launch->jump_called = true;
        auto entry = reinterpret_cast<void (*)(void*) noexcept>(request.entry_addr);
        entry(ctx);
        return launch->entry_called;
    }

    std::vector<util::u8> make_header_frame(std::string_view file_name, util::u32 file_size) {
        std::array<util::u8, 128> payload{};
        util::usize pos = 0;
        for (; pos < file_name.size() && pos < payload.size(); ++pos) {
            payload[pos] = static_cast<util::u8>(file_name[pos]);
        }
        if (pos < payload.size()) payload[pos++] = 0;

        char digits[16]{};
        int digit_count = std::snprintf(digits, sizeof(digits), "%u", file_size);
        if (digit_count < 0) digit_count = 0;
        for (int i = 0; i < digit_count && pos < payload.size(); ++i) {
            payload[pos++] = static_cast<util::u8>(digits[i]);
        }

        std::vector<util::u8> frame;
        frame.reserve(3 + payload.size() + 2);
        frame.push_back(modem::SOH);
        frame.push_back(0x00);
        frame.push_back(0xFF);
        frame.insert(frame.end(), payload.begin(), payload.end());
        const auto crc = modem::crc16_ccitt(std::span<const util::u8>(payload.data(), payload.size()));
        frame.push_back(static_cast<util::u8>((crc >> 8) & 0xFFu));
        frame.push_back(static_cast<util::u8>(crc & 0xFFu));
        return frame;
    }

    std::vector<util::u8> make_data_frame(util::u8 seq, std::span<const util::u8> data) {
        std::array<util::u8, 1024> payload{};
        for (auto& byte : payload) byte = kPad;
        for (util::usize i = 0; i < data.size(); ++i) {
            payload[i] = data[i];
        }

        std::vector<util::u8> frame;
        frame.reserve(3 + payload.size() + 2);
        frame.push_back(modem::STX);
        frame.push_back(seq);
        frame.push_back(static_cast<util::u8>(~seq));
        frame.insert(frame.end(), payload.begin(), payload.end());
        const auto crc = modem::crc16_ccitt(std::span<const util::u8>(payload.data(), payload.size()));
        frame.push_back(static_cast<util::u8>((crc >> 8) & 0xFFu));
        frame.push_back(static_cast<util::u8>(crc & 0xFFu));
        return frame;
    }

    template <class Transport>
    bool send_image(Transport& receiver, std::string_view file_name, std::span<const util::u8> image) {
        auto tx = drain_tx(receiver);
        const util::u8 want_crc[] = {modem::C};
        if (!expect_bytes(tx, std::span<const util::u8>(want_crc, 1))) return false;

        const auto header = make_header_frame(file_name, static_cast<util::u32>(image.size()));
        receiver.on_rx(std::span<const util::u8>(header.data(), header.size()));
        tx = drain_tx(receiver);
        const util::u8 want_header_ack[] = {modem::ACK, modem::C};
        if (!expect_bytes(tx, std::span<const util::u8>(want_header_ack, 2))) return false;

        util::u8 seq = 1;
        for (util::usize off = 0; off < image.size(); off += 1024) {
            const auto chunk = (image.size() - off > 1024) ? 1024 : (image.size() - off);
            const auto frame = make_data_frame(
                seq++,
                std::span<const util::u8>(image.data() + off, chunk));
            receiver.on_rx(std::span<const util::u8>(frame.data(), frame.size()));
            tx = drain_tx(receiver);
            const util::u8 want_ack[] = {modem::ACK};
            if (!expect_bytes(tx, std::span<const util::u8>(want_ack, 1))) return false;
        }

        const util::u8 eot[] = {modem::EOT};
        receiver.on_rx(std::span<const util::u8>(eot, 1));
        tx = drain_tx(receiver);
        const util::u8 want_eot_ack[] = {modem::ACK};
        return expect_bytes(tx, std::span<const util::u8>(want_eot_ack, 1));
    }

    template <class Transport>
    bool send_image_without_header(Transport& receiver, std::span<const util::u8> image) {
        auto tx = drain_tx(receiver);
        const util::u8 want_crc[] = {modem::C};
        if (!expect_bytes(tx, std::span<const util::u8>(want_crc, 1))) return false;

        util::u8 seq = 1;
        for (util::usize off = 0; off < image.size(); off += 1024) {
            const auto chunk = (image.size() - off > 1024) ? 1024 : (image.size() - off);
            const auto frame = make_data_frame(
                seq++,
                std::span<const util::u8>(image.data() + off, chunk));
            receiver.on_rx(std::span<const util::u8>(frame.data(), frame.size()));
            tx = drain_tx(receiver);
            const util::u8 want_ack[] = {modem::ACK};
            if (!expect_bytes(tx, std::span<const util::u8>(want_ack, 1))) return false;
        }

        const util::u8 eot[] = {modem::EOT};
        receiver.on_rx(std::span<const util::u8>(eot, 1));
        tx = drain_tx(receiver);
        const util::u8 want_eot_ack[] = {modem::ACK};
        return expect_bytes(tx, std::span<const util::u8>(want_eot_ack, 1));
    }
}

int main() {
    MockFlash flash{};
    auto storage = make_storage(flash);

    std::array<util::u8, 64> scratch{};
    boot::FlashConfig flash_cfg{
        .erase_size = 64,
        .write_size = 16,
        .scratch = scratch.data(),
        .scratch_size = static_cast<util::u32>(scratch.size())
    };

    boot::BootConfig cfg{
        .slot_a = {0, 512},
        .slot_b = {512, 512},
        .info = {1024, 64}
    };

    constexpr util::u32 key = 0xA5A5u;
    const auto image_a = build_image("image_a", true, 2, key);
    const auto image_b = build_image("image_b_upgrade", true, 3, key, 4);
    const auto image_bad_entry = build_image("tiny", true, 4, key, 8);
    const auto image_xip = build_image(
        "xip_image",
        true,
        5,
        key,
        2,
        static_cast<util::u16>(boot::ImageFlags::xip_payload));

    boot::BootInfo initial_info{};
    initial_info.active = boot::Slot::a;
    initial_info.pending = boot::Slot::a;
    initial_info.min_version = 1;

    boot::Policy policy{
        .min_version = 1,
        .sign_key = key,
        .require_signature = true
    };

    const bool slot_a_written = boot::flash_write(
        storage,
        cfg.slot_a.offset,
        std::span<const util::u8>(image_a.data(), image_a.size()),
        flash_cfg);

    boot::XyModemSessionConfig download_cfg{
        .boot = cfg,
        .policy = policy,
        .target_slot = boot::Slot::b,
        .flash = flash_cfg,
        .require_header = true,
        .trim_to_header_size = true,
        .max_size = cfg.slot_b.size,
        .seed_info = initial_info,
        .has_seed_info = true,
        .write_pending = true
    };
    boot::XyModemSession<1024> receiver(storage, download_cfg);
    receiver.start();
    const bool transfer_ok = send_image(
        receiver,
        "slot_b.bin",
        std::span<const util::u8>(image_b.data(), image_b.size()));
    const auto download = receiver.complete();
    const auto plan = receiver.decide_boot();
    MockLaunchContext launch_ctx{
        .expected_payload_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)),
        .expected_storage_entry_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)) + 4
    };
    platform::board::BoardCaps board_caps{};
    board_caps.boot_load = platform::board::BootLoadDesc{
        .ctx = &launch_ctx,
        .resolve_payload_base = resolve_mock_payload_base,
        .load_payload = load_mock_payload
    };
    board_caps.boot_exec = platform::board::BootExecDesc{
        .ctx = &launch_ctx,
        .prepare_jump = prepare_mock_execution,
        .jump = jump_mock_execution
    };
    auto handoff = receiver.prepare_handoff(board_caps);
    const bool prepared = handoff.rollback_prepared;
    const auto prepared_result = receiver.result();
    const auto rollback_plan = boot::decide_boot_policy(storage, cfg, policy);
    const auto& target = handoff.target;
    const auto& load = handoff.load;
    const auto& image = handoff.image;
    const bool executed = boot::execute_boot_handoff(handoff, board_caps);
    const auto& execution = handoff.execution;
    const bool marked = receiver.mark_selected_success();
    const auto final_result = receiver.result();
    const bool slot_b_valid = boot::verify_partition_policy(storage, cfg.slot_b, policy, final_result.info);

    MockFlash headerless_flash{};
    auto headerless_storage = make_storage(headerless_flash);
    const bool headerless_slot_a_written = boot::flash_write(
        headerless_storage,
        cfg.slot_a.offset,
        std::span<const util::u8>(image_a.data(), image_a.size()),
        flash_cfg);
    boot::XyModemSession<1024> headerless_session(headerless_storage, download_cfg);
    headerless_session.start();
    const bool headerless_transport = send_image_without_header(
        headerless_session,
        std::span<const util::u8>(image_b.data(), image_b.size()));
    const auto headerless = headerless_session.complete();
    const bool headerless_failed =
        headerless_slot_a_written &&
        headerless_transport &&
        !static_cast<bool>(headerless) &&
        headerless.stage == boot::SessionStage::failed &&
        headerless.transfer.header_missing &&
        !headerless.ready_to_boot;

    MockFlash bad_entry_flash{};
    auto bad_entry_storage = make_storage(bad_entry_flash);
    const bool bad_entry_written = boot::flash_write(
        bad_entry_storage,
        cfg.slot_b.offset,
        std::span<const util::u8>(image_bad_entry.data(), image_bad_entry.size()),
        flash_cfg);
    const bool bad_entry_valid =
        boot::verify_partition_policy(bad_entry_storage, cfg.slot_b, policy, initial_info);
    boot::BootPlan bad_entry_plan{};
    bad_entry_plan.boot = {boot::BootStatus::ok, boot::Slot::b};
    bad_entry_plan.info = initial_info;
    const auto bad_entry_target = boot::resolve_boot_target(bad_entry_storage, cfg, bad_entry_plan);
    const bool bad_entry_rejected =
        bad_entry_written &&
        !bad_entry_valid &&
        !static_cast<bool>(bad_entry_target);

    MockFlash xip_flash{};
    auto xip_storage = make_storage(xip_flash);
    const bool xip_written = boot::flash_write(
        xip_storage,
        cfg.slot_b.offset,
        std::span<const util::u8>(image_xip.data(), image_xip.size()),
        flash_cfg);
    MockLaunchContext xip_ctx{
        .expected_payload_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)),
        .expected_storage_entry_offset =
            cfg.slot_b.offset + static_cast<util::u32>(sizeof(boot::ImageHeader)) + 2,
        .expected_load_kind = platform::board::BootLoadKind::xip
    };
    platform::board::BoardCaps xip_caps{};
    xip_caps.boot_load = platform::board::BootLoadDesc{
        .ctx = &xip_ctx,
        .resolve_payload_base = resolve_mock_payload_base,
        .load_payload = load_mock_payload
    };
    boot::BootPlan xip_plan{};
    xip_plan.boot = {boot::BootStatus::ok, boot::Slot::b};
    xip_plan.info = initial_info;
    const auto xip_target = boot::resolve_boot_target(xip_storage, cfg, xip_plan);
    const auto xip_load = boot::make_boot_load_plan(xip_target);
    auto xip_image_loaded = boot::resolve_boot_loaded_image(xip_load, xip_caps);
    const bool xip_ready = boot::prepare_boot_loaded_image(xip_image_loaded, xip_caps);
    const bool xip_ok =
        xip_written &&
        static_cast<bool>(xip_target) &&
        static_cast<bool>(xip_load) &&
        xip_load.kind == boot::BootLoadKind::xip &&
        !xip_load.transfer_required &&
        xip_ready &&
        static_cast<bool>(xip_image_loaded) &&
        xip_ctx.resolve_called &&
        !xip_ctx.load_called;

    const bool ok = slot_a_written &&
                    transfer_ok &&
                    static_cast<bool>(download) &&
                    download.pending_set &&
                    download.boot_info_written &&
                    static_cast<bool>(plan) &&
                    plan.boot.status == boot::BootStatus::ok &&
                    slot_b_valid &&
                    plan.boot.slot == boot::Slot::b &&
                    plan.reason == boot::BootSelectionReason::pending_trial &&
                    plan.prepare_required &&
                    static_cast<bool>(handoff) &&
                    static_cast<bool>(target) &&
                    target.partition.offset == cfg.slot_b.offset &&
                    static_cast<bool>(load) &&
                    load.kind == boot::BootLoadKind::copy_to_ram &&
                    load.transfer_required &&
                    static_cast<bool>(image) &&
                    image.payload_ready &&
                    target.payload_offset == cfg.slot_b.offset + sizeof(boot::ImageHeader) &&
                    target.storage_entry_offset == cfg.slot_b.offset + sizeof(boot::ImageHeader) + 4 &&
                    prepared &&
                    prepared_result.boot_prepared &&
                    prepared_result.plan.prepared &&
                    !prepared_result.plan.prepare_required &&
                    static_cast<bool>(rollback_plan) &&
                    rollback_plan.boot.slot == boot::Slot::a &&
                    rollback_plan.reason == boot::BootSelectionReason::active &&
                    static_cast<bool>(execution) &&
                    execution.entry_addr == reinterpret_cast<util::usize>(&mock_boot_entry) &&
                    execution.prepared &&
                    execution.jumped &&
                    executed &&
                    launch_ctx.prepare_called &&
                    launch_ctx.jump_called &&
                    launch_ctx.entry_called &&
                    launch_ctx.resolve_called &&
                    launch_ctx.load_called &&
                    marked &&
                    final_result.success_marked &&
                    final_result.plan.prepared &&
                    !final_result.plan.confirm_required &&
                    final_result.info.active == boot::Slot::b &&
                    headerless_failed &&
                    bad_entry_rejected &&
                    xip_ok;

    std::printf("[boot] slot_a_written=%d\n", slot_a_written ? 1 : 0);
    std::printf("[boot] xymodem_transport=%d\n", transfer_ok ? 1 : 0);
    std::printf("[boot] session_stage=%u pending=%d bootinfo=%d\n",
                static_cast<unsigned>(download.stage),
                download.pending_set ? 1 : 0,
                download.boot_info_written ? 1 : 0);
    std::printf("[boot] xymodem_download=%d bytes=%u expected=%u status=%u\n",
                static_cast<bool>(download) ? 1 : 0,
                download.transfer.bytes_written,
                download.transfer.expected_size,
                static_cast<unsigned>(download.transfer.transport_status));
    std::printf("[boot] slot_b_valid=%d\n", slot_b_valid ? 1 : 0);
    std::printf("[boot] boot_plan=%s slot=%s prepare=%d\n",
                selection_reason_name(plan.reason),
                plan.boot.slot == boot::Slot::a ? "A" : "B",
                plan.prepare_required ? 1 : 0);
    std::printf("[boot] boot_target=%d payload=%u entry=%u\n",
                static_cast<bool>(target) ? 1 : 0,
                target.payload_offset,
                target.storage_entry_offset);
    std::printf("[boot] boot_load=%s transfer=%d ready=%d\n",
                load_kind_name(load.kind),
                load.transfer_required ? 1 : 0,
                image.payload_ready ? 1 : 0);
    std::printf("[boot] boot_handoff=%d rollback=%d ready=%d\n",
                static_cast<bool>(handoff) ? 1 : 0,
                handoff.rollback_prepared ? 1 : 0,
                handoff.ready_to_jump ? 1 : 0);
    std::printf("[boot] boot_exec=%d prepared=%d jumped=%d entry=%d\n",
                static_cast<bool>(execution) ? 1 : 0,
                execution.prepared ? 1 : 0,
                execution.jumped ? 1 : 0,
                launch_ctx.entry_called ? 1 : 0);
    std::printf("[boot] prepare_boot=%d rollback_plan=%s:%s\n",
                prepared ? 1 : 0,
                selection_reason_name(rollback_plan.reason),
                rollback_plan.boot.slot == boot::Slot::a ? "A" : "B");
    std::printf("[boot] mark_success=%d active=%s\n",
                marked ? 1 : 0,
                final_result.info.active == boot::Slot::a ? "A" : "B");
    std::printf("[boot] headerless_fail=%d stage=%u missing_header=%d\n",
                headerless_failed ? 1 : 0,
                static_cast<unsigned>(headerless.stage),
                headerless.transfer.header_missing ? 1 : 0);
    std::printf("[boot] bad_entry_rejected=%d\n", bad_entry_rejected ? 1 : 0);
    std::printf("[boot] xip_load=%d kind=%s\n", xip_ok ? 1 : 0, load_kind_name(xip_load.kind));
    std::printf("[boot] ok=%d\n", ok ? 1 : 0);
    return ok ? 0 : 1;
}
