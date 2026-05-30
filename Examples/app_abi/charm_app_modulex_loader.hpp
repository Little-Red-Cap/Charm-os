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
            return AppRunCode::load_failed;
    }
    return AppRunCode::load_failed;
}

[[nodiscard]] inline CharmAppMainFn app_modulex_resolve_entry(
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
            return reinterpret_cast<CharmAppMainFn>(sym.value);
        }
        if (header == nullptr || sym.value >= header->text_size) {
            return nullptr;
        }
        return reinterpret_cast<CharmAppMainFn>(text_base + sym.value);
    }
    return nullptr;
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
            result.code = AppModuleXLoadCode::dependency_failed;
            result.load.code = app_modulex_run_code(result.code);
            result.load.backend_error = static_cast<int>(result.code);
            return result;
        }
    } else if (config.resolve_dependency != nullptr) {
        if (!modulex::Linker::validate_deps(view, config.resolve_dependency)) {
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

    if (!modulex::Linker::relocate(view)) {
        result.code = AppModuleXLoadCode::relocate_failed;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    const auto entry = app_modulex_resolve_entry(loaded, config.entry_symbol);
    if (entry == nullptr) {
        result.code = AppModuleXLoadCode::entry_missing;
        result.load.code = app_modulex_run_code(result.code);
        result.load.backend_error = static_cast<int>(result.code);
        return result;
    }

    result.code = AppModuleXLoadCode::ok;
    result.load = AppLoadResult{
        .code = AppRunCode::ok,
        .image = LoadedAppImage::from_entry(image.name, image.format, entry),
    };
    return result;
}

} // namespace charm::app_abi
