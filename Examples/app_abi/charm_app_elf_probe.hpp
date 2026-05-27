#pragma once

#include "charm_app_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::app_abi {

enum class AppElfProbeCode : std::uint8_t {
    ok,
    invalid_argument,
    format_mismatch,
    bad_magic,
    bad_class,
    bad_endian,
    bad_header,
    bad_program_header,
    truncated_payload,
    no_load_segment,
    entry_outside_segment,
    overlapping_segments,
    rwx_segment,
    load_buffer_too_small,
    load_buffer_unaligned,
};

struct AppElfProbeResult {
    AppElfProbeCode code{AppElfProbeCode::ok};
    std::uint32_t entry_offset{0};
    std::uint32_t load_span{0};
    std::uint32_t segment_count{0};
    bool runnable{false};
};

struct AppElfLoadPlan {
    AppElfProbeResult probe{};
    LoadedAppImage loaded{};
    std::uintptr_t load_base{0};
    std::uintptr_t entry_address{0};
};

struct AppElfLoadPlanResult {
    AppRunCode code{AppRunCode::ok};
    int backend_error{0};
    AppElfLoadPlan plan{};
};

struct AppElfLoadBackend {
    AppElfLoadPlanResult last{};
};

struct AppElf32Header {
    std::uint8_t ident[16]{};
    std::uint16_t type{0};
    std::uint16_t machine{0};
    std::uint32_t version{0};
    std::uint32_t entry{0};
    std::uint32_t phoff{0};
    std::uint32_t shoff{0};
    std::uint32_t flags{0};
    std::uint16_t ehsize{0};
    std::uint16_t phentsize{0};
    std::uint16_t phnum{0};
    std::uint16_t shentsize{0};
    std::uint16_t shnum{0};
    std::uint16_t shstrndx{0};
};

struct AppElf32ProgramHeader {
    std::uint32_t type{0};
    std::uint32_t offset{0};
    std::uint32_t vaddr{0};
    std::uint32_t paddr{0};
    std::uint32_t filesz{0};
    std::uint32_t memsz{0};
    std::uint32_t flags{0};
    std::uint32_t align{0};
};

inline constexpr std::uint32_t kAppElfPtLoad = 1U;
inline constexpr std::uint32_t kAppElfPfX = 0x1U;
inline constexpr std::uint32_t kAppElfPfW = 0x2U;

