#include "charm_app_elf_probe.hpp"
#include "charm_app_received_image.hpp"
#include "charm_dev_loader_byte_transport.hpp"
#include "charm_dev_loader_received_image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

namespace {

namespace app_abi = charm::app_abi;
namespace fs = std::filesystem;
namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::vector<std::byte> bytes{};
};

struct SingleElfSource {
    app_abi::AppImage image{};
    app_abi::AppElfLoadBackend backend{};
};

bool storage_write(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(storage->bytes.data() + offset, bytes.data(), bytes.size());
    return true;
}

bool storage_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* storage = static_cast<MemoryStorage*>(ctx);
    if (storage == nullptr || offset > storage->bytes.size() ||
        bytes.size() > (storage->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(bytes.data(), storage->bytes.data() + offset, bytes.size());
    return true;
}

loader::Storage make_storage(MemoryStorage& memory) {
    return loader::Storage{
        .ctx = &memory,
        .base_address = 0x24070000U,
        .capacity_bytes = static_cast<std::uint32_t>(memory.bytes.size()),
        .write = storage_write,
        .read = storage_read,
    };
}

const app_abi::AppImage* find_single_elf(void* ctx, std::string_view name) noexcept {
    auto* source = static_cast<SingleElfSource*>(ctx);
    if (source == nullptr || source->image.name != name) {
        return nullptr;
    }
    return &source->image;
}

app_abi::AppLoadResult load_single_elf(void* ctx,
                                       const app_abi::AppImage& image,
                                       const app_abi::AppLoadBuffer& buffer) noexcept {
    auto* source = static_cast<SingleElfSource*>(ctx);
    if (source == nullptr || &source->image != &image) {
        return {.code = app_abi::AppRunCode::image_not_found};
    }
    return app_abi::app_elf_load_image(&source->backend, image, buffer);
}

CharmAppApi make_valid_api() {
    CharmAppApi api{};
    api.magic = CHARM_APP_API_MAGIC;
    api.version = CHARM_APP_API_VERSION;
    api.size = sizeof(CharmAppApi);
    return api;
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

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

loader::ByteTransportResult receive_payload(std::span<const std::byte> payload,
                                             MemoryStorage& storage,
                                             std::vector<std::byte>& transport_buffer) {
    const auto chunk_size = 257U;
    const auto chunk_count = static_cast<std::uint32_t>((payload.size() + chunk_size - 1U) / chunk_size);
    std::vector<std::byte> stream(payload.size() + ((chunk_count + 4U) * sizeof(loader::PacketHeader)) + 256U);
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = chunk_size,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   stream);
    if (built.code != loader::PacketStreamBuildCode::ok) {
        return {.code = loader::ByteTransportCode::invalid_argument};
    }

    loader::PacketRuntime packet_runtime{make_storage(storage)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{
            .buffer = std::span<std::byte>{transport_buffer.data(), transport_buffer.size()},
            .max_payload_size = 512,
        },
    };

    loader::ByteTransportResult result{};
    std::uint32_t offset = 0;
    while (offset < built.bytes_written) {
        const auto remaining = built.bytes_written - offset;
        const auto count = remaining < 113U ? remaining : 113U;
        result = transport.ingest(std::span<const std::byte>{stream.data() + offset, count});
        if (result.code != loader::ByteTransportCode::ok) {
            return result;
        }
        offset += count;
    }
    return transport.status();
}

bool stage_received_elf(std::string_view name,
                        std::span<const std::byte> payload,
                        std::vector<std::byte>& received_cache,
                        std::vector<std::byte>& app_cache,
                        app_abi::AppImage& out_image) {
    MemoryStorage storage{};
    storage.bytes.resize(payload.size());
    std::vector<std::byte> transport_buffer(2048);
    const auto status = receive_payload(payload, storage, transport_buffer);
    if (!expect(status.code == loader::ByteTransportCode::ok &&
                    status.packet.receive.stage == loader::Stage::launch_ready,
                "received ELF packetstream reaches launch_ready")) {
        return false;
    }

    received_cache.assign(payload.size(), std::byte{});
    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = status.packet.receive,
        .manifest = status.packet.manifest,
        .storage = make_storage(storage),
        .output = std::span<std::byte>{received_cache.data(), received_cache.size()},
    });
    if (!expect(read.code == loader::ReceivedImageReadCode::ok &&
                    read.bytes_read == payload.size(),
                "launch_ready ELF reads back from storage")) {
        return false;
    }

    app_cache.assign(payload.size(), std::byte{});
    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = name,
        .format = app_abi::AppImageFormat::elf,
        .image = read.image,
        .verified = read.code == loader::ReceivedImageReadCode::ok,
        .cache = std::span<std::byte>{app_cache.data(), app_cache.size()},
    });
    if (!expect(staged.code == app_abi::AppReceivedImageStageCode::ok,
                "received ELF stages as AppImage")) {
        return false;
    }
    out_image = staged.image;
    return true;
}

