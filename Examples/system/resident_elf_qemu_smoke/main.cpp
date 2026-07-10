#include "charm_app_elf_probe.hpp"
#include "charm_app_received_image.hpp"
#include "charm_app_store.hpp"
#include "charm_app_staged_runtime.hpp"
#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_received_image.hpp"
#include "qemu_virtual_backend.hpp"

#include "afe_error_app.elf.inc"
#include "appstore.bin.inc"
#include "argv_app.elf.inc"
#include "bss_app.elf.inc"
#include "console_error_app.elf.inc"
#include "data_app.elf.inc"
#include "display_describe_error_app.elf.inc"
#include "display_error_app.elf.inc"
#include "display_null_present_app.elf.inc"
#include "display_sequence_app.elf.inc"
#include "exit_app.elf.inc"
#include "exit_error_app.elf.inc"
#include "exit_negative_app.elf.inc"
#include "hello_app.elf.inc"
#include "input_error_app.elf.inc"
#include "input_sequence_app.elf.inc"
#include "input_wrap_app.elf.inc"
#include "large_fit_app.elf.inc"
#include "player_min.elf.inc"
#include "return_negative_app.elf.inc"
#include "storage_app.elf.inc"
#include "storage_catalog_app.elf.inc"
#include "storage_close_error_app.elf.inc"
#include "storage_error_app.elf.inc"
#include "storage_fd_exhaustion_app.elf.inc"
#include "storage_open_error_app.elf.inc"
#include "storage_write_error_app.elf.inc"
#include "storage_zero_io_app.elf.inc"
#include "time_app.elf.inc"
#include "time_sequence_app.elf.inc"
#include "too_large_app.elf.inc"
#include "unsupported_caps_app.elf.inc"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace {

namespace app_abi = charm::app_abi;
namespace loader = charm::dev_loader;
namespace qemu_backend = resident_elf_qemu;

struct QemuRuntimeDomainMemory {
    std::uintptr_t run_region_base;
    std::size_t run_region_size;
    std::size_t stage_cache_size;
    std::size_t packetstream_transport_buffer_size;
    std::size_t packetstream_stream_size;
};

inline constexpr QemuRuntimeDomainMemory kQemuMemory{
    .run_region_base = 0x20080000U,
    .run_region_size = 64U * 1024U,
    .stage_cache_size = 16U * 1024U,
    .packetstream_transport_buffer_size = 2048U,
    .packetstream_stream_size = 32768U,
};

alignas(16) __attribute__((section(".elf_load")))
static std::byte g_elf_load_region[kQemuMemory.run_region_size];
alignas(16) static std::byte g_stage_cache[kQemuMemory.stage_cache_size];
alignas(16) static std::byte g_packetstream_storage[kQemuMemory.stage_cache_size];
alignas(16) static std::byte g_packetstream_transport_buffer[kQemuMemory.packetstream_transport_buffer_size];
alignas(16) static std::byte g_packetstream_stream[kQemuMemory.packetstream_stream_size];
alignas(16) static std::byte g_packetstream_received_cache[kQemuMemory.stage_cache_size];
alignas(16) static const std::byte g_received_oversized_payload[kQemuMemory.stage_cache_size + 1U]{};
alignas(16) static unsigned char g_mutated_elf_payload[8192];
alignas(16) static const unsigned char g_bad_elf_magic_payload[64]{
    'N',
    'O',
    'T',
    'E',
    'L',
    'F',
};
alignas(16) static const unsigned char g_bad_elf_short_header_payload[8]{
    0x7f,
    'E',
    'L',
    'F',
    1,
    1,
};

struct MemoryStorage {
    std::byte* data{nullptr};
    std::size_t size{0};
};

bool memory_write_storage(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || storage->data == nullptr) {
        return false;
    }
    const auto end = static_cast<std::uint64_t>(offset) + bytes.size();
    if (end > storage->size) {
        return false;
    }
    std::memcpy(storage->data + offset, bytes.data(), bytes.size());
    return true;
}

bool memory_read_storage(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    const auto* storage = static_cast<const MemoryStorage*>(ctx);
    if (storage == nullptr || storage->data == nullptr) {
        return false;
    }
    const auto end = static_cast<std::uint64_t>(offset) + bytes.size();
    if (end > storage->size) {
        return false;
    }
    std::memcpy(bytes.data(), storage->data + offset, bytes.size());
    return true;
}

loader::Storage make_loader_storage(MemoryStorage& storage) noexcept {
    return loader::Storage{
        .ctx = &storage,
        .base_address = static_cast<std::uint32_t>(kQemuMemory.run_region_base),
        .capacity_bytes = static_cast<std::uint32_t>(storage.size),
        .write = memory_write_storage,
        .read = memory_read_storage,
    };
}

qemu_backend::VirtualStoreMedia make_qemu_store_media() noexcept {
    return qemu_backend::VirtualStoreMedia{
        .data = reinterpret_cast<const std::byte*>(qemu_appstore_bin),
        .size = qemu_appstore_bin_len,
    };
}

std::uint32_t qemu_store_entry_count() noexcept {
    if (qemu_appstore_bin_len < sizeof(app_abi::AppStoreHeader)) {
        return 0U;
    }
    app_abi::AppStoreHeader header{};
    std::memcpy(&header, qemu_appstore_bin, sizeof(header));
    if (!app_abi::app_store_header_valid(header)) {
        return 0U;
    }
    return header.entry_count;
}

void clear_run_region() noexcept {
    std::memset(g_elf_load_region, 0, sizeof(g_elf_load_region));
}

void reset_packetstream_buffers() noexcept {
    std::memset(g_packetstream_storage, 0, sizeof(g_packetstream_storage));
    std::memset(g_packetstream_transport_buffer, 0, sizeof(g_packetstream_transport_buffer));
    std::memset(g_packetstream_stream, 0, sizeof(g_packetstream_stream));
    std::memset(g_packetstream_received_cache, 0, sizeof(g_packetstream_received_cache));
}

bool copy_elf_for_mutation(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (image_bytes == nullptr || image_size == 0U || image_size > sizeof(g_mutated_elf_payload)) {
        qemu_backend::write("resident-elf-qemu: mutate-elf code=image_too_large size=");
        qemu_backend::write_dec(static_cast<std::uint32_t>(image_size));
        qemu_backend::write("\n");
        return false;
    }
    std::memset(g_mutated_elf_payload, 0, sizeof(g_mutated_elf_payload));
    std::memcpy(g_mutated_elf_payload, image_bytes, image_size);
    return true;
}

bool mutate_elf_entry_outside_segment(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    header.entry = 0xffffffffU;
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));
    return true;
}

bool mutate_elf_bad_class(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) || image_size < 5U) {
        return false;
    }
    g_mutated_elf_payload[4] = 2U;
    return true;
}

bool mutate_elf_bad_endian(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) || image_size < 6U) {
        return false;
    }
    g_mutated_elf_payload[5] = 2U;
    return true;
}

bool mutate_elf_bad_ident_version(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) || image_size < 7U) {
        return false;
    }
    g_mutated_elf_payload[6] = 0U;
    return true;
}

bool mutate_elf_bad_type(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    header.type = 1U;
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));
    return true;
}

bool mutate_elf_bad_machine(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    header.machine = 3U;
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));
    return true;
}

bool mutate_elf_bad_version(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    header.version = 0U;
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));
    return true;
}

bool mutate_elf_bad_ehsize(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    header.ehsize = static_cast<std::uint16_t>(sizeof(app_abi::AppElf32Header) - 1U);
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));
    return true;
}

bool mutate_elf_bad_phentsize(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    header.phentsize = static_cast<std::uint16_t>(sizeof(app_abi::AppElf32ProgramHeader) + 4U);
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));
    return true;
}

bool mutate_elf_bad_program_header(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    header.phoff = static_cast<std::uint32_t>(image_size + 1U);
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));
    return true;
}

