module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <span>
#include <vector>

export module usb.boardlog;

import usb.common;
import usb.replay;

export namespace usb::boardlog {
    enum class LoadError : usb::u8 {
        none = 0,
        syntax,
        invalid_hex,
        missing_descriptor,
        file_io,
    };

    struct LoadResult {
        usb::replay::Trace trace{};
        LoadError error{LoadError::none};
        std::size_t line{0};
        std::size_t imported_steps{0};
        std::size_t skipped_steps{0};

        [[nodiscard]] explicit operator bool() const noexcept {
            return error == LoadError::none;
        }
    };

    inline const char* error_name(LoadError err) noexcept {
        switch (err) {
        case LoadError::none: return "none";
        case LoadError::syntax: return "syntax";
        case LoadError::invalid_hex: return "invalid_hex";
        case LoadError::missing_descriptor: return "missing_descriptor";
        case LoadError::file_io: return "file_io";
        }
        return "unknown";
    }

    namespace detail {
        inline bool is_space(char ch) noexcept {
            return std::isspace(static_cast<unsigned char>(ch)) != 0;
        }

        inline std::string_view trim(std::string_view sv) noexcept {
            while (!sv.empty() && is_space(sv.front())) {
                sv.remove_prefix(1);
            }
            while (!sv.empty() && is_space(sv.back())) {
                sv.remove_suffix(1);
            }
            return sv;
        }

        inline bool parse_hex_uint(std::string_view text, unsigned& out) noexcept {
            text = trim(text);
            if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
                text.remove_prefix(2);
            }
            if (text.empty()) {
                return false;
            }
            unsigned value = 0;
            for (char ch : text) {
                unsigned digit = 0;
                if (ch >= '0' && ch <= '9') {
                    digit = static_cast<unsigned>(ch - '0');
                } else if (ch >= 'a' && ch <= 'f') {
                    digit = static_cast<unsigned>(ch - 'a' + 10);
                } else if (ch >= 'A' && ch <= 'F') {
                    digit = static_cast<unsigned>(ch - 'A' + 10);
                } else {
                    return false;
                }
                value = (value << 4u) | digit;
            }
            out = value;
            return true;
        }

        inline bool split_key_value(std::string_view token,
                                    std::string_view& key,
                                    std::string_view& value) noexcept {
            const auto pos = token.find('=');
            if (pos == std::string_view::npos) {
                return false;
            }
            key = token.substr(0, pos);
            value = token.substr(pos + 1);
            return !key.empty();
        }

        inline std::vector<std::string_view> split_tokens(std::string_view line) {
            std::vector<std::string_view> out{};
            line = trim(line);
            while (!line.empty()) {
                if (line.front() == '#') {
                    break;
                }
                std::size_t count = 0;
                while (count < line.size() && !is_space(line[count])) {
                    ++count;
                }
                out.push_back(line.substr(0, count));
                line.remove_prefix(count);
                line = trim(line);
            }
            return out;
        }

        inline bool parse_hex_byte_token(std::string_view text, usb::u8& out) noexcept {
            unsigned value = 0;
            if (!parse_hex_uint(text, value) || value > 0xFFu) {
                return false;
            }
            out = static_cast<usb::u8>(value);
            return true;
        }

        inline bool parse_desc_bytes(std::string_view line, std::vector<usb::u8>& out) {
            const auto tokens = split_tokens(line);
            bool saw_size = false;
            std::vector<usb::u8> bytes{};
            for (const auto token : tokens) {
                std::string_view key{};
                std::string_view value{};
                if (split_key_value(token, key, value)) {
                    if (key == "size") {
                        saw_size = true;
                    }
                    continue;
                }
                if (!saw_size) {
                    continue;
                }
                usb::u8 byte = 0;
                if (!parse_hex_byte_token(token, byte)) {
                    break;
                }
                bytes.push_back(byte);
            }
            if (!saw_size || bytes.empty()) {
                return false;
            }
            out = std::move(bytes);
            return true;
        }

        inline bool extract_setup_field(const std::vector<std::string_view>& tokens,
                                        std::string_view short_key,
                                        std::string_view long_key,
                                        unsigned& out) noexcept {
            for (const auto token : tokens) {
                std::string_view key{};
                std::string_view value{};
                if (!split_key_value(token, key, value)) {
                    continue;
                }
                if (key == short_key || key == long_key) {
                    return parse_hex_uint(value, out);
                }
            }
            return false;
        }

        inline void append_hex_nibble(std::string& out, unsigned value) {
            static constexpr char kHex[] = "0123456789ABCDEF";
            out.push_back(kHex[value & 0xFu]);
        }

        inline void append_hex_byte(std::string& out, usb::u8 value) {
            append_hex_nibble(out, value >> 4u);
            append_hex_nibble(out, value & 0x0Fu);
        }

        inline void append_hex_word(std::string& out, usb::u16 value) {
            append_hex_byte(out, static_cast<usb::u8>((value >> 8u) & 0xFFu));
            append_hex_byte(out, static_cast<usb::u8>(value & 0xFFu));
        }