[[nodiscard]] constexpr std::string_view app_elf_probe_code_name(AppElfProbeCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case AppElfProbeCode::ok:
            return "ok"sv;
        case AppElfProbeCode::invalid_argument:
            return "invalid_argument"sv;
        case AppElfProbeCode::format_mismatch:
            return "format_mismatch"sv;
        case AppElfProbeCode::bad_magic:
            return "bad_magic"sv;
        case AppElfProbeCode::bad_class:
            return "bad_class"sv;
        case AppElfProbeCode::bad_endian:
            return "bad_endian"sv;
        case AppElfProbeCode::bad_header:
            return "bad_header"sv;
        case AppElfProbeCode::bad_program_header:
            return "bad_program_header"sv;
        case AppElfProbeCode::truncated_payload:
            return "truncated_payload"sv;
        case AppElfProbeCode::no_load_segment:
            return "no_load_segment"sv;
        case AppElfProbeCode::entry_outside_segment:
            return "entry_outside_segment"sv;
        case AppElfProbeCode::overlapping_segments:
            return "overlapping_segments"sv;
        case AppElfProbeCode::rwx_segment:
            return "rwx_segment"sv;
        case AppElfProbeCode::load_buffer_too_small:
            return "load_buffer_too_small"sv;
        case AppElfProbeCode::load_buffer_unaligned:
            return "load_buffer_unaligned"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr AppRunCode app_elf_probe_code_to_run_code(AppElfProbeCode code) noexcept {
    switch (code) {
        case AppElfProbeCode::ok:
            return AppRunCode::ok;
        case AppElfProbeCode::invalid_argument:
            return AppRunCode::invalid_argument;
        case AppElfProbeCode::format_mismatch:
            return AppRunCode::not_supported;
        case AppElfProbeCode::bad_magic:
        case AppElfProbeCode::bad_class:
        case AppElfProbeCode::bad_endian:
        case AppElfProbeCode::bad_header:
        case AppElfProbeCode::bad_program_header:
        case AppElfProbeCode::truncated_payload:
        case AppElfProbeCode::no_load_segment:
        case AppElfProbeCode::entry_outside_segment:
        case AppElfProbeCode::overlapping_segments:
        case AppElfProbeCode::rwx_segment:
        case AppElfProbeCode::load_buffer_too_small:
        case AppElfProbeCode::load_buffer_unaligned:
            return AppRunCode::load_failed;
    }
    return AppRunCode::load_failed;
}

[[nodiscard]] constexpr std::uint32_t app_elf_align_remainder(std::uintptr_t value,
                                                              std::size_t align) noexcept {
    if (align <= 1U) {
        return 0U;
    }
    return static_cast<std::uint32_t>(value % align);
}

[[nodiscard]] inline AppElfProbeResult app_elf_probe_load(const AppImage& image,
                                                          const AppLoadBuffer& buffer) noexcept {
    AppElfProbeResult result{};
    if (image.image_base == nullptr || image.image_size == 0U || buffer.base == nullptr ||
        buffer.size == 0U) {
        result.code = AppElfProbeCode::invalid_argument;
        return result;
    }
    if (image.format != AppImageFormat::elf) {
        result.code = AppElfProbeCode::format_mismatch;
        return result;
    }
    if (app_elf_align_remainder(reinterpret_cast<std::uintptr_t>(buffer.base), buffer.align) != 0U) {
        result.code = AppElfProbeCode::load_buffer_unaligned;
        return result;
    }
    if (image.image_size < sizeof(AppElf32Header)) {
        result.code = AppElfProbeCode::bad_header;
        return result;
    }

    const auto* bytes = static_cast<const std::byte*>(image.image_base);
    const auto* raw = reinterpret_cast<const std::uint8_t*>(image.image_base);
    if (raw[0] != 0x7f || raw[1] != 'E' || raw[2] != 'L' || raw[3] != 'F') {
        result.code = AppElfProbeCode::bad_magic;
        return result;
    }
    if (raw[4] != 1U) {
        result.code = AppElfProbeCode::bad_class;
        return result;
    }
    if (raw[5] != 1U) {
        result.code = AppElfProbeCode::bad_endian;
        return result;
    }

    auto header = AppElf32Header{};
    std::memcpy(&header, image.image_base, sizeof(header));
    if (header.phoff > image.image_size || header.phentsize < sizeof(AppElf32ProgramHeader) ||
        header.phnum == 0U) {
        result.code = AppElfProbeCode::bad_program_header;
        return result;
    }
    const auto ph_table_bytes =
        static_cast<std::uint64_t>(header.phentsize) * static_cast<std::uint64_t>(header.phnum);
    if (static_cast<std::uint64_t>(header.phoff) + ph_table_bytes > image.image_size) {
        result.code = AppElfProbeCode::bad_program_header;
        return result;
    }

    bool min_set = false;
    std::uint32_t min_vaddr = 0;
    std::uint32_t max_vaddr = 0;
    std::uint32_t load_segments = 0;

    auto read_ph = [&](std::uint16_t index) noexcept {
        AppElf32ProgramHeader ph{};
        std::memcpy(&ph, bytes + header.phoff + (static_cast<std::uint32_t>(header.phentsize) * index), sizeof(ph));
        return ph;
    };

    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto ph = read_ph(i);
        if (ph.type != kAppElfPtLoad || (ph.memsz == 0U && ph.filesz == 0U)) {
            continue;
        }
        if ((ph.flags & kAppElfPfW) != 0U && (ph.flags & kAppElfPfX) != 0U) {
            result.code = AppElfProbeCode::rwx_segment;
            return result;
        }
        if (ph.memsz < ph.filesz ||
            static_cast<std::uint64_t>(ph.offset) + static_cast<std::uint64_t>(ph.filesz) > image.image_size) {
            result.code = AppElfProbeCode::truncated_payload;
            return result;
        }
        if (ph.align != 0U && (ph.offset % ph.align) != (ph.vaddr % ph.align)) {
            result.code = AppElfProbeCode::bad_program_header;
            return result;
        }
        if (!min_set || ph.vaddr < min_vaddr) {
            min_vaddr = ph.vaddr;
            min_set = true;
        }
        const auto end = ph.vaddr + ph.memsz;
        if (end < ph.vaddr) {
            result.code = AppElfProbeCode::bad_program_header;
            return result;
        }
        if (end > max_vaddr) {
            max_vaddr = end;
        }
        ++load_segments;
    }

    if (load_segments == 0U || !min_set || max_vaddr <= min_vaddr) {
        result.code = AppElfProbeCode::no_load_segment;
        return result;
    }

    const auto span = max_vaddr - min_vaddr;
    if (span > buffer.size) {
        result.code = AppElfProbeCode::load_buffer_too_small;
        result.load_span = span;
        result.segment_count = load_segments;
        return result;
    }

    bool entry_ok = false;
    for (std::uint16_t i = 0; i < header.phnum; ++i) {
        const auto ph = read_ph(i);
        if (ph.type != kAppElfPtLoad || (ph.memsz == 0U && ph.filesz == 0U)) {
            continue;
        }
        const auto dst_off = ph.vaddr - min_vaddr;
        const auto end_off = dst_off + ph.memsz;
        for (std::uint16_t j = 0; j < i; ++j) {
            const auto prev = read_ph(j);
            if (prev.type != kAppElfPtLoad || (prev.memsz == 0U && prev.filesz == 0U)) {
                continue;
            }
            const auto prev_off = prev.vaddr - min_vaddr;
            const auto prev_end = prev_off + prev.memsz;
            if (dst_off < prev_end && prev_off < end_off) {
                result.code = AppElfProbeCode::overlapping_segments;
                return result;
            }
        }
        if (header.entry >= ph.vaddr && header.entry < (ph.vaddr + ph.memsz)) {
            entry_ok = true;
        }
        auto* dst = static_cast<std::byte*>(buffer.base) + dst_off;
        std::memcpy(dst, bytes + ph.offset, ph.filesz);
        if (ph.memsz > ph.filesz) {
            std::memset(dst + ph.filesz, 0, ph.memsz - ph.filesz);
        }
    }

    if (!entry_ok || header.entry < min_vaddr) {
        result.code = AppElfProbeCode::entry_outside_segment;
        return result;
    }

    result.code = AppElfProbeCode::ok;
    result.entry_offset = header.entry - min_vaddr;
    result.load_span = span;
    result.segment_count = load_segments;
    result.runnable = true;
    return result;
}

