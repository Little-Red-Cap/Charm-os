#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace cap {
    struct TextSink {
        template <typename T>
        static constexpr bool satisfied_by = requires(T& sink, std::string_view text) {
            { sink.write(text) } noexcept -> std::same_as<void>;
        };
    };

    struct Clock {
        template <typename T>
        static constexpr bool satisfied_by = requires(T& clock) {
            { clock.now_ms() } noexcept -> std::same_as<std::uint32_t>;
        };
    };

    struct Display {
        template <typename T>
        static constexpr bool satisfied_by = requires(T& display, std::uint32_t color) {
            { display.fill(color) } noexcept -> std::same_as<void>;
        };
    };
}

namespace role {
    struct log {};
    struct debug_trace {};
    struct monotonic_time {};
    struct primary_display {};
    struct evidence {};
}

namespace provider {
    struct memory_log {};
    struct memory_trace {};
    struct fake_clock {};
    struct host_display {};
}

namespace rte {
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

    struct EvidenceCollector {
        std::array<EvidenceFrame, 8> frames{};
        std::size_t count{0};

        bool append(EvidenceFrame frame) noexcept {
            if (count >= frames.size()) {
                return false;
            }
            frames[count++] = frame;
            return true;
        }
    };

    template <typename Kind, typename Role>
    struct Requirement {
        using kind = Kind;
        using role = Role;
    };

    template <typename Kind, typename Role>
    struct Provided {
        using kind = Kind;
        using role = Role;
    };

    template <typename... Req>
    struct RequirementSet {};

    template <typename... Prov>
    struct ProviderSet {};

    template <typename Needle, typename Set>
    struct set_contains;

    template <typename Needle, template <typename...> typename Set, typename... Items>
    struct set_contains<Needle, Set<Items...>>
        : std::bool_constant<(... || std::same_as<Needle, Items>)> {};

    template <typename Needle, typename Set>
    inline constexpr bool set_contains_v = set_contains<Needle, Set>::value;

    template <typename Req, typename... Bindings>
    inline constexpr bool has_requirement_v =
        (... || std::same_as<typename Bindings::requirement, Req>);

    template <typename Kind, typename ProviderTag, typename Impl>
    struct ProviderRef {
        using kind = Kind;
        using provider = ProviderTag;
        using impl_type = Impl;

        Impl* impl{nullptr};

        constexpr explicit ProviderRef(Impl& value) noexcept : impl(&value) {
            static_assert(Kind::template satisfied_by<Impl>,
                          "provider implementation does not satisfy capability kind");
        }

        [[nodiscard]] constexpr Impl& get() const noexcept {
            return *impl;
        }
    };

    template <typename Req, typename Provider>
    struct RuntimeBinding {
        using requirement = Req;
        using provider = Provider;

        Provider provider_ref;

        constexpr explicit RuntimeBinding(Provider provider_in) noexcept : provider_ref(provider_in) {
            static_assert(std::same_as<typename Req::kind, typename Provider::kind>,
                          "runtime binding capability kind must match requirement capability kind");
        }

        [[nodiscard]] constexpr auto& get() noexcept {
            return provider_ref.get();
        }
    };

    template <typename... Bindings>
    class ContextView {
    public:
        constexpr explicit ContextView(Bindings... bindings) noexcept
            : bindings_(bindings...) {}

        template <typename Req>
            requires has_requirement_v<Req, Bindings...>
        [[nodiscard]] constexpr auto& get() noexcept {
            return binding_for<Req>().get();
        }

    private:
        using Tuple = std::tuple<Bindings...>;

        template <typename Req, std::size_t Index = 0>
        [[nodiscard]] constexpr auto& binding_for() noexcept {
            static_assert(Index < sizeof...(Bindings), "missing binding for requirement");
            using Binding = std::tuple_element_t<Index, Tuple>;
            if constexpr (std::same_as<typename Binding::requirement, Req>) {
                return std::get<Index>(bindings_);
            } else {
                return binding_for<Req, Index + 1>();
            }
        }

        Tuple bindings_;
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

    constexpr std::string_view status_text(EvidenceStatus status) noexcept {
        return status == EvidenceStatus::ok ? "ok" : "error";
    }

    void format_evidence_frame(PresentationBuffer& output, const EvidenceFrame& frame) noexcept {
        output.append("component=");
        output.append(frame.component);
        output.append(" capability=");
        output.append(frame.capability);
        output.append(" provider=");
        output.append(frame.provider);
        output.append(" status=");
        output.append(status_text(frame.status));
        for (std::size_t i = 0; i < frame.field_count; ++i) {
            output.append(" ");
            output.append(frame.fields[i].key);
            output.append("=");
            output.append(frame.fields[i].value);
        }
        output.append("\n");
        ++output.formatted_frames;
    }

