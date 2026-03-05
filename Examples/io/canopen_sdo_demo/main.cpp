#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

import canopen.types;
import canopen.od;
import canopen.sdo;
import canopen.transport;
import canopen.sdo_service;
import canopen.nmt;
import canopen.nmt_service;
import util.core;
import util.error;
import out.api;

namespace {
    struct StdoutSink {
        out::result<std::size_t> write(out::bytes b) noexcept {
            if (b.empty()) return out::ok<std::size_t>(0u);
            const auto n = std::fwrite(b.data(), 1, b.size(), stdout);
            return out::ok(n);
        }

        out::result<std::size_t> flush() noexcept {
            (void)std::fflush(stdout);
            return out::ok<std::size_t>(0u);
        }
    };

    StdoutSink g_console{};

    struct Check {
        int failures = 0;

        void expect(bool ok, const char* msg) noexcept {
            if (ok) return;
            ++failures;
            (void)out::println<"[fail] {}">(g_console, msg);
        }

        template <class T, class U>
        void expect_eq(const char* msg, const T& a, const U& b) noexcept {
            if (a == b) return;
            ++failures;
            (void)out::println<"[fail] {}: {} != {}">(g_console, msg, a, b);
        }
    };

    struct FrameQueue {
        std::array<canopen::CanFrame, 8> buf{};
        std::uint8_t head{0};
        std::uint8_t tail{0};
        std::uint8_t count{0};

        bool push(const canopen::CanFrame& f) noexcept {
            if (count >= buf.size()) return false;
            buf[tail] = f;
            tail = static_cast<std::uint8_t>((tail + 1) % buf.size());
            ++count;
            return true;
        }

        bool pop(canopen::CanFrame& f) noexcept {
            if (count == 0) return false;
            f = buf[head];
            head = static_cast<std::uint8_t>((head + 1) % buf.size());
            --count;
            return true;
        }
    };

    struct FakeTransport {
        FrameQueue rx{};
        FrameQueue tx{};
    };

    util::Result<util::usize> transport_recv(void* ctx, std::span<canopen::CanFrame> out) noexcept {
        if (!ctx || out.empty()) return util::unexpected(util::Errc::invalid_arg);
        auto* t = static_cast<FakeTransport*>(ctx);
        if (!t->rx.pop(out[0])) return util::unexpected(util::Errc::would_block);
        return util::Result<util::usize>{1};
    }

    util::Result<util::usize> transport_send(void* ctx, std::span<const canopen::CanFrame> in) noexcept {
        if (!ctx || in.empty()) return util::unexpected(util::Errc::invalid_arg);
        auto* t = static_cast<FakeTransport*>(ctx);
        if (!t->tx.push(in[0])) return util::unexpected(util::Errc::would_block);
        return util::Result<util::usize>{1};
    }

    canopen::CanFrame make_download(canopen::NodeId node,
                                    canopen::Index index,
                                    canopen::SubIndex sub,
                                    util::u32 value) noexcept {
        canopen::CanFrame f{};
        f.id = canopen::sdo_request_id(node);
        f.dlc = 8;
        f.data.fill(0);
        f.data[0] = 0x23; // expedited + size indicated, 4 bytes
        f.data[1] = static_cast<util::u8>(index & 0xFFu);
        f.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
        f.data[3] = sub;
        f.data[4] = static_cast<util::u8>(value & 0xFFu);
        f.data[5] = static_cast<util::u8>((value >> 8) & 0xFFu);
        f.data[6] = static_cast<util::u8>((value >> 16) & 0xFFu);
        f.data[7] = static_cast<util::u8>((value >> 24) & 0xFFu);
        return f;
    }

