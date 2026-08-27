#if !defined(__cpp_impl_reflection)
#error "__cpp_impl_reflection is required"
#endif

#include <meta>
#include <string_view>
#include <type_traits>

namespace sample {
struct TextSink {};
struct Clock {};
struct App {};
struct log {};
struct monotonic_time {};
struct main_app {};
} // namespace sample

struct ComponentSpecShape {
    int requirements;
    int provides;
};

namespace reflection_fixture {

template <auto ContractRef, auto KeyRef>
struct Relation {
    static constexpr auto contract_ref = ContractRef;
    static constexpr auto key_ref = KeyRef;
    using contract = [:ContractRef:];
    using key = [:KeyRef:];
};

template <typename... Items>
struct Set {};

template <typename Needle, typename SetT>
constexpr bool contains = false;

template <typename Needle, typename... Items>
constexpr bool contains<Needle, Set<Items...>> = (... || std::is_same_v<Needle, Items>);

template <typename RelationT, typename AvailableSet>
constexpr bool relation_present = contains<RelationT, AvailableSet>;

template <typename Required, typename Available>
struct RelationSets {
    using required_set = Required;
    using available_set = Available;
};

template <typename RequiredRelation, typename... AvailableSets>
constexpr bool relation_covered_by_sets =
    (... || relation_present<RequiredRelation, typename AvailableSets::available_set>);

template <typename RequiredSet, typename... AvailableSets>
struct required_set_covered_by;

template <typename... RequiredRelations, typename... AvailableSets>
struct required_set_covered_by<Set<RequiredRelations...>, AvailableSets...> {
    static constexpr bool value = (... && relation_covered_by_sets<RequiredRelations, AvailableSets...>);
};

template <typename RelationSet, typename... AvailableSets>
constexpr bool relations_covered_by =
    required_set_covered_by<typename RelationSet::required_set, AvailableSets...>::value;

} // namespace reflection_fixture

using LogRelation = reflection_fixture::Relation<^^sample::TextSink, ^^sample::log>;
using ClockRelation = reflection_fixture::Relation<^^sample::Clock, ^^sample::monotonic_time>;
using AppRelation = reflection_fixture::Relation<^^sample::App, ^^sample::main_app>;
using DemoAvailable = reflection_fixture::Set<LogRelation, AppRelation>;
using LogSet = reflection_fixture::RelationSets<reflection_fixture::Set<>, reflection_fixture::Set<LogRelation>>;
using ClockSet = reflection_fixture::RelationSets<reflection_fixture::Set<>, reflection_fixture::Set<ClockRelation>>;
using DemoSet = reflection_fixture::RelationSets<
    reflection_fixture::Set<LogRelation, ClockRelation>,
    reflection_fixture::Set<AppRelation>>;

constexpr int kSentinel = 7;

consteval bool component_spec_shape_is_discoverable() {
    const auto fields = std::meta::nonstatic_data_members_of(^^ComponentSpecShape,
                                                             std::meta::access_context::unchecked());
    return fields.size() == 2 &&
           std::meta::identifier_of(fields[0]) == std::string_view{"requirements"} &&
           std::meta::identifier_of(fields[1]) == std::string_view{"provides"};
}

static_assert(std::meta::is_type(LogRelation::contract_ref));
static_assert(std::meta::is_type(LogRelation::key_ref));
static_assert(std::meta::has_identifier(LogRelation::contract_ref));
static_assert(std::meta::identifier_of(LogRelation::contract_ref) == std::string_view{"TextSink"});
static_assert(std::meta::has_template_arguments(std::meta::dealias(^^LogRelation)));
static_assert(LogRelation::contract_ref == ^^sample::TextSink);
static_assert(std::is_same_v<LogRelation::contract, sample::TextSink>);
static_assert(std::is_same_v<LogRelation::key, sample::log>);
static_assert(std::is_same_v<typename [:std::meta::type_of(^^kSentinel):], const int>);
static_assert(std::meta::is_variable(^^kSentinel));
static_assert(std::meta::is_value(std::meta::constant_of(^^kSentinel)));
static_assert([:std::meta::constant_of(^^kSentinel):] == 7);
static_assert(component_spec_shape_is_discoverable());
static_assert(reflection_fixture::relation_present<LogRelation, DemoAvailable>);
static_assert(!reflection_fixture::relation_present<ClockRelation, DemoAvailable>);
static_assert(reflection_fixture::relations_covered_by<DemoSet, LogSet, ClockSet>);
static_assert(!reflection_fixture::relations_covered_by<DemoSet, LogSet>);
static_assert(reflection_fixture::relations_covered_by<LogSet>);

int main() {
    return 0;
}