bool expect_real_elf_load(std::string_view name, const fs::path& path) {
    std::vector<std::byte> payload{};
    if (!expect(read_file(path, payload), "sample ELF file reads")) {
        return false;
    }

    std::vector<std::byte> received_cache{};
    std::vector<std::byte> app_cache{};
    app_abi::AppImage image{};
    if (!stage_received_elf(name, payload, received_cache, app_cache, image)) {
        return false;
    }

    alignas(64) std::array<std::byte, 64 * 1024> load_buffer{};
    app_abi::AppElfLoadBackend backend{};
    const auto loaded = app_abi::app_elf_load_image(&backend,
                                                    image,
                                                    app_abi::AppLoadBuffer{
                                                        .base = load_buffer.data(),
                                                        .size = load_buffer.size(),
                                                        .align = 16,
                                                    });
    bool ok = true;
    ok = expect(loaded.code == app_abi::AppRunCode::ok, "real ELF loader backend succeeds") && ok;
    ok = expect(loaded.image.entry != nullptr, "real ELF loader backend materializes entry") && ok;
    ok = expect(backend.last.plan.probe.code == app_abi::AppElfProbeCode::ok,
                "real ELF load probe succeeds") && ok;
    ok = expect(backend.last.plan.probe.runnable, "real ELF load probe marks runnable") && ok;
    ok = expect(backend.last.plan.probe.segment_count != 0U, "real ELF has load segments") && ok;
    ok = expect(backend.last.plan.probe.load_span != 0U, "real ELF has non-zero load span") && ok;
    ok = expect(backend.last.plan.entry_address == reinterpret_cast<std::uintptr_t>(loaded.image.entry),
                "loader entry matches plan entry address") && ok;

    alignas(64) std::array<std::byte, 64 * 1024> runtime_load_buffer{};
    SingleElfSource source_ctx{.image = image};
    app_abi::AppImageSource source{
        .ctx = &source_ctx,
        .find = find_single_elf,
        .load = load_single_elf,
    };
    CharmAppApi api = make_valid_api();
    app_abi::AppRuntime<> runtime{};
    const auto prepared = runtime.prepare(app_abi::AppRunConfig{
        .source = &source,
        .load_buffer = app_abi::AppLoadBuffer{
            .base = runtime_load_buffer.data(),
            .size = runtime_load_buffer.size(),
            .align = 16,
        },
        .api = &api,
        .name = name,
        .arg_text = "dry-run",
    });
    ok = expect(prepared.ready, "real ELF prepares through AppRuntime without executing") && ok;
    ok = expect(prepared.result.stage == app_abi::AppRunStage::start &&
                    prepared.result.code == app_abi::AppRunCode::ok &&
                    !prepared.result.exited,
                "real ELF prepare stops at start stage") && ok;
    ok = expect(prepared.argc == 2 &&
                    std::string_view{prepared.argv[0]} == name &&
                    std::string_view{prepared.argv[1]} == "dry-run" &&
                    prepared.argv[2] == nullptr,
                "real ELF prepare builds argv") && ok;
    ok = expect(source_ctx.backend.last.plan.probe.code == app_abi::AppElfProbeCode::ok &&
                    source_ctx.backend.last.plan.entry_address ==
                        reinterpret_cast<std::uintptr_t>(prepared.image.entry),
                "real ELF prepare preserves loader diagnostics and entry") && ok;
    return ok;
}

