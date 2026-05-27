#pragma once

#include "charm_dev_loader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace charm::dev_loader {

enum class CommandKind : std::uint8_t {
    none,
    help,
    status,
    begin,
    fill,
    verify,
    launch_dry_run,
    abort,
    usage_error,
    unknown,
};

enum class CommandCode : std::uint8_t {
    ok,
    invalid_argument,
    unknown_command,
};

struct CommandResult {
    CommandKind kind{CommandKind::none};
    CommandCode command_code{CommandCode::ok};
    Result session{};
    ImageManifest manifest{};
    std::uint32_t cursor{0};
    bool active{false};
};

[[nodiscard]] constexpr std::string_view command_kind_name(CommandKind kind) noexcept {
    using namespace std::literals::string_view_literals;
    switch (kind) {
        case CommandKind::none:
            return "none"sv;
        case CommandKind::help:
            return "help"sv;
        case CommandKind::status:
            return "status"sv;
        case CommandKind::begin:
            return "begin"sv;
        case CommandKind::fill:
            return "fill"sv;
        case CommandKind::verify:
            return "verify"sv;
        case CommandKind::launch_dry_run:
            return "launch_dry_run"sv;
        case CommandKind::abort:
            return "abort"sv;
        case CommandKind::usage_error:
            return "usage_error"sv;
        case CommandKind::unknown:
            return "unknown"sv;
    }
    return "unknown"sv;
}

class CommandRuntime {
public:
    explicit constexpr CommandRuntime(Storage storage) noexcept : storage_(storage), receiver_(storage) {}

    [[nodiscard]] CommandResult handle(std::string_view line) noexcept {
        const auto cmd = trim_left(line);
        if (cmd.empty()) {
            return make_result(CommandKind::none);
        }
        if (cmd == "help") {
            return make_result(CommandKind::help);
        }
        if (cmd == "dev status") {
            return make_result(CommandKind::status);
        }
        if (cmd.starts_with("dev begin ")) {
            return begin(cmd.substr(10));
        }
        if (cmd.starts_with("dev fill ")) {
            return fill(cmd.substr(9));
        }
        if (cmd == "dev verify") {
            return make_result(CommandKind::verify, receiver_.verify());
        }
        if (cmd == "dev launch dry-run") {
            return make_result(CommandKind::launch_dry_run, receiver_.mark_launch_ready());
        }
        if (cmd == "dev abort") {
            receiver_.abort();
            return make_result(CommandKind::abort);
        }
        return make_result(CommandKind::unknown, receiver_.current(), CommandCode::unknown_command);
    }

    [[nodiscard]] CommandResult status() const noexcept {
        return make_result(CommandKind::status);
    }

private:
    [[nodiscard]] static constexpr std::string_view trim_left(std::string_view sv) noexcept {
        while (!sv.empty() && sv.front() == ' ') {
            sv.remove_prefix(1);
        }
        return sv;
    }

    [[nodiscard]] static constexpr std::pair<std::string_view, std::string_view> split_token(
        std::string_view sv) noexcept {
        sv = trim_left(sv);
        const auto pos = sv.find(' ');
        if (pos == std::string_view::npos) {
            return {sv, {}};
        }
        return {sv.substr(0, pos), trim_left(sv.substr(pos + 1U))};
    }

    [[nodiscard]] static std::optional<std::uint32_t> parse_u32(std::string_view sv) noexcept {
        sv = trim_left(sv);
        if (sv.empty()) {
            return std::nullopt;
        }
        std::uint32_t value = 0;
        int base = 10;
        if (sv.starts_with("0x") || sv.starts_with("0X")) {
            base = 16;
            sv.remove_prefix(2);
        }
        if (sv.empty()) {
            return std::nullopt;
        }
        for (const char ch : sv) {
            std::uint32_t digit = 0;
            if (ch >= '0' && ch <= '9') {
                digit = static_cast<std::uint32_t>(ch - '0');
            } else if (base == 16 && ch >= 'a' && ch <= 'f') {
                digit = static_cast<std::uint32_t>(ch - 'a' + 10);
            } else if (base == 16 && ch >= 'A' && ch <= 'F') {
                digit = static_cast<std::uint32_t>(ch - 'A' + 10);
            } else {
                return std::nullopt;
            }
            if (digit >= static_cast<std::uint32_t>(base)) {
                return std::nullopt;
            }
            value = (base == 16) ? ((value << 4U) | digit) : ((value * 10U) + digit);
        }
        return value;
    }

    [[nodiscard]] CommandResult make_result(CommandKind kind,
                                            Result result,
                                            CommandCode command_code = CommandCode::ok) const noexcept {
        return CommandResult{
            .kind = kind,
            .command_code = command_code,
            .session = result,
            .manifest = receiver_.manifest(),
            .cursor = receiver_.cursor(),
            .active = receiver_.active(),
        };
    }

    [[nodiscard]] CommandResult make_result(CommandKind kind,
                                            CommandCode command_code = CommandCode::ok) const noexcept {
        return make_result(kind, receiver_.current(), command_code);
    }

    [[nodiscard]] CommandResult usage_error() const noexcept {
        return make_result(CommandKind::usage_error, receiver_.current(), CommandCode::invalid_argument);
    }

    [[nodiscard]] CommandResult begin(std::string_view args) noexcept {
        auto [size_token, crc_token] = split_token(args);
        const auto size = parse_u32(size_token);
        if (!size || *size == 0U || *size > storage_.capacity_bytes || storage_.base_address == 0U) {
            return usage_error();
        }
        std::uint32_t crc = 0;
        std::uint32_t flags = kFlagSkipCrc;
        if (!crc_token.empty()) {
            const auto parsed_crc = parse_u32(crc_token);
            if (!parsed_crc) {
                return usage_error();
            }
            crc = *parsed_crc;
            flags = 0;
        }

        return make_result(CommandKind::begin, receiver_.begin(*size, crc, flags == 0U));
    }

    [[nodiscard]] CommandResult fill(std::string_view args) noexcept {
        auto [byte_token, count_token] = split_token(args);
        const auto value = parse_u32(byte_token);
        const auto count = parse_u32(count_token);
        if (!value || !count || *value > 0xFFU || *count == 0U) {
            return usage_error();
        }

        std::array<std::byte, 256> chunk{};
        chunk.fill(static_cast<std::byte>(*value & 0xFFU));
        std::uint32_t remaining = *count;
        Result result = receiver_.current();
        while (remaining != 0U) {
            const std::uint32_t n =
                remaining < chunk.size() ? remaining : static_cast<std::uint32_t>(chunk.size());
            result = receiver_.write(std::span<const std::byte>{chunk.data(), n});
            if (result.code != Code::ok) {
                return make_result(CommandKind::fill, result);
            }
            remaining -= n;
        }
        return make_result(CommandKind::fill, receiver_.current());
    }

    Storage storage_{};
    BinaryReceiveRuntime receiver_;
};

} // namespace charm::dev_loader
