module;

#include <string_view>

export module posix.program_image_modulex;

import module_core;
import module_loader;
import module_link;
import posix.program_image;
import util.core;
import util.error;

export namespace posix {
    inline ImageEntry addr_to_entry(modulex::Addr addr) noexcept {
        return reinterpret_cast<ImageEntry>(addr);
    }

    inline ImageEntry resolve_modulex_entry_symbol(const modulex::LoadResult& loaded,
                                                   std::string_view symbol) noexcept {
        for (util::u32 i = 0; i < loaded.sym_count; ++i) {
            auto v = loaded.sym_reader.at(loaded.symtab[i]);
            if (v.name == symbol) {
                return addr_to_entry(loaded.symtab[i].value);
            }
        }
        return nullptr;
    }

    struct ModuleXLoadConfig {
        modulex::ResolveExternal resolve_external{nullptr};
        modulex::ResolveDependency resolve_dependency{nullptr};
        modulex::ResolveDependencyCtx resolve_dependency_ctx{nullptr};
        void* dep_ctx{nullptr};
        ImageEntry entry_override{nullptr}; // test-only escape hatch; avoid in real images.
        bool use_entry_symbol{false};
        const char* entry_symbol{nullptr};
    };

    inline util::Result<ProgramImage> load_modulex_image(std::string_view name,
                                                         const modulex::ImageHeader* header,
                                                         const ModuleXLoadConfig& cfg) noexcept {
        if (!header) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto loaded = modulex::Loader::load(header);
        if (!loaded.ok) {
            return util::unexpected(util::Errc::invalid_arg);
        }

        const auto base = modulex::to_addr(header);
        if (cfg.resolve_dependency_ctx) {
            const auto dep = modulex::Linker::validate_deps_ctx(header, base,
                cfg.resolve_dependency_ctx, cfg.dep_ctx);
            if (!dep.ok) {
                return util::unexpected(util::Errc::not_supported);
            }
        } else if (cfg.resolve_dependency) {
            if (!modulex::Linker::validate_deps(header, base, cfg.resolve_dependency)) {
                return util::unexpected(util::Errc::not_supported);
            }
        }

        if (cfg.resolve_external) {
            if (!modulex::Linker::bind_externals(header, base, cfg.resolve_external)) {
                return util::unexpected(util::Errc::not_supported);
            }
        }

        if (!modulex::Linker::relocate(header, base)) {
            return util::unexpected(util::Errc::invalid_arg);
        }

        ProgramImage image{};
        image.kind = ImageKind::modulex;
        image.name = name;
        image.entry_abi = ImageEntryAbi::main_argv_envp_v1;
        if (cfg.entry_override) {
            image.entry = cfg.entry_override;
        } else if (cfg.use_entry_symbol) {
            const std::string_view needle = cfg.entry_symbol ? std::string_view{cfg.entry_symbol}
                                                             : std::string_view{"entry"};
            image.entry = resolve_modulex_entry_symbol(loaded, needle);
        } else {
            image.entry = addr_to_entry(loaded.entry);
        }
        if (!image.entry) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        return image;
    }
}
