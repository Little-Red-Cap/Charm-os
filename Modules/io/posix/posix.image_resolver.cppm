module;

#include <array>
#include <string_view>

export module posix.image_resolver;

import posix.exec_loader;
import posix.exec_source;
import posix.proc_types;
import posix.program_catalog;
import posix.program_image;
import util.core;
import util.error;

export namespace posix {
    template <class Service, class Catalog, class Registry, util::usize MaxPathLen>
    util::Result<ProgramImage> resolve_program_image(Service& service,
                                                     const SpawnConfig& cfg,
                                                     Catalog& catalog,
                                                     Registry& elf_mem_registry,
                                                     std::array<char, MaxPathLen>& resolved_path) noexcept {
        if (is_elf_mem_prefixed(cfg.path, cfg.argv)) {
            auto mem_name = resolve_elf_mem_name(cfg.path, cfg.argv);
            if (mem_name.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto* mem = elf_mem_registry.find(mem_name);
            if (!mem) {
                return util::unexpected(util::Errc::noent);
            }
            return load_elf_program_from_buffer(service, mem->data, mem->size);
        }

        if (is_elf_prefixed(cfg.path, cfg.argv)) {
            auto elf_path = resolve_elf_path(cfg.path, cfg.argv);
            if (elf_path.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return load_elf_program_from_file(service, elf_path);
        }

        const std::string_view name = resolve_registered_name<MaxPathLen>(
            cfg.path_mode == PathMode::search_path,
            cfg.path,
            cfg.argv,
            cfg.envp,
            resolved_path,
            [&](std::string_view candidate) noexcept { return catalog.find(candidate) != nullptr; });

        auto* entry = catalog.find(name);
        if (entry) {
            return entry->image;
        }

        auto elf_candidate = load_elf_candidate(service, cfg, resolved_path);
        if (elf_candidate) {
            return elf_candidate;
        }
        if (elf_candidate.error() == util::Errc::noent ||
            elf_candidate.error() == util::Errc::not_supported) {
            return util::unexpected(util::Errc::noent);
        }
        return util::unexpected(elf_candidate.error());
    }
}
