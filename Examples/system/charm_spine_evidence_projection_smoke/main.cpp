#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <tuple>

import init.graph;
import init.node;
import util.core;
import util.error;

namespace spine {
    enum class Phase {
        service,
        app,
    };

    enum class EvidenceStatus : std::uint8_t {
        ok,
        error,
    };

    struct EvidenceField {
        std::string_view key{};
        std::string_view value{};
    };

    struct EvidenceFrame {
        std::string_view component{};
        std::string_view capability{};
        std::string_view provider{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::array<EvidenceField, 4> fields{};
        std::size_t field_count{0};
    };

    struct InitTrace {
        std::array<std::string_view, 8> entries{};
        std::size_t count{0};

        void record(std::string_view name) noexcept {
            if (count < entries.size()) {
                entries[count++] = name;
            }
        }
    };

    struct EvidenceCollector {
        std::array<EvidenceFrame, 4> frames{};
        std::size_t count{0};

        util::Result<void> append(EvidenceFrame frame) noexcept {
            if (count >= frames.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            frames[count++] = frame;
            return {};
        }
    };

    using InitFn = util::Result<void> (*)(void*) noexcept;
    using EvidenceFn = EvidenceFrame (*)(const void*) noexcept;

    template <std::size_t ProvideCount, std::size_t RequireCount>
    struct ComponentDesc {
        std::string_view name{};
        Phase phase{Phase::service};
        std::array<std::string_view, ProvideCount> provides{};
        std::array<std::string_view, RequireCount> required_caps{};
        InitFn init{nullptr};
        EvidenceFn evidence{nullptr};
        void* ctx{nullptr};
    };

    constexpr init::Phase project_phase(Phase phase) noexcept {
        switch (phase) {
        case Phase::service:
            return init::Phase::service;
        case Phase::app:
            return init::Phase::app;
        }
        return init::Phase::app;
    }

    template <std::size_t Count>
    constexpr std::array<init::CapId, Count> project_cap_ids(
        const std::array<std::string_view, Count>& names) noexcept {
        std::array<init::CapId, Count> ids{};
        for (std::size_t i = 0; i < Count; ++i) {
            ids[i] = init::cap_id(names[i]);
        }
        return ids;
    }

    template <std::size_t ProvideCount, std::size_t RequireCount>
    struct ProjectedNode {
        std::array<init::CapId, ProvideCount> provides{};
        std::array<init::CapId, RequireCount> required_caps{};
        init::Node node{};

        constexpr void materialize_node(
            const ComponentDesc<ProvideCount, RequireCount>& component) noexcept {
            node = init::Node{
                component.name,
                project_phase(component.phase),
                static_cast<util::u32>(init::Runlevel::all),
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(required_caps.data(), required_caps.size()),
                component.init,
                nullptr,
                component.ctx,
            };
        }
    };

    template <std::size_t ProvideCount, std::size_t RequireCount>
    constexpr auto project_to_init_node(
        const ComponentDesc<ProvideCount, RequireCount>& component) noexcept {
        (void)component;
        ProjectedNode<ProvideCount, RequireCount> projected{
            .provides = project_cap_ids(component.provides),
            .required_caps = project_cap_ids(component.required_caps),
        };
        return projected;
    }

    template <typename... Components>
    auto project_to_init_nodes(const Components&... components) noexcept {
        return std::tuple{project_to_init_node(components)...};
    }

    template <typename Tuple, typename... Components>
    void materialize_init_nodes(Tuple& projected, const Components&... components) noexcept {
        std::apply([&](auto&... item) {
            (item.materialize_node(components), ...);
        }, projected);
    }

    template <typename Component>
    util::Result<void> collect_component_evidence(
        EvidenceCollector& collector,
        const Component& component) noexcept {
        if (!component.evidence) {
            return {};
        }
        return collector.append(component.evidence(component.ctx));
    }

    template <typename... Components>
    util::Result<void> collect_evidence(
        EvidenceCollector& collector,
        const Components&... components) noexcept {
        util::Result<void> result{};
        auto collect_one = [&](const auto& component) noexcept {
            if (!result) {
                return;
            }
            auto collected = collect_component_evidence(collector, component);
            if (!collected) {
                result = util::unexpected(collected.error());
            }
        };
        (collect_one(components), ...);
        return result;
    }

    constexpr std::string_view status_text(EvidenceStatus status) noexcept {
        switch (status) {
        case EvidenceStatus::ok:
            return "ok";
        case EvidenceStatus::error:
            return "error";
        }
        return "unknown";
    }
}

namespace {
    using EmptyRequires = std::array<std::string_view, 0>;

