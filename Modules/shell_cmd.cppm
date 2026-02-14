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
        std::span<const Command> children{};
        util::u32 caps_required{0};
    };

    constexpr bool is_space(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    template <util::usize MaxArgs>
    struct ArgBuffer {
        std::array<std::string_view, MaxArgs> args{};
        util::usize count{0};
    };

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
            if (!cmd.children.empty()) {
                std::array<char, 64> next{};
                const auto n = cmd.name.size() < next.size() - 2 ? cmd.name.size() : next.size() - 2;
                std::memcpy(next.data(), cmd.name.data(), n);
                next[n] = ' ';
                next[n + 1] = '\0';
                (void)emit_help(con, cmd.children, caps, std::string_view{next.data(), n + 1});
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
                return err(Errno::perm);
            }
            if (!cmd.children.empty()) {
                if (argv.size() <= 1) {
                    return emit_help(con, cmd.children, caps);
                }
                return run_argv(con, cmd.children, argv.subspan(1), caps);
            }
            if (cmd.run) {
                return cmd.run(con, static_cast<int>(argv.size()), argv);
            }
            break;
        }
        (void)write(con, "unknown command\n");
        return err(Errno::noent);
    }

    template <util::usize MaxArgs>
    inline Result run_line(Console& con, std::span<const Command> cmds, std::string_view line, util::u32 caps) noexcept {
        auto parsed = parse_line<MaxArgs>(line);
        const auto argv = std::span<std::string_view>(parsed.args.data(), parsed.count);
        return run_argv(con, cmds, argv, caps);
    }

    template <util::usize MaxArgs>
    inline Result run_line(Console& con, std::span<const Command> cmds, std::string_view line) noexcept {
        return run_line<MaxArgs>(con, cmds, line, 0);
    }
}