bool mutate_elf_truncated_payload(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    const auto ph_table_bytes =
        static_cast<std::uint64_t>(header.phentsize) * static_cast<std::uint64_t>(header.phnum);
    if (header.phoff > image_size ||
        header.phentsize < sizeof(app_abi::AppElf32ProgramHeader) ||
        static_cast<std::uint64_t>(header.phoff) + ph_table_bytes > image_size) {
        return false;
    }

    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto ph_offset = header.phoff + (static_cast<std::uint32_t>(header.phentsize) * i);
        app_abi::AppElf32ProgramHeader ph{};
        std::memcpy(&ph, g_mutated_elf_payload + ph_offset, sizeof(ph));
        if (ph.type == app_abi::kAppElfPtLoad && ph.filesz != 0U) {
            ph.offset = static_cast<std::uint32_t>(image_size);
            ph.filesz = 1U;
            ph.memsz = 1U;
            std::memcpy(g_mutated_elf_payload + ph_offset, &ph, sizeof(ph));
            return true;
        }
    }
    return false;
}

bool mutate_elf_no_load_segment(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    const auto ph_table_bytes =
        static_cast<std::uint64_t>(header.phentsize) * static_cast<std::uint64_t>(header.phnum);
    if (header.phoff > image_size ||
        header.phentsize < sizeof(app_abi::AppElf32ProgramHeader) ||
        static_cast<std::uint64_t>(header.phoff) + ph_table_bytes > image_size) {
        return false;
    }

    bool mutated = false;
    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto ph_offset = header.phoff + (static_cast<std::uint32_t>(header.phentsize) * i);
        app_abi::AppElf32ProgramHeader ph{};
        std::memcpy(&ph, g_mutated_elf_payload + ph_offset, sizeof(ph));
        if (ph.type == app_abi::kAppElfPtLoad) {
            ph.type = 0U;
            std::memcpy(g_mutated_elf_payload + ph_offset, &ph, sizeof(ph));
            mutated = true;
        }
    }
    return mutated;
}

bool mutate_elf_overlapping_segments(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    const auto ph_table_bytes =
        static_cast<std::uint64_t>(header.phentsize) * static_cast<std::uint64_t>(header.phnum);
    if (header.phoff > image_size ||
        header.phentsize < sizeof(app_abi::AppElf32ProgramHeader) ||
        static_cast<std::uint64_t>(header.phoff) + ph_table_bytes > image_size) {
        return false;
    }

    bool first_set = false;
    app_abi::AppElf32ProgramHeader first{};
    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto ph_offset = header.phoff + (static_cast<std::uint32_t>(header.phentsize) * i);
        app_abi::AppElf32ProgramHeader ph{};
        std::memcpy(&ph, g_mutated_elf_payload + ph_offset, sizeof(ph));
        if (ph.type != app_abi::kAppElfPtLoad || (ph.memsz == 0U && ph.filesz == 0U)) {
            continue;
        }
        if (!first_set) {
            first = ph;
            first_set = true;
            continue;
        }
        ph.vaddr = first.vaddr;
        ph.align = 0U;
        std::memcpy(g_mutated_elf_payload + ph_offset, &ph, sizeof(ph));
        return true;
    }
    return false;
}

bool mutate_elf_first_load_segment_rwx(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    const auto ph_table_bytes =
        static_cast<std::uint64_t>(header.phentsize) * static_cast<std::uint64_t>(header.phnum);
    if (header.phoff > image_size ||
        header.phentsize < sizeof(app_abi::AppElf32ProgramHeader) ||
        static_cast<std::uint64_t>(header.phoff) + ph_table_bytes > image_size) {
        return false;
    }

    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto ph_offset = header.phoff + (static_cast<std::uint32_t>(header.phentsize) * i);
        app_abi::AppElf32ProgramHeader ph{};
        std::memcpy(&ph, g_mutated_elf_payload + ph_offset, sizeof(ph));
        if (ph.type == app_abi::kAppElfPtLoad && (ph.memsz != 0U || ph.filesz != 0U)) {
            ph.flags |= app_abi::kAppElfPfW | app_abi::kAppElfPfX;
            std::memcpy(g_mutated_elf_payload + ph_offset, &ph, sizeof(ph));
            return true;
        }
    }
    return false;
}

bool mutate_elf_wrong_link_base(const unsigned char* image_bytes, std::size_t image_size) noexcept {
    if (!copy_elf_for_mutation(image_bytes, image_size) ||
        image_size < sizeof(app_abi::AppElf32Header)) {
        return false;
    }

    app_abi::AppElf32Header header{};
    std::memcpy(&header, g_mutated_elf_payload, sizeof(header));
    if (header.phoff < header.ehsize ||
        header.phoff > image_size ||
        header.phentsize != sizeof(app_abi::AppElf32ProgramHeader) ||
        header.phnum == 0U) {
        return false;
    }
    const auto ph_table_bytes =
        static_cast<std::uint64_t>(header.phentsize) * static_cast<std::uint64_t>(header.phnum);
    if (static_cast<std::uint64_t>(header.phoff) + ph_table_bytes > image_size) {
        return false;
    }

    constexpr std::uint32_t kLinkBaseDelta = 0x1000U;
    header.entry += kLinkBaseDelta;
    std::memcpy(g_mutated_elf_payload, &header, sizeof(header));

    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto ph_offset = header.phoff + (static_cast<std::uint32_t>(header.phentsize) * i);
        app_abi::AppElf32ProgramHeader ph{};
        std::memcpy(&ph, g_mutated_elf_payload + ph_offset, sizeof(ph));
        if (ph.type == app_abi::kAppElfPtLoad && (ph.memsz != 0U || ph.filesz != 0U)) {
            ph.vaddr += kLinkBaseDelta;
            ph.paddr += kLinkBaseDelta;
            std::memcpy(g_mutated_elf_payload + ph_offset, &ph, sizeof(ph));
        }
    }
    return true;
}

bool expect_mutated_hello_load_failure(std::string_view name,
                                       bool (*mutate)(const unsigned char*, std::size_t) noexcept,
                                       app_abi::AppElfProbeCode expected_probe) noexcept;

struct ElfLinkFacts {
    std::uint32_t link_base{0};
    std::uint32_t entry_vaddr{0};
    bool valid{false};
};

ElfLinkFacts read_elf_link_facts(const app_abi::AppImage& image) noexcept {
    ElfLinkFacts facts{};
    if (image.format != app_abi::AppImageFormat::elf ||
        image.image_base == nullptr ||
        image.image_size < sizeof(app_abi::AppElf32Header)) {
        return facts;
    }

    const auto* bytes = static_cast<const std::byte*>(image.image_base);
    app_abi::AppElf32Header header{};
    std::memcpy(&header, image.image_base, sizeof(header));
    if (header.phoff < header.ehsize ||
        header.phoff > image.image_size ||
        header.phentsize != sizeof(app_abi::AppElf32ProgramHeader) ||
        header.phnum == 0U) {
        return facts;
    }

    const auto ph_table_bytes =
        static_cast<std::uint64_t>(header.phentsize) * static_cast<std::uint64_t>(header.phnum);
    if (static_cast<std::uint64_t>(header.phoff) + ph_table_bytes > image.image_size) {
        return facts;
    }

    bool min_set = false;
    std::uint32_t min_vaddr = 0U;
    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        app_abi::AppElf32ProgramHeader ph{};
        std::memcpy(&ph, bytes + header.phoff + (static_cast<std::uint32_t>(header.phentsize) * i), sizeof(ph));
        if (ph.type != app_abi::kAppElfPtLoad || (ph.memsz == 0U && ph.filesz == 0U)) {
            continue;
        }
        if (!min_set || ph.vaddr < min_vaddr) {
            min_vaddr = ph.vaddr;
            min_set = true;
        }
    }
    if (!min_set) {
        return facts;
    }

    facts.link_base = min_vaddr;
    facts.entry_vaddr = header.entry;
    facts.valid = true;
    return facts;
}

app_abi::AppLoadResult load_elf(void* ctx,
                                const app_abi::AppImage& image,
                                const app_abi::AppLoadBuffer& buffer) noexcept {
    auto* backend = static_cast<app_abi::AppElfLoadBackend*>(ctx);
    const auto result = app_abi::app_elf_load_image(backend, image, buffer);
    if (backend == nullptr ||
        result.code != app_abi::AppRunCode::ok ||
        backend->last.plan.probe.code != app_abi::AppElfProbeCode::ok) {
        return result;
    }

    const auto facts = read_elf_link_facts(image);
    if (!facts.valid || facts.link_base != static_cast<std::uint32_t>(kQemuMemory.run_region_base)) {
        backend->last.code = app_abi::AppRunCode::load_failed;
        backend->last.backend_error = -1;
        return app_abi::AppLoadResult{
            .code = app_abi::AppRunCode::load_failed,
            .backend_error = backend->last.backend_error,
        };
    }
    return result;
}

