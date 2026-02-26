module;
#include <cstdint>
#include <cstring>

export module alg_text_parse;

#if CHARM_ENABLE_UI_VIVID
export import charm.gfx.color;
#else
export struct rgba {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    constexpr rgba(std::uint8_t rr = 0, std::uint8_t gg = 0, std::uint8_t bb = 0, std::uint8_t aa = 255)
        : r(rr), g(gg), b(bb), a(aa) {}
};
#endif

export namespace alg::text_parse {
    enum class TagKind {
        None,
        BoldOn,
        BoldOff,
        MonoOn,
        MonoOff,
        Color,
        LineBreak
    };

    struct Tag {
        TagKind kind{TagKind::None};
        rgba color{};
        bool reset_color{false};
    };

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

    inline bool parse_tag(const char* tag, Tag& out) noexcept {
        if (!tag || tag[0] == '\0') return false;
        out = Tag{};
        if (std::strcmp(tag, "b") == 0) {
            out.kind = TagKind::BoldOn;
            return true;
        }
        if (std::strcmp(tag, "/b") == 0) {
            out.kind = TagKind::BoldOff;
            return true;
        }
        if (std::strcmp(tag, "mono") == 0 || std::strcmp(tag, "code") == 0) {
            out.kind = TagKind::MonoOn;
            return true;
        }
        if (std::strcmp(tag, "/mono") == 0 || std::strcmp(tag, "/code") == 0) {
            out.kind = TagKind::MonoOff;
            return true;
        }
        if (std::strcmp(tag, "br") == 0) {
            out.kind = TagKind::LineBreak;
            return true;
        }
        if (std::strncmp(tag, "color=", 6) == 0) {
            rgba c{};
            if (!parse_color_hex(tag + 6, c)) return false;
            out.kind = TagKind::Color;
            out.color = c;
            return true;
        }
        if (std::strcmp(tag, "/color") == 0) {
            out.kind = TagKind::Color;
            out.reset_color = true;
            return true;
        }
        return false;
    }
} // namespace alg::text_parse
