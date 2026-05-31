#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

inline constexpr std::uint32_t kModuleXMagic = 0x43484D4DU; // "CHMM", little-endian.
inline constexpr std::uint16_t kModuleXVersion = 2U;
inline constexpr std::uint16_t kModuleXSymbolGlobal = 1U;
inline constexpr std::uint32_t kModuleXRelocAbsAddr = 1U;

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

struct ModuleXWireReloc32 {
    std::uint32_t offset{0};
    std::uint32_t type{0};
    std::uint32_t sym_index{0xFFFFFFFFU};
    std::uint32_t addend{0};
};

static_assert(sizeof(ModuleXWireHeader) == 76U);
static_assert(sizeof(ModuleXWireSymbol32) == 16U);
static_assert(sizeof(ModuleXWireReloc32) == 16U);

struct Payload {
    std::vector<std::byte> text{};
    std::vector<std::byte> ro{};
};

struct SymbolSpec {
    std::string name{};
    std::uint32_t text_offset{0};
    std::uint32_t size{0};
};

struct RelocAbsSpec {
    std::uint32_t target_text_offset{0};
    std::string symbol{};
    std::uint32_t addend{0};
};

struct Options {
    std::uint32_t entry_text_offset{0};
    std::vector<SymbolSpec> globals{};
    std::vector<RelocAbsSpec> reloc_abs{};
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

std::uint32_t align_up(std::uint32_t value, std::uint32_t alignment) {
    const auto rem = value % alignment;
    return rem == 0U ? value : value + (alignment - rem);
}

void pad_to(std::vector<std::byte>& output, std::uint32_t offset) {
    while (output.size() < offset) {
        output.push_back(std::byte{0});
    }
}

bool parse_u32(std::string_view text, std::uint32_t& out) {
    if (text.empty()) {
        return false;
    }
    const std::string copy{text};
    char* end = nullptr;
    errno = 0;
    const auto value = std::strtoull(copy.c_str(), &end, 0);
    if (errno != 0 || end == copy.c_str() || *end != '\0' ||
        value > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool parse_global_spec(std::string_view text, SymbolSpec& out) {
    const auto first = text.find(':');
    if (first == std::string_view::npos || first == 0U) {
        return false;
    }
    const auto second = text.find(':', first + 1U);
    if (second == std::string_view::npos || second == first + 1U ||
        second + 1U >= text.size()) {
        return false;
    }

    SymbolSpec parsed{};
    parsed.name = std::string{text.substr(0, first)};
    if (!parse_u32(text.substr(first + 1U, second - first - 1U), parsed.text_offset) ||
        !parse_u32(text.substr(second + 1U), parsed.size)) {
        return false;
    }
    out = std::move(parsed);
    return true;
}

bool parse_reloc_abs_spec(std::string_view text, RelocAbsSpec& out) {
    const auto first = text.find(':');
    if (first == std::string_view::npos || first == 0U) {
        return false;
    }
    const auto second = text.find(':', first + 1U);
    if (second == std::string_view::npos || second == first + 1U ||
        second + 1U >= text.size()) {
        return false;
    }

    RelocAbsSpec parsed{};
    parsed.symbol = std::string{text.substr(first + 1U, second - first - 1U)};
    if (!parse_u32(text.substr(0, first), parsed.target_text_offset) ||
        !parse_u32(text.substr(second + 1U), parsed.addend)) {
        return false;
    }
    out = std::move(parsed);
    return true;
}

int find_symbol_index(std::span<const SymbolSpec> globals, std::string_view name) {
    for (std::size_t i = 0; i < globals.size(); ++i) {
        if (globals[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool validate_options(const Options& options, std::uint32_t text_size) {
    if (options.entry_text_offset >= text_size) {
        std::fprintf(stderr, "entry offset is outside text: 0x%x >= 0x%x\n",
                     options.entry_text_offset,
                     text_size);
        return false;
    }
    for (std::size_t i = 0; i < options.globals.size(); ++i) {
        const auto& symbol = options.globals[i];
        if (symbol.name.empty()) {
            std::fprintf(stderr, "global symbol name must not be empty\n");
            return false;
        }
        if (symbol.text_offset >= text_size ||
            symbol.size > (text_size - symbol.text_offset)) {
            std::fprintf(stderr, "global symbol outside text: %s\n", symbol.name.c_str());
            return false;
        }
        for (std::size_t j = i + 1U; j < options.globals.size(); ++j) {
            if (symbol.name == options.globals[j].name) {
                std::fprintf(stderr, "duplicate global symbol: %s\n", symbol.name.c_str());
                return false;
            }
        }
    }
    for (const auto& reloc : options.reloc_abs) {
        if (reloc.target_text_offset > text_size ||
            sizeof(std::uint32_t) > (text_size - reloc.target_text_offset)) {
            std::fprintf(stderr, "abs relocation target outside text: 0x%x\n",
                         reloc.target_text_offset);
            return false;
        }
        if (find_symbol_index(options.globals, reloc.symbol) < 0) {
            std::fprintf(stderr, "abs relocation references missing symbol: %s\n",
                         reloc.symbol.c_str());
            return false;
        }
    }
    return true;
}

void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "usage: %s <text.bin> <rodata.bin> <output.modulex> "
                 "[--entry <text_off>] [--global <name>:<text_off>:<size>] "
                 "[--reloc-abs <target_text_off>:<symbol>:<addend>]\n",
                 argv0);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 2;
    }

    Options options{};
    bool has_explicit_globals = false;
    for (int i = 4; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--entry") {
            if (++i >= argc || !parse_u32(argv[i], options.entry_text_offset)) {
                std::fprintf(stderr, "invalid --entry value\n");
                return 2;
            }
        } else if (arg == "--global") {
            SymbolSpec symbol{};
            if (++i >= argc || !parse_global_spec(argv[i], symbol)) {
                std::fprintf(stderr, "invalid --global value\n");
                return 2;
            }
            has_explicit_globals = true;
            options.globals.push_back(std::move(symbol));
        } else if (arg == "--reloc-abs") {
            RelocAbsSpec reloc{};
            if (++i >= argc || !parse_reloc_abs_spec(argv[i], reloc)) {
                std::fprintf(stderr, "invalid --reloc-abs value\n");
                return 2;
            }
            options.reloc_abs.push_back(std::move(reloc));
        } else {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
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
    if (!has_explicit_globals) {
        options.globals.push_back(SymbolSpec{
            .name = kSymbolName,
            .text_offset = 0,
            .size = static_cast<std::uint32_t>(payload.text.size()),
        });
    }
    const auto text_size = static_cast<std::uint32_t>(payload.text.size());
    if (!validate_options(options, text_size)) {
        return 1;
    }

    std::vector<std::byte> strtab{};
    std::vector<ModuleXWireSymbol32> symbols{};
    symbols.reserve(options.globals.size());
    for (const auto& global : options.globals) {
        const auto name_offset = static_cast<std::uint32_t>(strtab.size());
        append_bytes(strtab, std::as_bytes(std::span<const char>{
                               global.name.data(),
                               global.name.size()}));
        strtab.push_back(std::byte{0});

        symbols.push_back(ModuleXWireSymbol32{
            .name_offset = name_offset,
            .value = global.text_offset,
            .size = global.size,
            .kind = kModuleXSymbolGlobal,
            .flags = 0,
        });
    }

    ModuleXWireHeader header{};
    header.magic = kModuleXMagic;
    header.version = kModuleXVersion;
    header.flags = 0;
    header.entry_offset = options.entry_text_offset;
    header.text_offset = sizeof(ModuleXWireHeader);
    header.text_size = text_size;
    header.ro_offset = header.text_offset + header.text_size;
    header.ro_size = static_cast<std::uint32_t>(payload.ro.size());
    header.data_offset = header.ro_offset + header.ro_size;
    header.data_size = 0;
    header.rel_offset = align_up(header.data_offset + header.data_size, 4U);
    header.rel_size = static_cast<std::uint32_t>(options.reloc_abs.size() * sizeof(ModuleXWireReloc32));
    header.sym_offset = align_up(header.rel_offset + header.rel_size, 4U);
    header.sym_size = static_cast<std::uint32_t>(symbols.size() * sizeof(ModuleXWireSymbol32));
    header.str_offset = align_up(header.sym_offset + header.sym_size, 4U);
    header.str_size = static_cast<std::uint32_t>(strtab.size());
    header.dep_offset = align_up(header.str_offset + header.str_size, 4U);
    header.dep_size = 0;
    header.image_size = header.dep_offset;

    std::vector<ModuleXWireReloc32> relocations{};
    relocations.reserve(options.reloc_abs.size());
    for (const auto& reloc : options.reloc_abs) {
        const auto symbol_index = find_symbol_index(options.globals, reloc.symbol);
        relocations.push_back(ModuleXWireReloc32{
            .offset = header.text_offset + reloc.target_text_offset,
            .type = kModuleXRelocAbsAddr,
            .sym_index = static_cast<std::uint32_t>(symbol_index),
            .addend = reloc.addend,
        });
    }

    std::vector<std::byte> output{};
    output.reserve(header.image_size);
    append_object(output, header);
    append_bytes(output, payload.text);
    append_bytes(output, payload.ro);
    pad_to(output, header.rel_offset);
    for (const auto& reloc : relocations) {
        append_object(output, reloc);
    }
    pad_to(output, header.sym_offset);
    for (const auto& symbol : symbols) {
        append_object(output, symbol);
    }
    pad_to(output, header.str_offset);
    append_bytes(output, strtab);
    pad_to(output, header.dep_offset);

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