void log_format(app_abi::AppImageFormat format) noexcept;

void log_probe(std::string_view name, const app_abi::AppImage& image, const app_abi::AppElfLoadBackend& backend) noexcept {
    const auto& probe = backend.last.plan.probe;
    const auto facts = read_elf_link_facts(image);
    const auto link_base = facts.valid ? facts.link_base : 0U;
    const auto entry_vaddr = facts.valid ? facts.entry_vaddr : 0U;
    const auto base_match = facts.valid && link_base == static_cast<std::uint32_t>(kQemuMemory.run_region_base);
    qemu_backend::write("resident-elf-qemu: load ");
    qemu_backend::log_view(name);
    qemu_backend::write(" format=elf probe=");
    qemu_backend::log_view(app_abi::app_elf_probe_code_name(probe.code));
    qemu_backend::write(" link_base=");
    qemu_backend::write_hex32(link_base);
    qemu_backend::write(" expected_base=");
    qemu_backend::write_hex32(static_cast<std::uint32_t>(kQemuMemory.run_region_base));
    qemu_backend::write(" entry=");
    qemu_backend::write_hex32(static_cast<std::uint32_t>(backend.last.plan.entry_address));
    qemu_backend::write(" entry_vaddr=");
    qemu_backend::write_hex32(entry_vaddr);
    qemu_backend::write(" span=");
    qemu_backend::write_dec(probe.load_span);
    qemu_backend::write(" segments=");
    qemu_backend::write_dec(probe.segment_count);
    qemu_backend::write(" base_match=");
    qemu_backend::write_dec(base_match ? 1U : 0U);
    qemu_backend::write("\n");

    const bool fits = probe.load_span <= kQemuMemory.run_region_size;
    const auto free = fits
        ? static_cast<std::uint32_t>(kQemuMemory.run_region_size - probe.load_span)
        : 0U;
    qemu_backend::write("resident-elf-qemu: capacity ");
    qemu_backend::log_view(name);
    qemu_backend::write(" needed=");
    qemu_backend::write_dec(probe.load_span);
    qemu_backend::write(" free=");
    qemu_backend::write_dec(free);
    qemu_backend::write(" fits=");
    qemu_backend::write_dec(fits ? 1U : 0U);
    qemu_backend::write(" region=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(kQemuMemory.run_region_size));
    qemu_backend::write(" probe=");
    qemu_backend::log_view(app_abi::app_elf_probe_code_name(probe.code));
    qemu_backend::write("\n");
}

bool run_image(std::string_view log_name,
               const app_abi::AppImage& image,
               std::string_view arg_text,
               int expected_exit_code = 0) noexcept {
    clear_run_region();
    qemu_backend::reset_capability_counters();

    app_abi::AppElfLoadBackend backend{};
    app_abi::StagedAppImageSource staged{
        .image = image,
        .load_ctx = &backend,
        .load = load_elf,
    };
    const auto source = app_abi::make_staged_app_image_source(staged);
    CharmAppApi api = qemu_backend::make_virtual_app_api();
    app_abi::AppRuntime<> runtime{};
    const auto result = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{
            .base = g_elf_load_region,
            .size = sizeof(g_elf_load_region),
            .align = 16U,
        },
        .api = &api,
        .name = image.name,
        .arg_text = arg_text,
    });

    log_probe(log_name, image, backend);
    qemu_backend::write("resident-elf-qemu: app ");
    qemu_backend::log_view(log_name);
    qemu_backend::write(" stage=");
    qemu_backend::log_view(app_abi::stage_name(result.stage));
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::code_name(result.code));
    qemu_backend::write(" exit=");
    qemu_backend::write_signed(result.exit_code);
    qemu_backend::write("\n");
    qemu_backend::log_capability_counters(log_name);

    return result.stage == app_abi::AppRunStage::exit &&
           result.code == app_abi::AppRunCode::ok &&
           result.exited &&
           result.exit_code == expected_exit_code &&
           backend.last.plan.probe.code == app_abi::AppElfProbeCode::ok &&
           read_elf_link_facts(image).link_base == static_cast<std::uint32_t>(kQemuMemory.run_region_base);
}

bool run_direct_app(std::string_view name,
                    const unsigned char* image_bytes,
                    std::size_t image_size,
                    std::string_view arg_text,
                    int expected_exit_code = 0) noexcept {
    return run_image(name,
                     app_abi::AppImage{
                         .name = name,
                         .format = app_abi::AppImageFormat::elf,
                         .image_base = image_bytes,
                         .image_size = image_size,
                     },
                     arg_text,
                     expected_exit_code);
}

bool run_received_app(std::string_view name,
                      const unsigned char* image_bytes,
                      std::size_t image_size,
                      std::string_view arg_text) noexcept {
    std::memset(g_stage_cache, 0, sizeof(g_stage_cache));
    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = name,
        .format = app_abi::AppImageFormat::elf,
        .image = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(image_bytes),
            image_size,
        },
        .verified = true,
        .cache = std::span<std::byte>{g_stage_cache, sizeof(g_stage_cache)},
    });

    qemu_backend::write("resident-elf-qemu: received stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::app_received_image_stage_code_name(staged.code));
    qemu_backend::write(" format=");
    log_format(staged.image.format);
    qemu_backend::write(" bytes=");
    qemu_backend::write_dec(staged.bytes_copied);
    qemu_backend::write("\n");

    if (staged.code != app_abi::AppReceivedImageStageCode::ok ||
        staged.image.format != app_abi::AppImageFormat::elf) {
        return false;
    }

    char received_name[64]{};
    constexpr std::string_view kPrefix = "received:";
    std::size_t cursor = 0;
    for (const char ch : kPrefix) {
        received_name[cursor++] = ch;
    }
    const auto copy_len = name.size() < (sizeof(received_name) - cursor - 1U)
        ? name.size()
        : (sizeof(received_name) - cursor - 1U);
    for (std::size_t i = 0; i < copy_len; ++i) {
        received_name[cursor++] = name[i];
    }
    return run_image(std::string_view{received_name, cursor}, staged.image, arg_text);
}

