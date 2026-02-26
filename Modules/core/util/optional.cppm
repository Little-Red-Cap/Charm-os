module;

#include <optional>

export module util.optional;

export namespace util {
    using std::optional;
    using std::nullopt_t;
    inline constexpr nullopt_t nullopt = std::nullopt;
}
