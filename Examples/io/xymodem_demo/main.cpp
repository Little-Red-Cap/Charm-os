import io.proto.modem_xymodem;
import io.channel;
import io.channel.adapters;
import out.api;
import out.channel;
import util.core;

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
    struct Pipe {
        std::mutex mu;
        std::condition_variable cv;
        std::deque<util::u8> q;

        bool try_read_byte(util::u8& out) {
            std::lock_guard<std::mutex> lock(mu);
            if (q.empty()) return false;
            out = q.front();
            q.pop_front();
            return true;
        }

        void write_byte(util::u8 b) {
            {
                std::lock_guard<std::mutex> lock(mu);
                q.push_back(b);
            }
            cv.notify_one();
        }

        void write_data(std::span<const util::u8> data) {
            {
                std::lock_guard<std::mutex> lock(mu);
                for (auto b : data) q.push_back(b);
            }
            cv.notify_one();
        }
    };

    struct Duplex {
        Pipe a_to_b;
        Pipe b_to_a;
    };
}

int main() {
    struct StdoutCtx {};
    StdoutCtx stdout_ctx{};
    io::UartChannel<StdoutCtx> stdout_ch{};
    stdout_ch.ctx = &stdout_ctx;
    stdout_ch.read = [](void*, io::MutByteView) noexcept -> io::result {
        return io::fail(io::errc::would_block);
    };
    stdout_ch.write = [](void*, io::ByteView buf) noexcept -> io::result {
        if (buf.empty()) return io::ok(0);
        const auto n = std::fwrite(buf.data(), 1, buf.size(), stdout);
        std::fflush(stdout);
        return io::ok(static_cast<util::usize>(n));
    };
    stdout_ch.flush = [](void*) noexcept -> io::result {
        std::fflush(stdout);
        return io::ok(0);
    };
    auto console = stdout_ch.channel();
    auto console_sink = out::make_channel_sink(console);

    Duplex link{};

    io::UartChannel<Duplex> sender{};
    sender.ctx = &link;
    sender.read = [](void* ctx, io::MutByteView buf) noexcept -> io::result {
        if (buf.empty()) return io::ok(0);
        auto* l = static_cast<Duplex*>(ctx);
        util::u8 b = 0;
        if (!l->b_to_a.try_read_byte(b)) return io::fail(io::errc::would_block);
        buf[0] = b;
        return io::ok(1);
    };
    sender.write = [](void* ctx, io::ByteView buf) noexcept -> io::result {
        auto* l = static_cast<Duplex*>(ctx);
        l->a_to_b.write_data(std::span<const util::u8>(buf.data(), buf.size()));
        return io::ok(buf.size());
    };
    sender.flush = [](void*) noexcept -> io::result { return io::ok(0); };

    io::UartChannel<Duplex> receiver{};
    receiver.ctx = &link;
    receiver.read = [](void* ctx, io::MutByteView buf) noexcept -> io::result {
        if (buf.empty()) return io::ok(0);
        auto* l = static_cast<Duplex*>(ctx);
        util::u8 b = 0;
        if (!l->a_to_b.try_read_byte(b)) return io::fail(io::errc::would_block);
        buf[0] = b;
        return io::ok(1);
    };
    receiver.write = [](void* ctx, io::ByteView buf) noexcept -> io::result {
        auto* l = static_cast<Duplex*>(ctx);
        l->b_to_a.write_data(std::span<const util::u8>(buf.data(), buf.size()));
        return io::ok(buf.size());
    };
    receiver.flush = [](void*) noexcept -> io::result { return io::ok(0); };

    auto sender_ch = sender.channel();
    auto receiver_ch = receiver.channel();

    std::array<util::u8, 2048> payload{};
    for (util::usize i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<util::u8>(i & 0xFF);
    }

    std::vector<util::u8> received;
    received.reserve(payload.size());

    modem::Config cfg{};
    cfg.timeout_ms = 200;
    cfg.max_retries = 10;
    cfg.use_1k = true;

    std::thread receiver([&] {
        auto res = modem::receive<1024>(
            receiver_ch,
            cfg,
            [](void* ctx, std::span<const util::u8> data, util::usize len) noexcept {
                auto* vec = static_cast<std::vector<util::u8>*>(ctx);
                vec->insert(vec->end(), data.begin(), data.begin() + len);
            },
            &received);
        if (!res) {
            out::error<"x/y receive failed: status={} bytes={}">(console_sink, static_cast<int>(res.status), res.bytes);
        }
    });

    struct SendState {
        std::array<util::u8, 2048>* buf{nullptr};
        util::usize offset{0};
    } send_state{&payload, 0};

    std::thread sender([&] {
        auto res = modem::send<1024>(
            sender_ch,
            cfg,
            [](void* ctx, std::span<util::u8> out, util::usize& len) noexcept {
                auto* state = static_cast<SendState*>(ctx);
                auto& buf = *state->buf;
                auto& off = state->offset;
                if (off >= buf.size()) return false;
                len = (std::min)(out.size(), buf.size() - off);
                for (util::usize i = 0; i < len; ++i) out[i] = buf[off + i];
                off += len;
                return true;
            },
            &send_state);
        if (!res) {
            out::error<"x/y send failed: status={} bytes={}">(console_sink, static_cast<int>(res.status), res.bytes);
        }
    });

    sender.join();
    receiver.join();

    bool ok = (received.size() >= payload.size());
    for (util::usize i = 0; i < payload.size() && i < received.size(); ++i) {
        if (received[i] != payload[i]) {
            ok = false;
            break;
        }
    }

    if (ok) {
        out::info<"x/y modem demo ok, bytes={}">(console_sink, received.size());
        return 0;
    }
    out::error<"x/y modem demo mismatch, bytes={}">(console_sink, received.size());
    return 1;
}