bool run_packetstream_received_app(std::string_view name,
                                   const unsigned char* image_bytes,
                                   std::size_t image_size,
                                   std::string_view arg_text) noexcept {
    std::memset(g_stage_cache, 0, sizeof(g_stage_cache));
    reset_packetstream_buffers();

    const std::span<const std::byte> payload{
        reinterpret_cast<const std::byte*>(image_bytes),
        image_size,
    };
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 257U,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   std::span<std::byte>{g_packetstream_stream, sizeof(g_packetstream_stream)});
    if (built.code != loader::PacketStreamBuildCode::ok) {
        qemu_backend::write("resident-elf-qemu: packetstream stage name=");
        qemu_backend::log_view(name);
        qemu_backend::write(" build=");
        qemu_backend::log_view(loader::packet_stream_build_code_name(built.code));
        qemu_backend::write("\n");
        return false;
    }

    MemoryStorage storage{
        .data = g_packetstream_storage,
        .size = sizeof(g_packetstream_storage),
    };
    loader::PacketRuntime packet_runtime{make_loader_storage(storage)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{
            .buffer = std::span<std::byte>{g_packetstream_transport_buffer, sizeof(g_packetstream_transport_buffer)},
            .max_payload_size = 512U,
        },
    };

    loader::ByteTransportResult status{};
    std::uint32_t offset = 0;
    std::uint32_t dispatches = 0;
    while (offset < built.bytes_written) {
        const auto remaining = built.bytes_written - offset;
        const auto count = remaining < 113U ? remaining : 113U;
        status = transport.ingest(std::span<const std::byte>{g_packetstream_stream + offset, count});
        dispatches += status.packets_dispatched;
        if (status.code != loader::ByteTransportCode::ok) {
            break;
        }
        offset += count;
    }
    if (status.code == loader::ByteTransportCode::ok) {
        status = transport.status();
    }

    qemu_backend::write("resident-elf-qemu: packetstream stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" transport=");
    qemu_backend::log_view(loader::byte_transport_code_name(status.code));
    qemu_backend::write(" packet=");
    qemu_backend::log_view(loader::packet_code_name(status.packet.packet_code));
    qemu_backend::write(" stage=");
    qemu_backend::log_view(loader::stage_name(status.packet.receive.stage));
    qemu_backend::write(" code=");
    qemu_backend::log_view(loader::code_name(status.packet.receive.code));
    qemu_backend::write(" payload=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(payload.size()));
    qemu_backend::write(" stream=");
    qemu_backend::write_dec(built.bytes_written);
    qemu_backend::write(" packets=");
    qemu_backend::write_dec(built.packet_count);
    qemu_backend::write(" dispatch=");
    qemu_backend::write_dec(dispatches);
    qemu_backend::write(" crc=");
    qemu_backend::write_hex32(status.packet.receive.actual_crc32);
    qemu_backend::write("/");
    qemu_backend::write_hex32(status.packet.receive.expected_crc32);
    qemu_backend::write("\n");

    if (status.code != loader::ByteTransportCode::ok ||
        status.packet.packet_code != loader::PacketCode::ok ||
        status.packet.receive.stage != loader::Stage::launch_ready ||
        status.packet.receive.code != loader::Code::ok) {
        return false;
    }

    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = status.packet.receive,
        .manifest = status.packet.manifest,
        .storage = make_loader_storage(storage),
        .output = std::span<std::byte>{g_packetstream_received_cache, payload.size()},
    });
    qemu_backend::write("resident-elf-qemu: packetstream read name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(loader::received_image_read_code_name(read.code));
    qemu_backend::write(" bytes=");
    qemu_backend::write_dec(read.bytes_read);
    qemu_backend::write("\n");
    if (read.code != loader::ReceivedImageReadCode::ok || read.bytes_read != payload.size()) {
        return false;
    }

    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = name,
        .format = app_abi::AppImageFormat::elf,
        .image = read.image,
        .verified = true,
        .cache = std::span<std::byte>{g_stage_cache, sizeof(g_stage_cache)},
    });
    qemu_backend::write("resident-elf-qemu: packetstream app-stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::app_received_image_stage_code_name(staged.code));
    qemu_backend::write(" format=");
    log_format(staged.image.format);
    qemu_backend::write(" bytes=");
    qemu_backend::write_dec(staged.bytes_copied);
    qemu_backend::write("\n");
    if (staged.code != app_abi::AppReceivedImageStageCode::ok ||
        staged.image.format != app_abi::AppImageFormat::elf) {
        return false;
    }

    char packet_name[64]{};
    constexpr std::string_view kPrefix = "packetstream:";
    std::size_t cursor = 0;
    for (const char ch : kPrefix) {
        packet_name[cursor++] = ch;
    }
    const auto copy_len = name.size() < (sizeof(packet_name) - cursor - 1U)
        ? name.size()
        : (sizeof(packet_name) - cursor - 1U);
    for (std::size_t i = 0; i < copy_len; ++i) {
        packet_name[cursor++] = name[i];
    }
    return run_image(std::string_view{packet_name, cursor}, staged.image, arg_text);
}

bool expect_runtime_failure(std::string_view log_name,
                            const app_abi::AppImage& image,
                            std::string_view arg_text,
                            app_abi::AppRunStage expected_stage,
                            app_abi::AppRunCode expected_code,
                            app_abi::AppElfProbeCode expected_probe,
                            void (*mutate_api)(CharmAppApi&) noexcept) noexcept;

bool expect_packetstream_load_failure(std::string_view name,
                                      const unsigned char* image_bytes,
                                      std::size_t image_size,
                                      app_abi::AppElfProbeCode expected_probe) noexcept {
    std::memset(g_stage_cache, 0, sizeof(g_stage_cache));
    reset_packetstream_buffers();

    const std::span<const std::byte> payload{
        reinterpret_cast<const std::byte*>(image_bytes),
        image_size,
    };
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 257U,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   std::span<std::byte>{g_packetstream_stream, sizeof(g_packetstream_stream)});
    if (built.code != loader::PacketStreamBuildCode::ok) {
        qemu_backend::write("resident-elf-qemu: packetstream stage name=");
        qemu_backend::log_view(name);
        qemu_backend::write(" build=");
        qemu_backend::log_view(loader::packet_stream_build_code_name(built.code));
        qemu_backend::write("\n");
        return false;
    }

    MemoryStorage storage{
        .data = g_packetstream_storage,
        .size = sizeof(g_packetstream_storage),
    };
    loader::PacketRuntime packet_runtime{make_loader_storage(storage)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{
            .buffer = std::span<std::byte>{g_packetstream_transport_buffer, sizeof(g_packetstream_transport_buffer)},
            .max_payload_size = 512U,
        },
    };

    loader::ByteTransportResult status{};
    std::uint32_t offset = 0;
    std::uint32_t dispatches = 0;
    while (offset < built.bytes_written) {
        const auto remaining = built.bytes_written - offset;
        const auto count = remaining < 113U ? remaining : 113U;
        status = transport.ingest(std::span<const std::byte>{g_packetstream_stream + offset, count});
        dispatches += status.packets_dispatched;
        if (status.code != loader::ByteTransportCode::ok) {
            break;
        }
        offset += count;
    }
    if (status.code == loader::ByteTransportCode::ok) {
        status = transport.status();
    }

    qemu_backend::write("resident-elf-qemu: packetstream stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" transport=");
    qemu_backend::log_view(loader::byte_transport_code_name(status.code));
    qemu_backend::write(" packet=");
    qemu_backend::log_view(loader::packet_code_name(status.packet.packet_code));
    qemu_backend::write(" stage=");
    qemu_backend::log_view(loader::stage_name(status.packet.receive.stage));
    qemu_backend::write(" code=");
    qemu_backend::log_view(loader::code_name(status.packet.receive.code));
    qemu_backend::write(" payload=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(payload.size()));
    qemu_backend::write(" stream=");
    qemu_backend::write_dec(built.bytes_written);
    qemu_backend::write(" packets=");
    qemu_backend::write_dec(built.packet_count);
    qemu_backend::write(" dispatch=");
    qemu_backend::write_dec(dispatches);
    qemu_backend::write(" crc=");
    qemu_backend::write_hex32(status.packet.receive.actual_crc32);
    qemu_backend::write("/");
    qemu_backend::write_hex32(status.packet.receive.expected_crc32);
    qemu_backend::write("\n");

    if (status.code != loader::ByteTransportCode::ok ||
        status.packet.packet_code != loader::PacketCode::ok ||
        status.packet.receive.stage != loader::Stage::launch_ready ||
        status.packet.receive.code != loader::Code::ok) {
        return false;
    }

    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = status.packet.receive,
        .manifest = status.packet.manifest,
        .storage = make_loader_storage(storage),
        .output = std::span<std::byte>{g_packetstream_received_cache, payload.size()},
    });
    qemu_backend::write("resident-elf-qemu: packetstream read name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(loader::received_image_read_code_name(read.code));
    qemu_backend::write(" bytes=");
    qemu_backend::write_dec(read.bytes_read);
    qemu_backend::write("\n");
    if (read.code != loader::ReceivedImageReadCode::ok || read.bytes_read != payload.size()) {
        return false;
    }

    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = name,
        .format = app_abi::AppImageFormat::elf,
        .image = read.image,
        .verified = true,
        .cache = std::span<std::byte>{g_stage_cache, sizeof(g_stage_cache)},
    });
    qemu_backend::write("resident-elf-qemu: packetstream app-stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::app_received_image_stage_code_name(staged.code));
    qemu_backend::write(" format=");
    log_format(staged.image.format);
    qemu_backend::write(" bytes=");
    qemu_backend::write_dec(staged.bytes_copied);
    qemu_backend::write("\n");
    if (staged.code != app_abi::AppReceivedImageStageCode::ok ||
        staged.image.format != app_abi::AppImageFormat::elf) {
        return false;
    }

    char packet_name[64]{};
    constexpr std::string_view kPrefix = "packetstream:";
    std::size_t cursor = 0;
    for (const char ch : kPrefix) {
        packet_name[cursor++] = ch;
    }
    const auto copy_len = name.size() < (sizeof(packet_name) - cursor - 1U)
        ? name.size()
        : (sizeof(packet_name) - cursor - 1U);
    for (std::size_t i = 0; i < copy_len; ++i) {
        packet_name[cursor++] = name[i];
    }

    return expect_runtime_failure(std::string_view{packet_name, cursor},
                                  staged.image,
                                  "",
                                  app_abi::AppRunStage::load,
                                  app_abi::AppRunCode::load_failed,
                                  expected_probe,
                                  nullptr);
}

