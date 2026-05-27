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
#include <vector>

namespace {

namespace app_abi = charm::app_abi;
namespace fs = std::filesystem;
namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::vector<std::byte> bytes{};
};

struct StageProbeResult {
    loader::ReceivedImageReadCode read_code{loader::ReceivedImageReadCode::ok};
    app_abi::AppReceivedImageStageCode stage_code{app_abi::AppReceivedImageStageCode::ok};
    app_abi::AppElfProbeResult probe{};
    app_abi::AppElfLoadPlanResult plan{};
    std::uint32_t read_bytes{0};
    std::uint32_t staged_bytes{0};
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
        .base_address = 0x24040000U,
        .capacity_bytes = static_cast<std::uint32_t>(memory.bytes.size()),
        .write = storage_write,
        .read = storage_read,
    };
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
                                             MemoryStorage& storage) {
    std::vector<std::byte> stream(payload.size() + 1024U);
    const auto built = loader::packet_stream_build(loader::PacketStreamBuildConfig{
                                                       .payload = payload,
                                                       .chunk_size = 257,
                                                       .check_crc = true,
                                                       .append_launch_dry_run = true,
                                                   },
                                                   stream);
    if (built.code != loader::PacketStreamBuildCode::ok) {
        return {.code = loader::ByteTransportCode::invalid_argument};
    }

    std::array<std::byte, 2048> transport_buffer{};
    loader::PacketRuntime packet_runtime{make_storage(storage)};
    loader::ByteTransportRuntime transport{
        packet_runtime,
        loader::ByteTransportConfig{
            .buffer = transport_buffer,
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

StageProbeResult stage_probe(const loader::ByteTransportResult& status,
                             loader::Storage storage,
                             std::span<std::byte> received_cache,
                             std::span<std::byte> app_cache,
                             std::span<std::byte> load_buffer) {
    StageProbeResult result{};
    const auto read = loader::received_image_read(loader::ReceivedImageReadConfig{
        .status = status.packet.receive,
        .manifest = status.packet.manifest,
        .storage = storage,
        .output = received_cache,
    });
    result.read_code = read.code;
    result.read_bytes = read.bytes_read;
    if (read.code != loader::ReceivedImageReadCode::ok) {
        return result;
    }

    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_app",
        .format = app_abi::AppImageFormat::elf,
        .image = read.image,
        .verified = true,
        .cache = app_cache,
    });
    result.stage_code = staged.code;
    result.staged_bytes = staged.bytes_copied;
    if (staged.code != app_abi::AppReceivedImageStageCode::ok) {
        return result;
    }

    result.plan = app_abi::app_elf_load_plan(staged.image, app_abi::AppLoadBuffer{
                                                               .base = load_buffer.data(),
                                                               .size = load_buffer.size(),
                                                               .align = 16,
                                                           });
    result.probe = result.plan.plan.probe;
    return result;
}

std::vector<std::byte> make_non_elf_payload() {
    return {
        std::byte{'n'},
        std::byte{'o'},
        std::byte{'t'},
        std::byte{'-'},
        std::byte{'e'},
        std::byte{'l'},
        std::byte{'f'},
    };
}

} // namespace

int main() {
    bool ok = true;
    const fs::path sample_dir{CHARM_APP_ABI_ELF_SAMPLE_DIR};

    std::vector<std::byte> elf{};
    ok = expect(read_file(sample_dir / "hello_app.elf", elf), "sample ELF file reads") && ok;

    MemoryStorage storage{};
    storage.bytes.resize(elf.size());
    const auto status = receive_payload(elf, storage);
    ok = expect(status.code == loader::ByteTransportCode::ok &&
                    status.packet.receive.stage == loader::Stage::launch_ready,
                "packetstream reaches launch_ready") && ok;

    std::vector<std::byte> received_cache(128U * 1024U);
    std::vector<std::byte> app_cache(128U * 1024U);
    std::vector<std::byte> load_buffer(64U * 1024U);
    const auto result = stage_probe(status,
                                    make_storage(storage),
                                    received_cache,
                                    app_cache,
                                    load_buffer);
    ok = expect(result.read_code == loader::ReceivedImageReadCode::ok &&
                    result.read_bytes == elf.size(),
                "launch_ready payload reads back") && ok;
    ok = expect(result.stage_code == app_abi::AppReceivedImageStageCode::ok &&
                    result.staged_bytes == elf.size(),
                "received payload stages as AppImage") && ok;
    ok = expect(result.probe.code == app_abi::AppElfProbeCode::ok &&
                    result.probe.runnable &&
                    result.probe.segment_count != 0U &&
                    result.probe.load_span != 0U,
                "staged ELF load-probes successfully") && ok;
    ok = expect(result.plan.code == app_abi::AppRunCode::ok &&
                    result.plan.plan.loaded.entry != nullptr &&
                    result.plan.plan.loaded.name == "received_app" &&
                    result.plan.plan.entry_address ==
                        (reinterpret_cast<std::uintptr_t>(load_buffer.data()) + result.probe.entry_offset),
                "staged ELF produces a disabled run plan") && ok;

    const auto not_ready = stage_probe(loader::ByteTransportResult{
                                           .packet = loader::PacketResult{
                                               .receive = loader::Result{
                                                   .stage = loader::Stage::verified,
                                                   .code = loader::Code::ok,
                                               },
                                               .manifest = status.packet.manifest,
                                           },
                                       },
                                       make_storage(storage),
                                       received_cache,
                                       app_cache,
                                       load_buffer);
    ok = expect(not_ready.read_code == loader::ReceivedImageReadCode::not_launch_ready,
                "non launch-ready payload does not stage") && ok;

    std::vector<std::byte> tiny_received(4);
    const auto read_too_small = stage_probe(status,
                                            make_storage(storage),
                                            tiny_received,
                                            app_cache,
                                            load_buffer);
    ok = expect(read_too_small.read_code == loader::ReceivedImageReadCode::output_too_small,
                "small received cache rejected") && ok;

    std::vector<std::byte> tiny_load(4);
    const auto load_too_small = stage_probe(status,
                                            make_storage(storage),
                                            received_cache,
                                            app_cache,
                                            tiny_load);
    ok = expect(load_too_small.probe.code == app_abi::AppElfProbeCode::load_buffer_too_small,
                "small load buffer rejected") && ok;
    ok = expect(load_too_small.plan.code == app_abi::AppRunCode::load_failed &&
                    load_too_small.plan.plan.loaded.entry == nullptr,
                "failed probe does not produce a loaded entry") && ok;

    auto non_elf = make_non_elf_payload();
    MemoryStorage non_elf_storage{};
    non_elf_storage.bytes.resize(non_elf.size());
    const auto non_elf_status = receive_payload(non_elf, non_elf_storage);
    std::vector<std::byte> non_elf_received(64);
    std::vector<std::byte> non_elf_app(64);
    std::vector<std::byte> non_elf_load(1024);
    const auto bad_magic = stage_probe(non_elf_status,
                                       make_storage(non_elf_storage),
                                       non_elf_received,
                                       non_elf_app,
                                       non_elf_load);
    ok = expect(bad_magic.read_code == loader::ReceivedImageReadCode::ok &&
                    bad_magic.stage_code == app_abi::AppReceivedImageStageCode::ok &&
                    bad_magic.probe.code == app_abi::AppElfProbeCode::bad_header,
                "non ELF payload stages but fails ELF probe") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-stage-probe-smoke] ok");
    return 0;
}
