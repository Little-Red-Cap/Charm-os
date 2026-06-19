#include "qemu_virtual_backend.hpp"

#include <cstddef>
#include <cstring>

namespace resident_elf_qemu {
namespace {

VirtualCapabilityCounters g_capability_counters{};
VirtualStoreMediaCounters g_store_media_counters{};
std::uint32_t g_tick_ms = 1000U;
std::uint32_t g_input_cursor = 0;

struct VirtualStorageFile {
    const char* path;
    const unsigned char* data;
    std::size_t size;
};

struct VirtualStorageOpenFile {
    bool open;
    std::uint32_t file_index;
    std::size_t cursor;
};

constexpr int kVirtualStorageFdBase = 3;
constexpr unsigned char kVirtualStorageReadme[] = "Charm QEMU virtual storage\n";
constexpr unsigned char kVirtualStorageAlpha[] = "alpha resource\n";
constexpr unsigned char kVirtualStorageBeta[] = "beta media blob\n";
constexpr VirtualStorageFile kVirtualStorageFiles[] = {
    VirtualStorageFile{
        .path = "/virtual/readme.txt",
        .data = kVirtualStorageReadme,
        .size = sizeof(kVirtualStorageReadme) - 1U,
    },
    VirtualStorageFile{
        .path = "/virtual/alpha.txt",
        .data = kVirtualStorageAlpha,
        .size = sizeof(kVirtualStorageAlpha) - 1U,
    },
    VirtualStorageFile{
        .path = "/virtual/beta.bin",
        .data = kVirtualStorageBeta,
        .size = sizeof(kVirtualStorageBeta) - 1U,
    },
};
VirtualStorageOpenFile g_storage_open_files[4]{};

constexpr CharmAppInputState kInputSequence[] = {
    CharmAppInputState{
        .encoder1_delta = 1,
        .encoder2_delta = 0,
        .encoder1_pressed = 0,
        .encoder2_pressed = 0,
        .pointer_detected = 1,
        .pointer_down = 0,
        .pointer_x = 3,
        .pointer_y = 5,
        .pointer_max_x = 15,
        .pointer_max_y = 15,
    },
    CharmAppInputState{
        .encoder1_delta = 0,
        .encoder2_delta = 1,
        .encoder1_pressed = 0,
        .encoder2_pressed = 0,
        .pointer_detected = 1,
        .pointer_down = 1,
        .pointer_x = 4,
        .pointer_y = 6,
        .pointer_max_x = 15,
        .pointer_max_y = 15,
    },
    CharmAppInputState{
        .encoder1_delta = -1,
        .encoder2_delta = 0,
        .encoder1_pressed = 1,
        .encoder2_pressed = 0,
        .pointer_detected = 1,
        .pointer_down = 1,
        .pointer_x = 5,
        .pointer_y = 7,
        .pointer_max_x = 15,
        .pointer_max_y = 15,
    },
    CharmAppInputState{
        .encoder1_delta = 0,
        .encoder2_delta = -1,
        .encoder1_pressed = 0,
        .encoder2_pressed = 1,
        .pointer_detected = 1,
        .pointer_down = 0,
        .pointer_x = 6,
        .pointer_y = 8,
        .pointer_max_x = 15,
        .pointer_max_y = 15,
    },
};

struct UartCmsdk {
    static constexpr std::uint32_t base = 0x40004000u;
    static constexpr std::uint32_t data = base + 0x00u;
    static constexpr std::uint32_t state = base + 0x04u;
    static constexpr std::uint32_t ctrl = base + 0x08u;
    static constexpr std::uint32_t state_txbf = 1u << 1;
    static constexpr std::uint32_t ctrl_tx_enable = 1u << 0;
    static constexpr std::uint32_t ctrl_rx_enable = 1u << 1;

    static void init() noexcept {
        auto* reg = reinterpret_cast<volatile std::uint32_t*>(ctrl);
        *reg = ctrl_tx_enable | ctrl_rx_enable;
    }