bool expect_packetstream_crc_mismatch(std::string_view name,
                                      const unsigned char* image_bytes,
                                      std::size_t image_size) noexcept {
    reset_packetstream_buffers();
    qemu_backend::reset_capability_counters();

    const std::span<const std::byte> payload{
        reinterpret_cast<const std::byte*>(image_bytes),
        image_size,
    };
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 257U,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   std::span<std::byte>{g_packetstream_stream, sizeof(g_packetstream_stream)});
    if (built.code != loader::PacketStreamBuildCode::ok) {
        qemu_backend::write("resident-elf-qemu: packetstream stage name=");
        qemu_backend::log_view(name);
        qemu_backend::write(" build=");
        qemu_backend::log_view(loader::packet_stream_build_code_name(built.code));
        qemu_backend::write("\n");
        return false;
    }

    const auto first_data_payload_offset = static_cast<std::uint32_t>(sizeof(loader::PacketHeader) * 2U);
    if (first_data_payload_offset >= built.bytes_written) {
        qemu_backend::write("resident-elf-qemu: packetstream stage name=");
        qemu_backend::log_view(name);
        qemu_backend::write(" build=bad_test_stream\n");
        return false;
    }
    g_packetstream_stream[first_data_payload_offset] ^= std::byte{0xff};

    MemoryStorage storage{
        .data = g_packetstream_storage,
        .size = sizeof(g_packetstream_storage),
    };
    loader::PacketRuntime packet_runtime{make_loader_storage(storage)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{
            .buffer = std::span<std::byte>{g_packetstream_transport_buffer, sizeof(g_packetstream_transport_buffer)},
            .max_payload_size = 512U,
        },
    };

    loader::ByteTransportResult status{};
    std::uint32_t offset = 0;
    std::uint32_t dispatches = 0;
    while (offset < built.bytes_written) {
        const auto remaining = built.bytes_written - offset;
        const auto count = remaining < 113U ? remaining : 113U;
        status = transport.ingest(std::span<const std::byte>{g_packetstream_stream + offset, count});
        dispatches += status.packets_dispatched;
        if (status.code != loader::ByteTransportCode::ok) {
            break;
        }
        offset += count;
    }
    if (status.code == loader::ByteTransportCode::ok) {
        status = transport.status();
    }

    qemu_backend::write("resident-elf-qemu: packetstream stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" transport=");
    qemu_backend::log_view(loader::byte_transport_code_name(status.code));
    qemu_backend::write(" packet=");
    qemu_backend::log_view(loader::packet_code_name(status.packet.packet_code));
    qemu_backend::write(" stage=");
    qemu_backend::log_view(loader::stage_name(status.packet.receive.stage));
    qemu_backend::write(" code=");
    qemu_backend::log_view(loader::code_name(status.packet.receive.code));
    qemu_backend::write(" payload=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(payload.size()));
    qemu_backend::write(" stream=");
    qemu_backend::write_dec(built.bytes_written);
    qemu_backend::write(" packets=");
    qemu_backend::write_dec(built.packet_count);
    qemu_backend::write(" dispatch=");
    qemu_backend::write_dec(dispatches);
    qemu_backend::write(" crc=");
    qemu_backend::write_hex32(status.packet.receive.actual_crc32);
    qemu_backend::write("/");
    qemu_backend::write_hex32(status.packet.receive.expected_crc32);
    qemu_backend::write("\n");
    qemu_backend::log_capability_counters(name);

    const auto& counters = qemu_backend::capability_counters();
    return status.code == loader::ByteTransportCode::packet_failed &&
           status.packet.packet_code == loader::PacketCode::receive_failed &&
           status.packet.kind == loader::PacketKind::verify &&
           status.packet.receive.stage == loader::Stage::failed &&
           status.packet.receive.code == loader::Code::crc_mismatch &&
           status.packet.receive.actual_crc32 != status.packet.receive.expected_crc32 &&
           dispatches == (built.packet_count - 1U) &&
           counters.console_bytes == 0U &&
           counters.time_now == 0U &&
           counters.display_describe == 0U &&
           counters.display_present == 0U &&
           counters.input_poll == 0U &&
           counters.storage_open == 0U &&
           counters.storage_read == 0U &&
           counters.storage_write == 0U &&
           counters.storage_close == 0U &&
           counters.afe_configure == 0U &&
           counters.afe_read == 0U &&
           counters.app_exit == 0U;
}

bool expect_received_stage_failure(std::string_view name,
                                   std::span<const std::byte> image,
                                   app_abi::AppReceivedImageStageCode expected_code) noexcept {
    std::memset(g_stage_cache, 0, sizeof(g_stage_cache));
    qemu_backend::reset_capability_counters();
    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = name,
        .format = app_abi::AppImageFormat::elf,
        .image = image,
        .verified = true,
        .cache = std::span<std::byte>{g_stage_cache, sizeof(g_stage_cache)},
    });

    qemu_backend::write("resident-elf-qemu: received stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::app_received_image_stage_code_name(staged.code));
    qemu_backend::write(" expected=");
    qemu_backend::log_view(app_abi::app_received_image_stage_code_name(expected_code));
    qemu_backend::write(" bytes=");
    qemu_backend::write_dec(staged.bytes_copied);
    qemu_backend::write(" image_size=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(image.size()));
    qemu_backend::write("\n");
    qemu_backend::log_capability_counters(name);

    const auto& counters = qemu_backend::capability_counters();
    return staged.code == expected_code &&
           staged.image.image_base == nullptr &&
           staged.image.image_size == 0U &&
           staged.bytes_copied == 0U &&
           counters.console_bytes == 0U &&
           counters.time_now == 0U &&
           counters.display_describe == 0U &&
           counters.display_present == 0U &&
           counters.input_poll == 0U &&
           counters.storage_open == 0U &&
           counters.storage_read == 0U &&
           counters.storage_write == 0U &&
           counters.storage_close == 0U &&
           counters.afe_configure == 0U &&
           counters.afe_read == 0U &&
           counters.app_exit == 0U;
}

bool expect_load_failure_with_buffer(std::string_view name,
                                     const unsigned char* image_bytes,
                                     std::size_t image_size,
                                     app_abi::AppElfProbeCode expected_probe,
                                     app_abi::AppLoadBuffer load_buffer) noexcept {
    clear_run_region();
    qemu_backend::reset_capability_counters();

    app_abi::AppElfLoadBackend backend{};
    app_abi::StagedAppImageSource staged{
        .image = app_abi::AppImage{
            .name = name,
            .format = app_abi::AppImageFormat::elf,
            .image_base = image_bytes,
            .image_size = image_size,
        },
        .load_ctx = &backend,
        .load = load_elf,
    };
    const auto source = app_abi::make_staged_app_image_source(staged);
    CharmAppApi api = qemu_backend::make_virtual_app_api();
    app_abi::AppRuntime<> runtime{};
    const auto result = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = load_buffer,
        .api = &api,
        .name = name,
        .arg_text = "",
    });

    log_probe(name, staged.image, backend);
    qemu_backend::write("resident-elf-qemu: app ");
    qemu_backend::log_view(name);
    qemu_backend::write(" stage=");
    qemu_backend::log_view(app_abi::stage_name(result.stage));
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::code_name(result.code));
    qemu_backend::write(" exit=");
    qemu_backend::write_signed(result.exit_code);
    qemu_backend::write("\n");
    qemu_backend::log_capability_counters(name);

    const auto& counters = qemu_backend::capability_counters();

    return result.stage == app_abi::AppRunStage::load &&
           result.code == app_abi::AppRunCode::load_failed &&
           !result.exited &&
           backend.last.plan.probe.code == expected_probe &&
           counters.console_bytes == 0U &&
           counters.time_now == 0U &&
           counters.display_describe == 0U &&
           counters.display_present == 0U &&
           counters.input_poll == 0U &&
           counters.app_exit == 0U;
}

