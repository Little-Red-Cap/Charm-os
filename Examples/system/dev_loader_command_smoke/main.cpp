#include "charm_dev_loader_commands.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

namespace {

namespace loader = charm::dev_loader;

struct MemoryStorage {
    std::array<std::byte, 128> bytes{};
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

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

} // namespace

int main() {
    bool ok = true;
    MemoryStorage memory{};
    loader::CommandRuntime runtime{make_storage(memory)};

    auto result = runtime.handle("dev status");
    ok = expect(result.kind == loader::CommandKind::status, "status command classified") && ok;
    ok = expect(result.session.stage == loader::Stage::idle, "initial session idle") && ok;

    std::array<std::byte, 4> aa{};
    aa.fill(std::byte{0xaa});
    const auto expected_crc = loader::crc32_update(0, std::span<const std::byte>{aa});
    char begin_command[32]{};
    std::snprintf(begin_command, sizeof(begin_command), "dev begin 4 0x%08x", expected_crc);

    result = runtime.handle(begin_command);
    ok = expect(result.kind == loader::CommandKind::begin, "begin command classified") && ok;
    ok = expect(result.session.stage == loader::Stage::receiving &&
                    result.manifest.size_bytes == 4 &&
                    result.manifest.flags == 0,
                "begin creates crc-checked manifest") && ok;

    result = runtime.handle("dev fill 0xaa 4");
    ok = expect(result.kind == loader::CommandKind::fill, "fill command classified") && ok;
    ok = expect(result.session.stage == loader::Stage::complete &&
                    result.session.received_bytes == 4 &&
                    result.cursor == 4,
                "fill writes and completes image") && ok;

    result = runtime.handle("dev verify");
    ok = expect(result.kind == loader::CommandKind::verify, "verify command classified") && ok;
    ok = expect(result.session.stage == loader::Stage::verified &&
                    result.session.code == loader::Code::ok,
                "verify accepts crc") && ok;

    result = runtime.handle("dev launch dry-run");
    ok = expect(result.kind == loader::CommandKind::launch_dry_run,
                "launch dry-run command classified") && ok;
    ok = expect(result.session.stage == loader::Stage::launch_ready,
                "launch dry-run marks launch-ready") && ok;

    result = runtime.handle("dev abort");
    ok = expect(result.kind == loader::CommandKind::abort, "abort command classified") && ok;
    ok = expect(result.session.stage == loader::Stage::idle &&
                    result.cursor == 0 &&
                    !result.active,
                "abort resets runtime state") && ok;

    result = runtime.handle("dev begin 4");
    ok = expect(result.session.stage == loader::Stage::receiving &&
                    (result.manifest.flags & loader::kFlagSkipCrc) != 0U,
                "begin without crc uses skip-crc mode") && ok;
    result = runtime.handle("dev fill 0x55 4");
    ok = expect(result.session.stage == loader::Stage::complete, "skip-crc fill completes") && ok;
    result = runtime.handle("dev verify");
    ok = expect(result.session.stage == loader::Stage::verified, "skip-crc verify succeeds") && ok;

    result = runtime.handle("dev begin 0");
    ok = expect(result.kind == loader::CommandKind::usage_error &&
                    result.command_code == loader::CommandCode::invalid_argument,
                "bad begin reports usage error") && ok;

    result = runtime.handle("dev nope");
    ok = expect(result.kind == loader::CommandKind::unknown &&
                    result.command_code == loader::CommandCode::unknown_command,
                "unknown command is stable") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[dev-loader-command-smoke] ok");
    return 0;
}
