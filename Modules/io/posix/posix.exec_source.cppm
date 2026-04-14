module;

#include <array>
#include <span>
#include <string_view>

export module posix.exec_source;

import posix.env;
import fs_path;
import util.core;
import util.error;

export namespace posix {
    inline bool is_absolute_exec_path(std::string_view path) noexcept {
        return !path.empty() && fs::is_sep(path.front());
    }

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

    template <util::usize MaxPathLen>
    util::Result<std::string_view> resolve_path_from_cwd(std::string_view cwd,
                                                         std::string_view path,
                                                         std::array<char, MaxPathLen>& resolved_path) noexcept {
        if (path.empty()) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (cwd.empty()) {
            cwd = "/";
        }
        if (!is_absolute_exec_path(cwd)) {
            return util::unexpected(util::Errc::invalid_arg);
        }

        resolved_path = {};
        util::usize size = 1;
        resolved_path[0] = '/';

        const auto append_component = [&](std::string_view component) noexcept -> util::Result<void> {
            if (component.empty() || component == ".") {
                return {};
            }
            if (component == "..") {
                while (size > 1 && resolved_path[size - 1] != '/') {
                    --size;
                }
                if (size > 1) {
                    --size;
                }
                return {};
            }
            if (size > 1) {
                if (size + 1 >= MaxPathLen) {
                    return util::unexpected(util::Errc::nametoolong);
                }
                resolved_path[size++] = '/';
            }
            if (size + component.size() >= MaxPathLen) {
                return util::unexpected(util::Errc::nametoolong);
            }
            for (char ch : component) {
                resolved_path[size++] = ch;
            }
            return {};
        };

        const auto append_path = [&](std::string_view text) noexcept -> util::Result<void> {
            util::usize start = 0;
            while (start < text.size()) {
                while (start < text.size() && fs::is_sep(text[start])) {
                    ++start;
                }
                util::usize end = start;
                while (end < text.size() && !fs::is_sep(text[end])) {
                    ++end;
                }
                if (end > start) {
                    auto step = append_component(text.substr(start, end - start));
                    if (!step) {
                        return step;
                    }
                }
                start = end + 1;
            }
            return {};
        };

        if (!is_absolute_exec_path(path)) {
            auto step = append_path(cwd);
            if (!step) {
                return util::unexpected(step.error());
            }
        }
        auto step = append_path(path);
        if (!step) {
            return util::unexpected(step.error());
        }
        resolved_path[size] = '\0';
        return std::string_view{resolved_path.data(), size};
    }

    template <util::usize MaxPathLen, typename FindFn>
    std::string_view resolve_registered_name(bool search_path,
                                             const char* path,
                                             std::span<const char* const> argv,
                                             std::span<const char* const> envp,
                                             const char* cwd,
                                             std::array<char, MaxPathLen>& resolved_path,
                                             FindFn&& find_fn) noexcept {
        const std::string_view path_sv = path ? std::string_view{path} : std::string_view{};
        const std::string_view argv0 = (!argv.empty() && argv[0] != nullptr)
            ? std::string_view{argv[0]}
            : std::string_view{};

        if (!search_path) {
            const auto target = !path_sv.empty() ? strip_modulex_prefix(path_sv)
                                                 : strip_modulex_prefix(argv0);
            if (target.empty()) {
                return {};
            }
            if (find_fn(target)) {
                return target;
            }
            auto resolved = resolve_path_from_cwd<MaxPathLen>(cwd ? std::string_view{cwd} : std::string_view{"/"},
                                                              target,
                                                              resolved_path);
            if (resolved && find_fn(resolved.value())) {
                return resolved.value();
            }
            return target;
        }

        const std::string_view target = !path_sv.empty() ? strip_modulex_prefix(path_sv)
                                                         : strip_modulex_prefix(argv0);
        if (target.empty()) return {};

        if (has_path_separator(target)) {
            if (find_fn(target)) {
                return target;
            }
            auto resolved = resolve_path_from_cwd<MaxPathLen>(cwd ? std::string_view{cwd} : std::string_view{"/"},
                                                              target,
                                                              resolved_path);
            if (resolved && find_fn(resolved.value())) {
                return resolved.value();
            }
            return target;
        }

        const auto path_list = envp_path(envp);
        if (path_list.empty()) {
            if (find_fn(target)) {
                return target;
            }
            auto resolved = resolve_path_from_cwd<MaxPathLen>(cwd ? std::string_view{cwd} : std::string_view{"/"},
                                                              target,
                                                              resolved_path);
            if (resolved && find_fn(resolved.value())) {
                return resolved.value();
            }
            return target;
        }

        resolved_path[0] = '\0';
        const bool found = for_each_path_candidate<MaxPathLen>(path_list, target,
            [&](std::string_view candidate) noexcept {
                auto resolved = resolve_path_from_cwd<MaxPathLen>(cwd ? std::string_view{cwd} : std::string_view{"/"},
                                                                  candidate,
                                                                  resolved_path);
                if (!resolved || !find_fn(resolved.value())) {
                    return false;
                }
                return true;
            });
        if (found) {
            return std::string_view{resolved_path.data()};
        }
        return {};
    }
}
