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
}

namespace role {
    struct log {};
    struct debug_trace {};
}

namespace provider {
    struct shared_console {};
    struct log_only_console {};
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
        std::string_view capability{};
        std::string_view provider{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::array<EvidenceField, 4> fields{};
        std::size_t field_count{0};
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

    template <typename Req>
    struct ProvidedFor;

    template <typename Kind, typename Role>
    struct ProvidedFor<Requirement<Kind, Role>> {
        using type = Provided<Kind, Role>;
    };

    template <typename ProviderTag, typename Provides>
    struct ProviderDesc {
        using tag = ProviderTag;
        using provided_set = Provides;
    };

    template <typename Req, typename ProviderTag>
    struct ProfileBinding {
        using requirement = Req;
        using provider_tag = ProviderTag;
    };

    template <typename Req, typename Provider>
    inline constexpr bool provider_declares_requirement_v =
        set_contains_v<typename ProvidedFor<Req>::type, typename Provider::provided_set>;

    template <typename Req, typename ProviderTag, typename Providers>
    struct provider_tag_declares_requirement;

    template <typename Req, typename ProviderTag, typename... Providers>
    struct provider_tag_declares_requirement<Req, ProviderTag, std::tuple<Providers...>>
        : std::bool_constant<(... || (std::same_as<ProviderTag, typename Providers::tag> &&
                                      provider_declares_requirement_v<Req, Providers>))> {};

    template <typename Req, typename ProviderTag, typename Providers>
    inline constexpr bool provider_tag_declares_requirement_v =
        provider_tag_declares_requirement<Req, ProviderTag, Providers>::value;

    template <typename Binding, typename Providers>
    inline constexpr bool binding_valid_v =
        provider_tag_declares_requirement_v<typename Binding::requirement,
                                            typename Binding::provider_tag,
                                            Providers>;

    template <typename Req, typename... Bindings>
    inline constexpr bool has_requirement_v =
        (... || std::same_as<typename Bindings::requirement, Req>);

    template <typename Req, typename Binding>
    inline constexpr bool binding_matches_req_v =
        std::same_as<typename Binding::requirement, Req>;

    template <typename Req, typename... Bindings>
    struct selected_provider;

    template <typename Req>
    struct selected_provider<Req> {
        static_assert(!std::same_as<Req, Req>, "missing binding for requirement");
    };

    template <typename Req, typename ProviderTag, typename... Rest>
    struct selected_provider<Req, ProfileBinding<Req, ProviderTag>, Rest...> {
        using type = ProviderTag;
    };

    template <typename Req, typename OtherReq, typename ProviderTag, typename... Rest>
    struct selected_provider<Req, ProfileBinding<OtherReq, ProviderTag>, Rest...>
        : selected_provider<Req, Rest...> {};

    template <typename Req, typename... Bindings>
    using selected_provider_t = typename selected_provider<Req, Bindings...>::type;

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
}

namespace {
    using LogReq = rte::Requirement<cap::TextSink, role::log>;
    using TraceReq = rte::Requirement<cap::TextSink, role::debug_trace>;
    using LogProv = rte::Provided<cap::TextSink, role::log>;
    using TraceProv = rte::Provided<cap::TextSink, role::debug_trace>;
    using SharedConsoleDesc = rte::ProviderDesc<provider::shared_console, rte::ProviderSet<LogProv, TraceProv>>;
    using LogOnlyConsoleDesc = rte::ProviderDesc<provider::log_only_console, rte::ProviderSet<LogProv>>;
    using Providers = std::tuple<SharedConsoleDesc, LogOnlyConsoleDesc>;
    using LogBinding = rte::ProfileBinding<LogReq, provider::shared_console>;
    using TraceBinding = rte::ProfileBinding<TraceReq, provider::shared_console>;
    using BadTraceBinding = rte::ProfileBinding<TraceReq, provider::log_only_console>;

    static_assert(rte::binding_valid_v<LogBinding, Providers>);
    static_assert(rte::binding_valid_v<TraceBinding, Providers>);
    static_assert(!rte::binding_valid_v<BadTraceBinding, Providers>);
    static_assert(std::same_as<rte::selected_provider_t<LogReq, LogBinding, TraceBinding>,
                               provider::shared_console>);
    static_assert(std::same_as<rte::selected_provider_t<TraceReq, LogBinding, TraceBinding>,
                               provider::shared_console>);

    struct SharedConsole {
        std::array<char, 192> bytes{};
        std::size_t used{0};
        std::uint32_t writes{0};

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
    };

    static_assert(cap::TextSink::satisfied_by<SharedConsole>);

    using SharedConsoleRef = rte::ProviderRef<cap::TextSink, provider::shared_console, SharedConsole>;
    using RuntimeLogBinding = rte::RuntimeBinding<LogReq, SharedConsoleRef>;
    using RuntimeTraceBinding = rte::RuntimeBinding<TraceReq, SharedConsoleRef>;
    using AppContext = rte::ContextView<RuntimeLogBinding, RuntimeTraceBinding>;

    static_assert(rte::has_requirement_v<LogReq, RuntimeLogBinding, RuntimeTraceBinding>);
    static_assert(rte::has_requirement_v<TraceReq, RuntimeLogBinding, RuntimeTraceBinding>);

    void app_tick(AppContext& context) noexcept {
        auto& log = context.get<LogReq>();
        auto& trace = context.get<TraceReq>();
        log.write("log:boot;");
        trace.write("trace:probe;");
    }

    rte::EvidenceFrame role_evidence(std::string_view capability) noexcept {
        return rte::EvidenceFrame{
            .capability = capability,
            .provider = "shared_console",
            .status = rte::EvidenceStatus::ok,
            .fields = {{
                {"binding", "explicit"},
                {"provider_identity", "shared_console"},
            }},
            .field_count = 2,
        };
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
    SharedConsole console{};
    AppContext context{
        RuntimeLogBinding{SharedConsoleRef{console}},
        RuntimeTraceBinding{SharedConsoleRef{console}},
    };

    app_tick(context);
    const auto log_evidence = role_evidence("TextSink.log");
    const auto trace_evidence = role_evidence("TextSink.debug_trace");

    if (!expect(console.view() == "log:boot;trace:probe;", "shared provider receives both role writes")) return 1;
    if (!expect(console.writes == 2, "role-specific access paths call the same provider explicitly")) return 1;
    if (!expect(log_evidence.capability == "TextSink.log", "log evidence preserves role")) return 1;
    if (!expect(trace_evidence.capability == "TextSink.debug_trace", "trace evidence preserves role")) return 1;
    if (!expect(log_evidence.provider == "shared_console", "log role keeps shared provider identity")) return 1;
    if (!expect(trace_evidence.provider == "shared_console", "trace role keeps shared provider identity")) return 1;

    std::puts("[rte-multi-role-provider-smoke] ok");
    return 0;
}
