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
    using etl::nullopt_t;
    inline constexpr nullopt_t nullopt{};
#else
    using std::optional;
    using std::nullopt_t;
    inline constexpr nullopt_t nullopt = std::nullopt;
#endif
}