        inline void append_hex_blob(std::string& out, std::span<const usb::u8> data) {
            for (auto byte : data) {
                append_hex_byte(out, byte);
            }
        }
    }

    inline LoadResult load_text(std::string_view text) {
        LoadResult out{};
        out.trace.version = 1;
        std::size_t line_no = 0;
        std::vector<usb::u8> dev_desc{};
        std::vector<usb::u8> cfg_desc{};

        const auto fail = [&](LoadError err) {
            out.error = err;
            out.line = line_no;
            return out;
        };

        while (!text.empty()) {
            ++line_no;
            const auto nl = text.find('\n');
            auto line = (nl == std::string_view::npos) ? text : text.substr(0, nl);
            text = (nl == std::string_view::npos) ? std::string_view{} : text.substr(nl + 1);
            line = detail::trim(line);
            if (line.empty()) {
                continue;
            }

            if (line.find("usb: dev_desc") != std::string_view::npos) {
                if (!detail::parse_desc_bytes(line, dev_desc)) {
                    return fail(LoadError::syntax);
                }
                continue;
            }
            if (line.find("usb: cfg_desc") != std::string_view::npos) {
                if (!detail::parse_desc_bytes(line, cfg_desc)) {
                    return fail(LoadError::syntax);
                }
                continue;
            }
            if (line.find("usb: setup") == std::string_view::npos) {
                continue;
            }

            const auto tokens = detail::split_tokens(line);
            unsigned bm = 0;
            unsigned b = 0;
            unsigned wv = 0;
            unsigned wi = 0;
            unsigned wl = 0;
            if (!detail::extract_setup_field(tokens, "bm", "bmRequestType", bm) ||
                !detail::extract_setup_field(tokens, "b", "bRequest", b) ||
                !detail::extract_setup_field(tokens, "wv", "wValue", wv) ||
                !detail::extract_setup_field(tokens, "wi", "wIndex", wi) ||
                !detail::extract_setup_field(tokens, "wl", "wLen", wl)) {
                return fail(LoadError::syntax);
            }

            usb::replay::TraceStep step{};
            step.setup.bm_request_type = static_cast<usb::u8>(bm);
            step.setup.b_request = static_cast<usb::u8>(b);
            step.setup.w_value = static_cast<usb::u16>(wv);
            step.setup.w_index = static_cast<usb::u16>(wi);
            step.setup.w_length = static_cast<usb::u16>(wl);

            const bool is_in = (bm & 0x80u) != 0u;
            if (is_in && b == 0x06u) {
                const auto desc_type = static_cast<usb::u8>((wv >> 8u) & 0xFFu);
                const std::vector<usb::u8>* source = nullptr;
                if (desc_type == 0x01u) {
                    source = &dev_desc;
                } else if (desc_type == 0x02u) {
                    source = &cfg_desc;
                } else {
                    ++out.skipped_steps;
                    continue;
                }
                if (!source || source->empty()) {
                    return fail(LoadError::missing_descriptor);
                }
                step.kind = usb::replay::StepKind::control_in;
                step.flag = false;
                const auto count = std::min<std::size_t>(source->size(), wl);
                step.data.assign(source->begin(), source->begin() + count);
            } else if (bm == 0xA1u && b == 0xFEu && wl == 0x0001u) {
                step.kind = usb::replay::StepKind::control_in;
                step.flag = false;
                step.data = {0x00u};
            } else if (!is_in && wl == 0u) {
                step.kind = usb::replay::StepKind::control_out;
                step.flag = true;
                step.data.clear();
            } else {
                ++out.skipped_steps;
                continue;
            }

            out.trace.steps.push_back(std::move(step));
            ++out.imported_steps;
        }

        return out;
    }

    inline LoadResult load_file(std::string_view path) {
        auto out = LoadResult{};
        std::ifstream file(std::string(path), std::ios::binary);
        if (!file) {
            out.error = LoadError::file_io;
            return out;
        }
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (!file.good() && !file.eof()) {
            out.error = LoadError::file_io;
            return out;
        }
        return load_text(text);
    }

    inline std::string to_text(const usb::replay::Trace& trace) {
        std::string out{};
        out.reserve(256);
        out += "trace usb.replay.v1\n";
        for (const auto& step : trace.steps) {
            switch (step.kind) {
            case usb::replay::StepKind::reset:
                out += "reset\n";
                break;
            case usb::replay::StepKind::connect:
                out += "connect ";
                out += step.flag ? "true\n" : "false\n";
                break;
            case usb::replay::StepKind::control_in:
            case usb::replay::StepKind::control_out: {
                out += (step.kind == usb::replay::StepKind::control_in) ? "control_in " : "control_out ";
                out += "bm=";
                detail::append_hex_byte(out, step.setup.bm_request_type);
                out += " b=";
                detail::append_hex_byte(out, step.setup.b_request);
                out += " wv=";
                detail::append_hex_word(out, step.setup.w_value);
                out += " wi=";
                detail::append_hex_word(out, step.setup.w_index);
                out += " wl=";
                detail::append_hex_word(out, step.setup.w_length);
                out += " zlp=";
                out += step.flag ? "1" : "0";
                out += " data=";
                if (step.data.empty()) {
                    out += "-";
                } else {
                    detail::append_hex_blob(out, std::span<const usb::u8>(step.data.data(), step.data.size()));
                }
                out += "\n";
                break;
            }
            case usb::replay::StepKind::out:
            case usb::replay::StepKind::in: {
                out += (step.kind == usb::replay::StepKind::out) ? "out " : "in ";
                out += "ep=";
                detail::append_hex_byte(out, step.ep);
                out += " zlp=";
                out += step.flag ? "1" : "0";
                out += " data=";
                if (step.data.empty()) {
                    out += "-";
                } else {
                    detail::append_hex_blob(out, std::span<const usb::u8>(step.data.data(), step.data.size()));
                }
                out += "\n";
                break;
            }
            }
        }
        return out;
    }
}

