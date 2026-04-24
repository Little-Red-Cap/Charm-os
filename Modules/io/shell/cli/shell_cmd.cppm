module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstring>
#include <string_view>

export module shell_cmd;

import shell_core;
import shell_stdio;
import util.core;

export namespace shell {
    struct Command {
        std::string_view name{};
        Result (*run)(Console&, int, std::span<std::string_view>) noexcept { nullptr };
        std::string_view help{};
        const Command* children{nullptr};
        util::usize child_count{0};
        util::u32 caps_required{0};
    };

    constexpr bool is_space(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    inline bool starts_with(std::string_view s, std::string_view prefix) noexcept {
        if (prefix.size() > s.size()) return false;
        return std::memcmp(s.data(), prefix.data(), prefix.size()) == 0;
    }

    template <util::usize MaxArgs>
    struct ArgBuffer {
        std::array<std::string_view, MaxArgs> args{};
        util::usize count{0};
    };

    template <util::usize MaxMatches>
    struct MatchBuffer {
        std::array<std::string_view, MaxMatches> matches{};
        util::usize count{0};

        void push(std::string_view sv) noexcept {
            if (count < MaxMatches) {
                matches[count++] = sv;
            }
        }
    };

    inline const Command* find_command(std::span<const Command> cmds, std::string_view name) noexcept {
        for (const auto& cmd : cmds) {
            if (cmd.name == name) return &cmd;
        }
        return nullptr;
    }

    template <util::usize MaxArgs>
    inline ArgBuffer<MaxArgs> parse_line(std::string_view line) noexcept {
        ArgBuffer<MaxArgs> out{};
        util::usize i = 0;
        while (i < line.size()) {
            while (i < line.size() && is_space(line[i])) ++i;
            if (i >= line.size()) break;
            if (out.count >= MaxArgs) break;
            const char* base = line.data();
            if (line[i] == '"') {
                ++i;
                const util::usize start = i;
                while (i < line.size()) {
                    if (line[i] == '\\' && i + 1 < line.size()) {
                        i += 2;
                        continue;
                    }
                    if (line[i] == '"') break;
                    ++i;
                }
                out.args[out.count++] = std::string_view{base + start, i - start};
                if (i < line.size() && line[i] == '"') ++i;
            } else {
                const util::usize start = i;
                while (i < line.size() && !is_space(line[i])) ++i;
                out.args[out.count++] = std::string_view{base + start, i - start};
            }
        }
        return out;
    }

    inline bool has_caps(util::u32 required, util::u32 caps) noexcept {
        return required == 0 || (caps & required) == required;
    }

    inline Result emit_help(Console& con, std::span<const Command> cmds, util::u32 caps, std::string_view prefix = {}) noexcept {
        for (const auto& cmd : cmds) {
            if (cmd.name.empty()) continue;
            if (!has_caps(cmd.caps_required, caps)) continue;
            if (!prefix.empty()) {
                (void)write(con, prefix);
            }
            (void)write(con, cmd.name);
            if (!cmd.help.empty()) {
                (void)write(con, " - ");
                (void)write(con, cmd.help);
            }
            (void)write(con, "\n");
            if (cmd.children && cmd.child_count > 0) {
                std::array<char, 64> next{};
                const auto n = cmd.name.size() < next.size() - 2 ? cmd.name.size() : next.size() - 2;
                std::memcpy(next.data(), cmd.name.data(), n);
                next[n] = ' ';
                next[n + 1] = '\0';
                (void)emit_help(con, std::span<const Command>(cmd.children, cmd.child_count),
                    caps, std::string_view{next.data(), n + 1});
            }
        }
        return ok();
    }

    inline Result emit_help(Console& con, std::span<const Command> cmds) noexcept {
        return emit_help(con, cmds, 0);
    }

    inline Result run_argv(Console& con, std::span<const Command> cmds, std::span<std::string_view> argv, util::u32 caps) noexcept {
        if (argv.empty()) return ok();
        const auto name = argv[0];
        for (const auto& cmd : cmds) {
            if (cmd.name != name) continue;
            if (!has_caps(cmd.caps_required, caps)) {
                (void)write(con, "permission denied\n");
                return err(Errc::perm);
            }
            if (cmd.children && cmd.child_count > 0) {
                if (argv.size() <= 1) {
                    return emit_help(con, std::span<const Command>(cmd.children, cmd.child_count), caps);
                }
                return run_argv(con,
                    std::span<const Command>(cmd.children, cmd.child_count),
                    argv.subspan(1), caps);
            }
            if (cmd.run) {
                return cmd.run(con, static_cast<int>(argv.size()), argv);
            }
            break;
        }
        (void)write(con, "unknown command\n");
        return err(Errc::noent);
    }

    template <util::usize MaxArgs>
    inline Result run_line(Console& con, std::span<const Command> cmds, std::string_view line, util::u32 caps) noexcept {
        auto parsed = parse_line<MaxArgs>(line);
        const auto argv = std::span<std::string_view>(parsed.args.data(), parsed.count);
        return run_argv(con, cmds, argv, caps);
    }

    inline std::string_view trim_spaces(std::string_view line) noexcept {
        std::size_t start = 0;
        while (start < line.size() && is_space(line[start])) ++start;
        std::size_t end = line.size();
        while (end > start && is_space(line[end - 1])) --end;
        return line.substr(start, end - start);
    }

    inline std::size_t append_escaped(char* dst, std::size_t cap, std::string_view src) noexcept {
        std::size_t n = 0;
        for (char c : src) {
            if (n + 2 >= cap) break;
            if (c == '"' || c == '\\') {
                dst[n++] = '\\';
                dst[n++] = c;
            } else if (c == '\r' || c == '\n') {
                dst[n++] = ' ';
            } else {
                dst[n++] = c;
            }
        }
        return n;
    }

    template <util::usize MaxArgs>
    inline Result run_pipeline(Console& con, std::span<const Command> cmds, std::string_view line, util::u32 caps) noexcept {
        std::array<std::string_view, 8> parts{};
        std::size_t part_count = 0;
        bool in_quote = false;
        std::size_t start = 0;
        for (std::size_t i = 0; i < line.size(); ++i) {
            const char c = line[i];
            if (c == '\\' && in_quote) {
                if (i + 1 < line.size()) ++i;
                continue;
            }
            if (c == '"') {
                in_quote = !in_quote;
                continue;
            }
            if (c == '|' && !in_quote) {
                if (part_count < parts.size()) {
                    parts[part_count++] = trim_spaces(line.substr(start, i - start));
                }
                start = i + 1;
            }
        }
        if (part_count < parts.size()) {
            parts[part_count++] = trim_spaces(line.substr(start));
        }
        if (part_count <= 1) {
            return run_line<MaxArgs>(con, cmds, line, caps);
        }

        std::string_view current = parts[0];
        std::array<char, 384> next_buf{};
        for (std::size_t i = 0; i < part_count; ++i) {
            if (current.empty()) {
                return err(Errc::inval);
            }
            if (i + 1 == part_count) {
                return run_line<MaxArgs>(con, cmds, current, caps);
            }
            struct Capture {
                std::array<char, 256> buf{};
                std::size_t len{0};
                static util::usize write_cb(void* ctx, Buffer b) noexcept {
                    auto* self = static_cast<Capture*>(ctx);
                    if (!self) return 0;
                    const auto cap = self->buf.size();
                    const auto copy = (self->len + b.size < cap) ? b.size : (cap - self->len);
                    std::memcpy(self->buf.data() + self->len, b.data, copy);
                    self->len += copy;
                    return copy;
                }
            } cap{};
            Console cap_con{&cap, &Capture::write_cb};
            auto st = run_line<MaxArgs>(cap_con, cmds, current, caps);
            if (!st) return st;

            const auto next = parts[i + 1];
            std::size_t used = 0;
            if (!next.empty()) {
                const auto copy = (next.size() < next_buf.size() - 1) ? next.size() : (next_buf.size() - 1);
                std::memcpy(next_buf.data(), next.data(), copy);
                used = copy;
            }
            if (used + 2 < next_buf.size()) {
                next_buf[used++] = ' ';
                next_buf[used++] = '"';
            }
            used += append_escaped(next_buf.data() + used, next_buf.size() - used, std::string_view{cap.buf.data(), cap.len});
            if (used + 1 < next_buf.size()) {
                next_buf[used++] = '"';
            }
            next_buf[used] = '\0';
            current = std::string_view{next_buf.data(), used};
        }
        return ok();
    }

    template <util::usize MaxArgs>
    inline Result run_lines(Console& con, std::span<const Command> cmds, std::string_view line, util::u32 caps) noexcept {
        std::size_t start = 0;
        bool in_quote = false;
        enum class Op { seq, and_then, or_else };
        Op op = Op::seq;
        bool last_ok = true;

        auto run_part = [&](std::string_view part) noexcept -> Result {
            if (part.empty()) return ok();
            const bool should_run = (op == Op::seq)
                || (op == Op::and_then && last_ok)
                || (op == Op::or_else && !last_ok);
            if (!should_run) return ok();
            auto st = run_pipeline<MaxArgs>(con, cmds, part, caps);
            last_ok = static_cast<bool>(st);
            return st;
        };

        for (std::size_t i = 0; i < line.size(); ++i) {
            const char c = line[i];
            if (c == '\\' && in_quote) {
                if (i + 1 < line.size()) ++i;
                continue;
            }
            if (c == '"') {
                in_quote = !in_quote;
                continue;
            }
            if (!in_quote && i + 1 < line.size()) {
                const char n = line[i + 1];
                if (c == '&' && n == '&') {
                    const auto part = trim_spaces(line.substr(start, i - start));
                    auto st = run_part(part);
                    if (!st) return st;
                    op = Op::and_then;
                    start = i + 2;
                    ++i;
                    continue;
                }
                if (c == '|' && n == '|') {
                    const auto part = trim_spaces(line.substr(start, i - start));
                    auto st = run_part(part);
                    if (!st) return st;
                    op = Op::or_else;
                    start = i + 2;
                    ++i;
                    continue;
                }
            }
            if (c == ';' && !in_quote) {
                const auto part = trim_spaces(line.substr(start, i - start));
                auto st = run_part(part);
                if (!st) return st;
                op = Op::seq;
                start = i + 1;
            }
        }
        const auto tail = trim_spaces(line.substr(start));
        return run_part(tail);
    }

    template <util::usize MaxArgs>
    inline Result run_lines(Console& con, std::span<const Command> cmds, std::string_view line) noexcept {
        return run_lines<MaxArgs>(con, cmds, line, 0);
    }

    template <util::usize MaxArgs>
    inline Result run_line(Console& con, std::span<const Command> cmds, std::string_view line) noexcept {
        return run_line<MaxArgs>(con, cmds, line, 0);
    }

    template <util::usize MaxMatches>
    inline MatchBuffer<MaxMatches> complete(std::span<const Command> cmds, std::string_view line) noexcept {
        MatchBuffer<MaxMatches> out{};
        util::usize i = 0;
        while (i < line.size() && is_space(line[i])) ++i;
        if (i >= line.size()) return out;
        const util::usize start = i;
        while (i < line.size() && !is_space(line[i])) ++i;
        const std::string_view head{line.data() + start, i - start};
        while (i < line.size() && is_space(line[i])) ++i;
        const bool has_tail = (i < line.size());
        if (!has_tail) {
            for (const auto& cmd : cmds) {
                if (starts_with(cmd.name, head)) {
                    out.push(cmd.name);
                }
            }
            return out;
        }

        const auto* parent = find_command(cmds, head);
        if (!parent || !parent->children || parent->child_count == 0) return out;
        const std::string_view tail{line.data() + i, line.size() - i};
        const auto kids = std::span<const Command>(parent->children, parent->child_count);
        for (const auto& cmd : kids) {
            if (starts_with(cmd.name, tail)) {
                out.push(cmd.name);
            }
        }
        return out;
    }
}
