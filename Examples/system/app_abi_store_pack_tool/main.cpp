#include "charm_app_store.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace app_abi = charm::app_abi;
namespace fs = std::filesystem;

struct Payload {
    std::string name{};
    std::vector<std::byte> bytes{};
    std::uint32_t flags{0};
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s <output.appstore.bin> <name[(:elf|:modulex)]=payload> "
                 "[name[(:elf|:modulex)]=payload...]\n",
                 argv0);
}

bool read_file(const fs::path& path, std::vector<std::byte>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const auto size = file.tellg();
    if (size <= 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(out.data()), size);
    return static_cast<bool>(file);
}

bool write_file(const fs::path& path, std::span<const std::byte> bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

std::size_t estimate_store_size(const std::vector<Payload>& payloads) {
    std::size_t size = sizeof(app_abi::AppStoreHeader) +
        (payloads.size() * sizeof(app_abi::AppStoreEntry)) +
        16U;
    for (const auto& payload : payloads) {
        size += payload.bytes.size() + 16U;
    }
    return std::max<std::size_t>(size, 256U);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 2;
    }

    std::vector<Payload> payloads{};
    payloads.reserve(static_cast<std::size_t>(argc - 2));
    for (int i = 2; i < argc; ++i) {
        std::string spec = argv[i];
        const auto eq = spec.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1U >= spec.size()) {
            std::fprintf(stderr, "invalid payload spec: %s\n", argv[i]);
            return 2;
        }

        Payload payload{};
        std::string name_spec = spec.substr(0, eq);
        const auto colon = name_spec.find(':');
        std::string format = "elf";
        if (colon != std::string::npos) {
            format = name_spec.substr(colon + 1U);
            name_spec = name_spec.substr(0, colon);
        }
        if (name_spec.empty() || format.empty()) {
            std::fprintf(stderr, "invalid payload spec: %s\n", argv[i]);
            return 2;
        }
        if (format == "elf") {
            payload.flags = app_abi::app_store_format_flags(app_abi::AppImageFormat::elf);
        } else if (format == "modulex") {
            payload.flags = app_abi::app_store_format_flags(app_abi::AppImageFormat::modulex);
        } else {
            std::fprintf(stderr, "unsupported payload format: %s\n", format.c_str());
            return 2;
        }

        payload.name = std::move(name_spec);
        const fs::path path = spec.substr(eq + 1U);
        if (!read_file(path, payload.bytes)) {
            std::fprintf(stderr, "failed to read payload: %s\n", path.string().c_str());
            return 1;
        }
        payloads.push_back(std::move(payload));
    }

    std::vector<app_abi::AppStoreBuildEntry> entries{};
    entries.reserve(payloads.size());
    for (const auto& payload : payloads) {
        entries.push_back(app_abi::AppStoreBuildEntry{
            .name = payload.name,
            .payload = std::span<const std::byte>{payload.bytes.data(), payload.bytes.size()},
            .flags = payload.flags,
        });
    }

    std::vector<std::byte> output(estimate_store_size(payloads));
    auto result = app_abi::app_store_build_image(entries, output);
    if (result.code == app_abi::AppStoreBuildCode::output_too_small) {
        output.resize(output.size() * 2U);
        result = app_abi::app_store_build_image(entries, output);
    }
    if (result.code != app_abi::AppStoreBuildCode::ok) {
        std::fprintf(stderr,
                     "app store build failed: %.*s\n",
                     static_cast<int>(app_abi::app_store_build_code_name(result.code).size()),
                     app_abi::app_store_build_code_name(result.code).data());
        return 1;
    }

    output.resize(result.bytes_written);
    if (!write_file(argv[1], output)) {
        std::fprintf(stderr, "failed to write output: %s\n", argv[1]);
        return 1;
    }

    std::printf("[app-abi-store-pack] wrote %u bytes entries=%u path=%s\n",
                result.bytes_written,
                result.entry_count,
                argv[1]);
    return 0;
}
