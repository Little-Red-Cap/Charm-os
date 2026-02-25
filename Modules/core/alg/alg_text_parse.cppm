module;

export module alg_text_parse;

import charm.gfx.color;

export namespace alg::text_parse {
    inline bool parse_hex_u8(const char* p, std::uint8_t& out) noexcept {
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };
        const int hi = hex(p[0]);
        const int lo = hex(p[1]);
        if (hi < 0 || lo < 0) return false;
        out = static_cast<std::uint8_t>((hi << 4) | lo);
        return true;
    }

    inline bool parse_color_hex(const char* tag, rgba& color) noexcept {
        const char* p = tag;
        if (p[0] == '#') ++p;
        if (!p[0] || !p[1] || !p[2] || !p[3] || !p[4] || !p[5]) return false;
        std::uint8_t r{}, g{}, b{};
        if (!parse_hex_u8(p, r)) return false;
        if (!parse_hex_u8(p + 2, g)) return false;
        if (!parse_hex_u8(p + 4, b)) return false;
        color = {r, g, b, 255};
        return true;
    }
} // namespace alg::text_parse
