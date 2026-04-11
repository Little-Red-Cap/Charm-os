module;

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <span>
#include <vector>

export module usb.replay;

import usb.common;
import usb.mock;

export namespace usb::replay {
    enum class StepKind : usb::u8 {
        reset,
        connect,
        control_in,
        control_out,
        clear_stall,
        out,
        in,
    };

    struct Step {
        StepKind kind{StepKind::reset};
        usb::SetupPacket setup{};
        usb::u8 ep{0};
        std::span<const usb::u8> data{};
        bool flag{false};
    };

    struct TraceStep {
        StepKind kind{StepKind::reset};
        usb::SetupPacket setup{};
        usb::u8 ep{0};
        std::vector<usb::u8> data{};
        bool flag{false};
    };

    inline constexpr std::string_view kTraceHeaderTag = "trace";
    inline constexpr std::string_view kTraceFormatV1 = "usb.replay.v1";

    struct Trace {
        usb::u32 version{1};
        std::vector<TraceStep> steps{};
    };

    struct Hooks {
        bool (*pump_in)(void* ctx, usb::mock::Session& session, usb::u8 ep) noexcept { nullptr };
        void* ctx{nullptr};
    };

    enum class LoadError : usb::u8 {
        none = 0,
        missing_header,
        unsupported_version,
        syntax,
        unknown_step,
        missing_field,
        invalid_number,
        invalid_hex,
        invalid_bool,
        file_io,
    };

    struct LoadResult {
        Trace trace{};
        LoadError error{LoadError::none};
        std::size_t line{0};

        [[nodiscard]] explicit operator bool() const noexcept { return error == LoadError::none; }
    };

    enum class Error : usb::u8 {
        none = 0,
        feed_failed,
        missing_in,
        unexpected_endpoint,
        ack_failed,
        payload_mismatch,
        zlp_mismatch,
    };

    struct Result {
        Error error{Error::none};
        std::size_t step_index{0};

        [[nodiscard]] explicit operator bool() const noexcept { return error == Error::none; }
    };


