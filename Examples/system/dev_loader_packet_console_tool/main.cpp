#include "charm_dev_loader_packet_console.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace loader = charm::dev_loader;

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s <input.packetstream> <output.commands> [--bytes-per-line N]\n",
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

bool write_file(const fs::path& path, std::span<const char> bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

bool parse_u32(std::string_view text, std::uint32_t& out) {
    const auto* first = text.data();
    const auto* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, out, 10);
    return ec == std::errc{} && ptr == last;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 2;
    }

    std::uint32_t bytes_per_line = 48;
    for (int i = 3; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--bytes-per-line") {
            if (i + 1 >= argc || !parse_u32(argv[++i], bytes_per_line)) {
                std::fprintf(stderr, "invalid --bytes-per-line value\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    std::vector<std::byte> stream{};
    if (!read_file(argv[1], stream)) {
        std::fprintf(stderr, "failed to read input: %s\n", argv[1]);
        return 1;
    }

    const loader::PacketConsoleBuildConfig config{
        .stream = std::span<const std::byte>{stream.data(), stream.size()},
        .bytes_per_line = bytes_per_line,
    };
    const auto required = loader::packet_console_required_size(config);
    std::vector<char> output(std::max<std::uint32_t>(required, 1U));
    const auto result = loader::packet_console_build(config, output);
    if (result.code != loader::PacketConsoleBuildCode::ok) {
        std::fprintf(stderr,
                     "packet console build failed: %.*s\n",
                     static_cast<int>(loader::packet_console_build_code_name(result.code).size()),
                     loader::packet_console_build_code_name(result.code).data());
        return 1;
    }

    output.resize(result.bytes_written);
    if (!write_file(argv[2], output)) {
        std::fprintf(stderr, "failed to write output: %s\n", argv[2]);
        return 1;
    }

    std::printf("[dev-loader-packet-console] wrote %u bytes lines=%u payload_per_line=%u path=%s\n",
                result.bytes_written,
                result.line_count,
                result.max_payload_bytes_per_line,
                argv[2]);
    return 0;
}
