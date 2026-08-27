#pragma once

#include "mvp_composition.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace charm::mvp::app {
    inline constexpr std::array requirements{
        Requirement{RequirementKey::report, ContractKey::text_sink},
        Requirement{RequirementKey::monotonic_time, ContractKey::clock},
        Requirement{RequirementKey::record_store, ContractKey::block_device},
    };

    enum class RunCode : std::uint8_t {
        ok = 0,
        invalid_context,
        clock_failed,
        unsupported_geometry,
        write_failed,
        flush_failed,
        read_failed,
        verify_failed,
        report_failed,
    };

    struct Evidence {
        std::uint64_t timestamp_ms{0};
        std::uint32_t record_checksum{0};
        std::size_t record_bytes{0};
        bool stored{false};
        bool verified{false};
        bool reported{false};
    };

    struct RunResult {
        RunCode code{RunCode::ok};
        Evidence evidence{};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == RunCode::ok;
        }
    };

    inline constexpr std::size_t record_size = 16;
    inline constexpr std::size_t max_block_size = 512;

    constexpr void encode_u64_le(std::span<std::byte, 8> out, std::uint64_t value) noexcept {
        for (std::size_t index = 0; index < out.size(); ++index) {
            out[index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
        }
    }

    [[nodiscard]] constexpr std::uint32_t checksum(
        std::span<const std::byte> bytes) noexcept {
        std::uint32_t value = 2166136261U;
        for (const auto byte : bytes) {
            value ^= std::to_integer<std::uint8_t>(byte);
            value *= 16777619U;
        }
        return value;
    }

    constexpr void encode_u32_le(std::span<std::byte, 4> out, std::uint32_t value) noexcept {
        for (std::size_t index = 0; index < out.size(); ++index) {
            out[index] = static_cast<std::byte>((value >> (index * 8U)) & 0xffU);
        }
    }

    [[nodiscard]] inline RunResult run(const ResolvedContext& context) noexcept {
        if (!context.valid()) {
            return {RunCode::invalid_context, {}};
        }

        const auto sample = context.monotonic_time->now_ms();
        if (!sample.is_ok()) {
            return {RunCode::clock_failed, {}};
        }

        const auto block_size = context.record_store->block_size();
        if (block_size < record_size || block_size > max_block_size ||
            context.record_store->block_count() == 0) {
            return {RunCode::unsupported_geometry, {.timestamp_ms = sample.milliseconds}};
        }

        std::array<std::byte, max_block_size> write_block{};
        std::array<std::byte, max_block_size> read_block{};
        write_block[0] = std::byte{'C'};
        write_block[1] = std::byte{'H'};
        write_block[2] = std::byte{'R'};
        write_block[3] = std::byte{'M'};
        encode_u64_le(std::span<std::byte, 8>{write_block.data() + 4, 8}, sample.milliseconds);
        const auto record_checksum = checksum(
            std::span<const std::byte>{write_block.data(), 12});
        encode_u32_le(std::span<std::byte, 4>{write_block.data() + 12, 4}, record_checksum);

        Evidence evidence{
            .timestamp_ms = sample.milliseconds,
            .record_checksum = record_checksum,
            .record_bytes = record_size,
        };

        const auto write = context.record_store->write(
            0, std::span<const std::byte>{write_block.data(), static_cast<std::size_t>(block_size)});
        if (!write.is_ok()) {
            return {RunCode::write_failed, evidence};
        }
        evidence.stored = true;

        if (!context.record_store->flush().is_ok()) {
            return {RunCode::flush_failed, evidence};
        }
        const auto read = context.record_store->read(
            0, std::span<std::byte>{read_block.data(), static_cast<std::size_t>(block_size)});
        if (!read.is_ok()) {
            return {RunCode::read_failed, evidence};
        }
        for (std::size_t index = 0; index < record_size; ++index) {
            if (read_block[index] != write_block[index]) {
                return {RunCode::verify_failed, evidence};
            }
        }
        evidence.verified = true;

        constexpr std::string_view report{"charm-mvp: ok\n"};
        const auto transfer = context.report->write(report);
        if (!transfer.is_ok() || transfer.bytes != report.size() ||
            !context.report->flush().is_ok()) {
            return {RunCode::report_failed, evidence};
        }
        evidence.reported = true;
        return {RunCode::ok, evidence};
    }
}