struct SyntheticSegment {
    std::uint32_t offset;
    std::uint32_t vaddr;
    std::uint32_t filesz;
    std::uint32_t memsz;
    std::uint32_t flags;
    std::uint32_t align;
    std::byte seed;
};

std::vector<std::byte> make_minimal_elf(std::uint32_t entry = 0x24070000U,
                                        std::uint32_t flags = app_abi::kAppElfPfX,
                                        std::uint32_t vaddr = 0x24070000U,
                                        std::uint32_t memsz = 4U,
                                        std::uint32_t filesz = 4U,
                                        std::uint32_t offset = 0x100U) {
    std::vector<std::byte> bytes(offset + filesz, std::byte{});
    app_abi::AppElf32Header header{};
    header.ident[0] = 0x7f;
    header.ident[1] = 'E';
    header.ident[2] = 'L';
    header.ident[3] = 'F';
    header.ident[4] = 1;
    header.ident[5] = 1;
    header.type = 2;
    header.machine = 40;
    header.version = 1;
    header.entry = entry;
    header.phoff = sizeof(app_abi::AppElf32Header);
    header.ehsize = sizeof(app_abi::AppElf32Header);
    header.phentsize = sizeof(app_abi::AppElf32ProgramHeader);
    header.phnum = 1;
    std::memcpy(bytes.data(), &header, sizeof(header));

    app_abi::AppElf32ProgramHeader ph{};
    ph.type = app_abi::kAppElfPtLoad;
    ph.offset = offset;
    ph.vaddr = vaddr;
    ph.paddr = vaddr;
    ph.filesz = filesz;
    ph.memsz = memsz;
    ph.flags = flags;
    ph.align = 4;
    std::memcpy(bytes.data() + header.phoff, &ph, sizeof(ph));
    for (std::uint32_t i = 0; i < filesz; ++i) {
        bytes[offset + i] = static_cast<std::byte>(0xA0U + i);
    }
    return bytes;
}

std::vector<std::byte> make_segmented_elf(std::span<const SyntheticSegment> segments,
                                          std::uint32_t entry) {
    std::uint32_t image_size = sizeof(app_abi::AppElf32Header) +
                               (segments.size() * sizeof(app_abi::AppElf32ProgramHeader));
    for (const auto& segment : segments) {
        const auto end = segment.offset + segment.filesz;
        if (end > image_size) {
            image_size = end;
        }
    }

    std::vector<std::byte> bytes(image_size, std::byte{0});
    app_abi::AppElf32Header header{};
    header.ident[0] = 0x7f;
    header.ident[1] = 'E';
    header.ident[2] = 'L';
    header.ident[3] = 'F';
    header.ident[4] = 1;
    header.ident[5] = 1;
    header.type = 2;
    header.machine = 40;
    header.version = 1;
    header.entry = entry;
    header.phoff = sizeof(app_abi::AppElf32Header);
    header.ehsize = sizeof(app_abi::AppElf32Header);
    header.phentsize = sizeof(app_abi::AppElf32ProgramHeader);
    header.phnum = static_cast<std::uint16_t>(segments.size());
    std::memcpy(bytes.data(), &header, sizeof(header));

    for (std::size_t index = 0; index < segments.size(); ++index) {
        const auto& segment = segments[index];
        app_abi::AppElf32ProgramHeader ph{};
        ph.type = app_abi::kAppElfPtLoad;
        ph.offset = segment.offset;
        ph.vaddr = segment.vaddr;
        ph.paddr = segment.vaddr;
        ph.filesz = segment.filesz;
        ph.memsz = segment.memsz;
        ph.flags = segment.flags;
        ph.align = segment.align;
        std::memcpy(bytes.data() + header.phoff + (index * sizeof(app_abi::AppElf32ProgramHeader)),
                    &ph,
                    sizeof(ph));
        for (std::uint32_t i = 0; i < segment.filesz; ++i) {
            bytes[segment.offset + i] =
                static_cast<std::byte>(static_cast<unsigned>(segment.seed) + (i & 0xffU));
        }
    }
    return bytes;
}

