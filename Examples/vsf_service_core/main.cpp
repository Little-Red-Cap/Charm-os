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
import service_trace_bus;

static void on_msg(const service::BusMessage& msg) noexcept {
    std::printf("[distbus] id=%u size=%zu kind=%u\n", msg.id, msg.size, static_cast<unsigned>(msg.kind));
}

static void on_trace(const service::BusMessage& msg) noexcept {
    if (msg.kind != service::BusKind::trace) return;
    auto rec = static_cast<const service::TraceRecord<util::u32, 4>*>(msg.data);
    if (!rec) return;
    std::printf("[trace] id=%u kind=%u payload=%llu count=%u\n",
                rec->id,
                static_cast<unsigned>(rec->kind),
                static_cast<unsigned long long>(rec->payload),
                rec->count);
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
    trace.push(service::TraceRecord<util::u32, 4>{1, 2, 3, 1, service::TraceKind::event});
    trace.push(service::TraceRecord<util::u32, 4>{2, 2, 0, 1, service::TraceKind::event});
    trace.push(service::TraceRecord<util::u32, 4>{2, 3, 42, 1, service::TraceKind::counter});
    trace.push(service::TraceRecord<util::u32, 4>{3, 3, 7, 1, service::TraceKind::counter});
    trace.push(service::TraceRecord<util::u32, 4>{3, 7, 0, 1, service::TraceKind::span_begin});
    trace.push(service::TraceRecord<util::u32, 4>{5, 7, 0, 1, service::TraceKind::span_end});

    service::TraceAggregator<util::u32, 4> agg;
    for (util::usize i = 0; i < trace.size(); ++i) {
        agg.observe(trace.data()[i]);
    }

    service::DistBus<2> bus;
    (void)bus.subscribe(&on_msg);
    service::BusFilter only_trace{};
    only_trace.match_kind = true;
    only_trace.kind = service::BusKind::trace;
    (void)bus.subscribe(&on_trace, only_trace);
    service::BusMessage msg{1, nullptr, 0};
    bus.publish(msg);
    for (util::usize i = 0; i < trace.size(); ++i) {
        service::publish_trace(bus, trace.data()[i]);
    }

    auto totals = agg.totals();
    std::printf("[trace] totals event=%u counter=%u span_begin=%u span_end=%u span_total=%llu span_max=%llu\n",
                totals.events, totals.counters, totals.span_begin, totals.span_end,
                static_cast<unsigned long long>(totals.span_total),
                static_cast<unsigned long long>(totals.span_max));

    for (auto& stat : agg.by_id()) {
        if (!stat.used) continue;
    std::printf("[trace] id=%u ev=%u cnt=%u sum=%llu min=%llu max=%llu span_cnt=%u span_total=%llu span_max=%llu\n",
                stat.id,
                stat.event_count,
                stat.counters,
                static_cast<unsigned long long>(stat.counter_sum),
                static_cast<unsigned long long>(stat.counter_min),
                static_cast<unsigned long long>(stat.counter_max),
                    stat.span_count,
                    static_cast<unsigned long long>(stat.span_total),
                    static_cast<unsigned long long>(stat.span_max));
    }

    std::puts("[service_core] ok");
    return 0;
}
