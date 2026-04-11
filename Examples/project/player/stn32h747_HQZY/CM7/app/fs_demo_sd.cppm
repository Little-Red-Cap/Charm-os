module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

#include "stm32h7xx_hal.h"
#include <span>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_sdmmc.h"
#include "sdmmc.h"

export module player.stm32h7.fs_demo_sd;

import util.core;
import boot_core;
import fs_block;
import fs_core;
import fs_errno;
import fs_fatfs;
import fs_stream;
import fs_vfs;
import out.api;
import out.channel;
import player.stm32h7.board_config;
import player.stm32h7.board_sdmmc;

#define CHARM_SDMMC_HANDLE hsd2
#define CHARM_SDMMC_INIT() MX_SDMMC2_SD_Init()
#define CHARM_SDMMC_CARD_INFO HAL_SD_CardInfoTypeDef
#define CHARM_SDMMC_CARD_STATE_TRANSFER HAL_SD_CARD_TRANSFER
#define CHARM_SDMMC_STATE_RESET HAL_SD_STATE_RESET
#define CHARM_SDMMC_GET_ERROR(handle) HAL_SD_GetError(handle)
#define CHARM_SDMMC_GET_STATE(handle) HAL_SD_GetCardState(handle)
#define CHARM_SDMMC_INIT_HANDLE(handle) HAL_SD_Init(handle)
#define CHARM_SDMMC_GET_INFO(handle, info) HAL_SD_GetCardInfo(handle, info)
#define CHARM_SDMMC_CONFIG_WIDE(handle, mode) HAL_SD_ConfigWideBusOperation(handle, mode)
#define CHARM_SDMMC_READ(handle, buf, lba, count, timeout) HAL_SD_ReadBlocks(handle, buf, lba, count, timeout)
#define CHARM_SDMMC_READ_DMA(handle, buf, lba, count) HAL_SD_ReadBlocks_DMA(handle, buf, lba, count)
#define CHARM_SDMMC_WRITE(handle, buf, lba, count, timeout) HAL_SD_WriteBlocks(handle, buf, lba, count, timeout)
#define CHARM_SDMMC_WRITE_DMA(handle, buf, lba, count) HAL_SD_WriteBlocks_DMA(handle, buf, lba, count)
static SD_HandleTypeDef& card = CHARM_SDMMC_HANDLE;

