module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module service_json;

import util.core;

export namespace service {
    struct JsonWriter {
        std::span<char> out;
        util::usize pos{0};

        constexpr bool push(char c) noexcept {
            if (pos >= out.size()) {
                return false;
            }
            out[pos++] = c;
            return true;
        }

        constexpr bool write(std::span<const char> s) noexcept {
            if (pos + s.size() > out.size()) {
                return false;
            }
            for (char c : s) {
                out[pos++] = c;
            }
            return true;
        }

        constexpr bool write_str(std::string_view sv) noexcept {
            return write(std::span<const char>(sv.data(), sv.size()));
        }

        constexpr bool write_u64(util::u64 value) noexcept {
            char buf[20];
            util::usize n = 0;
            do {
                buf[n++] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0);
            if (pos + n > out.size()) {
                return false;
            }
            for (util::usize i = 0; i < n; ++i) {
                out[pos++] = buf[n - 1 - i];
            }
            return true;
        }

        constexpr bool write_kv(std::string_view key, util::u64 value) noexcept {
            if (!push('"')) return false;
            if (!write_str(key)) return false;
            if (!push('"')) return false;
            if (!push(':')) return false;
            return write_u64(value);
        }
    };
}