    constexpr std::array kPowerProvides{std::string_view{"power.rail"}};
    constexpr std::array kDisplayProvides{std::string_view{"display.primary"}};
    constexpr std::array kDisplayRequires{std::string_view{"power.rail"}};
    constexpr std::array kAppProvides{std::string_view{"app.main"}};
    constexpr std::array kAppRequires{std::string_view{"display.primary"}};

    struct PowerState {
        spine::InitTrace* trace{nullptr};
        std::string_view profile{"h747-player-host-power"};
        bool ready{false};
        std::uint32_t init_calls{0};
        std::uint32_t formatted_log_writes{0};
    };

    struct DisplayState {
        spine::InitTrace* trace{nullptr};
        const PowerState* power{nullptr};
        std::string_view mode{"720x1280"};
        std::string_view format{"rgb888"};
        std::string_view provider{"host-raster-display"};
        bool ready{false};
        std::uint32_t init_calls{0};
        std::uint32_t formatted_log_writes{0};
    };

    struct AppState {
        spine::InitTrace* trace{nullptr};
        const DisplayState* display{nullptr};
        bool ready{false};
        std::uint32_t evidence_reads{0};
        std::uint32_t formatted_log_writes{0};
    };

    struct PresentationBuffer {
        std::array<char, 512> bytes{};
        std::size_t used{0};
        std::uint32_t formatted_frames{0};

        void append(std::string_view text) noexcept {
            const auto remaining = bytes.size() - used;
            const auto count = text.size() < remaining ? text.size() : remaining;
            if (count != 0) {
                std::memcpy(bytes.data() + used, text.data(), count);
                used += count;
            }
        }

        [[nodiscard]] std::string_view view() const noexcept {
            return {bytes.data(), used};
        }
    };

    util::Result<void> init_power(void* ctx) noexcept {
        auto& state = *static_cast<PowerState*>(ctx);
        state.trace->record("power");
        state.ready = true;
        ++state.init_calls;
        return {};
    }

    util::Result<void> init_display(void* ctx) noexcept {
        auto& state = *static_cast<DisplayState*>(ctx);
        if (!state.power || !state.power->ready) {
            return util::unexpected(util::Errc::bad_state);
        }
        state.trace->record("display");
        state.ready = true;
        ++state.init_calls;
        return {};
    }

    util::Result<void> init_app(void* ctx) noexcept {
        auto& state = *static_cast<AppState*>(ctx);
        if (!state.display || !state.display->ready) {
            return util::unexpected(util::Errc::bad_state);
        }
        state.trace->record("app");
        state.ready = true;
        return {};
    }

    spine::EvidenceFrame power_evidence(const void* ctx) noexcept {
        const auto& state = *static_cast<const PowerState*>(ctx);
        return spine::EvidenceFrame{
            .component = "power_service",
            .capability = "power.rail",
            .provider = "h747-player-power-profile",
            .status = state.ready ? spine::EvidenceStatus::ok : spine::EvidenceStatus::error,
            .fields = {{
                {"profile", state.profile},
                {"ready", state.ready ? "true" : "false"},
            }},
            .field_count = 2,
        };
    }

    spine::EvidenceFrame display_evidence(const void* ctx) noexcept {
        const auto& state = *static_cast<const DisplayState*>(ctx);
        return spine::EvidenceFrame{
            .component = "display_service",
            .capability = "display.primary",
            .provider = state.provider,
            .status = state.ready ? spine::EvidenceStatus::ok : spine::EvidenceStatus::error,
            .fields = {{
                {"mode", state.mode},
                {"format", state.format},
                {"ready", state.ready ? "true" : "false"},
            }},
            .field_count = 3,
        };
    }

    template <typename Tuple>
    constexpr auto node_ptrs(Tuple& projected) noexcept {
        return std::apply([](auto&... item) {
            return std::array<const init::Node*, sizeof...(item)>{&item.node...};
        }, projected);
    }

    void format_evidence_frame(
        PresentationBuffer& output,
        const spine::EvidenceFrame& frame) noexcept {
        output.append("component=");
        output.append(frame.component);
        output.append(" capability=");
        output.append(frame.capability);
        output.append(" provider=");
        output.append(frame.provider);
        output.append(" status=");
        output.append(spine::status_text(frame.status));
        for (std::size_t i = 0; i < frame.field_count; ++i) {
            output.append(" ");
            output.append(frame.fields[i].key);
            output.append("=");
            output.append(frame.fields[i].value);
        }
        output.append("\n");
        ++output.formatted_frames;
    }

    void format_evidence(
        PresentationBuffer& output,
        const spine::EvidenceCollector& collector) noexcept {
        for (std::size_t i = 0; i < collector.count; ++i) {
            format_evidence_frame(output, collector.frames[i]);
        }
    }

