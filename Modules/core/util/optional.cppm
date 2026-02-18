module;

#if defined(CHARM_USE_ETL)
#include <etl/optional.h>
#else
#include <optional>
#endif

export module util.optional;

export namespace util {
#if defined(CHARM_USE_ETL)
    using etl::optional;
    using etl::nullopt;
    using etl::nullopt_t;
#else
    using std::optional;
    using std::nullopt;
    using std::nullopt_t;
#endif
}
