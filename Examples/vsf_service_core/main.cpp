#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

import util.core;
import service_fifo;
import service_heap;
import service_pool;
import service_json;
import service_trace;
import service_distbus;

static void on_msg(const service::BusMessage& msg) noexcept {
    std::printf("[distbus] id=%u size=%zu\n", msg.id, msg.size);
}

int main() {
    service::Fifo<int, 4> fifo;
    (void)fifo.push(7);
    int v = 0;
    (void)fifo.pop(v);

    std::array<std::byte, 64> heap_buf{};
    service::LinearHeap heap(heap_buf.data(), heap_buf.size());
    (void)heap.alloc(16);

    service::Pool<8, 4> pool;
    pool.reset();
    void* p = pool.alloc();
    pool.free(p);

    std::array<char, 64> out{};
    service::JsonWriter json{std::span<char>(out.data(), out.size())};
    (void)json.push('{');
    (void)json.write_kv("x", 1);
    (void)json.push('}');

    service::TraceBuffer<util::u32, 4> trace;
    trace.push(service::TraceRecord<util::u32, 4>{1, 2, 3, 1});

    service::DistBus<2> bus;
    (void)bus.subscribe(&on_msg);
    service::BusMessage msg{1, nullptr, 0};
    bus.publish(msg);

    std::puts("[service_core] ok");
    return 0;
}