    [[nodiscard]] bool contains(std::string_view text, std::string_view needle) noexcept {
        return text.find(needle) != std::string_view::npos;
    }

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    spine::InitTrace trace{};
    PowerState power_state{.trace = &trace};
    DisplayState display_state{.trace = &trace, .power = &power_state};
    AppState app_state{.trace = &trace, .display = &display_state};
    spine::EvidenceCollector evidence{};

    const spine::ComponentDesc power_service{
        .name = "power_service",
        .phase = spine::Phase::service,
        .provides = kPowerProvides,
        .required_caps = EmptyRequires{},
        .init = init_power,
        .evidence = power_evidence,
        .ctx = &power_state,
    };
    const spine::ComponentDesc display_service{
        .name = "display_service",
        .phase = spine::Phase::service,
        .provides = kDisplayProvides,
        .required_caps = kDisplayRequires,
        .init = init_display,
        .evidence = display_evidence,
        .ctx = &display_state,
    };
    const spine::ComponentDesc app{
        .name = "demo_app",
        .phase = spine::Phase::app,
        .provides = kAppProvides,
        .required_caps = kAppRequires,
        .init = init_app,
        .evidence = nullptr,
        .ctx = &app_state,
    };

    auto projected = spine::project_to_init_nodes(power_service, display_service, app);
    spine::materialize_init_nodes(projected, power_service, display_service, app);
    auto nodes = node_ptrs(projected);
    init::Graph<6, 8> graph{};
    auto build = graph.build(nodes);
    if (!expect(build.has_value(), "component topology materializes init projection")) return 1;
    if (!expect(graph.ordered() == 3, "init projection topo sorts three nodes")) return 1;
    if (!expect(evidence.count == 0, "evidence collector is empty before init")) return 1;

    auto start = graph.start();
    if (!expect(start.has_value(), "init projection starts")) return 1;
    if (!expect(trace.count == 3, "init projection invokes three callbacks")) return 1;
    if (!expect(trace.entries[0] == "power", "power initializes first")) return 1;
    if (!expect(trace.entries[1] == "display", "display initializes after power")) return 1;
    if (!expect(trace.entries[2] == "app", "app initializes last")) return 1;
    if (!expect(evidence.count == 0, "init does not collect evidence")) return 1;

    auto collected = spine::collect_evidence(evidence, power_service, display_service, app);
    if (!expect(collected.has_value(), "evidence side channel collection succeeds")) return 1;
    if (!expect(evidence.count == 2, "missing app evidence producer is allowed")) return 1;
    if (!expect(evidence.frames[0].component == "power_service", "power evidence is structured")) return 1;
    if (!expect(evidence.frames[0].fields[0].key == "profile", "power evidence has profile key")) return 1;
    if (!expect(evidence.frames[0].fields[0].value == "h747-player-host-power",
                "power evidence has profile value")) return 1;
    if (!expect(evidence.frames[1].component == "display_service", "display evidence is structured")) return 1;
    if (!expect(evidence.frames[1].fields[0].key == "mode", "display evidence has mode key")) return 1;
    if (!expect(evidence.frames[1].fields[0].value == "720x1280", "display evidence has mode value")) return 1;
    if (!expect(evidence.frames[1].fields[1].key == "format", "display evidence has format key")) return 1;
    if (!expect(evidence.frames[1].fields[1].value == "rgb888", "display evidence has format value")) return 1;
    if (!expect(power_state.formatted_log_writes == 0, "power provider emits no formatted log")) return 1;
    if (!expect(display_state.formatted_log_writes == 0, "display provider emits no formatted log")) return 1;
    if (!expect(app_state.evidence_reads == 0, "app does not consume evidence")) return 1;

    PresentationBuffer presentation{};
    format_evidence(presentation, evidence);
    const auto rendered = presentation.view();
    if (!expect(presentation.formatted_frames == 2, "presentation formats collected evidence")) return 1;
    if (!expect(contains(rendered, "component=power_service"), "presentation includes power evidence")) return 1;
    if (!expect(contains(rendered, "component=display_service"), "presentation includes display evidence")) return 1;
    if (!expect(contains(rendered, "mode=720x1280"), "presentation includes display mode")) return 1;
    if (!expect(power_state.formatted_log_writes == 0, "presentation does not mutate power provider")) return 1;
    if (!expect(display_state.formatted_log_writes == 0, "presentation does not mutate display provider")) return 1;
    if (!expect(app_state.formatted_log_writes == 0, "presentation does not mutate app")) return 1;

    std::puts("[charm-spine-evidence-projection-smoke] ok");
    return 0;
}
