module;

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

export module fs_path;

import util.core;

export namespace fs {
    struct PathView {
        const char* data{nullptr};
        util::usize size{0};
    };

    constexpr bool is_sep(char c) noexcept { return c == '/' || c == '\\'; }

    // Strip前导分隔符，返回视图
    inline PathView normalize(std::string_view path) noexcept {
        util::usize i = 0;
        while (i < path.size() && is_sep(path[i])) ++i;
        return PathView{path.data() + i, static_cast<util::usize>(path.size() - i)};
    }

    // 从尾部移除分隔符
    inline PathView rstrip_seps(PathView p) noexcept {
        if (!p.data) return {nullptr, 0};
        util::usize end = p.size;
        while (end > 0 && is_sep(p.data[end - 1])) --end;
        return {p.data, end};
    }

    // 返回首个组件与剩余部分
    inline std::pair<PathView, PathView> split_first(PathView p) noexcept {
        if (!p.data || p.size == 0) return {{nullptr, 0}, {nullptr, 0}};
        util::usize i = 0;
        while (i < p.size && is_sep(p.data[i])) ++i;
        const util::usize start = i;
        while (i < p.size && !is_sep(p.data[i])) ++i;
        const util::usize len = i - start;
        util::usize rem = i;
        while (rem < p.size && is_sep(p.data[rem])) ++rem;
        return {{p.data + start, len}, {p.data + rem, p.size - rem}};
    }

    // 返回目录部分与最后一个组件
    inline std::pair<PathView, PathView> split_last(PathView p) noexcept {
        if (!p.data || p.size == 0) return {{nullptr, 0}, {nullptr, 0}};
        util::usize end = p.size;
        while (end > 0 && is_sep(p.data[end - 1])) --end;
        if (end == 0) return {{p.data, 0}, {nullptr, 0}};
        util::usize i = end;
        while (i > 0 && !is_sep(p.data[i - 1])) --i;
        PathView dir{p.data, i};
        while (dir.size > 0 && is_sep(dir.data[dir.size - 1])) --dir.size;
        PathView base{p.data + i, end - i};
        return {dir, base};
    }
}