bool expect_load_failure(std::string_view name,
                         const unsigned char* image_bytes,
                         std::size_t image_size,
                         app_abi::AppElfProbeCode expected_probe) noexcept {
    return expect_load_failure_with_buffer(name,
                                           image_bytes,
                                           image_size,
                                           expected_probe,
                                           app_abi::AppLoadBuffer{
                                               .base = g_elf_load_region,
                                               .size = sizeof(g_elf_load_region),
                                               .align = 16U,
                                           });
}

bool expect_mutated_hello_load_failure(std::string_view name,
                                       bool (*mutate)(const unsigned char*, std::size_t) noexcept,
                                       app_abi::AppElfProbeCode expected_probe) noexcept {
    if (mutate == nullptr || !mutate(hello_app_elf, hello_app_elf_len)) {
        qemu_backend::write("resident-elf-qemu: mutate-elf name=");
        qemu_backend::log_view(name);
        qemu_backend::write(" code=failed\n");
        return false;
    }
    return expect_load_failure(name,
                               g_mutated_elf_payload,
                               hello_app_elf_len,
                               expected_probe);
}

bool expect_runtime_failure(std::string_view log_name,
                            const app_abi::AppImage& image,
                            std::string_view arg_text,
                            app_abi::AppRunStage expected_stage,
                            app_abi::AppRunCode expected_code,
                            app_abi::AppElfProbeCode expected_probe,
                            void (*mutate_api)(CharmAppApi&) noexcept = nullptr) noexcept {
    clear_run_region();
    qemu_backend::reset_capability_counters();

    app_abi::AppElfLoadBackend backend{};
    app_abi::StagedAppImageSource staged{
        .image = image,
        .load_ctx = &backend,
        .load = load_elf,
    };
    const auto source = app_abi::make_staged_app_image_source(staged);
    CharmAppApi api = qemu_backend::make_virtual_app_api();
    if (mutate_api != nullptr) {
        mutate_api(api);
    }
    app_abi::AppRuntime<> runtime{};
    const auto result = runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{
            .base = g_elf_load_region,
            .size = sizeof(g_elf_load_region),
            .align = 16U,
        },
        .api = &api,
        .name = image.name,
        .arg_text = arg_text,
    });

    log_probe(log_name, image, backend);
    qemu_backend::write("resident-elf-qemu: app ");
    qemu_backend::log_view(log_name);
    qemu_backend::write(" stage=");
    qemu_backend::log_view(app_abi::stage_name(result.stage));
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::code_name(result.code));
    qemu_backend::write(" exit=");
    qemu_backend::write_signed(result.exit_code);
    qemu_backend::write("\n");
    qemu_backend::log_capability_counters(log_name);

    const auto& counters = qemu_backend::capability_counters();

    return result.stage == expected_stage &&
           result.code == expected_code &&
           !result.exited &&
           backend.last.plan.probe.code == expected_probe &&
           counters.console_bytes == 0U &&
           counters.time_now == 0U &&
           counters.display_describe == 0U &&
           counters.display_present == 0U &&
           counters.input_poll == 0U &&
           counters.storage_open == 0U &&
           counters.storage_read == 0U &&
           counters.storage_write == 0U &&
           counters.storage_close == 0U &&
           counters.afe_configure == 0U &&
           counters.afe_read == 0U &&
           counters.app_exit == 0U;
}

bool expect_prepare_only(std::string_view log_name,
                         const app_abi::AppImage& image,
                         std::string_view arg_text,
                         std::span<const std::string_view> expected_argv,
                         app_abi::AppElfProbeCode expected_probe) noexcept {
    clear_run_region();
    qemu_backend::reset_capability_counters();

    app_abi::AppElfLoadBackend backend{};
    app_abi::StagedAppImageSource staged{
        .image = image,
        .load_ctx = &backend,
        .load = load_elf,
    };
    const auto source = app_abi::make_staged_app_image_source(staged);
    CharmAppApi api = qemu_backend::make_virtual_app_api();
    app_abi::AppRuntime<> runtime{};
    const auto prepared = runtime.prepare(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{
            .base = g_elf_load_region,
            .size = sizeof(g_elf_load_region),
            .align = 16U,
        },
        .api = &api,
        .name = image.name,
        .arg_text = arg_text,
    });

    log_probe(log_name, image, backend);
    qemu_backend::write("resident-elf-qemu: prepare ");
    qemu_backend::log_view(log_name);
    qemu_backend::write(" stage=");
    qemu_backend::log_view(app_abi::stage_name(prepared.result.stage));
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::code_name(prepared.result.code));
    qemu_backend::write(" ready=");
    qemu_backend::write_dec(prepared.ready ? 1U : 0U);
    qemu_backend::write(" argc=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(prepared.argc));
    qemu_backend::write("\n");
    qemu_backend::log_capability_counters(log_name);

    const auto& counters = qemu_backend::capability_counters();
    bool argv_ok = prepared.argv != nullptr &&
                   prepared.argc >= 0 &&
                   static_cast<std::size_t>(prepared.argc) == expected_argv.size();
    if (argv_ok) {
        for (std::size_t i = 0; i < expected_argv.size(); ++i) {
            if (prepared.argv[i] == nullptr || std::string_view{prepared.argv[i]} != expected_argv[i]) {
                argv_ok = false;
                break;
            }
        }
    }
    if (argv_ok && prepared.argv[expected_argv.size()] != nullptr) {
        argv_ok = false;
    }

    return prepared.ready &&
           prepared.result.stage == app_abi::AppRunStage::start &&
           prepared.result.code == app_abi::AppRunCode::ok &&
           !prepared.result.exited &&
           argv_ok &&
           backend.last.plan.probe.code == expected_probe &&
           counters.console_bytes == 0U &&
           counters.time_now == 0U &&
           counters.display_describe == 0U &&
           counters.display_present == 0U &&
           counters.input_poll == 0U &&
           counters.storage_open == 0U &&
           counters.storage_read == 0U &&
           counters.storage_write == 0U &&
           counters.storage_close == 0U &&
           counters.afe_configure == 0U &&
           counters.afe_read == 0U &&
           counters.app_exit == 0U;
}

void invalidate_api_magic(CharmAppApi& api) noexcept {
    api.magic = 0U;
}

void log_format(app_abi::AppImageFormat format) noexcept {
    switch (format) {
        case app_abi::AppImageFormat::elf:
            qemu_backend::write("elf");
            return;
        case app_abi::AppImageFormat::modulex:
            qemu_backend::write("modulex");
            return;
        case app_abi::AppImageFormat::function:
        default:
            qemu_backend::write("function");
            return;
    }
}

