#pragma once

#include "charm_app_store.hpp"
#include "charm_app_api.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace resident_elf_qemu {

struct VirtualBackendContract {
    struct Identity {
        const char* runtime_domain;
        const char* machine;
        const char* cpu;
        const char* capabilities;
        const char* storage;
        const char* afe;
    };

    struct Scope {
        const char* proves;
        const char* does_not_prove;
    };

    struct Time {
        const char* kind;
        std::uint32_t start_ms;
        std::uint32_t step_ms;
        bool reset_per_run;
    };

    struct Display {
        const char* kind;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t stride_bytes;
        const char* format;
        std::uint32_t frame_bytes;
        const char* evidence;
    };

    struct Input {
        const char* kind;
        std::uint32_t sample_count;
        std::uint32_t pointer_max_x;
        std::uint32_t pointer_max_y;
        bool wraps;
        const char* evidence;
    };

    struct StorageMedia {
        const char* kind;
        std::uint32_t file_count;
        int fd_base;
        std::uint32_t fd_slots;
        const char* write_policy;
        const char* evidence;
    };

    struct AppExit {
        const char* kind;
        bool overrides_return;
    };

    Identity identity;
    Scope scope;
    Time time;
    Display display;
    Input input;
    StorageMedia storage_media;
    AppExit app_exit;
};

struct VirtualCapabilityCounters {
    std::uint32_t console_bytes{0};
    std::uint32_t time_now{0};
    std::uint32_t display_describe{0};
    std::uint32_t display_present{0};
    std::uint32_t display_checksum{0};
    std::uint32_t display_checksum_total{0};
    std::uint32_t display_hash{0};
    std::uint32_t display_hash_total{0};
    std::uint32_t display_frame_index{0};
    std::uint32_t input_poll{0};
    std::uint32_t input_checksum{0};
    std::uint32_t input_last_x{0};
    std::uint32_t input_last_y{0};
    std::uint32_t input_last_down{0};
    std::uint32_t storage_open{0};
    std::uint32_t storage_read{0};
    std::uint32_t storage_write{0};
    std::uint32_t storage_close{0};
    std::uint32_t storage_bytes{0};
    std::uint32_t afe_configure{0};
    std::uint32_t afe_read{0};
    std::uint32_t app_exit{0};
};

struct VirtualStoreMedia {
    const std::byte* data{nullptr};
    std::size_t size{0};
};

struct VirtualStoreMediaCounters {
    std::uint32_t read_calls{0};
    std::uint32_t read_bytes{0};
    std::uint32_t read_failures{0};
};

void backend_init() noexcept;
void write_byte(char ch) noexcept;
void write(const char* text) noexcept;
void write(std::string_view text) noexcept;
void write_hex32(std::uint32_t value) noexcept;
void write_dec(std::uint32_t value) noexcept;
void write_signed(int value) noexcept;
void log_line(const char* text) noexcept;
void log_view(std::string_view text) noexcept;

void reset_capability_counters() noexcept;
const VirtualCapabilityCounters& capability_counters() noexcept;
const VirtualBackendContract& backend_contract() noexcept;
void log_capability_counters(std::string_view name) noexcept;
CharmAppApi make_virtual_app_api() noexcept;
bool log_backend_self_check() noexcept;
bool log_backend_reset_self_check() noexcept;
bool probe_unsupported_capabilities() noexcept;
void log_backend_identity() noexcept;
void log_backend_capabilities() noexcept;
void log_backend_scope() noexcept;
void log_backend_contract() noexcept;

void reset_store_media_counters() noexcept;
const VirtualStoreMediaCounters& store_media_counters() noexcept;
charm::app_abi::AppStoreReader make_virtual_store_reader(VirtualStoreMedia& media) noexcept;
void log_store_media_counters(const VirtualStoreMedia& media) noexcept;

} // namespace resident_elf_qemu
