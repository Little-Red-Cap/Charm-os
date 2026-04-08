module;

#include <array>
#include <span>
#include <string_view>

export module posix.exec_source;

import posix.env;
import util.core;

export namespace posix {
    inline std::string_view strip_modulex_prefix(std::string_view name) noexcept {
        constexpr std::string_view kPrefix = "modulex:";
        if (name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix) {
            return name.substr(kPrefix.size());
        }
        return name;
    }

    inline std::string_view strip_elf_prefix(std::string_view name) noexcept {
        constexpr std::string_view kPrefix = "elf:";
        if (name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix) {
            return name.substr(kPrefix.size());
        }
        return name;
    }

    inline bool is_elf_prefix(std::string_view name) noexcept {
        constexpr std::string_view kPrefix = "elf:";
        return name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix;
    }

    inline std::string_view strip_elf_mem_prefix(std::string_view name) noexcept {
        constexpr std::string_view kPrefix = "elfmem:";
        if (name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix) {
            return name.substr(kPrefix.size());
        }
        return name;
    }

    inline bool is_elf_mem_prefix(std::string_view name) noexcept {
        constexpr std::string_view kPrefix = "elfmem:";
        return name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix;
    }

    inline std::string_view resolve_exec_path(const char* path,
                                              std::span<const char* const> argv) noexcept {
        if (path && path[0] != '\0') {
            return std::string_view{path};
        }
        if (!argv.empty() && argv[0] != nullptr) {
            return std::string_view{argv[0]};
        }
        return {};
    }

    inline std::string_view resolve_elf_path(const char* path,
                                             std::span<const char* const> argv) noexcept {
        return strip_elf_prefix(resolve_exec_path(path, argv));
    }

    inline std::string_view resolve_elf_mem_name(const char* path,
                                                 std::span<const char* const> argv) noexcept {
        return strip_elf_mem_prefix(resolve_exec_path(path, argv));
    }

    inline bool is_elf_prefixed(const char* path,
                                std::span<const char* const> argv) noexcept {
        if (path && is_elf_prefix(std::string_view{path})) return true;
        if (!argv.empty() && argv[0] != nullptr && is_elf_prefix(std::string_view{argv[0]})) {
            return true;
        }
        return false;
    }

    inline bool is_elf_mem_prefixed(const char* path,
                                    std::span<const char* const> argv) noexcept {
        if (path && is_elf_mem_prefix(std::string_view{path})) return true;
        if (!argv.empty() && argv[0] != nullptr && is_elf_mem_prefix(std::string_view{argv[0]})) {
            return true;
        }
        return false;
    }

    inline std::string_view envp_path(std::span<const char* const> envp) noexcept {
        return envp_get(envp, kPathKey);
    }

    template <util::usize MaxPathLen, typename FindFn>
    std::string_view resolve_registered_name(bool search_path,
                                             const char* path,
                                             std::span<const char* const> argv,
                                             std::span<const char* const> envp,
                                             std::array<char, MaxPathLen>& resolved_path,
                                             FindFn&& find_fn) noexcept {
        const std::string_view path_sv = path ? std::string_view{path} : std::string_view{};
        const std::string_view argv0 = (!argv.empty() && argv[0] != nullptr)
            ? std::string_view{argv[0]}
            : std::string_view{};

        if (!search_path) {
            return strip_modulex_prefix(path_sv);
        }

        const std::string_view target = !path_sv.empty() ? strip_modulex_prefix(path_sv)
                                                         : strip_modulex_prefix(argv0);
        if (target.empty()) return {};

        const auto path_list = envp_path(envp);
        if (path_list.empty()) {
            return target;
        }

        resolved_path[0] = '\0';
        const bool found = for_each_path_candidate<MaxPathLen>(path_list, target,
            [&](std::string_view candidate) noexcept {
                if (!find_fn(candidate)) {
                    return false;
                }
                const util::usize n = candidate.size() < (MaxPathLen - 1)
                    ? candidate.size()
                    : (MaxPathLen - 1);
                for (util::usize i = 0; i < n; ++i) {
                    resolved_path[i] = candidate[i];
                }
                resolved_path[n] = '\0';
                return true;
            });
        if (found) {
            return std::string_view{resolved_path.data()};
        }
        return target;
    }
}