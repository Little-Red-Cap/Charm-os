import io.proto.modem_xymodem;
import out.api;
import util.core;

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    struct Pipe {
        std::mutex mu;
        std::condition_variable cv;
        std::deque<util::u8> q;

        bool read_byte(util::u8& out, util::u32 timeout_ms) {
            std::unique_lock<std::mutex> lock(mu);
            if (q.empty()) {
                if (cv.wait_for(lock, std::chrono::milliseconds(timeout_ms)) == std::cv_status::timeout) {
                    return false;
                }
            }
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
    Duplex link{};

    modem::Callbacks sender_cb{};
    sender_cb.ctx = &link;
    sender_cb.read = [](void* ctx, util::u8& out, util::u32 timeout_ms) noexcept {
        auto* l = static_cast<Duplex*>(ctx);
        return l->b_to_a.read_byte(out, timeout_ms);
    };
    sender_cb.write = [](void* ctx, util::u8 byte) noexcept {
        auto* l = static_cast<Duplex*>(ctx);
        l->a_to_b.write_byte(byte);
    };
    sender_cb.write_data = [](void* ctx, std::span<const util::u8> data) noexcept {
        auto* l = static_cast<Duplex*>(ctx);
        l->a_to_b.write_data(data);
    };

    modem::Callbacks receiver_cb{};
    receiver_cb.ctx = &link;
    receiver_cb.read = [](void* ctx, util::u8& out, util::u32 timeout_ms) noexcept {
        auto* l = static_cast<Duplex*>(ctx);
        return l->a_to_b.read_byte(out, timeout_ms);
    };
    receiver_cb.write = [](void* ctx, util::u8 byte) noexcept {
        auto* l = static_cast<Duplex*>(ctx);
        l->b_to_a.write_byte(byte);
    };
    receiver_cb.write_data = [](void* ctx, std::span<const util::u8> data) noexcept {
        auto* l = static_cast<Duplex*>(ctx);
        l->b_to_a.write_data(data);
    };

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
            receiver_cb,
            cfg,
            [](void* ctx, std::span<const util::u8> data, util::usize len) noexcept {
                auto* vec = static_cast<std::vector<util::u8>*>(ctx);
                vec->insert(vec->end(), data.begin(), data.begin() + len);
            },
            &received);
        if (!res) {
            out::error<"x/y receive failed: status={} bytes={}">(static_cast<int>(res.status), res.bytes);
        }
    });

    struct SendState {
        std::array<util::u8, 2048>* buf{nullptr};
        util::usize offset{0};
    } send_state{&payload, 0};

    std::thread sender([&] {
        auto res = modem::send<1024>(
            sender_cb,
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
            out::error<"x/y send failed: status={} bytes={}">(static_cast<int>(res.status), res.bytes);
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
        out::info<"x/y modem demo ok, bytes={}">(received.size());
        return 0;
    }
    out::error<"x/y modem demo mismatch, bytes={}">(received.size());
    return 1;
}
