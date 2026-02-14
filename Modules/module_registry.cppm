module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module module_registry;

import util.core;

export namespace modulex {
    struct ModuleInfo {
        std::string_view name{};
        std::string_view version{};
        util::usize base{0};
    };

    constexpr util::u32 parse_major(std::string_view v) noexcept {
        util::u32 out = 0;
        util::usize i = 0;
        if (i < v.size() && (v[i] == 'v' || v[i] == '^')) ++i;
        if (i < v.size() && v[i] == 'v') ++i;
        while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
            out = out * 10 + static_cast<util::u32>(v[i] - '0');
            ++i;
        }
        return out;
    }

    inline bool match_version(std::string_view want, std::string_view have) noexcept {
        if (want.empty()) return true;
        if (want == have) return true;
        if (!want.empty() && want[0] == '^') {
            return parse_major(want) == parse_major(have);
        }
        if (!want.empty() && want[0] == '~') {
            return parse_major(want) == parse_major(have);
        }
        return false;
    }

    template <util::usize Max>
    struct Registry {
        std::array<ModuleInfo, Max> mods{};
        util::usize count{0};

        bool add(ModuleInfo info) noexcept {
            if (count >= Max) return false;
            mods[count++] = info;
            return true;
        }

        const ModuleInfo* find(std::string_view name) const noexcept {
            for (util::usize i = 0; i < count; ++i) {
                if (mods[i].name == name) return &mods[i];
            }
            return nullptr;
        }
    };
}
