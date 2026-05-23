module;

#include <concepts>
#include <array>
#include <string_view>
#if defined(CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION) \
    && CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION \
    && defined(__cpp_impl_reflection) \
    && __cpp_impl_reflection >= 202506L
#include <meta>
#endif

export module semantic.core;

import util.core;

export namespace semantic {
#if defined(CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION) \
    && CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION \
    && defined(__cpp_impl_reflection) \
    && __cpp_impl_reflection >= 202506L
    inline constexpr bool static_reflection_enabled = true;
#else
    inline constexpr bool static_reflection_enabled = false;
#endif

    enum class Result : util::u8 {
        ok = 0,
        failed,
    };

    [[nodiscard]] constexpr const char* result_name(Result result) noexcept
    {
        switch (result) {
        case Result::ok:
            return "ok";
        case Result::failed:
            return "failed";
        }
        return "unknown";
    }

    enum class Readiness : util::u8 {
        ready = 0,
        blocked,
    };

    [[nodiscard]] constexpr const char* readiness_name(Readiness readiness) noexcept
    {
        switch (readiness) {
        case Readiness::ready:
            return "ready";
        case Readiness::blocked:
            return "blocked";
        }
        return "unknown";
    }

    enum class Verdict : util::u8 {
        standing = 0,
        drifted,
        collapsed,
    };

    [[nodiscard]] constexpr const char* verdict_name(Verdict verdict) noexcept
    {
        switch (verdict) {
        case Verdict::standing:
            return "standing";
        case Verdict::drifted:
            return "drifted";
        case Verdict::collapsed:
            return "collapsed";
        }
        return "unknown";
    }

    enum class FailureDomain : util::u8 {
        none = 0,
        input,
        selection,
        compare,
        route,
        explain,
        handoff,
    };

    [[nodiscard]] constexpr const char*
    failure_domain_name(FailureDomain domain) noexcept
    {
        switch (domain) {
        case FailureDomain::none:
            return "none";
        case FailureDomain::input:
            return "input";
        case FailureDomain::selection:
            return "selection";
        case FailureDomain::compare:
            return "compare";
        case FailureDomain::route:
            return "route";
        case FailureDomain::explain:
            return "explain";
        case FailureDomain::handoff:
            return "handoff";
        }
        return "unknown";
    }

    template <typename Tag>
    struct Ref {
        std::string_view id{};
        std::string_view path{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return !id.empty() && !path.empty();
        }

        [[nodiscard]] constexpr bool operator==(
            const Ref&) const noexcept = default;
    };

    struct ArtifactRefTag {};
    struct EvidenceRefTag {};
    struct SurfaceRefTag {};
    struct SummaryRefTag {};
    struct SchemaRefTag {};
    struct ExplainHopRefTag {};

    using ArtifactRef = Ref<ArtifactRefTag>;
    using EvidenceRef = Ref<EvidenceRefTag>;
    using SurfaceRef = Ref<SurfaceRefTag>;
    using SummaryRef = Ref<SummaryRefTag>;
    using SchemaRef = Ref<SchemaRefTag>;
    using ExplainHopRef = Ref<ExplainHopRefTag>;

    template <typename T>
    struct NamedValue {
        const char* name{"field"};
        T value{};
    };

    template <typename T>
    [[nodiscard]] constexpr auto named_value(const char* name, T value) noexcept
        -> NamedValue<T>
    {
        return NamedValue<T>{
            .name = name,
            .value = value,
        };
    }

    template <typename Descriptor, typename Field, std::size_t Capacity>
    struct Projection {
        Descriptor descriptor{};
        std::array<Field, Capacity> fields{};
        util::u8 field_count{0};
        const char* result_name{"value"};
    };

    template <typename Descriptor, typename Field, std::size_t Capacity>
    [[nodiscard]] constexpr auto make_projection(
        Descriptor descriptor,
        std::array<Field, Capacity> fields,
        util::u8 field_count,
        const char* result_name) noexcept
        -> Projection<Descriptor, Field, Capacity>
    {
        const auto bounded = field_count <= Capacity
                                 ? field_count
                                 : static_cast<util::u8>(Capacity);
        return Projection<Descriptor, Field, Capacity>{
            .descriptor = descriptor,
            .fields = fields,
            .field_count = bounded,
            .result_name = result_name != nullptr ? result_name : "value",
        };
    }

    template <typename T>
    concept SummaryPathProvider = requires(const T& value) {
        { value.summary_path() } -> std::convertible_to<std::string_view>;
    };