bool run_store_app(std::string_view name,
                   std::string_view arg_text,
                   int expected_exit_code = 0) noexcept {
    std::memset(g_stage_cache, 0, sizeof(g_stage_cache));
    auto media = make_qemu_store_media();
    auto reader = qemu_backend::make_virtual_store_reader(media);
    const auto staged = app_abi::app_store_stage_named_image(
        reader,
        name,
        std::span<std::byte>{g_stage_cache, sizeof(g_stage_cache)});

    qemu_backend::write("resident-elf-qemu: store stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::app_store_read_code_name(staged.code));
    qemu_backend::write(" format=");
    log_format(staged.image.format);
    qemu_backend::write(" size=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(staged.image.image_size));
    qemu_backend::write("\n");

    if (staged.code != app_abi::AppStoreReadCode::ok ||
        staged.image.format != app_abi::AppImageFormat::elf) {
        return false;
    }

    char store_name[48]{};
    constexpr std::string_view kPrefix = "store:";
    std::size_t cursor = 0;
    for (const char ch : kPrefix) {
        store_name[cursor++] = ch;
    }
    const auto copy_len = name.size() < (sizeof(store_name) - cursor - 1U)
        ? name.size()
        : (sizeof(store_name) - cursor - 1U);
    for (std::size_t i = 0; i < copy_len; ++i) {
        store_name[cursor++] = name[i];
    }
    return run_image(std::string_view{store_name, cursor}, staged.image, arg_text, expected_exit_code);
}

bool expect_store_stage_failure(std::string_view name, app_abi::AppStoreReadCode expected_code) noexcept {
    std::memset(g_stage_cache, 0, sizeof(g_stage_cache));
    qemu_backend::reset_capability_counters();
    auto media = make_qemu_store_media();
    auto reader = qemu_backend::make_virtual_store_reader(media);
    const auto staged = app_abi::app_store_stage_named_image(
        reader,
        name,
        std::span<std::byte>{g_stage_cache, sizeof(g_stage_cache)});

    qemu_backend::write("resident-elf-qemu: store stage name=");
    qemu_backend::log_view(name);
    qemu_backend::write(" code=");
    qemu_backend::log_view(app_abi::app_store_read_code_name(staged.code));
    qemu_backend::write(" expected=");
    qemu_backend::log_view(app_abi::app_store_read_code_name(expected_code));
    qemu_backend::write(" image_size=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(staged.image.image_size));
    qemu_backend::write("\n");
    qemu_backend::log_capability_counters(name);

    const auto& counters = qemu_backend::capability_counters();
    return staged.code == expected_code &&
           staged.image.image_base == nullptr &&
           staged.image.image_size == 0U &&
           counters.console_bytes == 0U &&
           counters.time_now == 0U &&
           counters.display_describe == 0U &&
           counters.display_present == 0U &&
           counters.input_poll == 0U &&
           counters.app_exit == 0U;
}

} // namespace

extern "C" int resident_elf_qemu_main() {
    qemu_backend::backend_init();
    qemu_backend::reset_store_media_counters();
    qemu_backend::log_line("resident-elf-qemu: begin");
    qemu_backend::log_backend_identity();
    qemu_backend::log_backend_capabilities();
    qemu_backend::log_backend_scope();
    qemu_backend::log_backend_contract();
    bool ok = qemu_backend::log_backend_self_check();
    ok = qemu_backend::log_backend_reset_self_check() && ok;
    qemu_backend::write("resident-elf-qemu: run-region base=");
    qemu_backend::write_hex32(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_elf_load_region)));
    qemu_backend::write(" expected=");
    qemu_backend::write_hex32(static_cast<std::uint32_t>(kQemuMemory.run_region_base));
    qemu_backend::write(" size=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(sizeof(g_elf_load_region)));
    qemu_backend::write("\n");
    qemu_backend::write("resident-elf-qemu: stage-cache bytes=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(sizeof(g_stage_cache)));
    qemu_backend::write("\n");
    qemu_backend::write("resident-elf-qemu: packetstream-buffers storage=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(sizeof(g_packetstream_storage)));
    qemu_backend::write(" transport=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(sizeof(g_packetstream_transport_buffer)));
    qemu_backend::write(" stream=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(sizeof(g_packetstream_stream)));
    qemu_backend::write(" received=");
    qemu_backend::write_dec(static_cast<std::uint32_t>(sizeof(g_packetstream_received_cache)));
    qemu_backend::write("\n");
    qemu_backend::write("resident-elf-qemu: store entries=");
    qemu_backend::write_dec(qemu_store_entry_count());
    qemu_backend::write(" bytes=");
    qemu_backend::write_dec(qemu_appstore_bin_len);
    qemu_backend::write("\n");

    ok = (reinterpret_cast<std::uintptr_t>(g_elf_load_region) == kQemuMemory.run_region_base) && ok;
    ok = qemu_backend::probe_unsupported_capabilities() && ok;
    ok = run_direct_app("hello_app", hello_app_elf, hello_app_elf_len, "alpha beta") && ok;
    ok = run_received_app("hello_app", hello_app_elf, hello_app_elf_len, "alpha beta") && ok;
    ok = run_packetstream_received_app("hello_app", hello_app_elf, hello_app_elf_len, "alpha beta") && ok;
    ok = run_store_app("hello_app", "alpha beta") && ok;
    ok = run_direct_app("argv_app", argv_app_elf, argv_app_elf_len, "one two three") && ok;
    ok = run_store_app("argv_app", "one two three") && ok;
    constexpr std::string_view kPrepareArgvAppArgs[] = {
        "argv_app",
        "one",
        "two",
        "three",
    };
    ok = expect_prepare_only("prepare:argv_app",
                             app_abi::AppImage{
                                 .name = "argv_app",
                                 .format = app_abi::AppImageFormat::elf,
                                 .image_base = argv_app_elf,
                                 .image_size = argv_app_elf_len,
                             },
                             "one two three",
                             kPrepareArgvAppArgs,
                             app_abi::AppElfProbeCode::ok) && ok;
    ok = expect_runtime_failure("argv_overflow_app",
                                app_abi::AppImage{
                                    .name = "argv_app",
                                    .format = app_abi::AppImageFormat::elf,
                                    .image_base = argv_app_elf,
                                    .image_size = argv_app_elf_len,
                                },
                                "a b c d e f g h i j k l m n o p q",
                                app_abi::AppRunStage::argv,
                                app_abi::AppRunCode::argv_overflow,
                                app_abi::AppElfProbeCode::ok) && ok;
    ok = expect_runtime_failure("abi_mismatch_app",
                                app_abi::AppImage{
                                    .name = "hello_app",
                                    .format = app_abi::AppImageFormat::elf,
                                    .image_base = hello_app_elf,
                                    .image_size = hello_app_elf_len,
                                },
                                "alpha beta",
                                app_abi::AppRunStage::abi,
                                app_abi::AppRunCode::abi_mismatch,
                                app_abi::AppElfProbeCode::ok,
                                invalidate_api_magic) && ok;
    ok = expect_store_stage_failure("missing_app", app_abi::AppStoreReadCode::image_not_found) && ok;
    ok = run_direct_app("afe_error_app", afe_error_app_elf, afe_error_app_elf_len, "") && ok;
    ok = run_store_app("afe_error_app", "") && ok;
    ok = run_direct_app("bss_app", bss_app_elf, bss_app_elf_len, "") && ok;
    ok = run_store_app("bss_app", "") && ok;
    ok = run_direct_app("console_error_app", console_error_app_elf, console_error_app_elf_len, "") && ok;
    ok = run_store_app("console_error_app", "") && ok;
    ok = run_direct_app("data_app", data_app_elf, data_app_elf_len, "") && ok;
    ok = run_store_app("data_app", "") && ok;
    ok = run_direct_app("exit_app", exit_app_elf, exit_app_elf_len, "") && ok;
    ok = run_store_app("exit_app", "") && ok;
    ok = run_direct_app("exit_error_app", exit_error_app_elf, exit_error_app_elf_len, "", 42) && ok;
    ok = run_store_app("exit_error_app", "", 42) && ok;
    ok = run_direct_app("exit_negative_app", exit_negative_app_elf, exit_negative_app_elf_len, "") && ok;
    ok = run_store_app("exit_negative_app", "") && ok;
    ok = run_direct_app("return_negative_app", return_negative_app_elf, return_negative_app_elf_len, "", -5) && ok;
    ok = run_store_app("return_negative_app", "", -5) && ok;
    ok = run_direct_app("unsupported_caps_app", unsupported_caps_app_elf, unsupported_caps_app_elf_len, "") && ok;
    ok = run_store_app("unsupported_caps_app", "") && ok;
    ok = run_direct_app("storage_app", storage_app_elf, storage_app_elf_len, "") && ok;
    ok = run_store_app("storage_app", "") && ok;
    ok = run_direct_app("storage_catalog_app", storage_catalog_app_elf, storage_catalog_app_elf_len, "") && ok;
    ok = run_store_app("storage_catalog_app", "") && ok;
    ok = run_direct_app("storage_close_error_app", storage_close_error_app_elf, storage_close_error_app_elf_len, "") && ok;
    ok = run_store_app("storage_close_error_app", "") && ok;
    ok = run_direct_app("storage_error_app", storage_error_app_elf, storage_error_app_elf_len, "") && ok;
    ok = run_store_app("storage_error_app", "") && ok;
    ok = run_direct_app("storage_fd_exhaustion_app", storage_fd_exhaustion_app_elf, storage_fd_exhaustion_app_elf_len, "") && ok;
    ok = run_store_app("storage_fd_exhaustion_app", "") && ok;
    ok = run_direct_app("storage_open_error_app", storage_open_error_app_elf, storage_open_error_app_elf_len, "") && ok;
    ok = run_store_app("storage_open_error_app", "") && ok;
    ok = run_direct_app("storage_write_error_app", storage_write_error_app_elf, storage_write_error_app_elf_len, "") && ok;
    ok = run_store_app("storage_write_error_app", "") && ok;
    ok = run_direct_app("storage_zero_io_app", storage_zero_io_app_elf, storage_zero_io_app_elf_len, "") && ok;
    ok = run_store_app("storage_zero_io_app", "") && ok;
    ok = run_direct_app("display_describe_error_app", display_describe_error_app_elf, display_describe_error_app_elf_len, "") && ok;
    ok = run_store_app("display_describe_error_app", "") && ok;
    ok = run_direct_app("display_error_app", display_error_app_elf, display_error_app_elf_len, "") && ok;
    ok = run_store_app("display_error_app", "") && ok;
    ok = run_direct_app("display_null_present_app", display_null_present_app_elf, display_null_present_app_elf_len, "") && ok;
    ok = run_store_app("display_null_present_app", "") && ok;
    ok = run_direct_app("display_sequence_app", display_sequence_app_elf, display_sequence_app_elf_len, "") && ok;
    ok = run_store_app("display_sequence_app", "") && ok;
    ok = run_direct_app("input_error_app", input_error_app_elf, input_error_app_elf_len, "") && ok;
    ok = run_store_app("input_error_app", "") && ok;
    ok = run_direct_app("input_sequence_app", input_sequence_app_elf, input_sequence_app_elf_len, "") && ok;
    ok = run_store_app("input_sequence_app", "") && ok;
    ok = run_direct_app("input_wrap_app", input_wrap_app_elf, input_wrap_app_elf_len, "") && ok;
    ok = run_store_app("input_wrap_app", "") && ok;
    ok = run_direct_app("time_app", time_app_elf, time_app_elf_len, "") && ok;
    ok = run_store_app("time_app", "") && ok;
    ok = run_direct_app("time_sequence_app", time_sequence_app_elf, time_sequence_app_elf_len, "") && ok;
    ok = run_store_app("time_sequence_app", "") && ok;
    ok = run_direct_app("large_fit_app", large_fit_app_elf, large_fit_app_elf_len, "") && ok;
    ok = run_received_app("large_fit_app", large_fit_app_elf, large_fit_app_elf_len, "") && ok;
    ok = run_packetstream_received_app("large_fit_app", large_fit_app_elf, large_fit_app_elf_len, "") && ok;
    ok = run_store_app("large_fit_app", "") && ok;
    ok = expect_packetstream_crc_mismatch("packetstream_crc_mismatch",
                                          hello_app_elf,
                                          hello_app_elf_len) && ok;
    ok = expect_received_stage_failure(
        "received_too_large_app",
        std::span<const std::byte>{g_received_oversized_payload,
                                   sizeof(g_received_oversized_payload)},
        app_abi::AppReceivedImageStageCode::buffer_too_small) && ok;
    ok = expect_store_stage_failure("too_large_store_app", app_abi::AppStoreReadCode::image_too_large) && ok;
    ok = expect_load_failure("bad_elf_magic_app",
                             g_bad_elf_magic_payload,
                             sizeof(g_bad_elf_magic_payload),
                             app_abi::AppElfProbeCode::bad_magic) && ok;
    ok = expect_packetstream_load_failure("packetstream_bad_elf_magic_app",
                                          g_bad_elf_magic_payload,
                                          sizeof(g_bad_elf_magic_payload),
                                          app_abi::AppElfProbeCode::bad_magic) && ok;
    ok = expect_load_failure("bad_header_app",
                             g_bad_elf_short_header_payload,
                             sizeof(g_bad_elf_short_header_payload),
                             app_abi::AppElfProbeCode::bad_header) && ok;
    ok = expect_mutated_hello_load_failure("bad_class_app",
                                           mutate_elf_bad_class,
                                           app_abi::AppElfProbeCode::bad_class) && ok;
    ok = expect_mutated_hello_load_failure("bad_endian_app",
                                           mutate_elf_bad_endian,
                                           app_abi::AppElfProbeCode::bad_endian) && ok;
    ok = expect_mutated_hello_load_failure("bad_ident_version_app",
                                           mutate_elf_bad_ident_version,
                                           app_abi::AppElfProbeCode::bad_header) && ok;
    ok = expect_mutated_hello_load_failure("bad_type_app",
                                           mutate_elf_bad_type,
                                           app_abi::AppElfProbeCode::bad_header) && ok;
    ok = expect_mutated_hello_load_failure("bad_machine_app",
                                           mutate_elf_bad_machine,
                                           app_abi::AppElfProbeCode::bad_header) && ok;
    ok = expect_mutated_hello_load_failure("bad_version_app",
                                           mutate_elf_bad_version,
                                           app_abi::AppElfProbeCode::bad_header) && ok;
    ok = expect_mutated_hello_load_failure("bad_ehsize_app",
                                           mutate_elf_bad_ehsize,
                                           app_abi::AppElfProbeCode::bad_header) && ok;
    ok = expect_mutated_hello_load_failure("bad_phentsize_app",
                                           mutate_elf_bad_phentsize,
                                           app_abi::AppElfProbeCode::bad_program_header) && ok;
    ok = expect_mutated_hello_load_failure("bad_program_header_app",
                                           mutate_elf_bad_program_header,
                                           app_abi::AppElfProbeCode::bad_program_header) && ok;
    ok = expect_mutated_hello_load_failure("truncated_payload_app",
                                           mutate_elf_truncated_payload,
                                           app_abi::AppElfProbeCode::truncated_payload) && ok;
    ok = expect_mutated_hello_load_failure("no_load_segment_app",
                                           mutate_elf_no_load_segment,
                                           app_abi::AppElfProbeCode::no_load_segment) && ok;
    ok = expect_mutated_hello_load_failure("entry_outside_segment_app",
                                           mutate_elf_entry_outside_segment,
                                           app_abi::AppElfProbeCode::entry_outside_segment) && ok;
    ok = expect_mutated_hello_load_failure("overlapping_segments_app",
                                           mutate_elf_overlapping_segments,
                                           app_abi::AppElfProbeCode::overlapping_segments) && ok;
    ok = expect_mutated_hello_load_failure("rwx_segment_app",
                                           mutate_elf_first_load_segment_rwx,
                                           app_abi::AppElfProbeCode::rwx_segment) && ok;
    ok = expect_mutated_hello_load_failure("wrong_link_base_app",
                                           mutate_elf_wrong_link_base,
                                           app_abi::AppElfProbeCode::ok) && ok;
    ok = expect_load_failure("too_large_app",
                             too_large_app_elf,
                             too_large_app_elf_len,
                             app_abi::AppElfProbeCode::load_buffer_too_small) && ok;
    ok = expect_load_failure_with_buffer("unaligned_load_buffer_app",
                                         hello_app_elf,
                                         hello_app_elf_len,
                                         app_abi::AppElfProbeCode::load_buffer_unaligned,
                                         app_abi::AppLoadBuffer{
                                             .base = g_elf_load_region + 1U,
                                             .size = sizeof(g_elf_load_region) - 1U,
                                             .align = 16U,
                                         }) && ok;
    ok = run_direct_app("player_min", player_min_elf, player_min_elf_len, "") && ok;
    ok = run_received_app("player_min", player_min_elf, player_min_elf_len, "") && ok;
    ok = run_packetstream_received_app("player_min", player_min_elf, player_min_elf_len, "") && ok;
    ok = run_store_app("player_min", "") && ok;
    const auto store_media = make_qemu_store_media();
    qemu_backend::log_store_media_counters(store_media);
    const auto& store_counters = qemu_backend::store_media_counters();
    ok = store_counters.read_calls > 0U &&
         store_counters.read_bytes > 0U &&
         store_counters.read_failures == 0U &&
         ok;

    if (ok) {
        qemu_backend::log_line("resident-elf-qemu: ok");
    } else {
        qemu_backend::log_line("resident-elf-qemu: fail");
    }

    while (true) {
    }
}
