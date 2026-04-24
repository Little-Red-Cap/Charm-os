#include <cstdio>
#include <string_view>

import charm.system.app_host;
import charm.system.caps;
import init.connection;
import init.materialize;
import init.meta;
import init.node;
import init.observe;
import init.plan;
import init.recipe;
import kernel.eda;
import kernel.evt;
import kernel.ssu;
import service.deferred_signal;
import service.signal;
import service.state;
import util.core;
import util.delegate;
import util.error;

namespace {
    struct DemoContext {
        util::u32 graph_revision{0};
    };

    inline util::Result<void> start_knob_source(DemoContext&) noexcept {
        return {};
    }

    inline util::Result<void> start_volume_state(DemoContext&) noexcept {
        return {};
    }

    inline util::Result<void> start_worker_sink(DemoContext&) noexcept {
        return {};
    }

    using CapKnobDelta = init::cap_c<"demo.input.knob.delta">;
    using CapVolumeState = init::cap_c<"demo.audio.volume.state">;
    using CapWorkerApply = init::cap_c<"demo.audio.worker.apply">;

    using KnobRecipe = init::recipe_desc<
        "demo.input.knob",
        init::Phase::service,
        static_cast<util::u32>(init::Runlevel::full),
        init::cap_list<CapKnobDelta>,
        init::cap_list<>,
        DemoContext,
        &start_knob_source>;

    using VolumeRecipe = init::recipe_desc<
        "demo.audio.volume",
        init::Phase::service,
        static_cast<util::u32>(init::Runlevel::full),
        init::cap_list<CapVolumeState>,
        init::cap_list<CapKnobDelta>,
        DemoContext,
        &start_volume_state>;

    using WorkerRecipe = init::recipe_desc<
        "demo.audio.worker",
        init::Phase::app,
        static_cast<util::u32>(init::Runlevel::full),
        init::cap_list<CapWorkerApply>,
        init::cap_list<CapVolumeState>,
        DemoContext,
        &start_worker_sink>;

    struct WorkerTask {
        static constexpr kernel::Priority priority{0};
        static constexpr kernel::EventMask mask = kernel::event_mask(kernel::EventId::message);

        util::u32 handled{0};
        util::u32 last_payload{0};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return kernel::ssu::Meta{
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::demand,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "demo.signal_state.worker",
            };
        }

