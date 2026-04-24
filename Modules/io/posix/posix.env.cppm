module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

export module posix.env;

import util.core;

export namespace posix {
    inline constexpr std::string_view kPathKey{"PATH"};

    inline std::string_view envp_get(std::span<const char* const> envp,
                                     std::string_view key) noexcept {
        if (envp.empty() || key.empty()) return {};
        for (auto* entry : envp) {
            if (!entry) continue;
            std::string_view sv{entry};
            const auto pos = sv.find('=');
            if (pos == std::string_view::npos) continue;
            if (sv.substr(0, pos) == key) {
                return sv.substr(pos + 1);
            }
        }
        return {};
    }

    inline bool has_path_separator(std::string_view name) noexcept {
        return name.find('/') != std::string_view::npos;
    }

    template <util::usize MaxPathLen, typename Fn>
    bool for_each_path_candidate(std::string_view path_list,
                                 std::string_view name,
                                 Fn&& fn) noexcept {
        if (name.empty()) return false;
        if (has_path_separator(name)) {
            return fn(name);
        }
        if (path_list.empty()) {
            return fn(name);
        }

        char buffer[MaxPathLen]{};
        util::usize pos = 0;
        util::usize start = 0;
        while (start <= path_list.size()) {
            const auto end = path_list.find(':', start);
            const auto part = path_list.substr(
                start, end == std::string_view::npos ? path_list.size() - start : end - start);

            pos = 0;
            if (!part.empty()) {
                if (part.size() + 1 >= MaxPathLen) {
                    start = (end == std::string_view::npos) ? path_list.size() + 1 : end + 1;
                    continue;
                }
                for (char c : part) buffer[pos++] = c;
                if (buffer[pos - 1] != '/') {
                    buffer[pos++] = '/';
                }
            }
            if (pos + name.size() >= MaxPathLen) {
                start = (end == std::string_view::npos) ? path_list.size() + 1 : end + 1;
                continue;
            }
            for (char c : name) buffer[pos++] = c;
            buffer[pos] = '\0';

            if (fn(std::string_view{buffer, pos})) {
                return true;
            }
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
        return false;
    }

    template <util::usize MaxVars, util::usize MaxNameLen, util::usize MaxValueLen>
    class EnvTable {
    public:
        void clear() noexcept {
            for (auto& e : entries_) {
                e.used = false;
            }
            count_ = 0;
        }

        bool set(std::string_view name, std::string_view value) noexcept {
            if (name.empty() || value.size() >= MaxValueLen) return false;
            const auto idx = find_or_free(name);
            if (idx >= MaxVars) return false;
            Entry& e = entries_[idx];
            copy_to(e.name, name);
            copy_to(e.value, value);
            if (!e.used) {
                e.used = true;
                ++count_;
            }
            return true;
        }

        bool set_kv(std::string_view kv) noexcept {
            const auto pos = kv.find('=');
            if (pos == std::string_view::npos) return false;
            return set(kv.substr(0, pos), kv.substr(pos + 1));
        }

        std::string_view get(std::string_view name) const noexcept {
            const auto idx = find(name);
            if (idx >= MaxVars) return {};
            return std::string_view{entries_[idx].value.data()};
        }

        bool merge_envp(std::span<const char* const> envp) noexcept {
            for (auto* e : envp) {
                if (!e) continue;
                if (!set_kv(std::string_view{e})) return false;
            }
            return true;
        }

        util::usize size() const noexcept { return count_; }

    private:
        struct Entry {
            std::array<char, MaxNameLen> name{};
            std::array<char, MaxValueLen> value{};
            bool used{false};
        };

        static void copy_to(std::array<char, MaxNameLen>& out, std::string_view in) noexcept {
            const util::usize n = in.size() < (MaxNameLen - 1) ? in.size() : (MaxNameLen - 1);
            for (util::usize i = 0; i < n; ++i) {
                out[i] = in[i];
            }
            out[n] = '\0';
        }

        static void copy_to(std::array<char, MaxValueLen>& out, std::string_view in) noexcept {
            const util::usize n = in.size() < (MaxValueLen - 1) ? in.size() : (MaxValueLen - 1);
            for (util::usize i = 0; i < n; ++i) {
                out[i] = in[i];
            }
            out[n] = '\0';
        }

        util::usize find(std::string_view name) const noexcept {
            for (util::usize i = 0; i < MaxVars; ++i) {
                if (!entries_[i].used) continue;
                if (std::string_view{entries_[i].name.data()} == name) {
                    return i;
                }
            }
            return MaxVars;
        }

        util::usize find_or_free(std::string_view name) const noexcept {
            util::usize free_idx = MaxVars;
            for (util::usize i = 0; i < MaxVars; ++i) {
                if (!entries_[i].used) {
                    if (free_idx == MaxVars) free_idx = i;
                    continue;
                }
                if (std::string_view{entries_[i].name.data()} == name) {
                    return i;
                }
            }
            return free_idx;
        }

        std::array<Entry, MaxVars> entries_{};
        util::usize count_{0};
    };
}
