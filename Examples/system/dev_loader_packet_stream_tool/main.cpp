#include "charm_dev_loader_packet_stream.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
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
                 "usage: %s <input.bin> <output.packetstream> [--chunk N] [--no-crc] [--no-launch]\n",
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

    loader::PacketStreamBuildConfig config{};
    config.chunk_size = 256;
    config.check_crc = true;
    config.append_launch_dry_run = true;

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--no-crc") {
            config.check_crc = false;
        } else if (arg == "--no-launch") {
            config.append_launch_dry_run = false;
        } else if (arg == "--chunk") {
            if (i + 1 >= argc || !parse_u32(argv[++i], config.chunk_size)) {
                std::fprintf(stderr, "invalid --chunk value\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    std::vector<std::byte> payload{};
    if (!read_file(argv[1], payload)) {
        std::fprintf(stderr, "failed to read input: %s\n", argv[1]);
        return 1;
    }
    config.payload = std::span<const std::byte>{payload.data(), payload.size()};

    const auto required = loader::packet_stream_required_size(config);
    std::vector<std::byte> output(std::max<std::uint32_t>(required, 1U));
    const auto result = loader::packet_stream_build(config, output);
    if (result.code != loader::PacketStreamBuildCode::ok) {
        std::fprintf(stderr,
                     "packet stream build failed: %.*s\n",
                     static_cast<int>(loader::packet_stream_build_code_name(result.code).size()),
                     loader::packet_stream_build_code_name(result.code).data());
        return 1;
    }

    output.resize(result.bytes_written);
    if (!write_file(argv[2], output)) {
        std::fprintf(stderr, "failed to write output: %s\n", argv[2]);
        return 1;
    }

    std::printf("[dev-loader-packet-stream] wrote %u bytes packets=%u crc=0x%08x path=%s\n",
                result.bytes_written,
                result.packet_count,
                result.payload_crc32,
                argv[2]);
    return 0;
}
