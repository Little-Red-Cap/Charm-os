module;

#include <variant>

export module util.variant;

export namespace util {
    using std::variant;
    using std::monostate;
    using std::visit;
    using std::get;
    using std::get_if;
}
