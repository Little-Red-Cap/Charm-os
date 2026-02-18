module;

#if defined(CHARM_USE_ETL_EXPECTED)
#include <etl/expected.h>
#else
#include <expected>
#endif

export module util.expected;

export namespace util {
#if defined(CHARM_USE_ETL_EXPECTED)
    using etl::expected;
    using etl::unexpected;
#else
    using std::expected;
    using std::unexpected;
#endif
}
