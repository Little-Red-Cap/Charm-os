#include "charm_resident_platform_inspect.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

namespace inspect = charm::resident_platform::inspect;
namespace fs = std::filesystem;

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s <artifact_manifest.json> [--json] [--strict]\n",
                 argv0);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }

    bool json = false;
    bool strict = false;
    fs::path manifest_path{};

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--json") {
            json = true;
        } else if (arg == "--strict") {
            strict = true;
        } else if (!arg.empty() && arg.front() == '-') {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        } else if (manifest_path.empty()) {
            manifest_path = fs::path{argv[i]};
        } else {
            std::fprintf(stderr, "unexpected argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    if (manifest_path.empty()) {
        print_usage(argv[0]);
        return 2;
    }

    const auto summary = inspect::inspect_manifest(manifest_path, inspect::InspectOptions{
        .strict = strict,
    });
    if (json) {
        inspect::print_json(summary, stdout, strict);
    } else {
        inspect::print_human(summary, stdout, strict);
    }
    return summary.ok(strict) ? 0 : 1;
}
