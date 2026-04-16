#pragma once

#include <cstdint>

struct Armv7aFaultStatusDecode {
    std::uint32_t status_code;
    std::uint32_t domain;
    bool write;
    bool cache_maintenance;
    const char* description;
};

namespace armv7a::fault_status {
constexpr std::uint32_t kFsrStatusLowMask = 0x0Fu;
constexpr std::uint32_t kFsrStatusHighBit = 1u << 10;
constexpr std::uint32_t kFsrDomainShift = 4u;
constexpr std::uint32_t kFsrDomainMask = 0x0Fu;
constexpr std::uint32_t kFsrWrite = 1u << 11;
constexpr std::uint32_t kFsrCacheMaintenance = 1u << 13;

constexpr const char* kDataFaultDescriptions[32] = {
    "vector exception",
    "alignment exception",
    "terminal exception",
    "alignment exception",
    "external abort on linefetch",
    "section translation fault",
    "external abort on linefetch",
    "page translation fault",
    "external abort on non-linefetch",
    "section domain fault",
    "external abort on non-linefetch",
    "page domain fault",
    "external abort on translation",
    "section permission fault",
    "external abort on translation",
    "page permission fault",
    "unknown 16",
    "unknown 17",
    "unknown 18",
    "unknown 19",
    "lock abort",
    "unknown 21",
    "imprecise external abort",
    "unknown 23",
    "dcache parity error",
    "unknown 25",
    "unknown 26",
    "unknown 27",
    "unknown 28",
    "unknown 29",
    "unknown 30",
    "unknown 31",
};

constexpr const char* kPrefetchFaultDescriptions[32] = {
    "unknown 0",
    "unknown 1",
    "debug event",
    "section access flag fault",
    "unknown 4",
    "section translation fault",
    "page access flag fault",
    "page translation fault",
    "external abort on non-linefetch",
    "section domain fault",
    "unknown 10",
    "page domain fault",
    "external abort on translation",
    "section permission fault",
    "external abort on translation",
    "page permission fault",
    "unknown 16",
    "unknown 17",
    "unknown 18",
    "unknown 19",
    "unknown 20",
    "unknown 21",
    "unknown 22",
    "unknown 23",
    "unknown 24",
    "unknown 25",
    "unknown 26",
    "unknown 27",
    "unknown 28",
    "unknown 29",
    "unknown 30",
    "unknown 31",
};

constexpr std::uint32_t status_code(std::uint32_t fsr) noexcept
{
    return (fsr & kFsrStatusLowMask) | ((fsr & kFsrStatusHighBit) >> 6);
}

constexpr Armv7aFaultStatusDecode decode(std::uint32_t fsr,
                                         const char* const (&descriptions)[32],
                                         bool has_write_bits) noexcept
{
    const auto code = status_code(fsr);
    return Armv7aFaultStatusDecode{
        .status_code = code,
        .domain = (fsr >> kFsrDomainShift) & kFsrDomainMask,
        .write = has_write_bits && ((fsr & kFsrWrite) != 0u),
        .cache_maintenance = has_write_bits && ((fsr & kFsrCacheMaintenance) != 0u),
        .description = descriptions[code],
    };
}
} // namespace armv7a::fault_status

constexpr Armv7aFaultStatusDecode armv7a_decode_data_fault_status(std::uint32_t dfsr) noexcept
{
    return armv7a::fault_status::decode(
        dfsr, armv7a::fault_status::kDataFaultDescriptions, true);
}

constexpr Armv7aFaultStatusDecode armv7a_decode_prefetch_fault_status(
    std::uint32_t ifsr) noexcept
{
    return armv7a::fault_status::decode(
        ifsr, armv7a::fault_status::kPrefetchFaultDescriptions, false);
}
