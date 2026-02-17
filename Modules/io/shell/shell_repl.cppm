module;

#include <array>
#include <cstring>
#include <span>
#include <string_view>

export module shell_repl;

import util.core;
import shell_core;

export namespace shell {
    struct Prompt {
        std::string_view text{};
    };

    template <util::usize MaxLines>
    struct History {
        std::array<std::string_view, MaxLines> lines{};
        util::usize count{0};

        void push(std::string_view line) noexcept {
            if (count < MaxLines) {
                lines[count++] = line;
            } else if (MaxLines > 0) {
                for (util::usize i = 1; i < MaxLines; ++i) {
                    lines[i - 1] = lines[i];
                }
                lines[MaxLines - 1] = line;
            }
        }

        std::string_view last() const noexcept {
            if (count == 0) return {};
            return lines[count - 1];
        }
    };

    inline util::usize format_prompt(Prompt prompt, std::span<char> out) noexcept {
        if (out.empty()) return 0;
        const auto n = (prompt.text.size() + 1 < out.size()) ? prompt.text.size() : (out.size() - 1);
        std::memcpy(out.data(), prompt.text.data(), n);
        out[n] = '\0';
        return n;
    }
}
