#pragma once

#include "charm_app_api.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

namespace charm::app_abi {

enum class AppImageFormat : std::uint8_t {
    function,
    elf,
    modulex,
};

enum class AppRunStage : std::uint8_t {
    idle,
    lookup,
    load,
    abi,
    argv,
    start,
    exit,
};

enum class AppRunCode : std::uint8_t {
    ok,
    invalid_argument,
    image_not_found,
    not_supported,
    load_failed,
    abi_missing,
    abi_mismatch,
    argv_overflow,
    argv_name_too_long,
};

struct AppImage {
    std::string_view name{};
    AppImageFormat format{AppImageFormat::function};
    const void* image_base{nullptr};
    std::size_t image_size{0};
    const void* user{nullptr};
};

struct AppLoadBuffer {
    void* base{nullptr};
    std::size_t size{0};
    std::size_t align{0};
    void (*prepare)(void* ctx) noexcept{nullptr};
    void* prepare_ctx{nullptr};
};

struct LoadedAppImage {
    std::string_view name{};
    AppImageFormat format{AppImageFormat::function};
    CharmAppMainFn entry{nullptr};

    template <class Entry>
    static LoadedAppImage from_entry(std::string_view image_name,
                                     AppImageFormat image_format,
                                     Entry raw_entry) noexcept {
        static_assert(sizeof(Entry) == sizeof(CharmAppMainFn),
                      "dynamic App entry pointer must fit CharmAppMainFn");
        return LoadedAppImage{
            .name = image_name,
            .format = image_format,
            .entry = std::bit_cast<CharmAppMainFn>(raw_entry),
        };
    }
};

struct AppLoadResult {
    AppRunCode code{AppRunCode::ok};
    int backend_error{0};
    LoadedAppImage image{};
};

struct AppImageSource {
    void* ctx{nullptr};
    const AppImage* (*find)(void* ctx, std::string_view name) noexcept{nullptr};
    AppLoadResult (*load)(void* ctx, const AppImage& image, const AppLoadBuffer& buffer) noexcept{nullptr};
};

struct AppRunConfig {
    const AppImageSource* source{nullptr};
    AppLoadBuffer load_buffer{};
    CharmAppApi* api{nullptr};
    std::string_view name{};
    std::string_view arg_text{};
};

struct AppRunResult {
    AppRunStage stage{AppRunStage::idle};
    AppRunCode code{AppRunCode::ok};
    int backend_error{0};
    std::string_view name{};
    int exit_code{0};
    bool exited{false};
};

struct AppPreparedRun {
    AppRunResult result{};
    LoadedAppImage image{};
    CharmAppApi* api{nullptr};
    int argc{0};
    char** argv{nullptr};
    bool ready{false};
};

[[nodiscard]] constexpr std::string_view stage_name(AppRunStage stage) noexcept {
    using namespace std::literals::string_view_literals;
    switch (stage) {
        case AppRunStage::idle:
            return "idle"sv;
        case AppRunStage::lookup:
            return "lookup"sv;
        case AppRunStage::load:
            return "load"sv;
        case AppRunStage::abi:
            return "abi"sv;
        case AppRunStage::argv:
            return "argv"sv;
        case AppRunStage::start:
            return "start"sv;
        case AppRunStage::exit:
            return "exit"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::string_view code_name(AppRunCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case AppRunCode::ok:
            return "ok"sv;
        case AppRunCode::invalid_argument:
            return "invalid_argument"sv;
        case AppRunCode::image_not_found:
            return "image_not_found"sv;
        case AppRunCode::not_supported:
            return "not_supported"sv;
        case AppRunCode::load_failed:
            return "load_failed"sv;
        case AppRunCode::abi_missing:
            return "abi_missing"sv;
        case AppRunCode::abi_mismatch:
            return "abi_mismatch"sv;
        case AppRunCode::argv_overflow:
            return "argv_overflow"sv;
        case AppRunCode::argv_name_too_long:
            return "argv_name_too_long"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] inline bool validate_api(const CharmAppApi* api) noexcept {
    return api != nullptr &&
           api->magic == CHARM_APP_API_MAGIC &&
           api->version == CHARM_APP_API_VERSION &&
           api->size >= sizeof(CharmAppApi);
}

template <std::size_t MaxArgs = 16, std::size_t MaxArgBlob = 192, std::size_t MaxName = 48>
class AppRuntime {
public:
    [[nodiscard]] AppPreparedRun prepare(const AppRunConfig& config) noexcept {
        AppPreparedRun prepared{};
        prepared.result.name = config.name;

        if (config.source == nullptr || config.source->find == nullptr ||
            config.source->load == nullptr || config.name.empty()) {
            return fail_prepare(prepared, AppRunStage::lookup, AppRunCode::invalid_argument);
        }

        prepared.result.stage = AppRunStage::lookup;
        const AppImage* image = config.source->find(config.source->ctx, config.name);
        if (image == nullptr) {
            return fail_prepare(prepared, AppRunStage::lookup, AppRunCode::image_not_found);
        }
        prepared.result.name = image->name;

        prepared.result.stage = AppRunStage::load;
        AppLoadResult loaded = config.source->load(config.source->ctx, *image, config.load_buffer);
        if (loaded.code != AppRunCode::ok) {
            return fail_prepare(prepared, AppRunStage::load, loaded.code, loaded.backend_error);
        }
        if (config.load_buffer.prepare != nullptr) {
            config.load_buffer.prepare(config.load_buffer.prepare_ctx);
        }

        prepared.result.stage = AppRunStage::abi;
        if (loaded.image.entry == nullptr) {
            return fail_prepare(prepared, AppRunStage::abi, AppRunCode::abi_missing);
        }
        if (!validate_api(config.api)) {
            return fail_prepare(prepared, AppRunStage::abi, AppRunCode::abi_mismatch);
        }

        prepared.result.stage = AppRunStage::argv;
        if (image->name.size() + 1U > name_.size()) {
            return fail_prepare(prepared, AppRunStage::argv, AppRunCode::argv_name_too_long);
        }
        const auto parsed = build_argv(image->name, config.arg_text);
        if (parsed != AppRunCode::ok) {
            return fail_prepare(prepared, AppRunStage::argv, parsed);
        }

        prepared.result.stage = AppRunStage::start;
        prepared.result.code = AppRunCode::ok;
        prepared.result.exited = false;
        prepared.image = loaded.image;
        prepared.api = config.api;
        prepared.argc = static_cast<int>(argc_);
        prepared.argv = argv_.data();
        prepared.ready = true;
        return prepared;
    }

    [[nodiscard]] AppRunResult run(const AppRunConfig& config) noexcept {
        const auto prepared = prepare(config);
        if (!prepared.ready) {
            return prepared.result;
        }

        auto result = prepared.result;
        result.stage = AppRunStage::start;
        const int rc = prepared.image.entry(prepared.api, prepared.argc, prepared.argv);
        result.stage = AppRunStage::exit;
        result.code = AppRunCode::ok;
        result.exit_code = rc;
        result.exited = true;
        return result;
    }

private:
    static AppRunResult fail(AppRunResult result,
                             AppRunStage stage,
                             AppRunCode code,
                             int backend_error = 0) noexcept {
        result.stage = stage;
        result.code = code;
        result.backend_error = backend_error;
        result.exited = false;
        return result;
    }

    static AppPreparedRun fail_prepare(AppPreparedRun prepared,
                                       AppRunStage stage,
                                       AppRunCode code,
                                       int backend_error = 0) noexcept {
        prepared.result = fail(prepared.result, stage, code, backend_error);
        prepared.ready = false;
        return prepared;
    }

    static constexpr std::string_view trim_left(std::string_view sv) noexcept {
        while (!sv.empty() && sv.front() == ' ') {
            sv.remove_prefix(1);
        }
        return sv;
    }

    static constexpr std::pair<std::string_view, std::string_view> split_token(std::string_view sv) noexcept {
        sv = trim_left(sv);
        const auto pos = sv.find(' ');
        if (pos == std::string_view::npos) {
            return {sv, {}};
        }
        return {sv.substr(0, pos), trim_left(sv.substr(pos + 1U))};
    }

    AppRunCode build_argv(std::string_view name, std::string_view arg_text) noexcept {
        arg_blob_.fill('\0');
        name_.fill('\0');
        argv_.fill(nullptr);
        argc_ = 0;

        std::memcpy(name_.data(), name.data(), name.size());
        name_[name.size()] = '\0';
        argv_[argc_++] = name_.data();

        std::size_t cursor = 0;
        auto remaining = trim_left(arg_text);
        while (!remaining.empty()) {
            if (argc_ + 1U >= argv_.size()) {
                return AppRunCode::argv_overflow;
            }
            auto [token, rest] = split_token(remaining);
            if (token.empty()) {
                break;
            }
            if (cursor + token.size() + 1U > arg_blob_.size()) {
                return AppRunCode::argv_overflow;
            }
            char* dst = arg_blob_.data() + cursor;
            std::memcpy(dst, token.data(), token.size());
            dst[token.size()] = '\0';
            argv_[argc_++] = dst;
            cursor += token.size() + 1U;
            remaining = rest;
        }
        argv_[argc_] = nullptr;
        return AppRunCode::ok;
    }

    std::array<char, MaxArgBlob> arg_blob_{};
    std::array<char, MaxName> name_{};
    std::array<char*, MaxArgs + 2U> argv_{};
    std::size_t argc_{0};
};

} // namespace charm::app_abi