    void format_evidence(PresentationBuffer& output, const EvidenceCollector& collector) noexcept {
        for (std::size_t i = 0; i < collector.count; ++i) {
            format_evidence_frame(output, collector.frames[i]);
        }
    }
}

namespace {
    using LogReq = rte::Requirement<cap::TextSink, role::log>;
    using TraceReq = rte::Requirement<cap::TextSink, role::debug_trace>;
    using ClockReq = rte::Requirement<cap::Clock, role::monotonic_time>;
    using DisplayReq = rte::Requirement<cap::Display, role::primary_display>;
    using EvidenceReq = rte::Requirement<rte::EvidenceCollector, role::evidence>;

    struct MemoryTextSink {
        std::array<char, 128> bytes{};
        std::size_t used{0};
        std::uint32_t writes{0};
        std::uint32_t evidence_reads{0};

        void write(std::string_view text) noexcept {
            const auto remaining = bytes.size() - used;
            const auto count = text.size() < remaining ? text.size() : remaining;
            if (count != 0) {
                std::memcpy(bytes.data() + used, text.data(), count);
                used += count;
            }
            ++writes;
        }

        [[nodiscard]] std::string_view view() const noexcept {
            return {bytes.data(), used};
        }

        [[nodiscard]] rte::EvidenceFrame evidence(std::string_view component,
                                                  std::string_view capability,
                                                  std::string_view provider_name) noexcept {
            ++evidence_reads;
            return rte::EvidenceFrame{
                .component = component,
                .capability = capability,
                .provider = provider_name,
                .status = rte::EvidenceStatus::ok,
                .fields = {{
                    {"writes", writes == 2 ? "2" : "unexpected"},
                    {"buffer", view()},
                }},
                .field_count = 2,
            };
        }
    };

    struct FakeClock {
        std::uint32_t now{42};
        std::uint32_t evidence_reads{0};

        [[nodiscard]] std::uint32_t now_ms() noexcept {
            return now;
        }

        [[nodiscard]] rte::EvidenceFrame evidence() noexcept {
            ++evidence_reads;
            return rte::EvidenceFrame{
                .component = "clock_service",
                .capability = "Clock.monotonic_time",
                .provider = "fake_clock",
                .status = rte::EvidenceStatus::ok,
                .fields = {{
                    {"now_ms", now == 42 ? "42" : "unexpected"},
                }},
                .field_count = 1,
            };
        }
    };

    struct HostDisplay {
        std::uint32_t last_color{0};
        std::uint32_t fills{0};
        std::uint32_t evidence_reads{0};

        void fill(std::uint32_t color) noexcept {
            last_color = color;
            ++fills;
        }

        [[nodiscard]] rte::EvidenceFrame evidence() noexcept {
            ++evidence_reads;
            return rte::EvidenceFrame{
                .component = "display_service",
                .capability = "Display.primary_display",
                .provider = "host_display",
                .status = rte::EvidenceStatus::ok,
                .fields = {{
                    {"fills", fills == 1 ? "1" : "unexpected"},
                    {"last_color", last_color == 0x00FF00u ? "0x00FF00" : "unexpected"},
                }},
                .field_count = 2,
            };
        }
    };

    static_assert(cap::TextSink::satisfied_by<MemoryTextSink>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);
    static_assert(cap::Display::satisfied_by<HostDisplay>);

    using LogRef = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryTextSink>;
    using TraceRef = rte::ProviderRef<cap::TextSink, provider::memory_trace, MemoryTextSink>;
    using ClockRef = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using DisplayRef = rte::ProviderRef<cap::Display, provider::host_display, HostDisplay>;
    using RuntimeLogBinding = rte::RuntimeBinding<LogReq, LogRef>;
    using RuntimeTraceBinding = rte::RuntimeBinding<TraceReq, TraceRef>;
    using RuntimeClockBinding = rte::RuntimeBinding<ClockReq, ClockRef>;
    using RuntimeDisplayBinding = rte::RuntimeBinding<DisplayReq, DisplayRef>;
    using UiContext = rte::ContextView<RuntimeLogBinding, RuntimeClockBinding, RuntimeDisplayBinding>;
    using DiagContext = rte::ContextView<RuntimeTraceBinding, RuntimeClockBinding>;

