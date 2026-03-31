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
        if (cfg.entry_override) {
            image.entry = cfg.entry_override;
        } else if (cfg.use_entry_symbol) {
            const std::string_view needle = cfg.entry_symbol ? std::string_view{cfg.entry_symbol}
                                                             : std::string_view{"entry"};
            for (util::u32 i = 0; i < loaded.sym_count; ++i) {
                auto v = loaded.sym_reader.at(loaded.symtab[i]);
                if (v.name == needle) {
                    image.entry = modulex::addr_to_ptr<ImageEntry>(loaded.symtab[i].value);
                    break;
                }
            }
        } else {
            image.entry = modulex::addr_to_ptr<ImageEntry>(loaded.entry);
        }
        if (!image.entry) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        return image;
    }
}
