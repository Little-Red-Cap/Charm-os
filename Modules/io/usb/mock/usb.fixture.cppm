module;

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

export module usb.fixture;

import usb.common;
import usb.mock;
import usb.replay;

export namespace usb::fixture {
    [[nodiscard]] inline bool expect(bool cond,
                                     const char* message,
                                     std::FILE* stream = stderr) noexcept {
        if (!cond) {
            std::fprintf(stream, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    inline int report_replay_failure(const usb::replay::FileRunResult& replay,
                                     std::FILE* stream = stderr) noexcept {
        if (!replay.load) {
            std::fprintf(stream,
                         "[ERR] trace load failed line=%zu err=%s\n",
                         replay.load.line,
                         usb::replay::load_error_name(replay.load.error));
            return 1;
        }

        std::fprintf(stream,
                     "[ERR] replay failed step=%zu err=%s\n",
                     replay.replay.step_index,
                     usb::replay::error_name(replay.replay.error));
        return 1;
    }

    [[nodiscard]] inline bool run_replay_file(usb::mock::Session& session,
                                              std::string_view path,
                                              const usb::replay::Hooks& hooks = {},
                                              std::FILE* stream = stderr) noexcept {
        const auto replay = usb::replay::run_file(session, path, hooks);
        if (!replay) {
            report_replay_failure(replay, stream);
            return false;
        }
        return true;
    }

    inline constexpr std::string_view kManifestHeaderTag = "manifest";
    inline constexpr std::string_view kManifestFormatV1 = "usb.fixture.v1";
    inline constexpr std::string_view kSuiteHeaderTag = "suite";
    inline constexpr std::string_view kSuiteFormatV1 = "usb.fixture.suite.v1";

    struct ManifestEntry {
        std::string name{};
        std::string trace{};
    };

    struct Manifest {
        std::vector<ManifestEntry> entries{};
    };

    struct SuiteEntry {
        std::string manifest{};
    };

    struct Suite {
        std::vector<SuiteEntry> entries{};
    };

    enum class ManifestLoadError : usb::u8 {
        none = 0,
        missing_header,
        unsupported_version,
        syntax,
        missing_field,
        file_io,
    };

    struct ManifestLoadResult {
        Manifest manifest{};
        ManifestLoadError error{ManifestLoadError::none};
        std::size_t line{0};

        [[nodiscard]] explicit operator bool() const noexcept {
            return error == ManifestLoadError::none;
        }
    };

    enum class SuiteLoadError : usb::u8 {
        none = 0,
        missing_header,
        unsupported_version,
        syntax,
        missing_field,
        file_io,
    };

    struct SuiteLoadResult {
        Suite suite{};
        SuiteLoadError error{SuiteLoadError::none};
        std::size_t line{0};

        [[nodiscard]] explicit operator bool() const noexcept {
            return error == SuiteLoadError::none;
        }
    };

    inline const char* manifest_error_name(ManifestLoadError err) noexcept {
        switch (err) {
        case ManifestLoadError::none: return "none";
        case ManifestLoadError::missing_header: return "missing_header";
        case ManifestLoadError::unsupported_version: return "unsupported_version";
        case ManifestLoadError::syntax: return "syntax";
        case ManifestLoadError::missing_field: return "missing_field";
        case ManifestLoadError::file_io: return "file_io";
        }
        return "unknown";
    }

    inline const char* suite_error_name(SuiteLoadError err) noexcept {
        switch (err) {
        case SuiteLoadError::none: return "none";
        case SuiteLoadError::missing_header: return "missing_header";
        case SuiteLoadError::unsupported_version: return "unsupported_version";
        case SuiteLoadError::syntax: return "syntax";
        case SuiteLoadError::missing_field: return "missing_field";
        case SuiteLoadError::file_io: return "file_io";
        }
        return "unknown";
    }

    struct ManifestHooks {
        bool (*run_case)(void* ctx,
                         std::string_view case_name,
                         std::string_view trace_path,
                         std::FILE* stream) noexcept { nullptr };
        void* ctx{nullptr};
    };

    namespace detail {
        inline bool is_space(char ch) noexcept {
            return std::isspace(static_cast<unsigned char>(ch)) != 0;
        }

        inline std::string_view trim(std::string_view sv) noexcept {
            while (!sv.empty() && is_space(sv.front())) {
                sv.remove_prefix(1);
            }
            while (!sv.empty() && is_space(sv.back())) {
                sv.remove_suffix(1);
            }
            return sv;
        }

        inline std::vector<std::string_view> split_tokens(std::string_view line) {
            std::vector<std::string_view> out{};
            line = trim(line);
            while (!line.empty()) {
                if (line.front() == '#') {
                    break;
                }
                std::size_t count = 0;
                while (count < line.size() && !is_space(line[count])) {
                    ++count;
                }
                out.push_back(line.substr(0, count));
                line.remove_prefix(count);
                line = trim(line);
            }
            return out;
        }

        inline bool split_key_value(std::string_view token,
                                    std::string_view& key,
                                    std::string_view& value) noexcept {
            const auto pos = token.find('=');
            if (pos == std::string_view::npos) {
                return false;
            }
            key = token.substr(0, pos);
            value = token.substr(pos + 1);
            return !key.empty();
        }
    }

    inline ManifestLoadResult load_manifest_text(std::string_view text) {
        ManifestLoadResult out{};
        std::size_t line_no = 0;
        bool saw_header = false;

        const auto fail = [&](ManifestLoadError err) {
            out.error = err;
            out.line = line_no;
            return out;
        };

        while (!text.empty()) {
            ++line_no;
            const auto nl = text.find('\n');
            auto line = (nl == std::string_view::npos) ? text : text.substr(0, nl);
            text = (nl == std::string_view::npos) ? std::string_view{} : text.substr(nl + 1);
            line = detail::trim(line);
            if (line.empty() || line.front() == '#') {
                continue;
            }

            const auto tokens = detail::split_tokens(line);
            if (tokens.empty()) {
                continue;
            }

            if (!saw_header) {
                if (tokens[0] != kManifestHeaderTag) {
                    return fail(ManifestLoadError::missing_header);
                }
                if (tokens.size() != 2 || tokens[1] != kManifestFormatV1) {
                    return fail(ManifestLoadError::unsupported_version);
                }
                saw_header = true;
                continue;
            }

            if (tokens[0] != "case") {
                return fail(ManifestLoadError::syntax);
            }

            ManifestEntry entry{};
            bool has_name = false;
            bool has_trace = false;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                std::string_view key{};
                std::string_view value{};
                if (!detail::split_key_value(tokens[i], key, value)) {
                    return fail(ManifestLoadError::syntax);
                }
                if (key == "name") {
                    entry.name.assign(value.begin(), value.end());
                    has_name = true;
                } else if (key == "trace") {
                    entry.trace.assign(value.begin(), value.end());
                    has_trace = true;
                } else {
                    return fail(ManifestLoadError::syntax);
                }
            }

            if (!has_name || !has_trace) {
                return fail(ManifestLoadError::missing_field);
            }
            out.manifest.entries.push_back(std::move(entry));
        }

        if (!saw_header) {
            return fail(ManifestLoadError::missing_header);
        }
        return out;
    }

    inline ManifestLoadResult load_manifest_file(std::string_view path) {
        ManifestLoadResult out{};
        std::ifstream file(std::string(path), std::ios::binary);
        if (!file) {
            out.error = ManifestLoadError::file_io;
            return out;
        }
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (!file.good() && !file.eof()) {
            out.error = ManifestLoadError::file_io;
            return out;
        }
        return load_manifest_text(text);
    }

    inline int run_manifest_file(std::string_view manifest_path,
                                 const ManifestHooks& hooks,
                                 std::FILE* stream = stdout) noexcept {
        if (!hooks.run_case) {
            std::fprintf(stream, "[ERR] manifest runner missing callback\n");
            return 1;
        }

        const auto load = load_manifest_file(manifest_path);
        if (!load) {
            std::fprintf(stream,
                         "[ERR] manifest load failed line=%zu err=%s\n",
                         load.line,
                         manifest_error_name(load.error));
            return 1;
        }

        const auto manifest_fs = std::filesystem::path(std::string(manifest_path));
        const auto base_dir = manifest_fs.parent_path();

        for (const auto& entry : load.manifest.entries) {
            auto trace_fs = std::filesystem::path(entry.trace);
            if (trace_fs.is_relative()) {
                trace_fs = base_dir / trace_fs;
            }
            const auto trace_text = trace_fs.string();
            std::fprintf(stream, "[RUN] %s\n", entry.name.c_str());
            if (!hooks.run_case(hooks.ctx, entry.name, trace_text, stream)) {
                std::fprintf(stream, "[FAIL] %s\n", entry.name.c_str());
                return 1;
            }
            std::fprintf(stream, "[OK] %s\n", entry.name.c_str());
        }

        std::fprintf(stream,
                     "[OK] manifest passed cases=%zu\n",
                     load.manifest.entries.size());
        return 0;
    }

    inline SuiteLoadResult load_suite_text(std::string_view text) {
        SuiteLoadResult out{};
        std::size_t line_no = 0;
        bool saw_header = false;

        const auto fail = [&](SuiteLoadError err) {
            out.error = err;
            out.line = line_no;
            return out;
        };

        while (!text.empty()) {
            ++line_no;
            const auto nl = text.find('\n');
            auto line = (nl == std::string_view::npos) ? text : text.substr(0, nl);
            text = (nl == std::string_view::npos) ? std::string_view{} : text.substr(nl + 1);
            line = detail::trim(line);
            if (line.empty() || line.front() == '#') {
                continue;
            }

            const auto tokens = detail::split_tokens(line);
            if (tokens.empty()) {
                continue;
            }

            if (!saw_header) {
                if (tokens[0] != kSuiteHeaderTag) {
                    return fail(SuiteLoadError::missing_header);
                }
                if (tokens.size() != 2 || tokens[1] != kSuiteFormatV1) {
                    return fail(SuiteLoadError::unsupported_version);
                }
                saw_header = true;
                continue;
            }

            if (tokens[0] != "manifest") {
                return fail(SuiteLoadError::syntax);
            }

            SuiteEntry entry{};
            bool has_manifest = false;
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                std::string_view key{};
                std::string_view value{};
                if (!detail::split_key_value(tokens[i], key, value)) {
                    return fail(SuiteLoadError::syntax);
                }
                if (key == "path") {
                    entry.manifest.assign(value.begin(), value.end());
                    has_manifest = true;
                } else {
                    return fail(SuiteLoadError::syntax);
                }
            }

            if (!has_manifest) {
                return fail(SuiteLoadError::missing_field);
            }
            out.suite.entries.push_back(std::move(entry));
        }

        if (!saw_header) {
            return fail(SuiteLoadError::missing_header);
        }
        return out;
    }

    inline SuiteLoadResult load_suite_file(std::string_view path) {
        SuiteLoadResult out{};
        std::ifstream file(std::string(path), std::ios::binary);
        if (!file) {
            out.error = SuiteLoadError::file_io;
            return out;
        }
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (!file.good() && !file.eof()) {
            out.error = SuiteLoadError::file_io;
            return out;
        }
        return load_suite_text(text);
    }

    inline int run_suite_file(std::string_view suite_path,
                              const ManifestHooks& hooks,
                              std::FILE* stream = stdout) noexcept {
        if (!hooks.run_case) {
            std::fprintf(stream, "[ERR] suite runner missing callback\n");
            return 1;
        }

        const auto load = load_suite_file(suite_path);
        if (!load) {
            std::fprintf(stream,
                         "[ERR] suite load failed line=%zu err=%s\n",
                         load.line,
                         suite_error_name(load.error));
            return 1;
        }

        const auto suite_fs = std::filesystem::path(std::string(suite_path));
        const auto base_dir = suite_fs.parent_path();

        for (const auto& entry : load.suite.entries) {
            auto manifest_fs = std::filesystem::path(entry.manifest);
            if (manifest_fs.is_relative()) {
                manifest_fs = base_dir / manifest_fs;
            }
            const auto manifest_text = manifest_fs.string();
            std::fprintf(stream, "[SUITE] manifest=%s\n", manifest_text.c_str());
            if (run_manifest_file(manifest_text, hooks, stream) != 0) {
                return 1;
            }
        }

        std::fprintf(stream,
                     "[OK] suite passed manifests=%zu\n",
                     load.suite.entries.size());
        return 0;
    }
}