        void on_event(kernel::Event evt) noexcept {
            if (evt.id != kernel::EventId::message) {
                return;
            }
            ++handled;
            last_payload = kernel::payload_u32(evt);
        }
    };

    struct ConnectionStats {
        util::usize node_count{0};
        util::usize edge_count{0};
        util::usize connection_count{0};
        util::usize direct_count{0};
        util::usize deferred_count{0};
        bool saw_knob_to_state{false};
        bool saw_state_to_worker{false};
    };

    template <typename Plan>
    [[nodiscard]] bool inspect_connections(const Plan& plan_value, ConnectionStats& stats) noexcept {
        auto mats = init::materialize<8, 16>(plan_value);
        if (!mats) {
            std::printf("[ERR] materialize failed err=%d\n", static_cast<int>(mats.error()));
            return false;
        }

        auto view = init::observe(*mats);
        stats.node_count = view.node_count;
        stats.edge_count = view.edge_count;
        for (util::usize i = 0; i < view.node_count; ++i) {
            const auto& node = view.nodes[i];
            if (node.kind != init::materialized_node_kind::connection) {
                continue;
            }
            ++stats.connection_count;
            if (node.connection_mode == std::string_view{"direct"}) {
                ++stats.direct_count;
            } else if (node.connection_mode == std::string_view{"deferred"}) {
                ++stats.deferred_count;
            }
            if (node.name == std::string_view{"demo.connection.knob_to_state"}) {
                stats.saw_knob_to_state =
                    node.connection_source == CapKnobDelta::view()
                    && node.connection_sink == CapVolumeState::view()
                    && node.connection_mode == std::string_view{"direct"};
            } else if (node.name == std::string_view{"demo.connection.state_to_worker"}) {
                stats.saw_state_to_worker =
                    node.connection_source == CapVolumeState::view()
                    && node.connection_sink == CapWorkerApply::view()
                    && node.connection_mode == std::string_view{"deferred"};
            }
        }
        return true;
    }

    template <class Poster>
    struct RuntimeController {
        using deferred_type = service::deferred_signal<kernel::Event, Poster>;

        service::signal<void(int), 2> knob_delta{};
        service::state<int, 2> volume{10};
        deferred_type worker_post;

        int direct_events{0};
        int direct_sum{0};
        int state_changes{0};
        int last_old{10};
        int last_new{10};

        explicit RuntimeController(Poster& poster) noexcept
            : worker_post(poster) {}

        void on_knob_delta(int delta) noexcept {
            ++direct_events;
            direct_sum += delta;

            int next = volume.get() + delta;
            if (next < 0) {
                next = 0;
            } else if (next > 100) {
                next = 100;
            }
            (void)volume.set(next);
        }

        void on_volume_changed(const int& now, const int& old) noexcept {
            ++state_changes;
            last_old = old;
            last_new = now;
            (void)worker_post.post(kernel::make_event(kernel::EventId::message, static_cast<util::u32>(now)));
        }
    };

    struct EdgeProbe {
        int edge_count{0};
        int last_delta{0};

        void on_edge(int delta) noexcept {
            ++edge_count;
            last_delta = delta;
        }
    };

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    DemoContext ctx{};
    const auto plan_value = init::compose(
        init::bind<KnobRecipe>(ctx),
        init::bind<VolumeRecipe>(ctx),
        init::bind<WorkerRecipe>(ctx),
        init::as_plan(init::direct_connection<
                      "demo.connection.knob_to_state",
                      CapKnobDelta,
                      CapVolumeState>()),
        init::as_plan(init::deferred_connection<
                      "demo.connection.state_to_worker",
                      CapVolumeState,
                      CapWorkerApply,
                      init::Phase::app>()));

    ConnectionStats graph{};
    if (!inspect_connections(plan_value, graph)) return 1;
    if (!expect(graph.node_count == 5, "materialized graph keeps three recipes plus two connections")) return 1;
    if (!expect(graph.edge_count == 6, "materialized graph keeps recipe plus connection dependency edges")) return 1;
    if (!expect(graph.connection_count == 2, "materialized graph exposes both connections")) return 1;
    if (!expect(graph.direct_count == 1, "materialized graph exposes one direct connection")) return 1;
    if (!expect(graph.deferred_count == 1, "materialized graph exposes one deferred connection")) return 1;
    if (!expect(graph.saw_knob_to_state, "graph keeps knob->state direct wiring visible")) return 1;
    if (!expect(graph.saw_state_to_worker, "graph keeps state->worker deferred wiring visible")) return 1;

    using Host = charm::system::AppHost<
        charm::system::DefaultCaps,
        charm::system::AppHostConfig,
        WorkerTask>;

    Host host{};
    auto& worker = host.task<WorkerTask>();
    auto posters = host.posters<WorkerTask>();
    if (!expect(static_cast<bool>(posters), "materialize task-local poster set")) return 1;

    using Controller = RuntimeController<decltype(posters.demand)>;
    Controller controller{posters.demand};
    EdgeProbe probe{};

    const auto knob_controller_conn =
        controller.knob_delta.connect(util::delegate<int>::bind<&Controller::on_knob_delta>(controller));
    const auto knob_probe_conn =
        controller.knob_delta.connect(util::delegate<int>::bind<&EdgeProbe::on_edge>(probe));
    const auto state_conn =
        controller.volume.connect(util::delegate<const int&, const int&>::bind<&Controller::on_volume_changed>(controller));

    if (!expect(static_cast<bool>(knob_controller_conn), "connect knob controller slot")) return 1;
    if (!expect(static_cast<bool>(knob_probe_conn), "connect knob probe slot")) return 1;
    if (!expect(static_cast<bool>(state_conn), "connect state->worker bridge slot")) return 1;

    const auto first_dispatch = controller.knob_delta.emit(5);
    if (!expect(first_dispatch == 2, "signal fanout stays bounded and synchronous")) return 1;
    if (!expect(controller.direct_events == 1 && controller.direct_sum == 5, "controller handled knob delta synchronously")) {
        return 1;
    }
    if (!expect(probe.edge_count == 1 && probe.last_delta == 5, "probe sees same-domain edge event")) return 1;
    if (!expect(controller.volume.get() == 15, "edge event updates state truth immediately")) return 1;
    if (!expect(controller.state_changes == 1, "state change notifies exactly once")) return 1;
    if (!expect(controller.last_old == 10 && controller.last_new == 15, "state change reports old/new truth")) return 1;
    if (!expect(worker.handled == 0, "worker is not direct-called before scheduler runs")) return 1;

    if (!expect(host.dispatch_batch(8) >= 1, "scheduler consumes first deferred post")) return 1;
    if (!expect(worker.handled == 1 && worker.last_payload == 15, "worker receives deferred payload after dispatch")) {
        return 1;
    }

    const auto second_dispatch = controller.knob_delta.emit(0);
    if (!expect(second_dispatch == 2, "zero delta is still a bounded edge event")) return 1;
    if (!expect(probe.edge_count == 2 && probe.last_delta == 0, "probe still sees edge event for zero delta")) return 1;
    if (!expect(controller.volume.get() == 15, "zero delta does not change state truth")) return 1;
    if (!expect(controller.state_changes == 1, "unchanged state does not re-notify")) return 1;
    (void)host.dispatch_batch(8);
    if (!expect(worker.handled == 1, "unchanged state does not create extra deferred work")) return 1;

    const auto third_dispatch = controller.knob_delta.emit(-4);
    if (!expect(third_dispatch == 2, "negative delta still fans out synchronously")) return 1;
    if (!expect(controller.volume.get() == 11, "negative delta updates state truth")) return 1;
    if (!expect(controller.state_changes == 2, "second real state change notifies once")) return 1;
    if (!expect(controller.last_old == 15 && controller.last_new == 11, "second state change reports old/new truth")) {
        return 1;
    }
    if (!expect(worker.handled == 1, "second deferred update still waits for scheduler")) return 1;

    if (!expect(host.dispatch_batch(8) >= 1, "scheduler consumes second deferred post")) return 1;
    if (!expect(worker.handled == 2 && worker.last_payload == 11, "worker receives second deferred payload")) {
        return 1;
    }

    std::printf("[graph] nodes=%llu edges=%llu direct=%llu deferred=%llu\n",
                static_cast<unsigned long long>(graph.node_count),
                static_cast<unsigned long long>(graph.edge_count),
                static_cast<unsigned long long>(graph.direct_count),
                static_cast<unsigned long long>(graph.deferred_count));
    std::printf("[direct] edge_count=%d direct_sum=%d volume=%d\n",
                probe.edge_count,
                controller.direct_sum,
                controller.volume.get());
    std::printf("[state] changes=%d old=%d new=%d\n",
                controller.state_changes,
                controller.last_old,
                controller.last_new);
    std::printf("[worker] handled=%u last_payload=%u task=%zu\n",
                static_cast<unsigned>(worker.handled),
                static_cast<unsigned>(worker.last_payload),
                posters.task_id().value);
    std::puts("[signal_state_closure_demo] ok");
    return 0;
}