namespace {
    namespace board = player::stm32h7::board;
    constexpr auto kSdmmc = board::kSdmmc;
    static out::channel_sink* g_sink = nullptr;
    constexpr util::u32 kLogRetryMs = 20;
    template <out::fixed_string Fmt, typename... Args>
    inline void log(Args&&... args) noexcept {
        if (!g_sink) return;
        const util::u32 start = HAL_GetTick();
        while (true) {
            auto r = out::try_println<Fmt>(*g_sink, std::forward<Args>(args)...);
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
        const util::u32 flush_start = HAL_GetTick();
        while (true) {
            auto r = g_sink->flush();
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - flush_start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
    }

    constexpr util::u32 kTimeoutMs = 1000;

    void sdmmc_log_read_fail(util::u32 lba, util::u32 count, bool use_dma) noexcept {
        const auto err = static_cast<util::u32>(CHARM_SDMMC_GET_ERROR(&card));
        const auto state = static_cast<util::u32>(CHARM_SDMMC_GET_STATE(&card));
        const auto sta = static_cast<util::u32>(card.Instance->STA);
        const auto cmd = static_cast<util::u32>(card.Instance->CMD);
        const auto arg = static_cast<util::u32>(card.Instance->ARG);
        const auto resp1 = static_cast<util::u32>(card.Instance->RESP1);
        log<"fs sdmmc: read fail lba={} cnt={} dma={} err=0x{:08X} state=0x{:08X} sta=0x{:08X} cmd=0x{:08X} arg=0x{:08X} resp1=0x{:08X}">(
            lba, count, static_cast<int>(use_dma), err, state, sta, cmd, arg, resp1);
    }

    void sdmmc_log_read_timeout(util::u32 lba, util::u32 count) noexcept {
        const auto state = static_cast<util::u32>(CHARM_SDMMC_GET_STATE(&card));
        const auto sta = static_cast<util::u32>(card.Instance->STA);
        const auto cmd = static_cast<util::u32>(card.Instance->CMD);
        const auto resp1 = static_cast<util::u32>(card.Instance->RESP1);
        log<"fs sdmmc: read timeout lba={} cnt={} state=0x{:08X} sta=0x{:08X} cmd=0x{:08X} resp1=0x{:08X}">(
            lba, count, state, sta, cmd, resp1);
    }

    void sdmmc_probe_read(util::u32 lba) noexcept {
        alignas(4) std::array<util::u8, 512> buf{};
        if (CHARM_SDMMC_READ(&card, buf.data(), lba, 1, kTimeoutMs) != HAL_OK) {
            sdmmc_log_read_fail(lba, 1, false);
            return;
        }
        const util::u32 start = HAL_GetTick();
        while (CHARM_SDMMC_GET_STATE(&card) != CHARM_SDMMC_CARD_STATE_TRANSFER) {
            if ((HAL_GetTick() - start) > kTimeoutMs) {
                sdmmc_log_read_timeout(lba, 1);
                return;
            }
        }
        util::u32 non_ff = 0;
        for (const auto v : buf) {
            if (v != 0xFFu) ++non_ff;
        }
        log<"fs sdmmc: probe lba{} non_ff={} head {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}">(
            lba,
            non_ff,
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
            buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
        log<"fs sdmmc: probe lba{} tail {:02X} {:02X}">(
            lba, buf[510], buf[511]);
    }

    bool sdmmc_read_raw(util::u32 lba, std::span<util::u8> out) noexcept {
        if (out.size() != 512) return false;
        if (CHARM_SDMMC_READ(&card, out.data(), lba, 1, kTimeoutMs) != HAL_OK) {
            sdmmc_log_read_fail(lba, 1, false);
            return false;
        }
        const util::u32 start = HAL_GetTick();
        while (CHARM_SDMMC_GET_STATE(&card) != CHARM_SDMMC_CARD_STATE_TRANSFER) {
            if ((HAL_GetTick() - start) > kTimeoutMs) {
                sdmmc_log_read_timeout(lba, 1);
                return false;
            }
        }
        return true;
    }

    util::u32 le32(const util::u8* p) noexcept {
        return static_cast<util::u32>(p[0])
            | (static_cast<util::u32>(p[1]) << 8)
            | (static_cast<util::u32>(p[2]) << 16)
            | (static_cast<util::u32>(p[3]) << 24);
    }

    bool is_pow2(util::u8 v) noexcept {
        return v != 0 && ((v & (v - 1u)) == 0);
    }

    bool is_fat_boot_sector(const util::u8* buf) noexcept {
        if (!buf) return false;
        if (buf[510] != 0x55u || buf[511] != 0xAAu) return false;
        const util::u16 bps = static_cast<util::u16>(buf[11]) | (static_cast<util::u16>(buf[12]) << 8);
        if (!(bps == 512 || bps == 1024 || bps == 2048 || bps == 4096)) return false;
        const util::u8 spc = buf[13];
        if (spc == 0 || !is_pow2(spc)) return false;
        const util::u16 rsv = static_cast<util::u16>(buf[14]) | (static_cast<util::u16>(buf[15]) << 8);
        if (rsv == 0) return false;
        const util::u8 fats = buf[16];
        if (!(fats == 1u || fats == 2u)) return false;
        const util::u16 root_entries = static_cast<util::u16>(buf[17]) | (static_cast<util::u16>(buf[18]) << 8);
        const util::u16 tot_sec16 = static_cast<util::u16>(buf[19]) | (static_cast<util::u16>(buf[20]) << 8);
        const util::u32 tot_sec32 = le32(buf + 32);
        if (tot_sec16 == 0 && tot_sec32 == 0) return false;
        const util::u8 media = buf[21];
        if (!(media == 0xF0u || media == 0xF8u || media == 0xF9u || media == 0xFAu ||
                media == 0xFBu || media == 0xFCu || media == 0xFDu || media == 0xFEu ||
                media == 0xFFu)) {
            return false;
        }
        const util::u16 fatsz16 = static_cast<util::u16>(buf[22]) | (static_cast<util::u16>(buf[23]) << 8);
        const util::u32 fatsz32 = le32(buf + 36);
        if (fatsz16 == 0 && fatsz32 == 0) return false;
        if (root_entries != 0 && (root_entries % 16u) != 0) return false;
        if (std::memcmp(buf + 0x36, "FAT", 3) == 0) return true;
        if (std::memcmp(buf + 0x52, "FAT", 3) == 0) return true;
        return false;
    }

    struct FatProbe {
        util::u32 lba{};
        util::u32 total_sectors{};
        bool valid{false};
    };

    bool probe_fat_lba(util::u32 lba, FatProbe& out) noexcept {
        alignas(4) std::array<util::u8, 512> buf{};
        if (!sdmmc_read_raw(lba, std::span<util::u8>(buf.data(), buf.size()))) return false;
        if (!is_fat_boot_sector(buf.data())) return false;
        const util::u16 tot16 = static_cast<util::u16>(buf[19]) | (static_cast<util::u16>(buf[20]) << 8);
        const util::u32 tot32 = le32(buf.data() + 32);
        const util::u32 total = (tot32 != 0) ? tot32 : tot16;
        if (total == 0) return false;
        out.lba = lba;
        out.total_sectors = total;
        out.valid = true;
        return true;
    }

    void log_fat_bpb(util::u32 lba) noexcept {
        alignas(4) std::array<util::u8, 512> buf{};
        if (!sdmmc_read_raw(lba, std::span<util::u8>(buf.data(), buf.size()))) return;
        if (!is_fat_boot_sector(buf.data())) return;
        const util::u16 bps = static_cast<util::u16>(buf[11]) | (static_cast<util::u16>(buf[12]) << 8);
        const util::u8 spc = buf[13];
        const util::u16 rsv = static_cast<util::u16>(buf[14]) | (static_cast<util::u16>(buf[15]) << 8);
        const util::u8 fats = buf[16];
        const util::u16 root_entries = static_cast<util::u16>(buf[17]) | (static_cast<util::u16>(buf[18]) << 8);
        const util::u16 tot16 = static_cast<util::u16>(buf[19]) | (static_cast<util::u16>(buf[20]) << 8);
        const util::u32 tot32 = le32(buf.data() + 32);
        const util::u16 fatsz16 = static_cast<util::u16>(buf[22]) | (static_cast<util::u16>(buf[23]) << 8);
        const util::u32 fatsz32 = le32(buf.data() + 36);
        char label[12]{};
        const util::u8* label_src = (root_entries != 0) ? (buf.data() + 43) : (buf.data() + 71);
        for (int i = 0; i < 11; ++i) label[i] = static_cast<char>(label_src[i]);
        label[11] = '\0';
        log<"fs sdmmc: bpb lba={} bps={} spc={} rsv={} fats={} root={} tot16={} tot32={} fatsz16={} fatsz32={} label='{}'">(
            lba, bps, spc, rsv, fats, root_entries, tot16, tot32, fatsz16, fatsz32, label);
    }

    void dump_fat16_root(util::u32 part_lba) noexcept {
        alignas(4) std::array<util::u8, 512> bpb{};
        if (!sdmmc_read_raw(part_lba, std::span<util::u8>(bpb.data(), bpb.size()))) return;
        if (!is_fat_boot_sector(bpb.data())) return;
        const util::u16 bps = static_cast<util::u16>(bpb[11]) | (static_cast<util::u16>(bpb[12]) << 8);
        const util::u8 spc = bpb[13];
        const util::u16 rsv = static_cast<util::u16>(bpb[14]) | (static_cast<util::u16>(bpb[15]) << 8);
        const util::u8 fats = bpb[16];
        const util::u16 root_entries = static_cast<util::u16>(bpb[17]) | (static_cast<util::u16>(bpb[18]) << 8);
        const util::u16 fatsz16 = static_cast<util::u16>(bpb[22]) | (static_cast<util::u16>(bpb[23]) << 8);
        if (root_entries == 0 || bps != 512 || fatsz16 == 0 || spc == 0) return;
        const util::u32 root_dir_sectors = ((root_entries * 32u) + (bps - 1u)) / bps;
        const util::u32 first_root = part_lba + rsv + (static_cast<util::u32>(fats) * fatsz16);
        log<"fs sdmmc: root16 lba={} sectors={} entries={}">(
            first_root, root_dir_sectors, root_entries);
        std::array<util::u8, 512> sec{};
        if (!sdmmc_read_raw(first_root, std::span<util::u8>(sec.data(), sec.size()))) return;
        for (int i = 0; i < 16; ++i) {
            const util::u8* ent = sec.data() + (i * 32);
            if (ent[0] == 0x00) break;
            if (ent[0] == 0xE5) continue;
            if (ent[11] == 0x0F) continue;
            char name[12]{};
            for (int j = 0; j < 11; ++j) name[j] = static_cast<char>(ent[j]);
            name[11] = '\0';
            log<"fs sdmmc: root16 entry {} '{}' attr=0x{:02X}">(
                i, name, ent[11]);
        }
    }

    void dump_fat_root_any(util::u32 part_lba) noexcept {
        alignas(4) std::array<util::u8, 512> bpb{};
        if (!sdmmc_read_raw(part_lba, std::span<util::u8>(bpb.data(), bpb.size()))) return;
        if (!is_fat_boot_sector(bpb.data())) return;
        const util::u16 bps = static_cast<util::u16>(bpb[11]) | (static_cast<util::u16>(bpb[12]) << 8);
        const util::u8 spc = bpb[13];
        const util::u16 rsv = static_cast<util::u16>(bpb[14]) | (static_cast<util::u16>(bpb[15]) << 8);
        const util::u8 fats = bpb[16];
        const util::u16 root_entries = static_cast<util::u16>(bpb[17]) | (static_cast<util::u16>(bpb[18]) << 8);
        const util::u16 fatsz16 = static_cast<util::u16>(bpb[22]) | (static_cast<util::u16>(bpb[23]) << 8);
        const util::u32 fatsz32 = le32(bpb.data() + 36);
        if (root_entries != 0) {
            dump_fat16_root(part_lba);
            return;
        }
        if (bps != 512 || spc == 0) return;
        const util::u32 fatsz = (fatsz16 != 0) ? fatsz16 : fatsz32;
        if (fatsz == 0) return;
        const util::u32 root_cluster = le32(bpb.data() + 44);
        const util::u32 first_data = part_lba + rsv + (static_cast<util::u32>(fats) * fatsz);
        const util::u32 root_lba = first_data + ((root_cluster - 2u) * spc);
        log<"fs sdmmc: root32 lba={} cluster={} spc={}">(
            root_lba, root_cluster, spc);
        std::array<util::u8, 512> sec{};
        if (!sdmmc_read_raw(root_lba, std::span<util::u8>(sec.data(), sec.size()))) return;
        for (int i = 0; i < 16; ++i) {
            const util::u8* ent = sec.data() + (i * 32);
            if (ent[0] == 0x00) break;
            if (ent[0] == 0xE5) continue;
            if (ent[11] == 0x0F) continue;
            char name[12]{};
            for (int j = 0; j < 11; ++j) name[j] = static_cast<char>(ent[j]);
            name[11] = '\0';
            log<"fs sdmmc: root32 entry {} '{}' attr=0x{:02X}">(
                i, name, ent[11]);
        }
    }

    util::u64 le64(const util::u8* p) noexcept {
        const util::u64 lo = le32(p);
        const util::u64 hi = le32(p + 4);
        return lo | (hi << 32);
    }

    bool is_zero_guid(const util::u8* p) noexcept {
        for (int i = 0; i < 16; ++i) {
            if (p[i] != 0) return false;
        }
        return true;
    }

    util::u32 sdmmc_detect_partition_lba() noexcept {
        alignas(4) std::array<util::u8, 512> buf{};
        if (!sdmmc_read_raw(0, std::span<util::u8>(buf.data(), buf.size()))) return 0;
        const bool lba0_fat = is_fat_boot_sector(buf.data());
        if (buf[510] != 0x55u || buf[511] != 0xAAu) return lba0_fat ? 0u : 0u;
        const util::u8* p0 = &buf[0x1BE];
        util::u32 mbr_lba = 0;
        for (int i = 0; i < 4; ++i) {
            const util::u8* ent = p0 + (i * 16);
            const util::u8 type = ent[4];
            const util::u32 sectors = le32(ent + 12);
            if (type == 0x00) continue;
            mbr_lba = le32(ent + 8);
            if (mbr_lba != 0 && sectors != 0) {
                return mbr_lba;
            }
            if (type == 0xEE) {
                if (!sdmmc_read_raw(1, std::span<util::u8>(buf.data(), buf.size()))) return mbr_lba;
                if (std::memcmp(buf.data(), "EFI PART", 8) != 0) return mbr_lba;
                const util::u64 entries_lba = le64(buf.data() + 72);
                const util::u32 entry_size = le32(buf.data() + 84);
                if (entry_size < 56) return mbr_lba;
                if (!sdmmc_read_raw(static_cast<util::u32>(entries_lba),
                        std::span<util::u8>(buf.data(), buf.size()))) {
                    return mbr_lba;
                }
                if (is_zero_guid(buf.data())) return mbr_lba;
                const util::u64 first_lba = le64(buf.data() + 32);
                if (first_lba > 0xFFFFFFFFu) return mbr_lba;
                return static_cast<util::u32>(first_lba);
            }
            return mbr_lba;
        }
        return lba0_fat ? 0u : 0u;
    }

    void sdmmc_log_partitions() noexcept {
        alignas(4) std::array<util::u8, 512> buf{};
        if (!sdmmc_read_raw(0, std::span<util::u8>(buf.data(), buf.size()))) return;
        if (is_fat_boot_sector(buf.data())) {
            log<"fs sdmmc: fat boot at lba0">();
        }
        if (buf[510] != 0x55u || buf[511] != 0xAAu) {
            log<"fs sdmmc: mbr sig missing {:02X} {:02X}">(buf[510], buf[511]);
            return;
        }
        const util::u8* p0 = &buf[0x1BE];
        for (int i = 0; i < 4; ++i) {
            const util::u8* ent = p0 + (i * 16);
            const util::u8 status = ent[0];
            const util::u8 type = ent[4];
            const util::u32 lba = le32(ent + 8);
            const util::u32 sectors = le32(ent + 12);
            log<"fs sdmmc: mbr p{} status=0x{:02X} type=0x{:02X} lba={} sectors={}">(
                i, status, type, lba, sectors);
        }
        const util::u32 auto_lba = sdmmc_detect_partition_lba();
        if (auto_lba != 0) {
            log<"fs sdmmc: auto partition lba={}">(
                auto_lba);
        }
    }

    void sdmmc_diag_after_fail() noexcept {
        SDMMC_InitTypeDef init = {};
        init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
        init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
        init.BusWide = SDMMC_BUS_WIDE_1B;
        init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
        init.ClockDiv = kSdmmc.init_clock_div;
        (void)SDMMC_Init(card.Instance, init);
        (void)SDMMC_PowerState_ON(card.Instance);
        HAL_Delay(50);

        const auto e0 = SDMMC_CmdGoIdleState(card.Instance);
        const auto e8 = SDMMC_CmdOperCond(card.Instance);
        const auto r8 = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
        const auto e55 = SDMMC_CmdAppCommand(card.Instance, 0);
        const auto r55 = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
        const auto e41 = SDMMC_CmdAppOperCommand(
            card.Instance,
            SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
        const auto r41 = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
        const auto e41s = SDMMC_CmdAppOperCommand(
            card.Instance,
            SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY | SD_SWITCH_1_8V_CAPACITY);
        const auto r41s = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);

        util::u32 e41_loop = e41;
        util::u32 r41_loop = r41;
        util::u32 e41_nov = 0;
        util::u32 r41_nov = 0;
        util::u32 e41_nohcs = 0;
        util::u32 r41_nohcs = 0;
        util::u32 e41_raw = 0;
        util::u32 r41_raw = 0;
        constexpr util::u32 kOcrRaw = 0x00FF8000u;
        for (int i = 0; i < 1000 && ((r41_loop & 0x80000000u) == 0u); ++i) {
            e41_loop = SDMMC_CmdAppCommand(card.Instance, 0);
            r41_loop = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
            e41_loop = SDMMC_CmdAppOperCommand(
                card.Instance,
                SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
            r41_loop = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
            HAL_Delay(1);
        }
        if ((r41_loop & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_nov & 0x80000000u) == 0u); ++i) {
                e41_nov = SDMMC_CmdAppCommand(card.Instance, 0);
                r41_nov = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
                e41_nov = SDMMC_CmdAppOperCommand(
                    card.Instance,
                    SDMMC_VOLTAGE_WINDOW_SD | SDMMC_HIGH_CAPACITY);
                r41_nov = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }
        if ((r41_loop & 0x80000000u) == 0u && (r41_nov & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_nohcs & 0x80000000u) == 0u); ++i) {
                e41_nohcs = SDMMC_CmdAppCommand(card.Instance, 0);
                r41_nohcs = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
                e41_nohcs = SDMMC_CmdAppOperCommand(
                    card.Instance,
                    SDMMC_VOLTAGE_WINDOW_SD);
                r41_nohcs = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }
        if ((r41_loop & 0x80000000u) == 0u && (r41_nov & 0x80000000u) == 0u &&
            (r41_nohcs & 0x80000000u) == 0u) {
            for (int i = 0; i < 1000 && ((r41_raw & 0x80000000u) == 0u); ++i) {
                e41_raw = SDMMC_CmdAppCommand(card.Instance, 0);
                r41_raw = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
                e41_raw = SDMMC_CmdAppOperCommand(card.Instance, kOcrRaw);
                r41_raw = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
                HAL_Delay(1);
            }
        }

        const auto e2 = SDMMC_CmdSendCID(card.Instance);
        const auto r2_1 = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
        const auto r2_2 = SDMMC_GetResponse(card.Instance, SDMMC_RESP2);
        const auto r2_3 = SDMMC_GetResponse(card.Instance, SDMMC_RESP3);
        const auto r2_4 = SDMMC_GetResponse(card.Instance, SDMMC_RESP4);

        uint16_t rca = 0;
        const auto e3 = SDMMC_CmdSetRelAdd(card.Instance, &rca);
        const auto r3 = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
        const util::u32 rca_arg = static_cast<util::u32>(rca) << 16;

        const auto e9 = SDMMC_CmdSendCSD(card.Instance, rca_arg);
        const auto r9_1 = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
        const auto r9_2 = SDMMC_GetResponse(card.Instance, SDMMC_RESP2);
        const auto r9_3 = SDMMC_GetResponse(card.Instance, SDMMC_RESP3);
        const auto r9_4 = SDMMC_GetResponse(card.Instance, SDMMC_RESP4);

        const auto e7 = SDMMC_CmdSelDesel(card.Instance, rca_arg);
        const auto r7 = SDMMC_GetResponse(card.Instance, SDMMC_RESP1);
        card.Instance->ICR = 0xFFFFFFFFu;

        log<"fs sdmmc: diag e0=0x{:08X} e8=0x{:08X} r8=0x{:08X}">(
            static_cast<util::u32>(e0),
            static_cast<util::u32>(e8),
            static_cast<util::u32>(r8));
        log<"fs sdmmc: diag e55=0x{:08X} r55=0x{:08X}">(
            static_cast<util::u32>(e55),
            static_cast<util::u32>(r55));
        log<"fs sdmmc: diag e41=0x{:08X} r41=0x{:08X} e41s=0x{:08X} r41s=0x{:08X}">(
            static_cast<util::u32>(e41),
            static_cast<util::u32>(r41),
            static_cast<util::u32>(e41s),
            static_cast<util::u32>(r41s));
        log<"fs sdmmc: diag e41l=0x{:08X} r41l=0x{:08X}">(
            static_cast<util::u32>(e41_loop),
            static_cast<util::u32>(r41_loop));
        log<"fs sdmmc: diag e41n=0x{:08X} r41n=0x{:08X}">(
            static_cast<util::u32>(e41_nov),
            static_cast<util::u32>(r41_nov));
        log<"fs sdmmc: diag e41h=0x{:08X} r41h=0x{:08X}">(
            static_cast<util::u32>(e41_nohcs),
            static_cast<util::u32>(r41_nohcs));
        log<"fs sdmmc: diag e41r=0x{:08X} r41r=0x{:08X}">(
            static_cast<util::u32>(e41_raw),
            static_cast<util::u32>(r41_raw));
        log<"fs sdmmc: diag e2=0x{:08X} r2={:08X} {:08X} {:08X} {:08X}">(
            static_cast<util::u32>(e2),
            static_cast<util::u32>(r2_1),
            static_cast<util::u32>(r2_2),
            static_cast<util::u32>(r2_3),
            static_cast<util::u32>(r2_4));
        log<"fs sdmmc: diag e3=0x{:08X} r3=0x{:08X} rca=0x{:04X}">(
            static_cast<util::u32>(e3),
            static_cast<util::u32>(r3),
            static_cast<util::u32>(rca));
        log<"fs sdmmc: diag e9=0x{:08X} r9={:08X} {:08X} {:08X} {:08X}">(
            static_cast<util::u32>(e9),
            static_cast<util::u32>(r9_1),
            static_cast<util::u32>(r9_2),
            static_cast<util::u32>(r9_3),
            static_cast<util::u32>(r9_4));
        log<"fs sdmmc: diag e7=0x{:08X} r7=0x{:08X}">(
            static_cast<util::u32>(e7),
            static_cast<util::u32>(r7));
    }

    struct SdBlockDevice {
        bool init() noexcept {
            if constexpr (kSdmmc.verbose) {
                log<"fs sdmmc: init begin">();
            }

            CHARM_SDMMC_INIT();
            card.Init.ClockDiv = kSdmmc.init_clock_div;
            card.Init.BusWide = SDMMC_BUS_WIDE_1B;
            card.State = CHARM_SDMMC_STATE_RESET;
            const auto init_status = CHARM_SDMMC_INIT_HANDLE(&card);
            if constexpr (kSdmmc.verbose) {
                const auto diag = board::sdmmc_diag_snapshot();
                log<"fs sdmmc: rcc src=0x{:08X} SDMMC1_en={}">(
                    diag.rcc_src,
                    diag.sdmmc1_en);
                log<"fs sdmmc: rcc ahb2=0x{:08X} ahb3=0x{:08X}">(
                    diag.rcc_ahb2,
                    diag.rcc_ahb3);
                if constexpr (kSdmmc.verbose_gpio) {
                    log<"fs sdmmc: gpioa moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        diag.gpioa_moder, diag.gpioa_pupd, diag.gpioa_afr0, diag.gpioa_afr1);
                    log<"fs sdmmc: gpioc moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        diag.gpioc_moder, diag.gpioc_pupd, diag.gpioc_afr0, diag.gpioc_afr1);
                    log<"fs sdmmc: gpiod moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        diag.gpiod_moder, diag.gpiod_pupd, diag.gpiod_afr0, diag.gpiod_afr1);
                    log<"fs sdmmc: gpioe moder=0x{:08X} pupd=0x{:08X} afr0=0x{:08X} afr1=0x{:08X}">(
                        diag.gpioe_moder, diag.gpioe_pupd, diag.gpioe_afr0, diag.gpioe_afr1);
                }
                log<"fs sdmmc: pins cmd={} d0={} d1={} d2={} d3={} ck={}">(
                    diag.pin_cmd,
                    diag.pin_d0,
                    diag.pin_d1,
                    diag.pin_d2,
                    diag.pin_d3,
                    diag.pin_ck);
            }
            if (init_status != HAL_OK) {
                if constexpr (kSdmmc.verbose) {
                    const auto err = static_cast<util::u32>(CHARM_SDMMC_GET_ERROR(&card));
                    const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                    const auto clkcr = static_cast<util::u32>(card.Instance->CLKCR);
                    const auto sta = static_cast<util::u32>(card.Instance->STA);
                    const auto power = static_cast<util::u32>(card.Instance->POWER);
                    const auto cmd = static_cast<util::u32>(card.Instance->CMD);
                    const auto arg = static_cast<util::u32>(card.Instance->ARG);
                    const auto resp1 = static_cast<util::u32>(card.Instance->RESP1);
                    log<"fs sdmmc: HAL_SD_Init failed err=0x{:08X}">(
                        err);
                    log<"fs sdmmc: ker_ck={}Hz clkcr=0x{:08X} sta=0x{:08X}">(
                        clk, clkcr, sta);
                    log<"fs sdmmc: power=0x{:08X} cmd=0x{:08X} arg=0x{:08X} resp1=0x{:08X}">(
                        power, cmd, arg, resp1);
                    sdmmc_diag_after_fail();
                }
                return false;
            }

            if constexpr (kSdmmc.verbose) {
                const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                const auto clkcr = static_cast<util::u32>(card.Instance->CLKCR);
                log<"fs sdmmc: ker_ck={}Hz clkcr=0x{:08X}">(
                    clk, clkcr);
            }

            if constexpr (kSdmmc.try_4bit) {
                if (CHARM_SDMMC_CONFIG_WIDE(&card, SDMMC_BUS_WIDE_4B) != HAL_OK) {
                    if constexpr (kSdmmc.verbose) {
                        log<"fs sdmmc: 4b failed, try 1b">();
                    }
                    if (CHARM_SDMMC_CONFIG_WIDE(&card, SDMMC_BUS_WIDE_1B) != HAL_OK) {
                        if constexpr (kSdmmc.verbose) {
                            log<"fs sdmmc: 1b failed">();
                        }
                        return false;
                    }
                }
            } else {
                if (CHARM_SDMMC_CONFIG_WIDE(&card, SDMMC_BUS_WIDE_1B) != HAL_OK) {
                    if constexpr (kSdmmc.verbose) {
                        log<"fs sdmmc: 1b failed">();
                    }
                    return false;
                }
            }
            MODIFY_REG(card.Instance->CLKCR, SDMMC_CLKCR_CLKDIV, kSdmmc.xfer_clock_div);
            card.Init.ClockDiv = kSdmmc.xfer_clock_div;
            if constexpr (kSdmmc.verbose) {
                const auto clk = static_cast<util::u32>(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC));
                const auto clkcr = static_cast<util::u32>(card.Instance->CLKCR);
                log<"fs sdmmc: xfer ker_ck={}Hz clkcr=0x{:08X}">(
                    clk, clkcr);
            }

            if (CHARM_SDMMC_GET_INFO(&card, &info_) != HAL_OK) {
                if constexpr (kSdmmc.verbose) {
                    log<"fs sdmmc: card info failed">();
                }
                return false;
            }
            block_size_ = info_.LogBlockSize;
            block_count_ = info_.LogBlockNbr;
            util::u32 part_lba = kSdmmc.partition_lba;
            FatProbe best{};
            auto consider = [&](util::u32 lba) {
                FatProbe probe{};
                if (probe_fat_lba(lba, probe)) {
                    if (!best.valid || probe.total_sectors > best.total_sectors) {
                        best = probe;
                    }
                }
            };
            if (part_lba == 0) {
                const util::u32 auto_lba = sdmmc_detect_partition_lba();
                if (auto_lba != 0) {
                    if constexpr (kSdmmc.verbose) {
                        log<"fs sdmmc: partition auto={}">(
                            auto_lba);
                    }
                    part_lba = auto_lba;
                }
            }
            consider(part_lba);
            consider(0);
            consider(2048);
            if (best.valid && best.lba != part_lba) {
                if constexpr (kSdmmc.verbose) {
                    log<"fs sdmmc: fat boot pick lba={} sectors={}">(
                        best.lba, best.total_sectors);
                }
                part_lba = best.lba;
                block_count_ = best.total_sectors;
            }
            if constexpr (kSdmmc.verbose) {
                log_fat_bpb(part_lba);
                dump_fat_root_any(part_lba);
                dump_fat_root_any(0);
                dump_fat_root_any(2048);
            }
            part_lba_ = part_lba;
            if (part_lba > 0) {
                if (part_lba >= block_count_) {
                    if constexpr (kSdmmc.verbose) {
                        log<"fs sdmmc: partition lba {} >= block_count {}">(
                            part_lba, block_count_);
                    }
                    return false;
                }
                block_count_ -= part_lba;
            }
            if constexpr (kSdmmc.verbose) {
                log<"fs sdmmc: block_size={} block_count={} part_lba={}">(
                    block_size_, block_count_, part_lba);
                log<"fs sdmmc: dma enabled={}">(
                    kSdmmc.use_dma ? 1 : 0);
            }
            if constexpr (kSdmmc.probe_read) {
                sdmmc_probe_read(0);
                sdmmc_probe_read(2048);
            }
            if constexpr (kSdmmc.verbose) {
                sdmmc_log_partitions();
            }
            device_.ctx = this;
            device_.read = &SdBlockDevice::read_impl;
            device_.write = &SdBlockDevice::write_impl;
            device_.erase = &SdBlockDevice::erase_impl;
            device_.flush = &SdBlockDevice::flush_impl;
            device_.block_size = block_size_;
            device_.block_count = block_count_;
            return true;
        }

        [[nodiscard]] fs::BlockDevice& device() noexcept { return device_; }
        [[nodiscard]] fs::Status read_blocks(util::u64 lba, std::span<util::u8> data) noexcept {
            return read_impl(this, lba, data);
        }

    private:
        static bool can_dma(const void* data, std::size_t size) noexcept {
            return ((reinterpret_cast<std::uintptr_t>(data) & 0x3u) == 0u) && ((size & 0x3u) == 0u);
        }

        static fs::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
            auto* self = static_cast<SdBlockDevice*>(ctx);
            if (!self || data.empty() || (data.size() % self->block_size_) != 0) {
                return fs::Status{fs::Errc::inval};
            }
            const util::u32 count = static_cast<util::u32>(data.size() / self->block_size_);
            const util::u32 part_lba = self->part_lba_;
            for (util::u32 i = 0; i < count; ++i) {
                auto* dst = data.data() + (static_cast<std::size_t>(i) * self->block_size_);
                const util::u32 lba_i = static_cast<util::u32>(lba + i + part_lba);
                const bool use_dma = kSdmmc.use_dma && can_dma(dst, self->block_size_);
                if (use_dma) {
                    if (CHARM_SDMMC_READ_DMA(&card, dst, lba_i, 1) != HAL_OK) {
                        sdmmc_log_read_fail(lba_i, 1, true);
                        return fs::Status{fs::Errc::io};
                    }
                } else {
                    if (CHARM_SDMMC_READ(&card, dst, lba_i, 1, kTimeoutMs) != HAL_OK) {
                        sdmmc_log_read_fail(lba_i, 1, false);
                        return fs::Status{fs::Errc::io};
                    }
                }
                const util::u32 start = HAL_GetTick();
                while (CHARM_SDMMC_GET_STATE(&card) != CHARM_SDMMC_CARD_STATE_TRANSFER) {
                    if ((HAL_GetTick() - start) > kTimeoutMs) {
                        sdmmc_log_read_timeout(lba_i, 1);
                        return fs::Status{fs::Errc::timeout};
                    }
                }
            }
            return fs::Status{fs::Errc::ok};
        }

        static fs::Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
            auto* self = static_cast<SdBlockDevice*>(ctx);
            if (!self || data.empty() || (data.size() % self->block_size_) != 0) {
                return fs::Status{fs::Errc::inval};
            }
            const util::u32 count = static_cast<util::u32>(data.size() / self->block_size_);
            const util::u32 part_lba = self->part_lba_;
            const util::u32 start_lba = static_cast<util::u32>(lba + part_lba);
            const bool use_dma = kSdmmc.use_dma && can_dma(data.data(), data.size());
            if (use_dma) {
                if (CHARM_SDMMC_WRITE_DMA(&card, const_cast<uint8_t*>(data.data()),
                        start_lba, count) != HAL_OK) {
                    return fs::Status{fs::Errc::io};
                }
            } else {
                if (CHARM_SDMMC_WRITE(&card, const_cast<uint8_t*>(data.data()),
                        start_lba, count, kTimeoutMs) != HAL_OK) {
                    return fs::Status{fs::Errc::io};
                }
            }
            const util::u32 start = HAL_GetTick();
            while (CHARM_SDMMC_GET_STATE(&card) != CHARM_SDMMC_CARD_STATE_TRANSFER) {
                if ((HAL_GetTick() - start) > kTimeoutMs) return fs::Status{fs::Errc::timeout};
            }
            return fs::Status{fs::Errc::ok};
        }

