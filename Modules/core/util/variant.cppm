module;

#if defined(CHARM_USE_ETL)
#include <etl/variant.h>
#else
#include <variant>
#endif

export module util.variant;

export namespace util {
#if defined(CHARM_USE_ETL)
    using etl::variant;
    using etl::monostate;
    using etl::visit;
    using etl::get;
    using etl::get_if;
#else
    using std::variant;
    using std::monostate;
    using std::visit;
    using std::get;
    using std::get_if;
#endif
}
