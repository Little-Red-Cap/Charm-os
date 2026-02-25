#include <array>
#include <cstdio>
#include <cstring>

import charm.foundation;
import charm.runtime;

struct MockFlash {
    std::array<util::u8, 2048> data{};

    bool read(util::u32 offset, util::span<util::u8> out) noexcept {
        if (offset + out.size() > data.size()) return false;
        std::memcpy(out.data(), data.data() + offset, out.size());
        return true;
    }

    bool write(util::u32 offset, util::span<const util::u8> in) noexcept {
        if (offset + in.size() > data.size()) return false;
        std::memcpy(data.data() + offset, in.data(), in.size());
        return true;
    }

    bool erase(util::u32 offset, util::u32 size) noexcept {
        if (offset + size > data.size()) return false;
        std::memset(data.data() + offset, 0xFF, size);
        return true;
    }
};

static void write_image(MockFlash& flash, util::u32 offset, const char* payload,
                        bool valid, util::u32 version, util::u32 key) {
    const auto len = static_cast<util::u32>(std::strlen(payload));
    boot::ImageHeader h{};
    h.payload_size = len;
    h.image_size = len + sizeof(boot::ImageHeader);
    h.payload_crc32 = valid
        ? boot::crc32_update(0, reinterpret_cast<const util::u8*>(payload), len)
        : 0x12345678;
    h.entry_offset = 0;
    h.image_version = version;
    h.min_version = 1;
    h.flags = static_cast<util::u16>(boot::ImageFlags::signed_image);
    h.signature = boot::calc_signature(key, reinterpret_cast<const util::u8*>(&h.payload_crc32),
                                       sizeof(h.payload_crc32));
    std::array<util::u8, 256> data{};
    const auto total = static_cast<util::u32>(sizeof(h) + len);
    std::memcpy(data.data(), &h, sizeof(h));
    std::memcpy(data.data() + sizeof(h), payload, len);
    std::array<util::u8, 64> scratch{};
    boot::FlashConfig cfg{.erase_size = 64, .write_size = 16, .scratch = scratch.data(),
                          .scratch_size = static_cast<util::u32>(scratch.size())};
    boot::flash_write(boot::Storage{&flash,
                      +[](void* ctx, util::u32 off, util::span<util::u8> out) noexcept {
                          return static_cast<MockFlash*>(ctx)->read(off, out);
                      },
                      +[](void* ctx, util::u32 off, util::span<const util::u8> in) noexcept {
                          return static_cast<MockFlash*>(ctx)->write(off, in);
                      },
                      +[](void* ctx, util::u32 off, util::u32 size) noexcept {
                          return static_cast<MockFlash*>(ctx)->erase(off, size);
                      }},
                      offset, util::span<const util::u8>(data.data(), total), cfg);
}

int main() {
    MockFlash flash{};
    boot::Storage storage{
        &flash,
        +[](void* ctx, util::u32 off, util::span<util::u8> out) noexcept {
            return static_cast<MockFlash*>(ctx)->read(off, out);
        },
        +[](void* ctx, util::u32 off, util::span<const util::u8> in) noexcept {
            return static_cast<MockFlash*>(ctx)->write(off, in);
        },
        +[](void* ctx, util::u32 off, util::u32 size) noexcept {
            return static_cast<MockFlash*>(ctx)->erase(off, size);
        }
    };

    boot::BootConfig cfg{
        .slot_a = {0, 512},
        .slot_b = {512, 512},
        .info = {1024, 64}
    };

    constexpr util::u32 key = 0xA5A5u;
    write_image(flash, cfg.slot_a.offset, "image_a", true, 2, key);
    write_image(flash, cfg.slot_b.offset, "image_b", true, 0, key);

    boot::BootInfo info{};
    info.active = boot::Slot::a;
    info.pending = boot::Slot::b;
    info.min_version = 1;
    (void)boot::write_boot_info(storage, cfg.info, info);

    boot::Policy policy{.min_version = 1, .sign_key = key, .require_signature = true};
    auto pick = boot::select_slot_policy(storage, cfg, info, policy);
    std::printf("[boot] pick=%s\n", pick.slot == boot::Slot::a ? "A" : "B");

    {
        std::array<util::u8, 64> packet{};
        boot::UartFrame frame{};
        frame.seq = 1;
        frame.offset = cfg.slot_a.offset;
        const char* patch = "patch";
        frame.size = static_cast<util::u16>(std::strlen(patch));
        std::memcpy(packet.data(), &frame, sizeof(frame));
        std::memcpy(packet.data() + sizeof(frame), patch, frame.size);
        frame.crc = boot::crc16(packet.data() + sizeof(frame), frame.size);
        std::memcpy(packet.data(), &frame, sizeof(frame));
        boot::UartRx rx{packet.data(), static_cast<util::usize>(sizeof(frame) + frame.size)};
        std::array<util::u8, 64> scratch{};
        boot::FlashConfig cfgf{.erase_size = 64, .write_size = 16, .scratch = scratch.data(),
                               .scratch_size = static_cast<util::u32>(scratch.size())};
        const bool ok = boot::uart_apply_frame(storage, cfgf, frame, rx);
        std::printf("[boot] uart_apply=%d\n", ok ? 1 : 0);
    }
    {
        std::array<util::u8, 64> packet{};
        boot::UartFrame frame{};
        frame.seq = 2;
        frame.offset = cfg.slot_a.offset + cfg.slot_a.size;
        const char* patch = "x";
        frame.size = static_cast<util::u16>(std::strlen(patch));
        std::memcpy(packet.data(), &frame, sizeof(frame));
        std::memcpy(packet.data() + sizeof(frame), patch, frame.size);
        frame.crc = boot::crc16(packet.data() + sizeof(frame), frame.size);
        std::memcpy(packet.data(), &frame, sizeof(frame));
        boot::UartRx rx{packet.data(), static_cast<util::usize>(sizeof(frame) + frame.size)};
        std::array<util::u8, 64> scratch{};
        boot::FlashConfig cfgf{.erase_size = 64, .write_size = 16, .scratch = scratch.data(),
                               .scratch_size = static_cast<util::u32>(scratch.size())};
        boot::UartPolicy policy{.allowed = cfg.slot_a, .enforce_range = true};
        boot::UartState state{};
        const bool ok = boot::uart_apply_frame_policy(storage, cfgf, frame, rx, policy, state);
        std::printf("[boot] uart_apply_oob=%d\n", ok ? 1 : 0);
    }

    const bool marked = boot::mark_success(storage, cfg, info, pick.slot);
    std::printf("[boot] mark_success=%d\n", marked ? 1 : 0);
    std::printf("[boot] active=%s\n", info.active == boot::Slot::a ? "A" : "B");
    return 0;
}
