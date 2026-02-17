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

    struct Semver {
        util::u32 major{0};
        util::u32 minor{0};
        util::u32 patch{0};
        bool has_minor{false};
        bool has_patch{false};
        bool wildcard_minor{false};
        bool wildcard_patch{false};
    };

    inline Semver parse_semver(std::string_view v) noexcept {
        Semver out{};
        util::usize i = 0;
        if (i < v.size() && (v[i] == 'v' || v[i] == '^' || v[i] == '~' || v[i] == '=')) ++i;
        auto parse_num = [&](util::u32& dst, bool& has) noexcept {
            if (i >= v.size()) return;
            if (v[i] == 'x' || v[i] == '*') {
                has = true;
                ++i;
                return;
            }
            util::u32 val = 0;
            bool any = false;
            while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
                any = true;
                val = val * 10 + static_cast<util::u32>(v[i] - '0');
                ++i;
            }
            if (any) {
                dst = val;
                has = true;
            }
        };
        bool has_major = false;
        parse_num(out.major, has_major);
        if (i < v.size() && v[i] == '.') {
            ++i;
            out.has_minor = true;
            if (i < v.size() && (v[i] == 'x' || v[i] == '*')) {
                out.wildcard_minor = true;
                ++i;
            } else {
                parse_num(out.minor, out.has_minor);
            }
            if (i < v.size() && v[i] == '.') {
                ++i;
                out.has_patch = true;
                if (i < v.size() && (v[i] == 'x' || v[i] == '*')) {
                    out.wildcard_patch = true;
                    ++i;
                } else {
                    parse_num(out.patch, out.has_patch);
                }
            }
        }
        return out;
    }

    inline int compare_semver(const Semver& a, const Semver& b) noexcept {
        if (a.major != b.major) return a.major < b.major ? -1 : 1;
        if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
        if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
        return 0;
    }

    inline bool match_version(std::string_view want, std::string_view have) noexcept {
        if (want.empty()) return true;
        if (want == have) return true;
        if (want.size() >= 2 && want[0] == '>') {
            const bool eq = want[1] == '=';
            const auto w = parse_semver(want.substr(eq ? 2 : 1));
            const auto h = parse_semver(have);
            const int cmp = compare_semver(h, w);
            return eq ? cmp >= 0 : cmp > 0;
        }
        if (want.size() >= 2 && want[0] == '<') {
            const bool eq = want[1] == '=';
            const auto w = parse_semver(want.substr(eq ? 2 : 1));
            const auto h = parse_semver(have);
            const int cmp = compare_semver(h, w);
            return eq ? cmp <= 0 : cmp < 0;
        }
        if (!want.empty() && want[0] == '^') {
            const auto w = parse_semver(want);
            const auto h = parse_semver(have);
            return w.major == h.major;
        }
        if (!want.empty() && want[0] == '~') {
            const auto w = parse_semver(want);
            const auto h = parse_semver(have);
            return w.major == h.major && w.minor == h.minor;
        }
        if (!want.empty() && want[0] == '=') {
            return parse_semver(want.substr(1)).major == parse_semver(have).major
                && parse_semver(want.substr(1)).minor == parse_semver(have).minor
                && parse_semver(want.substr(1)).patch == parse_semver(have).patch;
        }
        if (want.find('*') != std::string_view::npos || want.find('x') != std::string_view::npos) {
            const auto w = parse_semver(want);
            const auto h = parse_semver(have);
            if (w.major != h.major) return false;
            if (!w.wildcard_minor && w.minor != h.minor) return false;
            if (!w.wildcard_patch && w.has_patch && w.patch != h.patch) return false;
            return true;
        }
        return parse_major(want) == parse_major(have);
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