app_abi::AppElfProbeResult probe_bytes(std::span<const std::byte> bytes,
                                       app_abi::AppImageFormat format,
                                       std::span<std::byte> load_buffer,
                                       std::size_t align = 16U) {
    const app_abi::AppImage image{
        .name = "fixture",
        .format = format,
        .image_base = bytes.data(),
        .image_size = bytes.size(),
    };
    return app_abi::app_elf_probe_load(image, app_abi::AppLoadBuffer{
                                                  .base = load_buffer.data(),
                                                  .size = load_buffer.size(),
                                                  .align = align,
                                              });
}

bool expect_large_segmented_elf_load() {
    static constexpr std::uint32_t kBase = 0x24070000U;
    static constexpr std::uint32_t kEntryOffset = 0x120U;
    static constexpr std::uint32_t kLoadSpan = 0xC000U;
    const std::array<SyntheticSegment, 3> segments{{
        SyntheticSegment{
            .offset = 0x1000U,
            .vaddr = kBase,
            .filesz = 0x300U,
            .memsz = 0x300U,
            .flags = 0x5U,
            .align = 4U,
            .seed = std::byte{0x10U},
        },
        SyntheticSegment{
            .offset = 0x2000U,
            .vaddr = kBase + 0x4000U,
            .filesz = 0x1000U,
            .memsz = 0x1000U,
            .flags = 0x4U,
            .align = 4U,
            .seed = std::byte{0x30U},
        },
        SyntheticSegment{
            .offset = 0x4000U,
            .vaddr = kBase + 0xA000U,
            .filesz = 0x1800U,
            .memsz = 0x2000U,
            .flags = 0x6U,
            .align = 4U,
            .seed = std::byte{0x60U},
        },
    }};

    const auto elf = make_segmented_elf(segments, kBase + kEntryOffset);
    std::vector<std::byte> received_cache{};
    std::vector<std::byte> app_cache{};
    app_abi::AppImage image{};
    bool ok = stage_received_elf("large_segmented",
                                 elf,
                                 received_cache,
                                 app_cache,
                                 image);

    alignas(64) std::array<std::byte, 64 * 1024> load{};
    load.fill(std::byte{0xCCU});
    app_abi::AppElfLoadBackend backend{};
    const auto loaded = app_abi::app_elf_load_image(&backend,
                                                    image,
                                                    app_abi::AppLoadBuffer{
                                                        .base = load.data(),
                                                        .size = load.size(),
                                                        .align = 16,
                                                    });
    ok = expect(loaded.code == app_abi::AppRunCode::ok, "large segmented ELF load succeeds") && ok;
    ok = expect(backend.last.plan.probe.code == app_abi::AppElfProbeCode::ok &&
                    backend.last.plan.probe.entry_offset == kEntryOffset &&
                    backend.last.plan.probe.load_span == kLoadSpan &&
                    backend.last.plan.probe.segment_count == segments.size(),
                "large segmented ELF reports entry/span/segment metadata") && ok;
    ok = expect(backend.last.plan.entry_address ==
                    reinterpret_cast<std::uintptr_t>(load.data() + kEntryOffset),
                "large segmented ELF materializes entry from load base plus offset") && ok;
    ok = expect(load[0x0000U] == std::byte{0x10U} &&
                    load[0x02ffU] == std::byte{0x0fU} &&
                    load[0x4000U] == std::byte{0x30U} &&
                    load[0x4fffU] == std::byte{0x2fU} &&
                    load[0xA000U] == std::byte{0x60U} &&
                    load[0xB7ffU] == std::byte{0x5fU},
                "large segmented ELF copies each PT_LOAD payload") && ok;
    ok = expect(load[0xB800U] == std::byte{0x00U} &&
                    load[0xBfffU] == std::byte{0x00U},
                "large segmented ELF zero-fills memsz beyond filesz") && ok;

    alignas(64) std::array<std::byte, 32 * 1024> small_load{};
    const auto too_small = probe_bytes(elf, app_abi::AppImageFormat::elf, small_load);
    ok = expect(too_small.code == app_abi::AppElfProbeCode::load_buffer_too_small &&
                    too_small.load_span == kLoadSpan &&
                    too_small.segment_count == segments.size(),
                "large segmented ELF preserves capacity diagnostics on small buffer") && ok;
    return ok;
}

