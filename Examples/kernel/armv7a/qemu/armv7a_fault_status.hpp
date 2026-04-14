#pragma once

#include <cstdint>

struct Armv7aFaultStatusDecode {
    std::uint32_t status_code;
    std::uint32_t domain;
    bool write;
    bool cache_maintenance;
    const char* description;
};

Armv7aFaultStatusDecode armv7a_decode_data_fault_status(std::uint32_t dfsr);
Armv7aFaultStatusDecode armv7a_decode_prefetch_fault_status(std::uint32_t ifsr);
