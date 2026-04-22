#include <cstdio>

import charm.net;
import util.core;

namespace {
    using Link = net::lab::DuplexLink<192>;
    using Pump = net::IcmpStackPump<192, 4, 192, 4, 64>;
    using Node = net::lab::StackNode<Pump>;

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(net::ByteView bytes,
                                const util::u8 (&expected)[N]) noexcept {
        if (bytes.size() != N) {
            return false;
        }
        for (util::usize index = 0; index < N; ++index) {
            if (bytes[index] != expected[index]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs,
                                const net::MacAddress& rhs) noexcept {
        for (util::usize index = 0; index < lhs.bytes.size(); ++index) {
            if (lhs.bytes[index] != rhs.bytes[index]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] net::lab::LinkDirectionStats delta_stats(
        const net::lab::LinkDirectionStats& after,
        const net::lab::LinkDirectionStats& before) noexcept {
        return net::lab::LinkDirectionStats{
            .scheduled = after.scheduled - before.scheduled,
            .delivered = after.delivered - before.delivered,
            .dropped = after.dropped - before.dropped,
            .rejected = after.rejected - before.rejected,
            .pending = after.pending,
        };
    }

    [[nodiscard]] bool service_step(net::icmp::echo::Probe<16>& probe,
                                    Node& client_node,
                                    Node& server_node,
                                    Link& link) noexcept {
        link.advance(1);

        auto client_progress = client_node.service(1);
        if (!client_progress) {
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            return false;
        }

        probe.tick(link.now_ticks());
        return true;
    }

    [[nodiscard]] bool drive_until_probe_ready(net::icmp::echo::Probe<16>& probe,
                                               Node& client_node,
                                               Node& server_node,
                                               Link& link,
                                               util::usize max_steps = 64) noexcept {
        for (util::usize step = 0; step < max_steps; ++step) {
            if (!service_step(probe, client_node, server_node, link)) {
                return false;
            }

            if (probe.ready()
                && client_node.pump().pending_count() == 0
                && server_node.pump().pending_count() == 0
                && link.idle()) {
                return true;
            }
        }
        return false;
    }
}

int main() {
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    static constexpr util::u8 payload[]{'l', 'a', 'b', '!'};

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    Link link{};
    link.set_latency_a_to_b(2);
    link.set_latency_b_to_a(3);

    Node client_node{};
    auto client_init = client_node.init(
        link.endpoint_a(),
        client_mac,
        client_ip,
        net::IcmpStackPumpConfig{
            .egress = net::IcmpEgressPumpConfig{
                .retry_interval_ticks = 8,
                .max_attempts = 4,
            }
        });
    if (!client_init || !client_node.ready()) {
        return fail("net lab smoke client init failed\n", 1);
    }

    Node server_node{};
    auto server_init = server_node.init(
        link.endpoint_b(),
        server_mac,
        server_ip,
        net::IcmpStackPumpConfig{
            .egress = net::IcmpEgressPumpConfig{
                .retry_interval_ticks = 8,
                .max_attempts = 4,
            }
        });
    if (!server_init || !server_node.ready()) {
        return fail("net lab smoke server init failed\n", 2);
    }

    if (!same_mac(link.endpoint_a().mac(), client_mac)
        || !same_mac(link.endpoint_b().mac(), server_mac)) {
        return fail("net lab smoke endpoint mac mismatch\n", 3);
    }

    net::icmp::echo::Probe<16> probe{server_ip};
    net::icmp::echo::AutoReplyServer server{};
    auto bound_probe = probe.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    if (!bound_probe
        || !bound_server
        || !client_node.pump().has_echo_sink()
        || !server_node.pump().has_echo_sink()) {
        return fail("net lab smoke protocol bind failed\n", 4);
    }

    const auto baseline_a_to_b = link.stats_a_to_b();
    const auto baseline_b_to_a = link.stats_b_to_a();

    auto first_ping = probe.ping(payload, link.now_ticks(), 20);
    if (!first_ping
        || first_ping.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab smoke first ping submit failed\n", 5);
    }

    auto client_progress = client_node.service(1);
    if (!client_progress) {
        return fail("net lab smoke first client step failed\n", 6);
    }

    const auto early_a_to_b = link.stats_a_to_b();
    const auto early_b_to_a = link.stats_b_to_a();
    if (early_a_to_b.scheduled < 1
        || early_a_to_b.delivered != 0
        || early_a_to_b.dropped != 0
        || early_a_to_b.rejected != 0
        || early_a_to_b.pending != early_a_to_b.scheduled
        || early_b_to_a.scheduled != 0
        || early_b_to_a.pending != 0
        || link.pending_count() != early_a_to_b.pending
        || link.idle()
        || link.endpoint_b().has_rx()) {
        return fail("net lab smoke pending stat mismatch\n", 7);
    }

    auto server_progress = server_node.service(1);
    if (!server_progress) {
        return fail("net lab smoke first server step failed\n", 8);
    }

    link.advance(1);
    probe.tick(link.now_ticks());

    const auto after_one_tick = link.stats_a_to_b();
    if (after_one_tick.pending != early_a_to_b.pending
        || after_one_tick.delivered != 0
        || link.now_ticks() != 1
        || link.endpoint_b().has_rx()) {
        return fail("net lab smoke latency tick mismatch\n", 9);
    }

    if (!drive_until_probe_ready(probe, client_node, server_node, link)) {
        return fail("net lab smoke delayed roundtrip stalled\n", 10);
    }

    const auto first_result = probe.result();
    const auto first_a_to_b = link.stats_a_to_b();
    const auto first_b_to_a = link.stats_b_to_a();
    const auto first_delta_a_to_b = delta_stats(first_a_to_b, baseline_a_to_b);
    const auto first_delta_b_to_a = delta_stats(first_b_to_a, baseline_b_to_a);
    if (!first_result.ready()
        || !first_result.ok()
        || !first_result.has_value()
        || first_result.timed_out()
        || first_result.cancelled()
        || first_result.failed()
        || !bytes_eq(first_result.value_payload(), payload)
        || probe.reply_count() != 1
        || probe.timeout_count() != 0
        || probe.drop_count() != 0
        || first_delta_a_to_b.scheduled < 1
        || first_delta_a_to_b.delivered < 1
        || first_delta_a_to_b.dropped != 0
        || first_delta_a_to_b.rejected != 0
        || first_delta_a_to_b.pending != 0
        || first_delta_b_to_a.scheduled < 1
        || first_delta_b_to_a.delivered < 1
        || first_delta_b_to_a.dropped != 0
        || first_delta_b_to_a.rejected != 0
        || first_delta_b_to_a.pending != 0
        || !link.idle()) {
        return fail("net lab smoke delayed roundtrip mismatch\n", 11);
    }

    const auto timeout_before_a_to_b = first_a_to_b;
    const auto timeout_before_b_to_a = first_b_to_a;
    link.drop_next_b_to_a();
    auto timeout_ping = probe.ping(payload, link.now_ticks(), 8);
    if (!timeout_ping
        || timeout_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab smoke timeout ping submit failed\n", 12);
    }

    if (!drive_until_probe_ready(probe, client_node, server_node, link, 32)) {
        return fail("net lab smoke timeout roundtrip stalled\n", 13);
    }

    const auto timeout_result = probe.result();
    const auto timeout_a_to_b = link.stats_a_to_b();
    const auto timeout_b_to_a = link.stats_b_to_a();
    const auto timeout_delta_a_to_b = delta_stats(timeout_a_to_b, timeout_before_a_to_b);
    const auto timeout_delta_b_to_a = delta_stats(timeout_b_to_a, timeout_before_b_to_a);
    if (!timeout_result.ready()
        || !timeout_result.timed_out()
        || timeout_result.ok()
        || timeout_result.has_value()
        || timeout_result.cancelled()
        || timeout_result.failed()
        || timeout_result.identifier() != timeout_ping.value().info.identifier
        || timeout_result.sequence() != timeout_ping.value().info.sequence
        || probe.timeout_count() != 1
        || probe.reply_count() != 1
        || timeout_delta_a_to_b.scheduled < 1
        || timeout_delta_a_to_b.delivered < 1
        || timeout_delta_a_to_b.dropped != 0
        || timeout_delta_a_to_b.rejected != 0
        || timeout_delta_a_to_b.pending != 0
        || timeout_delta_b_to_a.scheduled < 1
        || timeout_delta_b_to_a.dropped != 1
        || timeout_delta_b_to_a.rejected != 0
        || timeout_delta_b_to_a.pending != 0
        || server.request_count() != 2
        || server.reply_count() != 2
        || server.drop_count() != 0
        || server.transmitted_count() != 2
        || !link.idle()) {
        return fail("net lab smoke drop injection mismatch\n", 14);
    }

    const auto recovery_before_a_to_b = timeout_a_to_b;
    const auto recovery_before_b_to_a = timeout_b_to_a;
    auto recovery_ping = probe.ping(payload, link.now_ticks(), 12);
    if (!recovery_ping
        || recovery_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab smoke recovery ping submit failed\n", 15);
    }

    if (!drive_until_probe_ready(probe, client_node, server_node, link, 32)) {
        return fail("net lab smoke recovery roundtrip stalled\n", 16);
    }

    const auto recovery_result = probe.result();
    const auto recovery_a_to_b = link.stats_a_to_b();
    const auto recovery_b_to_a = link.stats_b_to_a();
    const auto recovery_delta_a_to_b = delta_stats(recovery_a_to_b, recovery_before_a_to_b);
    const auto recovery_delta_b_to_a = delta_stats(recovery_b_to_a, recovery_before_b_to_a);
    if (!recovery_result.ready()
        || !recovery_result.ok()
        || !recovery_result.has_value()
        || recovery_result.timed_out()
        || recovery_result.cancelled()
        || recovery_result.failed()
        || !bytes_eq(recovery_result.value_payload(), payload)
        || probe.reply_count() != 2
        || probe.timeout_count() != 1
        || probe.error_count() != 0
        || recovery_delta_a_to_b.scheduled < 1
        || recovery_delta_a_to_b.delivered < 1
        || recovery_delta_a_to_b.dropped != 0
        || recovery_delta_a_to_b.rejected != 0
        || recovery_delta_a_to_b.pending != 0
        || recovery_delta_b_to_a.scheduled < 1
        || recovery_delta_b_to_a.delivered < 1
        || recovery_delta_b_to_a.dropped != 0
        || recovery_delta_b_to_a.rejected != 0
        || recovery_delta_b_to_a.pending != 0
        || recovery_b_to_a.dropped != timeout_b_to_a.dropped
        || link.pending_count() != 0
        || !link.idle()
        || link.endpoint_a().rx_pending() != 0
        || link.endpoint_b().rx_pending() != 0
        || client_node.pump().pending_count() != 0
        || server_node.pump().pending_count() != 0
        || server.request_count() != 3
        || server.reply_count() != 3
        || server.drop_count() != 0
        || server.transmitted_count() != 3) {
        return fail("net lab smoke recovery mismatch\n", 17);
    }

    std::puts("net lab smoke: ok");
    return 0;
}