        static fs::Status erase_impl(void*, util::u64, util::u64) noexcept {
            return fs::Status{fs::Errc::nosys};
        }

        static fs::Status flush_impl(void*) noexcept {
            return fs::Status{fs::Errc::ok};
        }

        fs::BlockDevice device_{};
        CHARM_SDMMC_CARD_INFO info_{};
        util::u32 block_size_{512};
        util::u32 block_count_{0};
        util::u32 part_lba_{0};
    };

    static SdBlockDevice g_sd{};
    static bool g_sd_inited = false;

    bool sd_init_once() noexcept {
        if (g_sd_inited) return true;
        g_sd_inited = g_sd.init();
        return g_sd_inited;
    }
} // namespace

export bool fs_boot_init() noexcept {
    if (!sd_init_once()) {
        log<"fs boot: sd init failed">();
        return false;
    }
    static fs::FatFsMount mount{};
    auto st = mount.mount(g_sd.device(), false);
    if (!st) {
        log<"fs boot: mount failed {}">(static_cast<int>(st.err));
        return false;
    }
    fs::clear_mounts();
    (void)fs::add_mount("/", mount.mount_point());
    log<"fs boot: mount ok">();
    return true;
}

export fs::BlockDevice* fs_sd_block_device() noexcept {
    if (!sd_init_once()) return nullptr;
    return &g_sd.device();
}