bool expect_error_paths() {
    bool ok = true;
    alignas(64) std::array<std::byte, 512> load{};

    auto elf = make_minimal_elf();
    auto result = probe_bytes(elf, app_abi::AppImageFormat::elf, load);
    ok = expect(result.code == app_abi::AppElfProbeCode::ok &&
                    result.entry_offset == 0U &&
                    result.load_span == 4U &&
                    result.segment_count == 1U &&
                    load[0] == std::byte{0xA0},
                "minimal ELF probe succeeds, copies segment, and reports metadata") && ok;

    result = probe_bytes(elf, app_abi::AppImageFormat::function, load);
    ok = expect(result.code == app_abi::AppElfProbeCode::format_mismatch,
                "format mismatch rejected") && ok;

    auto bad_magic = elf;
    bad_magic[0] = std::byte{0};
    result = probe_bytes(bad_magic, app_abi::AppImageFormat::elf, load);
    ok = expect(result.code == app_abi::AppElfProbeCode::bad_magic,
                "bad magic rejected") && ok;

    auto truncated = elf;
    truncated.resize(0x101U);
    result = probe_bytes(truncated, app_abi::AppImageFormat::elf, load);
    ok = expect(result.code == app_abi::AppElfProbeCode::truncated_payload,
                "truncated payload rejected") && ok;

    std::array<std::byte, 2> tiny_load{};
    result = probe_bytes(elf, app_abi::AppImageFormat::elf, tiny_load, 1);
    ok = expect(result.code == app_abi::AppElfProbeCode::load_buffer_too_small,
                "small load buffer rejected") && ok;

    std::array<std::byte, 64> unaligned_storage{};
    result = probe_bytes(elf,
                         app_abi::AppImageFormat::elf,
                         std::span<std::byte>{unaligned_storage.data() + 1, unaligned_storage.size() - 1},
                         16);
    ok = expect(result.code == app_abi::AppElfProbeCode::load_buffer_unaligned,
                "unaligned load buffer rejected") && ok;

    auto entry_outside = make_minimal_elf(0x24080000U);
    result = probe_bytes(entry_outside, app_abi::AppImageFormat::elf, load);
    ok = expect(result.code == app_abi::AppElfProbeCode::entry_outside_segment,
                "entry outside load segment rejected") && ok;

    auto rwx = make_minimal_elf(0x24070000U, app_abi::kAppElfPfW | app_abi::kAppElfPfX);
    result = probe_bytes(rwx, app_abi::AppImageFormat::elf, load);
    ok = expect(result.code == app_abi::AppElfProbeCode::rwx_segment,
                "RWX segment rejected") && ok;

    app_abi::AppElfLoadBackend backend{};
    const app_abi::AppImage valid_image{
        .name = "fixture",
        .format = app_abi::AppImageFormat::elf,
        .image_base = elf.data(),
        .image_size = elf.size(),
    };
    auto loaded = app_abi::app_elf_load_image(&backend,
                                              valid_image,
                                              app_abi::AppLoadBuffer{
                                                  .base = load.data(),
                                                  .size = load.size(),
                                                  .align = 16,
                                              });
    ok = expect(loaded.code == app_abi::AppRunCode::ok &&
                    loaded.image.entry != nullptr &&
                    backend.last.plan.entry_address == reinterpret_cast<std::uintptr_t>(loaded.image.entry),
                "ELF load backend materializes LoadedAppImage") && ok;

    loaded = app_abi::app_elf_load_image(nullptr,
                                         valid_image,
                                         app_abi::AppLoadBuffer{
                                             .base = load.data(),
                                             .size = load.size(),
                                             .align = 16,
                                         });
    ok = expect(loaded.code == app_abi::AppRunCode::invalid_argument,
                "ELF load backend rejects missing backend state") && ok;

    const app_abi::AppImage bad_magic_image{
        .name = "bad_magic",
        .format = app_abi::AppImageFormat::elf,
        .image_base = bad_magic.data(),
        .image_size = bad_magic.size(),
    };
    loaded = app_abi::app_elf_load_image(&backend,
                                         bad_magic_image,
                                         app_abi::AppLoadBuffer{
                                             .base = load.data(),
                                             .size = load.size(),
                                             .align = 16,
                                         });
    ok = expect(loaded.code == app_abi::AppRunCode::load_failed &&
                    loaded.backend_error == static_cast<int>(app_abi::AppElfProbeCode::bad_magic) &&
                    backend.last.plan.probe.code == app_abi::AppElfProbeCode::bad_magic,
                "ELF load backend preserves probe failure diagnostics") && ok;

    auto overlap = make_minimal_elf();
    overlap.resize(0x208U);
    app_abi::AppElf32Header overlap_header{};
    std::memcpy(&overlap_header, overlap.data(), sizeof(overlap_header));
    overlap_header.phnum = 2;
    std::memcpy(overlap.data(), &overlap_header, sizeof(overlap_header));
    app_abi::AppElf32ProgramHeader ph{};
    ph.type = app_abi::kAppElfPtLoad;
    ph.offset = 0x200U;
    ph.vaddr = 0x24070002U;
    ph.paddr = ph.vaddr;
    ph.filesz = 4U;
    ph.memsz = 4U;
    ph.flags = app_abi::kAppElfPfX;
    ph.align = 1U;
    std::memcpy(overlap.data() + overlap_header.phoff + sizeof(app_abi::AppElf32ProgramHeader),
                &ph,
                sizeof(ph));
    result = probe_bytes(overlap, app_abi::AppImageFormat::elf, load);
    ok = expect(result.code == app_abi::AppElfProbeCode::overlapping_segments,
                "overlapping PT_LOAD segments rejected") && ok;

    MemoryStorage storage{};
    storage.bytes.resize(elf.size());
    std::vector<std::byte> transport_buffer(1024);
    const auto status = receive_payload(elf, storage, transport_buffer);
    std::vector<std::byte> received_cache(elf.size());
    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = loader::Result{.stage = loader::Stage::verified, .code = loader::Code::ok},
        .manifest = status.packet.manifest,
        .storage = make_storage(storage),
        .output = std::span<std::byte>{received_cache.data(), received_cache.size()},
    });
    ok = expect(read.code == loader::ReceivedImageReadCode::not_launch_ready,
                "non launch-ready received ELF rejected before staging") && ok;

    return ok;
}

} // namespace

int main() {
    bool ok = true;
    const fs::path sample_dir{CHARM_APP_ABI_ELF_SAMPLE_DIR};
    ok = expect_real_elf_load("hello_app", sample_dir / "hello_app.elf") && ok;
    ok = expect_real_elf_load("player_min", sample_dir / "player_min.elf") && ok;
    ok = expect_large_segmented_elf_load() && ok;
    ok = expect_error_paths() && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-received-elf-smoke] ok");
    return 0;
}
