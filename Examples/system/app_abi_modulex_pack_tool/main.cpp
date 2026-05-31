#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

inline constexpr std::uint32_t kModuleXMagic = 0x43484D4DU; // "CHMM", little-endian.
inline constexpr std::uint16_t kModuleXVersion = 2U;
inline constexpr std::uint16_t kModuleXSymbolGlobal = 1U;

struct ModuleXWireHeader {
    std::uint32_t magic{kModuleXMagic};
    std::uint16_t version{kModuleXVersion};
    std::uint16_t flags{0};
    std::uint32_t entry_offset{0};
    std::uint32_t text_offset{0};
    std::uint32_t ro_offset{0};
    std::uint32_t data_offset{0};
    std::uint32_t rel_offset{0};
    std::uint32_t sym_offset{0};
    std::uint32_t str_offset{0};
    std::uint32_t str_size{0};
    std::uint32_t dep_offset{0};
    std::uint32_t dep_size{0};
    std::uint32_t text_size{0};
    std::uint32_t ro_size{0};
    std::uint32_t data_size{0};
    std::uint32_t bss_size{0};
    std::uint32_t rel_size{0};
    std::uint32_t sym_size{0};
    std::uint32_t image_size{0};
};

struct ModuleXWireSymbol32 {
    std::uint32_t name_offset{0};
    std::uint32_t value{0};
    std::uint32_t size{0};
    std::uint16_t kind{0};
    std::uint16_t flags{0};
};

static_assert(sizeof(ModuleXWireHeader) == 76U);
static_assert(sizeof(ModuleXWireSymbol32) == 16U);

struct Payload {
    std::vector<std::byte> text{};
    std::vector<std::byte> ro{};
};

bool read_file(const fs::path& path, std::vector<std::byte>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const auto size = file.tellg();
    if (size < 0) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!out.empty()) {
        file.read(reinterpret_cast<char*>(out.data()), size);
    }
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

void append_bytes(std::vector<std::byte>& output, std::span<const std::byte> bytes) {
    output.insert(output.end(), bytes.begin(), bytes.end());
}

template <class T>
void append_object(std::vector<std::byte>& output, const T& value) {
    const auto* bytes = reinterpret_cast<const std::byte*>(&value);
    append_bytes(output, std::span<const std::byte>{bytes, sizeof(T)});
}

void print_usage(const char* argv0) {
    std::fprintf(stderr, "usage: %s <text.bin> <rodata.bin> <output.modulex>\n", argv0);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 2;
    }

    Payload payload{};
    if (!read_file(argv[1], payload.text) || payload.text.empty()) {
        std::fprintf(stderr, "failed to read non-empty text payload: %s\n", argv[1]);
        return 1;
    }
    if (!read_file(argv[2], payload.ro)) {
        std::fprintf(stderr, "failed to read rodata payload: %s\n", argv[2]);
        return 1;
    }
    if (payload.text.size() > UINT32_MAX || payload.ro.size() > UINT32_MAX) {
        std::fprintf(stderr, "modulex payload too large\n");
        return 1;
    }

    constexpr char kSymbolName[] = "charm_app_main";
    ModuleXWireHeader header{};
    header.magic = kModuleXMagic;
    header.version = kModuleXVersion;
    header.flags = 0;
    header.entry_offset = 0;
    header.text_offset = sizeof(ModuleXWireHeader);
    header.text_size = static_cast<std::uint32_t>(payload.text.size());
    header.ro_offset = header.text_offset + header.text_size;
    header.ro_size = static_cast<std::uint32_t>(payload.ro.size());
    header.data_offset = header.ro_offset + header.ro_size;
    header.data_size = 0;
    header.rel_offset = header.data_offset;
    header.rel_size = 0;
    header.sym_offset = header.rel_offset;
    header.sym_size = sizeof(ModuleXWireSymbol32);
    header.str_offset = header.sym_offset + header.sym_size;
    header.str_size = sizeof(kSymbolName);
    header.dep_offset = header.str_offset + header.str_size;
    header.dep_size = 0;
    header.image_size = header.dep_offset;

    ModuleXWireSymbol32 symbol{};
    symbol.name_offset = 0;
    symbol.value = 0;
    symbol.size = header.text_size;
    symbol.kind = kModuleXSymbolGlobal;
    symbol.flags = 0;

    std::vector<std::byte> output{};
    output.reserve(header.image_size);
    append_object(output, header);
    append_bytes(output, payload.text);
    append_bytes(output, payload.ro);
    append_object(output, symbol);
    append_bytes(output, std::as_bytes(std::span<const char>{kSymbolName, sizeof(kSymbolName)}));

    if (output.size() != header.image_size) {
        std::fprintf(stderr, "internal size mismatch: %zu != %u\n", output.size(), header.image_size);
        return 1;
    }
    if (!write_file(argv[3], output)) {
        std::fprintf(stderr, "failed to write output: %s\n", argv[3]);
        return 1;
    }

    std::printf("[app-abi-modulex-pack] wrote %zu bytes path=%s\n", output.size(), argv[3]);
    return 0;
}
