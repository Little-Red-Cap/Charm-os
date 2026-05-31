#pragma once

#include "charm_app_runtime.hpp"

#include <cstdint>
#include <limits>
#include <string_view>

// Include this header after importing module_core/module_view/module_loader/module_link.
namespace charm::app_abi {

enum class AppModuleXLoadCode : std::uint8_t {
    ok,
    invalid_argument,
    format_mismatch,
    load_failed,
    dependency_failed,
    external_failed,
    relocate_failed,
    entry_missing,
    unsupported_bss,
    unsupported_xip,
};

struct AppModuleXLoadConfig {
    modulex::ResolveExternal resolve_external{nullptr};
    modulex::ResolveDependency resolve_dependency{nullptr};
    modulex::ResolveDependencyCtx resolve_dependency_ctx{nullptr};
    void* dependency_ctx{nullptr};
    std::string_view entry_symbol{"charm_app_main"};
};

struct AppModuleXLoadResult {
    AppModuleXLoadCode code{AppModuleXLoadCode::ok};
    AppLoadResult load{};
    modulex::ImageError validate_error{modulex::ImageError::ok};
    modulex::DepError dependency_error{modulex::DepError::ok};
    std::uint32_t dependency_index{0};
    std::uint32_t entry_offset{0};
    std::uint32_t image_span{0};
    bool relocated{false};
};

[[nodiscard]] constexpr std::string_view app_modulex_load_code_name(
    AppModuleXLoadCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case AppModuleXLoadCode::ok:
            return "ok"sv;
        case AppModuleXLoadCode::invalid_argument:
            return "invalid_argument"sv;
        case AppModuleXLoadCode::format_mismatch:
            return "format_mismatch"sv;
        case AppModuleXLoadCode::load_failed:
            return "load_failed"sv;
        case AppModuleXLoadCode::dependency_failed:
            return "dependency_failed"sv;
        case AppModuleXLoadCode::external_failed:
            return "external_failed"sv;
        case AppModuleXLoadCode::relocate_failed:
            return "relocate_failed"sv;
        case AppModuleXLoadCode::entry_missing:
            return "entry_missing"sv;
        case AppModuleXLoadCode::unsupported_bss:
            return "unsupported_bss"sv;
        case AppModuleXLoadCode::unsupported_xip:
            return "unsupported_xip"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::string_view app_modulex_image_error_name(
    modulex::ImageError error) noexcept {
    using namespace std::literals::string_view_literals;
    switch (error) {
        case modulex::ImageError::ok:
            return "ok"sv;
        case modulex::ImageError::null_image:
            return "null_image"sv;
        case modulex::ImageError::bad_magic:
            return "bad_magic"sv;
        case modulex::ImageError::bad_version:
            return "bad_version"sv;
        case modulex::ImageError::bad_size:
            return "bad_size"sv;
        case modulex::ImageError::bad_entry:
            return "bad_entry"sv;
        case modulex::ImageError::bad_reloc:
            return "bad_reloc"sv;
        case modulex::ImageError::bad_sym:
            return "bad_sym"sv;
        case modulex::ImageError::bad_dep:
            return "bad_dep"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::string_view app_modulex_dep_error_name(
    modulex::DepError error) noexcept {
    using namespace std::literals::string_view_literals;
    switch (error) {
        case modulex::DepError::ok:
            return "ok"sv;
        case modulex::DepError::bad_name:
            return "bad_name"sv;
        case modulex::DepError::resolve_failed:
            return "resolve_failed"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr AppRunCode app_modulex_run_code(AppModuleXLoadCode code) noexcept {
    switch (code) {
        case AppModuleXLoadCode::ok:
            return AppRunCode::ok;
        case AppModuleXLoadCode::format_mismatch:
            return AppRunCode::not_supported;
        case AppModuleXLoadCode::invalid_argument:
        case AppModuleXLoadCode::load_failed:
        case AppModuleXLoadCode::dependency_failed:
        case AppModuleXLoadCode::external_failed:
        case AppModuleXLoadCode::relocate_failed:
        case AppModuleXLoadCode::entry_missing:
        case AppModuleXLoadCode::unsupported_bss:
        case AppModuleXLoadCode::unsupported_xip:
            return AppRunCode::load_failed;
    }
    return AppRunCode::load_failed;
}

struct AppModuleXEntryResolveResult {
    CharmAppMainFn entry{nullptr};
    std::uint32_t offset{0};
};

[[nodiscard]] inline AppModuleXEntryResolveResult app_modulex_resolve_entry(
    const modulex::LoadResult& loaded,
    std::string_view symbol) noexcept {
    if (symbol.empty()) {
        symbol = "charm_app_main";
    }
    const auto* header = loaded.sym_reader.img;
    const modulex::Addr text_base = header != nullptr
        ? modulex::to_addr(header) + modulex::layout_text(*header)
        : modulex::Addr{0};
    for (std::uint32_t i = 0; i < loaded.sym_count; ++i) {
        const auto view = loaded.sym_reader.at(loaded.symtab[i]);
        if (view.name != symbol) {
            continue;
        }
        const auto& sym = loaded.symtab[i];
        if (sym.kind == modulex::SymbolKind::external) {
            return {
                reinterpret_cast<CharmAppMainFn>(sym.value),
                static_cast<std::uint32_t>(0xFFFFFFFFU),
            };
        }
        if (header == nullptr || sym.value >= header->text_size) {
            return {};
        }
        return {
            reinterpret_cast<CharmAppMainFn>(text_base + sym.value),
            static_cast<std::uint32_t>(sym.value),
        };
    }
    return {};
}

[[nodiscard]] inline AppModuleXLoadResult app_modulex_load_image(
    const AppImage& image,
    const AppModuleXLoadConfig& config = {}) noexcept {
    AppModuleXLoadResult result{};
    if (image.image_base == nullptr || image.image_size < sizeof(modulex::ImageHeader)) {
        result.code = AppModuleXLoadCode::invalid_argument;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }
    if (image.image_size > std::numeric_limits<std::uint32_t>::max()) {
        result.code = AppModuleXLoadCode::invalid_argument;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }
    if (image.format != AppImageFormat::modulex) {
        result.code = AppModuleXLoadCode::format_mismatch;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    const auto view = modulex::make_view(
        image.image_base,
        static_cast<std::uint32_t>(image.image_size));
    const auto validation = modulex::validate(view);
    result.validate_error = validation.err;
    if (!validation.ok) {
        result.code = AppModuleXLoadCode::load_failed;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    const auto& header = *view.header;
    result.image_span = header.image_size != 0U ? header.image_size : view.size;
    if (header.bss_size != 0U) {
        result.code = AppModuleXLoadCode::unsupported_bss;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }
    const auto xip_flags =
        static_cast<std::uint16_t>(modulex::ImageFlags::xip_text) |
        static_cast<std::uint16_t>(modulex::ImageFlags::xip_ro) |
        static_cast<std::uint16_t>(modulex::ImageFlags::xip_data);
    if ((header.flags & xip_flags) != 0U) {
        result.code = AppModuleXLoadCode::unsupported_xip;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    const auto loaded = modulex::Loader::load(view);
    if (!loaded.ok) {
        result.code = AppModuleXLoadCode::load_failed;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    if (config.resolve_dependency_ctx != nullptr) {
        const auto dep = modulex::Linker::validate_deps_ctx(
            view,
            config.resolve_dependency_ctx,
            config.dependency_ctx);
        if (!dep.ok) {
            result.dependency_error = dep.error;
            result.dependency_index = dep.failed_index;
            result.code = AppModuleXLoadCode::dependency_failed;
            result.load.code = app_modulex_run_code(result.code);
            result.load.backend_error = static_cast<int>(result.code);
            return result;
        }
    } else if (config.resolve_dependency != nullptr) {
        if (!modulex::Linker::validate_deps(view, config.resolve_dependency)) {
            result.dependency_error = modulex::DepError::resolve_failed;
            result.code = AppModuleXLoadCode::dependency_failed;
            result.load.code = app_modulex_run_code(result.code);
            result.load.backend_error = static_cast<int>(result.code);
            return result;
        }
    }

    if (config.resolve_external != nullptr &&
        !modulex::Linker::bind_externals(view, config.resolve_external)) {
        result.code = AppModuleXLoadCode::external_failed;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    const bool has_relocations = header.rel_size != 0U;
    if (!modulex::Linker::relocate(view)) {
        result.code = AppModuleXLoadCode::relocate_failed;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }
    result.relocated = has_relocations;

    const auto entry = app_modulex_resolve_entry(loaded, config.entry_symbol);
    result.entry_offset = entry.offset;
    if (entry.entry == nullptr) {
        result.code = AppModuleXLoadCode::entry_missing;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    result.code = AppModuleXLoadCode::ok;
    result.load = AppLoadResult{
        .code = AppRunCode::ok,
        .image = LoadedAppImage::from_entry(image.name, image.format, entry.entry),
    };
    return result;
}

} // namespace charm::app_abi
