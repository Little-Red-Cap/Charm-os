#if !defined(__cpp_impl_reflection)
#error "__cpp_impl_reflection is required"
#endif

#include <meta>
#include <string_view>
#include <type_traits>

namespace cap {
struct TextSink {};
struct Clock {};
struct App {};
} // namespace cap

namespace role {
struct log {};
struct monotonic_time {};
struct main_app {};
} // namespace role

struct ComponentSpecShape {
    int requirements;
    int provides;
};

namespace rte {

template <auto KindRef, auto RoleRef>
struct Requirement {
    static constexpr auto kind_ref = KindRef;
    static constexpr auto role_ref = RoleRef;
    using kind = [:KindRef:];
    using role = [:RoleRef:];
};

template <auto KindRef, auto RoleRef>
struct Provided {
    static constexpr auto kind_ref = KindRef;
    static constexpr auto role_ref = RoleRef;
    using kind = [:KindRef:];
    using role = [:RoleRef:];
};

template <typename Req>
struct ProvidedFor;

template <auto KindRef, auto RoleRef>
struct ProvidedFor<Requirement<KindRef, RoleRef>> {
    using type = Provided<KindRef, RoleRef>;
};

template <typename... Items>
struct Set {};

template <typename Needle, typename SetT>
constexpr bool contains = false;

template <typename Needle, typename... Items>
constexpr bool contains<Needle, Set<Items...>> = (... || std::is_same_v<Needle, Items>);

template <typename Req, typename ProviderSet>
constexpr bool provided_by = contains<typename ProvidedFor<Req>::type, ProviderSet>;

template <typename Requires, typename Provides>
struct ComponentDesc {
    using required_set = Requires;
    using provided_set = Provides;
};

template <typename Req, typename... ProviderComponents>
constexpr bool req_satisfied_by_components =
    (... || provided_by<Req, typename ProviderComponents::provided_set>);

template <typename RequiredSet, typename... ProviderComponents>
struct required_set_satisfied_by;

template <typename... Reqs, typename... ProviderComponents>
struct required_set_satisfied_by<Set<Reqs...>, ProviderComponents...> {
    static constexpr bool value = (... && req_satisfied_by_components<Reqs, ProviderComponents...>);
};

template <typename Component, typename... ProviderComponents>
constexpr bool requirements_satisfied_by =
    required_set_satisfied_by<typename Component::required_set, ProviderComponents...>::value;

} // namespace rte

using LogReq = rte::Requirement<^^cap::TextSink, ^^role::log>;
using ClockReq = rte::Requirement<^^cap::Clock, ^^role::monotonic_time>;
using LogProv = rte::Provided<^^cap::TextSink, ^^role::log>;
using ClockProv = rte::Provided<^^cap::Clock, ^^role::monotonic_time>;
using AppProv = rte::Provided<^^cap::App, ^^role::main_app>;
using DemoProvides = rte::Set<LogProv, AppProv>;
using LogService = rte::ComponentDesc<rte::Set<>, rte::Set<LogProv>>;
using ClockService = rte::ComponentDesc<rte::Set<>, rte::Set<ClockProv>>;
using DemoApp = rte::ComponentDesc<rte::Set<LogReq, ClockReq>, rte::Set<AppProv>>;

constexpr int kSentinel = 7;

consteval bool component_spec_shape_is_discoverable() {
    const auto fields = std::meta::nonstatic_data_members_of(^^ComponentSpecShape,
                                                             std::meta::access_context::unchecked());
    return fields.size() == 2 &&
           std::meta::identifier_of(fields[0]) == std::string_view{"requirements"} &&
           std::meta::identifier_of(fields[1]) == std::string_view{"provides"};
}

static_assert(std::meta::is_type(LogReq::kind_ref));
static_assert(std::meta::is_type(LogReq::role_ref));
static_assert(std::meta::has_identifier(LogReq::kind_ref));
static_assert(std::meta::identifier_of(LogReq::kind_ref) == std::string_view{"TextSink"});
static_assert(std::meta::has_template_arguments(std::meta::dealias(^^LogReq)));
static_assert(LogReq::kind_ref == ^^cap::TextSink);
static_assert(std::is_same_v<LogReq::kind, cap::TextSink>);
static_assert(std::is_same_v<LogReq::role, role::log>);
static_assert(std::is_same_v<typename [:std::meta::type_of(^^kSentinel):], const int>);
static_assert(std::meta::is_variable(^^kSentinel));
static_assert(std::meta::is_value(std::meta::constant_of(^^kSentinel)));
static_assert([:std::meta::constant_of(^^kSentinel):] == 7);
static_assert(component_spec_shape_is_discoverable());
static_assert(rte::provided_by<LogReq, DemoProvides>);
static_assert(!rte::provided_by<ClockReq, DemoProvides>);
static_assert(rte::requirements_satisfied_by<DemoApp, LogService, ClockService>);
static_assert(!rte::requirements_satisfied_by<DemoApp, LogService>);
static_assert(rte::requirements_satisfied_by<LogService>);

int main() {
    return 0;
}