[[nodiscard]] inline AppElfLoadPlanResult app_elf_load_plan(const AppImage& image,
                                                            const AppLoadBuffer& buffer) noexcept {
    AppElfLoadPlanResult result{};
    result.plan.probe = app_elf_probe_load(image, buffer);
    result.plan.load_base = reinterpret_cast<std::uintptr_t>(buffer.base);
    result.code = app_elf_probe_code_to_run_code(result.plan.probe.code);
    if (result.plan.probe.code != AppElfProbeCode::ok) {
        result.backend_error = static_cast<int>(result.plan.probe.code);
        return result;
    }

    result.plan.entry_address = result.plan.load_base + result.plan.probe.entry_offset;
    result.plan.loaded = LoadedAppImage::from_entry(image.name, image.format, result.plan.entry_address);
    return result;
}

[[nodiscard]] inline AppLoadResult app_elf_load_image(AppElfLoadBackend* backend,
                                                      const AppImage& image,
                                                      const AppLoadBuffer& buffer) noexcept {
    if (backend == nullptr) {
        return AppLoadResult{.code = AppRunCode::invalid_argument};
    }

    backend->last = app_elf_load_plan(image, buffer);
    if (backend->last.code != AppRunCode::ok) {
        return AppLoadResult{
            .code = backend->last.code,
            .backend_error = backend->last.backend_error,
        };
    }

    return AppLoadResult{
        .code = AppRunCode::ok,
        .image = backend->last.plan.loaded,
    };
}

} // namespace charm::app_abi
