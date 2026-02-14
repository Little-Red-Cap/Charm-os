module;

#include <cstddef>
#include <cstdint>

export module service_trace_bus;

import util.core;
import service_trace;
import service_distbus;

export namespace service {
    template <typename Tick, util::usize Capacity>
    inline BusMessage make_trace_message(const TraceRecord<Tick, Capacity>& rec) noexcept {
        BusMessage msg{};
        msg.id = rec.id;
        msg.data = &rec;
        msg.size = sizeof(rec);
        msg.priority = 0;
        msg.kind = BusKind::trace;
        return msg;
    }

    template <util::usize MaxSubs, typename Tick, util::usize Capacity>
    inline void publish_trace(DistBus<MaxSubs>& bus, const TraceRecord<Tick, Capacity>& rec) noexcept {
        bus.publish(make_trace_message(rec));
    }
}
