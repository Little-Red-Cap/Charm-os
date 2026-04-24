module;
#include <cstdint>
#include <string_view>
export module out.domain;
// Dependency contract (DO NOT VIOLATE)
// Allowed out.* imports: (none)
// Forbidden out.* imports: out.*
// Rationale: compile-time gating knobs only (level/domain).
// If you need functionality from a higher layer, add an extension point in this layer instead.


export namespace out {

    enum class level : std::uint8_t { off, error, warn, info, debug, trace };

    // Build-time log level: compile-time constant, no runtime cost.
    inline constexpr level build_level =
    #if defined(LOG_LEVEL_TRACE)
      level::trace;
#elif defined(LOG_LEVEL_DEBUG)
          level::debug;
#elif defined(LOG_LEVEL_INFO)
              level::info;
#elif defined(LOG_LEVEL_WARN)
                  level::warn;
#elif defined(LOG_LEVEL_ERROR)
                      level::error;
#else
                          level::off;
#endif

    // Domain tag: compile-time filter per module (type tag only).
    struct default_domain {};
    // Domain tag: raw output (print/println) gate.
    struct raw_domain {};

    // template <class T>
    // struct domain_t { using type = T; };

    // Domain filter: compile-time enable/disable.
    template <class Domain>
    inline constexpr bool domain_enabled = true;

    // Usage example:
    // template <> inline constexpr bool domain_enabled<my_module> = false;

    // Optional domain name: empty means no prefix.
    template <class Domain>
    inline constexpr std::string_view domain_name{};

}