    canopen::CanFrame make_upload(canopen::NodeId node,
                                  canopen::Index index,
                                  canopen::SubIndex sub) noexcept {
        canopen::CanFrame f{};
        f.id = canopen::sdo_request_id(node);
        f.dlc = 8;
        f.data.fill(0);
        f.data[0] = 0x40; // upload request
        f.data[1] = static_cast<util::u8>(index & 0xFFu);
        f.data[2] = static_cast<util::u8>((index >> 8) & 0xFFu);
        f.data[3] = sub;
        return f;
    }

    void dump_frame(const char* tag, const canopen::CanFrame& f) noexcept {
        (void)out::println<"[{}] id=0x{:03X} dlc={} d0={:02X} d1={:02X} d2={:02X} d3={:02X} d4={:02X} d5={:02X} d6={:02X} d7={:02X}">(
            g_console,
            tag,
            f.id,
            f.dlc,
            f.data[0], f.data[1], f.data[2], f.data[3],
            f.data[4], f.data[5], f.data[6], f.data[7]);
    }

    util::u16 crc16_ccitt(std::span<const std::byte> data) noexcept {
        util::u16 crc = 0x0000u;
        for (auto b : data) {
            crc ^= static_cast<util::u16>(static_cast<util::u8>(b)) << 8;
            for (int i = 0; i < 8; ++i) {
                if ((crc & 0x8000u) != 0) {
                    crc = static_cast<util::u16>((crc << 1) ^ 0x1021u);
                } else {
                    crc = static_cast<util::u16>(crc << 1);
                }
            }
        }
        return crc;
    }

    void expect_abort(Check& ck,
                      const char* label,
                      const canopen::CanFrame& f,
                      canopen::Index index,
                      canopen::SubIndex sub,
                      util::u32 code) noexcept {
        ck.expect_eq("abort id", f.id, canopen::sdo_response_id(1));
        ck.expect_eq("abort cmd", f.data[0], static_cast<util::u8>(0x80u));
        ck.expect_eq("abort idx lo", f.data[1], static_cast<util::u8>(index & 0xFFu));
        ck.expect_eq("abort idx hi", f.data[2], static_cast<util::u8>((index >> 8) & 0xFFu));
        ck.expect_eq("abort sub", f.data[3], sub);
        const util::u32 got = static_cast<util::u32>(f.data[4]) |
                              (static_cast<util::u32>(f.data[5]) << 8) |
                              (static_cast<util::u32>(f.data[6]) << 16) |
                              (static_cast<util::u32>(f.data[7]) << 24);
        ck.expect_eq(label, got, code);
    }

    canopen::CanFrame make_nmt(canopen::NmtCommand cmd, canopen::NodeId node) noexcept {
        canopen::CanFrame f{};
        f.id = canopen::nmt_id();
        f.dlc = 2;
        f.data.fill(0);
        f.data[0] = static_cast<util::u8>(cmd);
        f.data[1] = node;
        return f;
    }
}