    static void write_byte(char ch) noexcept {
        auto* status = reinterpret_cast<volatile std::uint32_t*>(state);
        auto* out = reinterpret_cast<volatile std::uint32_t*>(data);
        while ((*status & state_txbf) != 0u) {
        }
        *out = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
    }
};

void write_hex_byte(unsigned char value) noexcept {
    static constexpr char kHex[] = "0123456789abcdef";
    write_byte(kHex[(value >> 4U) & 0x0fU]);
    write_byte(kHex[value & 0x0fU]);
}

void write_signed(int value) noexcept {
    if (value < 0) {
        write_byte('-');
        write_dec(static_cast<std::uint32_t>(-value));
        return;
    }
    write_dec(static_cast<std::uint32_t>(value));
}

int console_write(const char* text, std::size_t len) {
    if (text == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    g_capability_counters.console_bytes += static_cast<std::uint32_t>(len);
    for (std::size_t i = 0; i < len; ++i) {
        write_byte(text[i]);
    }
    return static_cast<int>(len);
}

std::uint32_t now_ms() {
    ++g_capability_counters.time_now;
    g_tick_ms += 17U;
    return g_tick_ms;
}

int display_describe(CharmAppDisplayMode* out_mode) {
    if (out_mode == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    *out_mode = CharmAppDisplayMode{
        .width = 16U,
        .height = 16U,
        .stride_bytes = 16U * 4U,
        .format = CHARM_APP_PIXEL_FORMAT_ARGB8888,
    };
    ++g_capability_counters.display_describe;
    write("resident-elf-qemu: display describe width=16 height=16 stride=64 format=argb8888 frame_bytes=1024\n");
    return CHARM_APP_STATUS_OK;
}

int display_present(const void* pixels, std::uint32_t bytes) {
    ++g_capability_counters.display_present;
    constexpr std::uint32_t kFrameBytes = 16U * 16U * 4U;
    if (pixels == nullptr || bytes != kFrameBytes) {
        write("resident-elf-qemu: display present bytes=");
        write_dec(bytes);
        write(" code=invalid_argument expected=");
        write_dec(kFrameBytes);
        write("\n");
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }

    std::uint32_t checksum = 0U;
    std::uint32_t hash = 2166136261U;
    const auto* data = static_cast<const unsigned char*>(pixels);
    for (std::uint32_t i = 0; i < bytes; ++i) {
        checksum = (checksum + data[i]) & 0xffffffffU;
        hash ^= data[i];
        hash *= 16777619U;
    }
    g_capability_counters.display_checksum = checksum;
    g_capability_counters.display_checksum_total =
        (g_capability_counters.display_checksum_total + checksum) & 0xffffffffU;
    g_capability_counters.display_hash = hash;
    g_capability_counters.display_hash_total =
        (g_capability_counters.display_hash_total ^ hash) & 0xffffffffU;
    g_capability_counters.display_frame_index = g_capability_counters.display_present;
    write("resident-elf-qemu: display present bytes=");
    write_dec(bytes);
    write(" checksum=");
    write_dec(checksum);
    write(" hash=");
    write_hex32(hash);
    write(" frame=");
    write_dec(g_capability_counters.display_frame_index);
    write("\n");
    write("resident-elf-qemu: display dump bytes=");
    write_dec(bytes);
    write(" checksum=");
    write_dec(checksum);
    write(" hash=");
    write_hex32(hash);
    write(" frame=");
    write_dec(g_capability_counters.display_frame_index);
    write(" hex=");
    for (std::uint32_t i = 0; i < bytes; ++i) {
        write_hex_byte(data[i]);
    }
    write("\n");
    return CHARM_APP_STATUS_OK;
}

int input_poll(CharmAppInputState* out_state) {
    ++g_capability_counters.input_poll;
    if (out_state == nullptr) {
        write("resident-elf-qemu: input poll code=invalid_argument\n");
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    constexpr std::uint32_t kInputCount =
        static_cast<std::uint32_t>(sizeof(kInputSequence) / sizeof(kInputSequence[0]));
    *out_state = kInputSequence[g_input_cursor % kInputCount];
    ++g_input_cursor;
    g_capability_counters.input_last_x = out_state->pointer_x;
    g_capability_counters.input_last_y = out_state->pointer_y;
    g_capability_counters.input_last_down = out_state->pointer_down;
    g_capability_counters.input_checksum +=
        static_cast<std::uint32_t>(out_state->pointer_x) +
        static_cast<std::uint32_t>(out_state->pointer_y) +
        static_cast<std::uint32_t>(out_state->pointer_down) +
        static_cast<std::uint32_t>(out_state->pointer_detected) +
        static_cast<std::uint32_t>(out_state->encoder1_delta + 8) +
        static_cast<std::uint32_t>(out_state->encoder2_delta + 8);
    write("resident-elf-qemu: input poll encoder1=");
    write_dec(static_cast<std::uint32_t>(out_state->encoder1_delta));
    write(" pointer=");
    write_dec(out_state->pointer_x);
    write(",");
    write_dec(out_state->pointer_y);
    write(" max=");
    write_dec(out_state->pointer_max_x);
    write(",");
    write_dec(out_state->pointer_max_y);
    write(" detected=");
    write_dec(out_state->pointer_detected);
    write(" down=");
    write_dec(out_state->pointer_down);
    write("\n");
    return CHARM_APP_STATUS_OK;
}

bool text_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs++ != *rhs++) {
            return false;
        }
    }
    return *lhs == '\0' && *rhs == '\0';
}

int storage_open(const char* path, int flags, int mode) {
    ++g_capability_counters.storage_open;
    if (path == nullptr) {
        write("resident-elf-qemu: storage open path=<null> code=invalid_argument fd=-1\n");
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    if (flags != 0 || mode != 0) {
        write("resident-elf-qemu: storage open path=");
        write(path);
        write(" code=unsupported fd=-1\n");
        return CHARM_APP_STATUS_UNSUPPORTED;
    }
    for (std::uint32_t file_index = 0;
         file_index < (sizeof(kVirtualStorageFiles) / sizeof(kVirtualStorageFiles[0]));
         ++file_index) {
        if (!text_equals(path, kVirtualStorageFiles[file_index].path)) {
            continue;
        }
        for (std::uint32_t slot = 0;
             slot < (sizeof(g_storage_open_files) / sizeof(g_storage_open_files[0]));
             ++slot) {
            if (g_storage_open_files[slot].open) {
                continue;
            }
            g_storage_open_files[slot] = VirtualStorageOpenFile{
                .open = true,
                .file_index = file_index,
                .cursor = 0U,
            };
            const int fd = kVirtualStorageFdBase + static_cast<int>(slot);
            write("resident-elf-qemu: storage open path=");
            write(path);
            write(" code=ok fd=");
            write_dec(static_cast<std::uint32_t>(fd));
            write(" size=");
            write_dec(static_cast<std::uint32_t>(kVirtualStorageFiles[file_index].size));
            write("\n");
            return fd;
        }
        write("resident-elf-qemu: storage open path=");
        write(path);
        write(" code=io_error fd=-1\n");
        return CHARM_APP_STATUS_IO_ERROR;
    }
    write("resident-elf-qemu: storage open path=");
    write(path);
    write(" code=unsupported fd=-1\n");
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int storage_read(int fd, void* buf, std::size_t len) {
    ++g_capability_counters.storage_read;
    const int slot = fd - kVirtualStorageFdBase;
    if (slot < 0 ||
        static_cast<std::size_t>(slot) >= (sizeof(g_storage_open_files) / sizeof(g_storage_open_files[0])) ||
        !g_storage_open_files[slot].open) {
        write("resident-elf-qemu: storage read fd=");
        write_signed(fd);
        write(" code=unsupported requested=");
        write_dec(static_cast<std::uint32_t>(len));
        write(" count=0 offset=0 remaining=0\n");
        return CHARM_APP_STATUS_UNSUPPORTED;
    }
    auto& open_file = g_storage_open_files[slot];
    const auto& file = kVirtualStorageFiles[open_file.file_index];
    if (buf == nullptr && len != 0U) {
        const auto offset = open_file.cursor;
        const std::size_t remaining = file.size - open_file.cursor;
        write("resident-elf-qemu: storage read fd=");
        write_signed(fd);
        write(" code=invalid_argument requested=");
        write_dec(static_cast<std::uint32_t>(len));
        write(" count=0 offset=");
        write_dec(static_cast<std::uint32_t>(offset));
        write(" remaining=");
        write_dec(static_cast<std::uint32_t>(remaining));
        write("\n");
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    const auto offset = open_file.cursor;
    const std::size_t remaining = file.size - open_file.cursor;
    const std::size_t count = remaining < len ? remaining : len;
    if (count != 0U) {
        std::memcpy(buf, file.data + open_file.cursor, count);
        open_file.cursor += count;
        g_capability_counters.storage_bytes += static_cast<std::uint32_t>(count);
    }
    write("resident-elf-qemu: storage read fd=");
    write_dec(static_cast<std::uint32_t>(fd));
    write(" code=ok requested=");
    write_dec(static_cast<std::uint32_t>(len));
    write(" count=");
    write_dec(static_cast<std::uint32_t>(count));
    write(" offset=");
    write_dec(static_cast<std::uint32_t>(offset));
    write(" remaining=");
    write_dec(static_cast<std::uint32_t>(file.size - open_file.cursor));
    write("\n");
    return static_cast<int>(count);
}

int storage_write(int, const void*, std::size_t) {
    ++g_capability_counters.storage_write;
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int storage_close(int fd) {
    ++g_capability_counters.storage_close;
    const int slot = fd - kVirtualStorageFdBase;
    if (slot >= 0 &&
        static_cast<std::size_t>(slot) < (sizeof(g_storage_open_files) / sizeof(g_storage_open_files[0])) &&
        g_storage_open_files[slot].open) {
        g_storage_open_files[slot] = {};
        write("resident-elf-qemu: storage close fd=");
        write_signed(fd);
        write(" code=ok\n");
        return CHARM_APP_STATUS_OK;
    }
    write("resident-elf-qemu: storage close fd=");
    write_signed(fd);
    write(" code=unsupported\n");
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int afe_configure(std::uint32_t, std::uint32_t) {
    ++g_capability_counters.afe_configure;
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int afe_read(void*, std::size_t) {
    ++g_capability_counters.afe_read;
    return CHARM_APP_STATUS_UNSUPPORTED;
}

void app_exit(int code) {
    ++g_capability_counters.app_exit;
    write("resident-elf-qemu: app.exit code=");
    write_dec(static_cast<std::uint32_t>(code));
    write("\n");
}

bool virtual_store_read(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept {
    auto* media = static_cast<VirtualStoreMedia*>(ctx);
    ++g_store_media_counters.read_calls;
    if (media == nullptr || media->data == nullptr) {
        ++g_store_media_counters.read_failures;
        return false;
    }
    const auto end = static_cast<std::uint64_t>(offset) + bytes.size();
    if (end > media->size) {
        ++g_store_media_counters.read_failures;
        return false;
    }
    std::memcpy(bytes.data(), media->data + offset, bytes.size());
    g_store_media_counters.read_bytes += static_cast<std::uint32_t>(bytes.size());
    return true;
}

} // namespace

void backend_init() noexcept {
    UartCmsdk::init();
}

void write_byte(char ch) noexcept {
    UartCmsdk::write_byte(ch);
}

void write(const char* text) noexcept {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        write_byte(*text++);
    }
}

void write(std::string_view text) noexcept {
    for (const char ch : text) {
        write_byte(ch);
    }
}

void write_hex32(std::uint32_t value) noexcept {
    static constexpr char kHex[] = "0123456789abcdef";
    write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        write_byte(kHex[(value >> shift) & 0x0fU]);
    }
}

void write_dec(std::uint32_t value) noexcept {
    char buf[11]{};
    char* cursor = buf + sizeof(buf);
    *--cursor = '\0';
    do {
        *--cursor = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    write(cursor);
}

void log_line(const char* text) noexcept {
    write(text);
    write("\n");
}

void log_view(std::string_view text) noexcept {
    write(text);
}

void reset_capability_counters() noexcept {
    g_capability_counters = {};
    g_tick_ms = 1000U;
    g_input_cursor = 0;
    for (auto& open_file : g_storage_open_files) {
        open_file = {};
    }
}

const VirtualCapabilityCounters& capability_counters() noexcept {
    return g_capability_counters;
}

void log_capability_counters(std::string_view name) noexcept {
    write("resident-elf-qemu: caps ");
    log_view(name);
    write(" console=");
    write_dec(g_capability_counters.console_bytes);
    write(" time=");
    write_dec(g_capability_counters.time_now);
    write(" describe=");
    write_dec(g_capability_counters.display_describe);
    write(" present=");
    write_dec(g_capability_counters.display_present);
    write(" input=");
    write_dec(g_capability_counters.input_poll);
    write(" exit=");
    write_dec(g_capability_counters.app_exit);
    write(" display_checksum=");
    write_dec(g_capability_counters.display_checksum);
    write(" display_checksum_total=");
    write_dec(g_capability_counters.display_checksum_total);
    write(" storage=");
    write_dec(g_capability_counters.storage_open);
    write("/");
    write_dec(g_capability_counters.storage_read);
    write("/");
    write_dec(g_capability_counters.storage_write);
    write("/");
    write_dec(g_capability_counters.storage_close);
    write(" storage_bytes=");
    write_dec(g_capability_counters.storage_bytes);
    write(" input_checksum=");
    write_dec(g_capability_counters.input_checksum);
    write(" input_last=");
    write_dec(g_capability_counters.input_last_x);
    write(",");
    write_dec(g_capability_counters.input_last_y);
    write(",");
    write_dec(g_capability_counters.input_last_down);
    write(" display_hash=");
    write_hex32(g_capability_counters.display_hash);
    write(" display_hash_total=");
    write_hex32(g_capability_counters.display_hash_total);
    write(" display_frame=");
    write_dec(g_capability_counters.display_frame_index);
    write("\n");
}

CharmAppApi make_virtual_app_api() noexcept {
    CharmAppApi api{};
    api.magic = CHARM_APP_API_MAGIC;
    api.version = CHARM_APP_API_VERSION;
    api.size = sizeof(CharmAppApi);
    api.console.write = console_write;
    api.time.now_ms = now_ms;
    api.display.describe = display_describe;
    api.display.present = display_present;
    api.input.poll = input_poll;
    api.storage.open = storage_open;
    api.storage.read = storage_read;
    api.storage.write = storage_write;
    api.storage.close = storage_close;
    api.afe.configure = afe_configure;
    api.afe.read = afe_read;
    api.app.exit = app_exit;
    return api;
}

bool probe_unsupported_capabilities() noexcept {
    reset_capability_counters();
    CharmAppApi api = make_virtual_app_api();
    const int storage_code = api.storage.open != nullptr
        ? api.storage.open("/unsupported", 0, 0)
        : -1;
    const int storage_read_code = api.storage.read != nullptr
        ? api.storage.read(-1, nullptr, 0)
        : -1;
    const int storage_write_code = api.storage.write != nullptr
        ? api.storage.write(-1, nullptr, 0)
        : -1;
    const int storage_close_code = api.storage.close != nullptr
        ? api.storage.close(-1)
        : -1;
    const int afe_code = api.afe.configure != nullptr
        ? api.afe.configure(1U, 48000U)
        : -1;
    const int afe_read_code = api.afe.read != nullptr
        ? api.afe.read(nullptr, 0)
        : -1;
    write("resident-elf-qemu: unsupported storage_open=");
    write_dec(static_cast<std::uint32_t>(storage_code));
    write(" storage_read=");
    write_dec(static_cast<std::uint32_t>(storage_read_code));
    write(" storage_write=");
    write_dec(static_cast<std::uint32_t>(storage_write_code));
    write(" storage_close=");
    write_dec(static_cast<std::uint32_t>(storage_close_code));
    write(" afe_configure=");
    write_dec(static_cast<std::uint32_t>(afe_code));
    write(" afe_read=");
    write_dec(static_cast<std::uint32_t>(afe_read_code));
    write(" storage_count=");
    write_dec(g_capability_counters.storage_open);
    write("/");
    write_dec(g_capability_counters.storage_read);
    write("/");
    write_dec(g_capability_counters.storage_write);
    write("/");
    write_dec(g_capability_counters.storage_close);
    write(" afe_count=");
    write_dec(g_capability_counters.afe_configure);
    write("/");
    write_dec(g_capability_counters.afe_read);
    write("\n");
    const bool ok = storage_code == CHARM_APP_STATUS_UNSUPPORTED &&
                    storage_read_code == CHARM_APP_STATUS_UNSUPPORTED &&
                    storage_write_code == CHARM_APP_STATUS_UNSUPPORTED &&
                    storage_close_code == CHARM_APP_STATUS_UNSUPPORTED &&
                    afe_code == CHARM_APP_STATUS_UNSUPPORTED &&
                    afe_read_code == CHARM_APP_STATUS_UNSUPPORTED &&
                    g_capability_counters.storage_open == 1U &&
                    g_capability_counters.storage_read == 1U &&
                    g_capability_counters.storage_write == 1U &&
                    g_capability_counters.storage_close == 1U &&
                    g_capability_counters.afe_configure == 1U &&
                    g_capability_counters.afe_read == 1U;
    reset_capability_counters();
    return ok;
}

void reset_store_media_counters() noexcept {
    g_store_media_counters = {};
}

const VirtualStoreMediaCounters& store_media_counters() noexcept {
    return g_store_media_counters;
}

charm::app_abi::AppStoreReader make_virtual_store_reader(VirtualStoreMedia& media) noexcept {
    return charm::app_abi::AppStoreReader{
        .ctx = &media,
        .read = virtual_store_read,
    };
}

void log_store_media_counters(const VirtualStoreMedia& media) noexcept {
    write("resident-elf-qemu: store-media kind=memory bytes=");
    write_dec(static_cast<std::uint32_t>(media.size));
    write(" read_calls=");
    write_dec(g_store_media_counters.read_calls);
    write(" read_bytes=");
    write_dec(g_store_media_counters.read_bytes);
    write(" read_failures=");
    write_dec(g_store_media_counters.read_failures);
    write("\n");
}

} // namespace resident_elf_qemu