    struct FileRunResult {
        LoadResult load{};
        Result replay{};

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(load) && static_cast<bool>(replay);
        }
    };

    struct InTransaction {
        std::vector<usb::u8> data{};
        bool saw_zlp{false};
    };

    inline const char* load_error_name(LoadError err) noexcept {
        switch (err) {
        case LoadError::none: return "none";
        case LoadError::missing_header: return "missing_header";
        case LoadError::unsupported_version: return "unsupported_version";
        case LoadError::syntax: return "syntax";
        case LoadError::unknown_step: return "unknown_step";
        case LoadError::missing_field: return "missing_field";
        case LoadError::invalid_number: return "invalid_number";
        case LoadError::invalid_hex: return "invalid_hex";
        case LoadError::invalid_bool: return "invalid_bool";
        case LoadError::file_io: return "file_io";
        }
        return "unknown";
    }

    inline const char* error_name(Error err) noexcept {
        switch (err) {
        case Error::none: return "none";
        case Error::feed_failed: return "feed_failed";
        case Error::missing_in: return "missing_in";
        case Error::unexpected_endpoint: return "unexpected_endpoint";
        case Error::ack_failed: return "ack_failed";
        case Error::payload_mismatch: return "payload_mismatch";
        case Error::zlp_mismatch: return "zlp_mismatch";
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

        template <typename UInt>
        inline bool parse_hex_uint(std::string_view text, UInt& out) noexcept {
            text = trim(text);
            if (text.starts_with("0x") || text.starts_with("0X")) {
                text.remove_prefix(2);
            }
            if (text.empty()) {
                return false;
            }
            UInt value{};
            const auto* first = text.data();
            const auto* last = text.data() + text.size();
            const auto res = std::from_chars(first, last, value, 16);
            if (res.ec != std::errc{} || res.ptr != last) {
                return false;
            }
            out = value;
            return true;
        }

        inline bool parse_bool(std::string_view text, bool& out) noexcept {
            text = trim(text);
            if (text == "1" || text == "true" || text == "on") {
                out = true;
                return true;
            }
            if (text == "0" || text == "false" || text == "off") {
                out = false;
                return true;
            }
            return false;
        }

        inline bool parse_hex_bytes(std::string_view text, std::vector<usb::u8>& out) {
            text = trim(text);
            if (text.empty() || text == "-") {
                out.clear();
                return true;
            }

            std::string cleaned{};
            cleaned.reserve(text.size());
            for (const auto ch : text) {
                if (std::isxdigit(static_cast<unsigned char>(ch)) != 0) {
                    cleaned.push_back(ch);
                    continue;
                }
                switch (ch) {
                case ',':
                case ':':
                case '_':
                case '-':
                    break;
                default:
                    return false;
                }
            }

            if ((cleaned.size() % 2u) != 0u) {
                return false;
            }

            out.clear();
            out.reserve(cleaned.size() / 2u);
            for (std::size_t index = 0; index < cleaned.size(); index += 2) {
                unsigned value = 0;
                const std::string_view byte_text{cleaned.data() + index, 2};
                if (!parse_hex_uint(byte_text, value)) {
                    return false;
                }
                out.push_back(static_cast<usb::u8>(value));
            }
            return true;
        }
    }

    inline Result drain_in_transaction(usb::mock::Session& session,
                                       usb::u8 ep,
                                       InTransaction& tx,
                                       const Hooks& hooks,
                                       std::size_t step_index) {
        tx.data.clear();
        tx.saw_zlp = false;

        for (;;) {
            if (!session.has_pending_in()) {
                if (!hooks.pump_in || !hooks.pump_in(hooks.ctx, session, ep)) {
                    break;
                }
            }

            auto pkt = session.poll_in();
            if (!pkt) {
                break;
            }
            if (pkt->ep != ep) {
                return Result{Error::unexpected_endpoint, step_index};
            }

            if (pkt->zlp) {
                tx.saw_zlp = true;
            } else {
                tx.data.insert(tx.data.end(), pkt->data.begin(), pkt->data.end());
            }

            if (!session.ack_in(pkt->ep, pkt->data.size(), pkt->zlp)) {
                return Result{Error::ack_failed, step_index};
            }
        }

        return {};
    }

    template <typename StepRange>
    inline Result run_steps(usb::mock::Session& session,
                            const StepRange& script,
                            const Hooks& hooks = {}) {
        InTransaction tx{};

        for (std::size_t index = 0; index < script.size(); ++index) {
            const auto& step = script[index];
            const auto expected = std::span<const usb::u8>(step.data.data(), step.data.size());
            switch (step.kind) {
            case StepKind::reset:
                session.signal_reset();
                break;

            case StepKind::connect:
                session.signal_connect(step.flag);
                break;

            case StepKind::control_in: {
                session.feed_setup(step.setup);
                const auto res = drain_in_transaction(session, 0x80, tx, {}, index);
                if (!res) {
                    return res;
                }
                if (tx.data.size() != expected.size() ||
                    !std::equal(tx.data.begin(), tx.data.end(), expected.begin(), expected.end())) {
                    return Result{Error::payload_mismatch, index};
                }
                if (tx.saw_zlp != step.flag) {
                    return Result{Error::zlp_mismatch, index};
                }
                break;
            }

            case StepKind::control_out: {
                session.feed_setup(step.setup);
                if (!expected.empty() && !session.feed_out(0x00, expected)) {
                    return Result{Error::feed_failed, index};
                }
                const auto res = drain_in_transaction(session, 0x80, tx, {}, index);
                if (!res) {
                    return res;
                }
                if (!tx.data.empty()) {
                    return Result{Error::payload_mismatch, index};
                }
                if (tx.saw_zlp != step.flag) {
                    return Result{Error::zlp_mismatch, index};
                }
                break;
            }

            case StepKind::clear_stall: {
                usb::SetupPacket setup{};
                setup.bm_request_type = 0x02;
                setup.b_request = static_cast<usb::u8>(usb::StandardRequest::clear_feature);
                setup.w_value = 0;
                setup.w_index = step.ep;
                setup.w_length = 0;
                session.feed_setup(setup);
                const auto res = drain_in_transaction(session, 0x80, tx, {}, index);
                if (!res) {
                    return res;
                }
                if (!tx.data.empty()) {
                    return Result{Error::payload_mismatch, index};
                }
                if (!tx.saw_zlp) {
                    return Result{Error::zlp_mismatch, index};
                }
                break;
            }

            case StepKind::out:
                if (!session.feed_out(step.ep, expected)) {
                    return Result{Error::feed_failed, index};
                }
                break;

            case StepKind::in: {
                const auto res = drain_in_transaction(session, step.ep, tx, hooks, index);
                if (!res) {
                    return res;
                }
                if (tx.data.size() != expected.size() ||
                    !std::equal(tx.data.begin(), tx.data.end(), expected.begin(), expected.end())) {
                    return Result{Error::payload_mismatch, index};
                }
                if (tx.saw_zlp != step.flag) {
                    return Result{Error::zlp_mismatch, index};
                }
                break;
            }
            }
        }

        return {};
    }

    inline Result run(usb::mock::Session& session,
                      std::span<const Step> script,
                      const Hooks& hooks = {}) {
        return run_steps(session, script, hooks);
    }

    inline Result run(usb::mock::Session& session,
                      const Trace& trace,
                      const Hooks& hooks = {}) {
        return run_steps(session, trace.steps, hooks);
    }

    inline LoadResult load_text(std::string_view text) {
        LoadResult out{};
        std::size_t line_no = 0;
        bool saw_header = false;

        auto fail = [&](LoadError err) {
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
            if (line.empty() || line.front() == '#') {
                continue;
            }

            const auto tokens = detail::split_tokens(line);
            if (tokens.empty()) {
                continue;
            }

            if (!saw_header) {
                if (tokens[0] != kTraceHeaderTag) {
                    return fail(LoadError::missing_header);
                }
                if (tokens.size() != 2 || tokens[1] != kTraceFormatV1) {
                    return fail(LoadError::unsupported_version);
                }
                out.trace.version = 1;
                saw_header = true;
                continue;
            }

            TraceStep step{};
            bool has_ep = false;
            bool has_bm = false;
            bool has_b = false;
            bool has_wv = false;
            bool has_wi = false;
            bool has_wl = false;
            unsigned bm = 0;
            unsigned b = 0;
            unsigned wv = 0;
            unsigned wi = 0;
            unsigned wl = 0;

            if (tokens[0] == "reset") {
                step.kind = StepKind::reset;
            } else if (tokens[0] == "connect") {
                step.kind = StepKind::connect;
            } else if (tokens[0] == "control_in") {
                step.kind = StepKind::control_in;
            } else if (tokens[0] == "control_out") {
                step.kind = StepKind::control_out;
            } else if (tokens[0] == "clear_stall") {
                step.kind = StepKind::clear_stall;
            } else if (tokens[0] == "out") {
                step.kind = StepKind::out;
            } else if (tokens[0] == "in") {
                step.kind = StepKind::in;
            } else {
                return fail(LoadError::unknown_step);
            }

            for (std::size_t i = 1; i < tokens.size(); ++i) {
                std::string_view key{};
                std::string_view value{};
                if (!detail::split_key_value(tokens[i], key, value)) {
                    if (step.kind == StepKind::connect) {
                        bool flag = false;
                        if (!detail::parse_bool(tokens[i], flag)) {
                            return fail(LoadError::syntax);
                        }
                        step.flag = flag;
                        continue;
                    }
                    return fail(LoadError::syntax);
                }

                if (key == "bm") {
                    has_bm = detail::parse_hex_uint(value, bm);
                    if (!has_bm) return fail(LoadError::invalid_number);
                } else if (key == "b") {
                    has_b = detail::parse_hex_uint(value, b);
                    if (!has_b) return fail(LoadError::invalid_number);
                } else if (key == "wv") {
                    has_wv = detail::parse_hex_uint(value, wv);
                    if (!has_wv) return fail(LoadError::invalid_number);
                } else if (key == "wi") {
                    has_wi = detail::parse_hex_uint(value, wi);
                    if (!has_wi) return fail(LoadError::invalid_number);
                } else if (key == "wl") {
                    has_wl = detail::parse_hex_uint(value, wl);
                    if (!has_wl) return fail(LoadError::invalid_number);
                } else if (key == "ep") {
                    unsigned ep = 0;
                    if (!detail::parse_hex_uint(value, ep)) {
                        return fail(LoadError::invalid_number);
                    }
                    step.ep = static_cast<usb::u8>(ep);
                    has_ep = true;
                } else if (key == "data") {
                    if (!detail::parse_hex_bytes(value, step.data)) {
                        return fail(LoadError::invalid_hex);
                    }
                } else if (key == "zlp" || key == "flag") {
                    if (!detail::parse_bool(value, step.flag)) {
                        return fail(LoadError::invalid_bool);
                    }
                } else {
                    return fail(LoadError::syntax);
                }
            }

            switch (step.kind) {
            case StepKind::reset:
                break;
            case StepKind::connect:
                break;
            case StepKind::control_in:
            case StepKind::control_out:
                if (!(has_bm && has_b && has_wv && has_wi && has_wl)) {
                    return fail(LoadError::missing_field);
                }
                step.setup.bm_request_type = static_cast<usb::u8>(bm);
                step.setup.b_request = static_cast<usb::u8>(b);
                step.setup.w_value = static_cast<usb::u16>(wv);
                step.setup.w_index = static_cast<usb::u16>(wi);
                step.setup.w_length = static_cast<usb::u16>(wl);
                break;
            case StepKind::clear_stall:
            case StepKind::out:
            case StepKind::in:
                if (!has_ep) {
                    return fail(LoadError::missing_field);
                }
                break;
            }

            out.trace.steps.push_back(std::move(step));
        }

        if (!saw_header) {
            return fail(LoadError::missing_header);
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
        if (!file.good() and not file.eof()) {
            out.error = LoadError::file_io;
            return out;
        }
        return load_text(text);
    }


    inline FileRunResult run_file(usb::mock::Session& session,
                                  std::string_view path,
                                  const Hooks& hooks = {}) {
        FileRunResult out{};
        out.load = load_file(path);
        if (!out.load) {
            return out;
        }
        out.replay = run(session, out.load.trace, hooks);
        return out;
    }

}