int main() {
    (void)out::println<"[canopen_sdo_demo] start">(g_console);

    Check ck{};
    util::u32 value = 0x12345678u;
    const util::u16 ro_value = 0x55AAu;
    std::array<std::byte, 4> blob{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
        std::byte{0x40},
    };
    canopen::BytesRef blob_ref{std::span<std::byte>(blob)};
    std::array<std::byte, 16> seg_buf{};
    util::u32 seg_size = 10;
    canopen::BytesVarRef seg_ref{std::span<std::byte>(seg_buf), &seg_size};
    std::array<canopen::Entry, 4> entries{
        canopen::make_entry(0x2000, 0x00, value, canopen::Access::read_write),
        canopen::make_entry_ro(0x2002, 0x00, ro_value),
        canopen::make_bytes_entry(0x2001, 0x00, blob_ref, canopen::Access::read_write, true),
        canopen::make_bytes_var_entry(0x3000, 0x00, seg_ref, canopen::Access::read_write),
    };

    canopen::ObjectDictionary od{entries};
    canopen::SdoServer sdo{od, {.node_id = 1}};

    FakeTransport transport_ctx{};
    canopen::Transport transport{
        &transport_ctx,
        {.recv = &transport_recv, .send = &transport_send},
    };

    canopen::SdoService service{sdo, transport, {.rx_budget = 2}};

    transport_ctx.rx.push(make_download(1, 0x2000, 0x00, 0xAABBCCDDu));
    (void)service.poll();

    canopen::CanFrame tx{};
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("download_rsp", tx);
        ck.expect_eq("download rsp id", tx.id, canopen::sdo_response_id(1));
        ck.expect_eq("download rsp cmd", tx.data[0], static_cast<util::u8>(0x60u));
    }

    transport_ctx.rx.push(make_upload(1, 0x2000, 0x00));
    (void)service.poll();
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("upload_rsp", tx);
        ck.expect_eq("upload rsp id", tx.id, canopen::sdo_response_id(1));
        ck.expect_eq("upload rsp cmd", tx.data[0], static_cast<util::u8>(0x43u));
    }

    // Variable-size byte entry: download 2 bytes into a 4-byte object.
    {
        canopen::CanFrame f{};
        f.id = canopen::sdo_request_id(1);
        f.dlc = 8;
        f.data.fill(0);
        f.data[0] = 0x2Bu; // expedited, size indicated, 2 bytes
        f.data[1] = 0x01;
        f.data[2] = 0x20;
        f.data[3] = 0x00;
        f.data[4] = 0xFE;
        f.data[5] = 0xED;
        transport_ctx.rx.push(f);
        (void)service.poll();
        if (transport_ctx.tx.pop(tx)) {
            dump_frame("bytes_rsp", tx);
            ck.expect_eq("bytes rsp cmd", tx.data[0], static_cast<util::u8>(0x60u));
        }
    }

    // Read-only entry: download should abort with ReadOnly.
    transport_ctx.rx.push(make_download(1, 0x2002, 0x00, 0x11223344u));
    (void)service.poll();
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("ro_abort", tx);
        expect_abort(ck, "ro abort code", tx, 0x2002, 0x00, 0x06010002u);
    }

    // Missing entry: upload should abort.
    transport_ctx.rx.push(make_upload(1, 0x2003, 0x00));
    (void)service.poll();
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("abort_rsp", tx);
        expect_abort(ck, "missing abort code", tx, 0x2003, 0x00, 0x06020000u);
    }

    // Block download (10 bytes) into 0x3000.
    {
        canopen::CanFrame init{};
        init.id = canopen::sdo_request_id(1);
        init.dlc = 8;
        init.data.fill(0);
        init.data[0] = 0xC6u; // block download init (crc+size)
        init.data[1] = 0x00;
        init.data[2] = 0x30;
        init.data[3] = 0x00;
        init.data[4] = 10;
        transport_ctx.rx.push(init);
        (void)service.poll();
        if (transport_ctx.tx.pop(tx)) {
            dump_frame("seg_init_rsp", tx);
            ck.expect_eq("seg init cmd", tx.data[0], static_cast<util::u8>(0xA4u));
        }

        canopen::CanFrame seg0{};
        seg0.id = canopen::sdo_request_id(1);
        seg0.dlc = 8;
        seg0.data.fill(0);
        seg0.data[0] = 0x01u; // seq=1
        for (int i = 0; i < 7; ++i) {
            seg0.data[1 + i] = static_cast<util::u8>(0xA0u + i);
        }
        transport_ctx.rx.push(seg0);
        (void)service.poll();
        ck.expect(!transport_ctx.tx.pop(tx), "seg0 no ack");

        canopen::CanFrame seg1{};
        seg1.id = canopen::sdo_request_id(1);
        seg1.dlc = 8;
        seg1.data.fill(0);
        seg1.data[0] = 0x82u; // seq=2, last=1
        seg1.data[1] = 0xA7;
        seg1.data[2] = 0xA8;
        seg1.data[3] = 0xA9;
        transport_ctx.rx.push(seg1);
        (void)service.poll();
        if (transport_ctx.tx.pop(tx)) {
            dump_frame("seg1_rsp", tx);
            ck.expect_eq("seg1 cmd", tx.data[0], static_cast<util::u8>(0xA2u));
        }

        canopen::CanFrame end{};
        end.id = canopen::sdo_request_id(1);
        end.dlc = 8;
        end.data.fill(0);
        end.data[0] = 0xC1u; // block download end
        end.data[1] = 4u; // unused bytes in last segment
        const std::array<std::byte, 10> payload{
            std::byte{0xA0}, std::byte{0xA1}, std::byte{0xA2}, std::byte{0xA3},
            std::byte{0xA4}, std::byte{0xA5}, std::byte{0xA6}, std::byte{0xA7},
            std::byte{0xA8}, std::byte{0xA9},
        };
        const auto crc = crc16_ccitt(payload);
        end.data[2] = static_cast<util::u8>(crc & 0xFFu);
        end.data[3] = static_cast<util::u8>((crc >> 8) & 0xFFu);
        transport_ctx.rx.push(end);
        (void)service.poll();
        if (transport_ctx.tx.pop(tx)) {
            dump_frame("seg_end_rsp", tx);
            ck.expect_eq("seg end cmd", tx.data[0], static_cast<util::u8>(0xA1u));
            ck.expect_eq("seg end layout b1", tx.data[1], static_cast<util::u8>(0u));
            ck.expect_eq("seg end layout b2", tx.data[2], static_cast<util::u8>(0u));
            ck.expect_eq("seg end layout b3", tx.data[3], static_cast<util::u8>(0u));
        }
    }

    // Block download end: n mismatch should abort.
    {
        FakeTransport neg_ctx{};
        canopen::SdoServer neg_sdo{od, {.node_id = 1}};
        canopen::Transport neg_transport{
            &neg_ctx,
            {.recv = &transport_recv, .send = &transport_send},
        };
        canopen::SdoService neg_service{neg_sdo, neg_transport, {.rx_budget = 2}};

        canopen::CanFrame init{};
        init.id = canopen::sdo_request_id(1);
        init.dlc = 8;
        init.data.fill(0);
        init.data[0] = 0xC6u;
        init.data[1] = 0x00;
        init.data[2] = 0x30;
        init.data[3] = 0x00;
        init.data[4] = 10;
        neg_ctx.rx.push(init);
        (void)neg_service.poll();
        (void)neg_ctx.tx.pop(tx);

        canopen::CanFrame seg0{};
        seg0.id = canopen::sdo_request_id(1);
        seg0.dlc = 8;
        seg0.data.fill(0);
        seg0.data[0] = 0x01u;
        for (int i = 0; i < 7; ++i) {
            seg0.data[1 + i] = static_cast<util::u8>(0xA0u + i);
        }
        neg_ctx.rx.push(seg0);
        (void)neg_service.poll();

        canopen::CanFrame seg1{};
        seg1.id = canopen::sdo_request_id(1);
        seg1.dlc = 8;
        seg1.data.fill(0);
        seg1.data[0] = 0x82u;
        seg1.data[1] = 0xA7;
        seg1.data[2] = 0xA8;
        seg1.data[3] = 0xA9;
        neg_ctx.rx.push(seg1);
        (void)neg_service.poll();
        (void)neg_ctx.tx.pop(tx);

        canopen::CanFrame end{};
        end.id = canopen::sdo_request_id(1);
        end.dlc = 8;
        end.data.fill(0);
        end.data[0] = 0xC1u;
        end.data[1] = 3u; // wrong n (expected 4)
        const std::array<std::byte, 10> payload{
            std::byte{0xA0}, std::byte{0xA1}, std::byte{0xA2}, std::byte{0xA3},
            std::byte{0xA4}, std::byte{0xA5}, std::byte{0xA6}, std::byte{0xA7},
            std::byte{0xA8}, std::byte{0xA9},
        };
        const auto crc = crc16_ccitt(payload);
        end.data[2] = static_cast<util::u8>(crc & 0xFFu);
        end.data[3] = static_cast<util::u8>((crc >> 8) & 0xFFu);
        neg_ctx.rx.push(end);
        (void)neg_service.poll();
        if (neg_ctx.tx.pop(tx)) {
            dump_frame("seg_end_bad_n", tx);
            expect_abort(ck, "seg end bad n", tx, 0x3000, 0x00, 0x05040001u);
        } else {
            ck.expect(false, "seg end bad n abort missing");
        }
    }

    // Block download end: CRC error should abort.
    {
        FakeTransport neg_ctx{};
        canopen::SdoServer neg_sdo{od, {.node_id = 1}};
        canopen::Transport neg_transport{
            &neg_ctx,
            {.recv = &transport_recv, .send = &transport_send},
        };
        canopen::SdoService neg_service{neg_sdo, neg_transport, {.rx_budget = 2}};

        canopen::CanFrame init{};
        init.id = canopen::sdo_request_id(1);
        init.dlc = 8;
        init.data.fill(0);
        init.data[0] = 0xC6u;
        init.data[1] = 0x00;
        init.data[2] = 0x30;
        init.data[3] = 0x00;
        init.data[4] = 10;
        neg_ctx.rx.push(init);
        (void)neg_service.poll();
        (void)neg_ctx.tx.pop(tx);

        canopen::CanFrame seg0{};
        seg0.id = canopen::sdo_request_id(1);
        seg0.dlc = 8;
        seg0.data.fill(0);
        seg0.data[0] = 0x01u;
        for (int i = 0; i < 7; ++i) {
            seg0.data[1 + i] = static_cast<util::u8>(0xA0u + i);
        }
        neg_ctx.rx.push(seg0);
        (void)neg_service.poll();

        canopen::CanFrame seg1{};
        seg1.id = canopen::sdo_request_id(1);
        seg1.dlc = 8;
        seg1.data.fill(0);
        seg1.data[0] = 0x82u;
        seg1.data[1] = 0xA7;
        seg1.data[2] = 0xA8;
        seg1.data[3] = 0xA9;
        neg_ctx.rx.push(seg1);
        (void)neg_service.poll();
        (void)neg_ctx.tx.pop(tx);

        canopen::CanFrame end{};
        end.id = canopen::sdo_request_id(1);
        end.dlc = 8;
        end.data.fill(0);
        end.data[0] = 0xC1u;
        end.data[1] = 4u;
        end.data[2] = 0x00u; // wrong CRC
        end.data[3] = 0x00u;
        neg_ctx.rx.push(end);
        (void)neg_service.poll();
        if (neg_ctx.tx.pop(tx)) {
            dump_frame("seg_end_bad_crc", tx);
            expect_abort(ck, "seg end bad crc", tx, 0x3000, 0x00, 0x05040004u);
        } else {
            ck.expect(false, "seg end bad crc abort missing");
        }
    }

    // Block upload for 0x3000.
    {
        canopen::CanFrame req{};
        req.id = canopen::sdo_request_id(1);
        req.dlc = 8;
        req.data.fill(0);
        req.data[0] = 0xA4u; // block upload init
        req.data[1] = 0x00;
        req.data[2] = 0x30;
        req.data[3] = 0x00;
        transport_ctx.rx.push(req);
    }
    (void)service.poll();
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("seg_up_init", tx);
        ck.expect_eq("seg up init cmd", tx.data[0], static_cast<util::u8>(0xC4u));
        ck.expect_eq("seg up size", tx.data[4], static_cast<util::u8>(10u));
    }

    canopen::CanFrame up_req0{};
    up_req0.id = canopen::sdo_request_id(1);
    up_req0.dlc = 8;
    up_req0.data.fill(0);
    up_req0.data[0] = 0xA2u; // block upload ack
    up_req0.data[1] = 0x00u; // ack seq
    up_req0.data[2] = 0x02u; // block size
    transport_ctx.rx.push(up_req0);
    (void)service.poll();
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("seg_up0", tx);
        ck.expect_eq("seg up0 cmd", tx.data[0], static_cast<util::u8>(0x01u));
    }

    (void)service.poll();
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("seg_up1", tx);
        ck.expect_eq("seg up1 cmd", tx.data[0], static_cast<util::u8>(0x82u));
    }

    (void)service.poll();
    if (transport_ctx.tx.pop(tx)) {
        dump_frame("seg_up_end", tx);
        ck.expect_eq("seg up end cmd", tx.data[0], static_cast<util::u8>(0xC1u));
        ck.expect_eq("seg up end n", tx.data[1], static_cast<util::u8>(4u));
    }

    canopen::CanFrame up_end{};
    up_end.id = canopen::sdo_request_id(1);
    up_end.dlc = 8;
    up_end.data.fill(0);
    up_end.data[0] = 0xA1u; // block upload end ack
    up_end.data[1] = 4u;    // unused bytes in last segment
    transport_ctx.rx.push(up_end);
    (void)service.poll();
    ck.expect(!transport_ctx.tx.pop(tx), "seg up end ack no tx");

    // Block upload ack sequence mismatch should abort.
    {
        FakeTransport neg_ctx{};
        canopen::SdoServer neg_sdo{od, {.node_id = 1}};
        canopen::Transport neg_transport{
            &neg_ctx,
            {.recv = &transport_recv, .send = &transport_send},
        };
        canopen::SdoService neg_service{neg_sdo, neg_transport, {.rx_budget = 1}};

        canopen::CanFrame req{};
        req.id = canopen::sdo_request_id(1);
        req.dlc = 8;
        req.data.fill(0);
        req.data[0] = 0xA4u;
        req.data[1] = 0x00;
        req.data[2] = 0x30;
        req.data[3] = 0x00;
        neg_ctx.rx.push(req);
        (void)neg_service.poll();
        (void)neg_ctx.tx.pop(tx);

        canopen::CanFrame ack{};
        ack.id = canopen::sdo_request_id(1);
        ack.dlc = 8;
        ack.data.fill(0);
        ack.data[0] = 0xA2u;
        ack.data[1] = 0x01u; // wrong ack seq
        ack.data[2] = 0x02u;
        neg_ctx.rx.push(ack);
        (void)neg_service.poll();
        if (neg_ctx.tx.pop(tx)) {
            dump_frame("seg_up_bad_ack", tx);
            expect_abort(ck, "seg up bad ack", tx, 0x3000, 0x00, 0x05030000u);
        } else {
            ck.expect(false, "seg up bad ack abort missing");
        }
    }

    // Timeout: start segmented upload then advance time past timeout.
    {
        FakeTransport timeout_ctx{};
        canopen::SdoServer timeout_sdo{od, {.node_id = 1, .timeout_ms = 5}};
        canopen::Transport timeout_transport{
            &timeout_ctx,
            {.recv = &transport_recv, .send = &transport_send},
        };
        canopen::SdoService timeout_service{timeout_sdo, timeout_transport, {.rx_budget = 1}};

        util::u32 now_ms = 100u;
        timeout_ctx.rx.push(make_upload(1, 0x3000, 0x00));
        (void)timeout_service.poll_time(now_ms);
        (void)timeout_ctx.tx.pop(tx); // segmented upload init

        now_ms += 10u;
        (void)timeout_service.poll_time(now_ms);
        if (timeout_ctx.tx.pop(tx)) {
            dump_frame("timeout_abort", tx);
            expect_abort(ck, "timeout abort code", tx, 0x3000, 0x00, 0x05040000u);
        } else {
            ck.expect(false, "timeout abort missing");
        }
    }

    // Timeout boundary: within timeout should not abort.
    {
        FakeTransport timeout_ctx{};
        canopen::SdoServer timeout_sdo{od, {.node_id = 1, .timeout_ms = 5}};
        canopen::Transport timeout_transport{
            &timeout_ctx,
            {.recv = &transport_recv, .send = &transport_send},
        };
        canopen::SdoService timeout_service{timeout_sdo, timeout_transport, {.rx_budget = 1}};

        util::u32 now_ms = 200u;
        timeout_ctx.rx.push(make_upload(1, 0x3000, 0x00));
        (void)timeout_service.poll_time(now_ms);
        (void)timeout_ctx.tx.pop(tx); // segmented upload init

        now_ms += 4u; // within timeout
        (void)timeout_service.poll_time(now_ms);
        ck.expect(!timeout_ctx.tx.pop(tx), "timeout boundary should not abort");
    }

    // NMT: boot-up, heartbeat, and reset handling.
    {
        FakeTransport nmt_ctx{};
        canopen::NmtNode node{{
            .node_id = 3,
            .heartbeat_ms = 10,
            .send_bootup = true,
        }};
        canopen::Transport nmt_transport{
            &nmt_ctx,
            {.recv = &transport_recv, .send = &transport_send},
        };
        canopen::NmtService nmt_service{node, nmt_transport, {.rx_budget = 1}};

        util::u32 now_ms = 100;
        (void)nmt_service.poll_time(now_ms);
        if (nmt_ctx.tx.pop(tx)) {
            dump_frame("nmt_bootup", tx);
            ck.expect_eq("nmt bootup id", tx.id, canopen::heartbeat_id(3));
            ck.expect_eq("nmt bootup state", tx.data[0], static_cast<util::u8>(canopen::NodeState::initializing));
        } else {
            ck.expect(false, "nmt bootup missing");
        }

        now_ms += 5;
        (void)nmt_service.poll_time(now_ms);
        ck.expect(!nmt_ctx.tx.pop(tx), "nmt heartbeat too early");

        now_ms += 5;
        (void)nmt_service.poll_time(now_ms);
        if (nmt_ctx.tx.pop(tx)) {
            dump_frame("nmt_hb0", tx);
            ck.expect_eq("nmt hb id", tx.id, canopen::heartbeat_id(3));
            ck.expect_eq("nmt hb state", tx.data[0], static_cast<util::u8>(canopen::NodeState::pre_operational));
        } else {
            ck.expect(false, "nmt heartbeat missing");
        }

        nmt_ctx.rx.push(make_nmt(canopen::NmtCommand::start, 3));
        (void)nmt_service.poll();
        ck.expect_eq("nmt start state", node.state(), canopen::NodeState::operational);

        now_ms += 10;
        (void)nmt_service.poll_time(now_ms);
        if (nmt_ctx.tx.pop(tx)) {
            dump_frame("nmt_hb1", tx);
            ck.expect_eq("nmt hb1 state", tx.data[0], static_cast<util::u8>(canopen::NodeState::operational));
        } else {
            ck.expect(false, "nmt heartbeat op missing");
        }

        nmt_ctx.rx.push(make_nmt(canopen::NmtCommand::reset_node, 3));
        (void)nmt_service.poll();
        ck.expect(node.reset_node_pending(), "nmt reset flag");
        node.clear_reset_flags();
        node.set_state(canopen::NodeState::pre_operational);

        now_ms += 1;
        (void)nmt_service.poll_time(now_ms);
        if (nmt_ctx.tx.pop(tx)) {
            dump_frame("nmt_bootup2", tx);
            ck.expect_eq("nmt bootup2 state", tx.data[0], static_cast<util::u8>(canopen::NodeState::initializing));
        } else {
            ck.expect(false, "nmt bootup2 missing");
        }
    }

    if (ck.failures == 0) {
        (void)out::println<"[canopen_sdo_demo] done">(g_console);
        return 0;
    }
    (void)out::println<"[canopen_sdo_demo] FAIL count={}">(g_console, ck.failures);
    return 1;
}
