#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace charm::dev_loader {

enum class PacketConsoleBuildCode : std::uint8_t {
    ok,
    invalid_argument,
    input_empty,
    bytes_per_line_zero,
    output_too_small,
};

struct PacketConsoleBuildConfig {
    std::span<const std::byte> stream{};
    std::uint32_t bytes_per_line{48};
    std::string_view command_prefix{"dev packet ingest "};
};

struct PacketConsoleBuildResult {
    PacketConsoleBuildCode code{PacketConsoleBuildCode::ok};
    std::uint32_t bytes_written{0};
    std::uint32_t line_count{0};
    std::uint32_t max_payload_bytes_per_line{0};
};

[[nodiscard]] constexpr std::string_view packet_console_build_code_name(PacketConsoleBuildCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case PacketConsoleBuildCode::ok:
            return "ok"sv;
        case PacketConsoleBuildCode::invalid_argument:
            return "invalid_argument"sv;
        case PacketConsoleBuildCode::input_empty:
            return "input_empty"sv;
        case PacketConsoleBuildCode::bytes_per_line_zero:
            return "bytes_per_line_zero"sv;
        case PacketConsoleBuildCode::output_too_small:
            return "output_too_small"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr char packet_console_hex_nibble(std::uint8_t value) noexcept {
    constexpr char kHex[] = "0123456789ABCDEF";
    return kHex[value & 0x0FU];
}

[[nodiscard]] constexpr bool packet_console_config_valid(const PacketConsoleBuildConfig& config) noexcept {
    return (config.stream.empty() || config.stream.data() != nullptr) &&
           config.command_prefix.data() != nullptr &&
           !config.command_prefix.empty();
}

[[nodiscard]] constexpr std::uint32_t packet_console_line_count(const PacketConsoleBuildConfig& config) noexcept {
    if (config.stream.empty() || config.bytes_per_line == 0U) {
        return 0;
    }
    const auto size = static_cast<std::uint32_t>(config.stream.size());
    return (size + config.bytes_per_line - 1U) / config.bytes_per_line;
}

[[nodiscard]] constexpr std::uint32_t packet_console_required_size(const PacketConsoleBuildConfig& config) noexcept {
    const auto lines = packet_console_line_count(config);
    if (lines == 0U || config.command_prefix.empty()) {
        return 0;
    }

    const auto prefix = static_cast<std::uint32_t>(config.command_prefix.size());
    const auto full_lines = static_cast<std::uint32_t>(config.stream.size()) / config.bytes_per_line;
    const auto remainder = static_cast<std::uint32_t>(config.stream.size()) % config.bytes_per_line;
    const auto full_line_size = prefix + (config.bytes_per_line * 2U) + 1U;
    const auto partial_line_size = remainder == 0U ? 0U : prefix + (remainder * 2U) + 1U;
    return (full_lines * full_line_size) + partial_line_size;
}

[[nodiscard]] inline PacketConsoleBuildResult packet_console_build(const PacketConsoleBuildConfig& config,
                                                                   std::span<char> output) noexcept {
    if (!packet_console_config_valid(config) || output.data() == nullptr) {
        return {.code = PacketConsoleBuildCode::invalid_argument};
    }
    if (config.stream.empty()) {
        return {.code = PacketConsoleBuildCode::input_empty};
    }
    if (config.bytes_per_line == 0U) {
        return {.code = PacketConsoleBuildCode::bytes_per_line_zero};
    }

    const auto required = packet_console_required_size(config);
    if (required == 0U || required > output.size()) {
        return {.code = PacketConsoleBuildCode::output_too_small};
    }

    std::uint32_t cursor = 0;
    std::uint32_t input = 0;
    std::uint32_t lines = 0;
    const auto total = static_cast<std::uint32_t>(config.stream.size());

    while (input < total) {
        for (const char ch : config.command_prefix) {
            output[cursor++] = ch;
        }

        const auto remaining = total - input;
        const auto count = remaining < config.bytes_per_line ? remaining : config.bytes_per_line;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto value = static_cast<std::uint8_t>(config.stream[input + i]);
            output[cursor++] = packet_console_hex_nibble(value >> 4U);
            output[cursor++] = packet_console_hex_nibble(value);
        }
        output[cursor++] = '\n';
        input += count;
        ++lines;
    }

    return PacketConsoleBuildResult{
        .code = PacketConsoleBuildCode::ok,
        .bytes_written = cursor,
        .line_count = lines,
        .max_payload_bytes_per_line = config.bytes_per_line,
    };
}

} // namespace charm::dev_loader