    static_assert(!rte::has_requirement_v<EvidenceReq, RuntimeLogBinding, RuntimeClockBinding, RuntimeDisplayBinding>);
    static_assert(!rte::has_requirement_v<EvidenceReq, RuntimeTraceBinding, RuntimeClockBinding>);
    static_assert(!rte::has_requirement_v<TraceReq, RuntimeLogBinding, RuntimeClockBinding, RuntimeDisplayBinding>);
    static_assert(!rte::has_requirement_v<DisplayReq, RuntimeTraceBinding, RuntimeClockBinding>);

    void ui_tick(UiContext& context) noexcept {
        auto& log = context.get<LogReq>();
        auto& clock = context.get<ClockReq>();
        auto& display = context.get<DisplayReq>();
        log.write("ui=");
        log.write(clock.now_ms() == 42 ? "42" : "unexpected");
        display.fill(0x00FF00u);
    }

    void diag_tick(DiagContext& context) noexcept {
        auto& trace = context.get<TraceReq>();
        auto& clock = context.get<ClockReq>();
        trace.write("diag=");
        trace.write(clock.now_ms() == 42 ? "42" : "unexpected");
    }

    bool collect_profile_evidence(rte::EvidenceCollector& collector,
                                  MemoryTextSink& log,
                                  MemoryTextSink& trace,
                                  FakeClock& clock,
                                  HostDisplay& display) noexcept {
        return collector.append(log.evidence("log_service", "TextSink.log", "memory_log")) &&
               collector.append(trace.evidence("trace_service", "TextSink.debug_trace", "memory_trace")) &&
               collector.append(clock.evidence()) &&
               collector.append(display.evidence());
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
    MemoryTextSink log{};
    MemoryTextSink trace{};
    FakeClock clock{};
    HostDisplay display{};
    rte::EvidenceCollector collector{};

    UiContext ui_context{
        RuntimeLogBinding{LogRef{log}},
        RuntimeClockBinding{ClockRef{clock}},
        RuntimeDisplayBinding{DisplayRef{display}},
    };
    DiagContext diag_context{
        RuntimeTraceBinding{TraceRef{trace}},
        RuntimeClockBinding{ClockRef{clock}},
    };

    ui_tick(ui_context);
    diag_tick(diag_context);

    if (!expect(collector.count == 0, "apps do not receive evidence collector through context")) return 1;
    if (!expect(log.evidence_reads == 0, "log evidence is not collected by app tick")) return 1;
    if (!expect(trace.evidence_reads == 0, "trace evidence is not collected by app tick")) return 1;
    if (!expect(display.evidence_reads == 0, "display evidence is not collected by app tick")) return 1;

    const auto log_writes_before = log.writes;
    const auto trace_writes_before = trace.writes;
    const auto fills_before = display.fills;
    if (!expect(collect_profile_evidence(collector, log, trace, clock, display),
                "profile-wide evidence collection succeeds")) return 1;

    if (!expect(collector.count == 4, "profile-wide evidence includes all providers")) return 1;
    if (!expect(collector.frames[0].component == "log_service", "log evidence is collected")) return 1;
    if (!expect(collector.frames[1].component == "trace_service", "trace evidence is collected outside ui context")) return 1;
    if (!expect(collector.frames[2].component == "clock_service", "shared clock evidence is collected once")) return 1;
    if (!expect(collector.frames[3].component == "display_service", "display evidence is collected outside diag context")) return 1;
    if (!expect(log.writes == log_writes_before, "evidence collection does not mutate log writes")) return 1;
    if (!expect(trace.writes == trace_writes_before, "evidence collection does not mutate trace writes")) return 1;
    if (!expect(display.fills == fills_before, "evidence collection does not mutate display fills")) return 1;

    rte::PresentationBuffer presentation{};
    rte::format_evidence(presentation, collector);
    if (!expect(presentation.formatted_frames == 4, "presentation formats collected evidence")) return 1;
    if (!expect(contains(presentation.view(), "component=trace_service"), "presentation includes trace evidence")) return 1;
    if (!expect(contains(presentation.view(), "component=display_service"), "presentation includes display evidence")) return 1;
    if (!expect(log.writes == log_writes_before, "presentation does not mutate log provider")) return 1;
    if (!expect(trace.writes == trace_writes_before, "presentation does not mutate trace provider")) return 1;
    if (!expect(display.fills == fills_before, "presentation does not mutate display provider")) return 1;

    std::puts("[rte-evidence-slice-smoke] ok");
    return 0;
}