    template <typename T>
    concept SchemaIdProvider = requires(const T& value) {
        { value.schema_id() } -> std::convertible_to<std::string_view>;
    };

    template <typename T>
    concept ExplainableTarget = SummaryPathProvider<T> && SchemaIdProvider<T>;

    template <typename T>
    concept EvidenceSource = requires(const T& value) {
        { value.evidence_path() } -> std::convertible_to<std::string_view>;
        { value.result() } -> std::convertible_to<Result>;
    };

    template <typename T>
    concept WitnessCarrier = requires(const T& value) {
        { value.summary_path() } -> std::convertible_to<std::string_view>;
        { value.verdict() } -> std::convertible_to<Verdict>;
    };

    template <typename T>
    concept HandoffTarget = requires(const T& value) {
        { value.entry_name() } -> std::convertible_to<std::string_view>;
        { value.selected_summary_path() } -> std::convertible_to<std::string_view>;
    };

    template <typename ToField, typename FromField, std::size_t Capacity>
    [[nodiscard]] constexpr auto copy_named_fields(
        const std::array<FromField, Capacity>& source,
        util::u8 field_count) noexcept -> std::array<ToField, Capacity>
    {
        std::array<ToField, Capacity> out{};
        const auto bounded = field_count <= Capacity
                                 ? field_count
                                 : static_cast<util::u8>(Capacity);
        for (util::u8 index = 0; index < bounded; ++index) {
            out[index] = named_value(source[index].name, source[index].value);
        }
        return out;
    }

    template <typename T>
    [[nodiscard]] consteval auto reflected_member_names()
    {
        if constexpr (static_reflection_enabled) {
#if defined(CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION) \
    && CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION \
    && defined(__cpp_impl_reflection) \
    && __cpp_impl_reflection >= 202506L
            constexpr auto ctx = std::meta::access_context::current();
            constexpr auto members = std::define_static_array(
                std::meta::nonstatic_data_members_of(^^T, ctx));
            std::array<std::string_view, members.size()> out{};
            for (std::size_t index = 0; index < members.size(); ++index) {
                out[index] = std::meta::identifier_of(members[index]);
            }
            return out;
#endif
        } else {
            return std::array<std::string_view, 0>{};
        }
    }

    template <typename T, std::size_t N>
    [[nodiscard]] consteval bool reflected_member_names_match(
        const std::array<std::string_view, N>& expected)
    {
        if constexpr (static_reflection_enabled) {
            constexpr auto actual = reflected_member_names<T>();
            if (actual.size() != expected.size()) {
                return false;
            }
            for (std::size_t index = 0; index < expected.size(); ++index) {
                if (actual[index] != expected[index]) {
                    return false;
                }
            }
            return true;
        } else {
            (void)expected;
            return false;
        }
    }

    template <typename T, std::size_t N>
    [[nodiscard]] consteval bool reflected_member_names_match_when_enabled(
        const std::array<std::string_view, N>& expected)
    {
        if constexpr (static_reflection_enabled) {
            return reflected_member_names_match<T>(expected);
        } else {
            (void)expected;
            return true;
        }
    }

    namespace detail {
        struct ExplainableTargetProbe {
            [[nodiscard]] constexpr std::string_view schema_id() const noexcept
            {
                return "schema";
            }

            [[nodiscard]] constexpr std::string_view summary_path() const noexcept
            {
                return "summary";
            }
        };

        struct EvidenceSourceProbe {
            [[nodiscard]] constexpr std::string_view evidence_path() const noexcept
            {
                return "evidence";
            }

            [[nodiscard]] constexpr Result result() const noexcept
            {
                return Result::ok;
            }
        };

        struct WitnessCarrierProbe {
            [[nodiscard]] constexpr std::string_view summary_path() const noexcept
            {
                return "summary";
            }

            [[nodiscard]] constexpr Verdict verdict() const noexcept
            {
                return Verdict::standing;
            }
        };

        struct HandoffTargetProbe {
            [[nodiscard]] constexpr std::string_view entry_name() const noexcept
            {
                return "entry";
            }

            [[nodiscard]] constexpr std::string_view selected_summary_path() const noexcept
            {
                return "summary";
            }
        };
    }

    static_assert(ExplainableTarget<detail::ExplainableTargetProbe>);
    static_assert(EvidenceSource<detail::EvidenceSourceProbe>);
    static_assert(WitnessCarrier<detail::WitnessCarrierProbe>);
    static_assert(HandoffTarget<detail::HandoffTargetProbe>);
}