export bool fs_sd_selftest(util::u32 start_lba, util::u32 blocks, util::u32 stride) noexcept {
    if (!g_sd.init()) {
        log<"fs sdmmc: selftest init failed">();
        return false;
    }
    std::array<util::u8, 512> buf{};
    util::u32 crc = 0;
    util::u32 ok_blocks = 0;
    const util::u32 t0 = HAL_GetTick();
    for (util::u32 i = 0; i < blocks; ++i) {
        const util::u32 lba = start_lba + (i * stride);
        auto st = g_sd.read_blocks(lba, std::span<util::u8>(buf.data(), buf.size()));
        if (!st) {
            log<"fs sdmmc: selftest read fail lba={} err={}">(
                lba, static_cast<int>(st.err));
            break;
        }
        crc = boot::crc32_update(crc, buf.data(), buf.size());
        ++ok_blocks;
    }
    const util::u32 ms = HAL_GetTick() - t0;
    const util::u32 bytes = ok_blocks * static_cast<util::u32>(buf.size());
    log<"fs sdmmc: selftest blocks={} stride={} crc=0x{:08X} ms={} bytes={}">(
        ok_blocks, stride, crc, ms, bytes);
    return ok_blocks == blocks;
}

export void fs_demo_run() noexcept {
    (void)0;
}

export void fs_set_console_sink(out::channel_sink& sink) noexcept {
    g_sink = &sink;
}



