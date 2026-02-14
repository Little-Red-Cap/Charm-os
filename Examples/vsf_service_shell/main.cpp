#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>

import util.core;
import service_ring_buffer;
import service_stream;
import service_trace;
import service_json;
import shell_core;
import shell_stdio;

struct TestStream {
    std::array<char, 64> buf{};
    std::size_t pos{0};

    service::StreamStatus read(std::span<util::u8>) noexcept {
        return service::StreamStatus{service::StreamResult::error};
    }

    service::StreamStatus write(std::span<const util::u8> in) noexcept {
        const auto n = in.size();
        if (pos + n > buf.size()) {
            return service::StreamStatus{service::StreamResult::error};
        }
        for (std::size_t i = 0; i < n; ++i) {
            buf[pos++] = static_cast<char>(in[i]);
        }
        return service::StreamStatus{service::StreamResult::ok};
    }

    service::StreamStatus flush() noexcept {
        return service::StreamStatus{service::StreamResult::ok};
    }
};

static util::usize console_write(shell::Buffer buf) noexcept {
    return std::fwrite(buf.data, 1, buf.size, stdout);
}

int main() {
    service::RingBuffer<int, 4> rb;
    (void)rb.push(1);
    int v = 0;
    (void)rb.pop(v);

    service::TraceBuffer<util::u32, 4> trace;
    trace.push(service::TraceRecord<util::u32, 4>{1, 1, 42, 1});

    std::array<char, 64> out{};
    service::JsonWriter json{std::span<char>(out.data(), out.size())};
    (void)json.push('{');
    (void)json.write_kv("v", 42);
    (void)json.push('}');

    shell::Console con{&console_write};
    (void)shell::write(con, "[shell] ok\n");

    return 0;
}
