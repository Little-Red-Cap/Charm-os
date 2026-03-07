module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <span>
#include <string_view>
#include <utility>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_hal_i2s.h"

export module player.stm32h7.audio_mp3_demo;

import audio.decoder.mp3;
import audio.decoder.flac;
import boot_core;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import media.stream.source;
import media.stream.types;
import out.api;
import out.channel;
import util.core;
import util.expected;

extern "C" I2S_HandleTypeDef hi2s1;
extern "C" DMA_HandleTypeDef hdma_spi1_tx;

export bool audio_mp3_play_path(std::string_view open_path) noexcept;
export bool audio_mp3_start(std::string_view open_path) noexcept;
export bool audio_mp3_update() noexcept;
export void audio_mp3_stop() noexcept;
export bool audio_mp3_is_active() noexcept;
export void audio_mp3_set_log_suppressed(bool enabled) noexcept;

namespace {
#if defined(__GNUC__)
#define CHARM_DMA_BUFFER __attribute__((section(".dma_buffer"), aligned(32)))
#else
#define CHARM_DMA_BUFFER
#endif

#ifndef DMA_LISR_TCIF0
#define DMA_LISR_TCIF0 0x00000020U
#define DMA_LISR_HTIF0 0x00000010U
#define DMA_LIFCR_CTCIF0 0x00000020U
#define DMA_LIFCR_CHTIF0 0x00000010U
#endif

    static out::channel_sink* g_sink = nullptr;
    static bool g_log_suppressed = false;
    constexpr util::u32 kLogRetryMs = 20;
    template <out::fixed_string Fmt, typename... Args>
    inline void log(Args&&... args) noexcept {
        if (!g_sink || g_log_suppressed) return;
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

    constexpr std::uint32_t kTimeoutMs = 1000;
    constexpr std::size_t kI2sBufFrames = 1024;
    constexpr int kGainShift = 0;
    constexpr std::size_t kI2sWordsPerFrame = 2; // 16-bit stereo: 1 word per channel
    constexpr bool kVerbose = false;
    constexpr bool kRuntimeLog = false;
    constexpr bool kUpdateLog = false;
    constexpr bool kStartupLog = true;
    constexpr bool kOpenTrace = true;
    constexpr bool kStatLog = true;
    constexpr util::u32 kOpenReadLogEvery = 64;
    constexpr util::u32 kRunReadLogEvery = 512;
    constexpr util::u32 kReadLogMinMs = 200;
    constexpr std::size_t kMaxRead = 8192;
    constexpr bool kUseFixedPath = true;
    constexpr const char kFixedPath[] = "/jtwayne-pianos-by-jtwayne-7-174717.mp3";

    static bool g_opening = false;
    static util::u32 g_read_calls = 0;
    static util::u32 g_read_zero = 0;
    static util::u32 g_read_err = 0;
    static util::u32 g_read_enter = 0;
    static util::u32 g_read_slow = 0;
    static util::u32 g_seek_calls = 0;
    static util::u32 g_tell_calls = 0;
    static util::u32 g_size_calls = 0;
    static util::u32 g_read_clamp = 0;
    static util::u32 g_last_read_log_ms = 0;
    static bool g_allow_large_read = false;

    inline void clean_dcache(const void* addr, std::size_t size) noexcept {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        if (!addr || size == 0) return;
        if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U) return;
        const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(addr);
        if (raw >= 0x30000000u && raw < 0x30010000u) return;
        std::uintptr_t start = reinterpret_cast<std::uintptr_t>(addr);
        std::uintptr_t end = start + size;
        start &= ~static_cast<std::uintptr_t>(31);
        end = (end + 31u) & ~static_cast<std::uintptr_t>(31);
        SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(start),
            static_cast<int32_t>(end - start));
#else
        (void)addr;
        (void)size;
#endif
    }

    inline void flush_i2s_block(std::span<std::uint16_t> block) noexcept {
        clean_dcache(block.data(), block.size_bytes());
    }

    inline void allow_unaligned_access() noexcept {
#if defined(SCB_CCR_UNALIGN_TRP_Msk)
        SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;
#endif
    }

    extern "C" void charm_audio_i2s_half_notify();
    extern "C" void charm_audio_i2s_full_notify();

    inline void poll_i2s_dma_flags() noexcept {
#if defined(DMA1)
        const std::uint32_t lisr = DMA1->LISR;
        if ((lisr & DMA_LISR_HTIF0) != 0u) {
            DMA1->LIFCR = DMA_LIFCR_CHTIF0;
            charm_audio_i2s_half_notify();
        }
        if ((lisr & DMA_LISR_TCIF0) != 0u) {
            DMA1->LIFCR = DMA_LIFCR_CTCIF0;
            charm_audio_i2s_full_notify();
        }
#endif
    }

    inline void poll_i2s_ndtr(std::size_t half_words) noexcept {
        static int last_phase = -1;
        const auto* dma_inst = static_cast<DMA_Stream_TypeDef*>(hdma_spi1_tx.Instance);
        if (!dma_inst || half_words == 0) return;
        const int ndtr = static_cast<int>(dma_inst->NDTR);
        if (ndtr <= 0) return;
        const int phase = (ndtr <= static_cast<int>(half_words)) ? 1 : 0;
        if (last_phase < 0) {
            last_phase = phase;
            return;
        }
        if (phase != last_phase) {
            if (phase == 1) {
                charm_audio_i2s_half_notify();
            } else {
                charm_audio_i2s_full_notify();
            }
            last_phase = phase;
        }
    }

    char ascii_lower(char c) noexcept {
        if (c >= 'A' && c <= 'Z') return static_cast<char>(c + ('a' - 'A'));
        return c;
    }

    std::size_t name_len(std::string_view name) noexcept {
        if (!name.data()) return 0;
        if (name.size() > 0) return name.size();
        return std::strlen(name.data());
    }

    bool is_mp3_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'm' && c2 == 'p' && c3 == '3';
    }

    bool is_wav_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 4) return false;
        const char c0 = ascii_lower(name.data()[len - 4]);
        const char c1 = ascii_lower(name.data()[len - 3]);
        const char c2 = ascii_lower(name.data()[len - 2]);
        const char c3 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'w' && c2 == 'a' && c3 == 'v';
    }

    bool is_flac_name(std::string_view name) noexcept {
        const auto len = name_len(name);
        if (len < 5) return false;
        const char c0 = ascii_lower(name.data()[len - 5]);
        const char c1 = ascii_lower(name.data()[len - 4]);
        const char c2 = ascii_lower(name.data()[len - 3]);
        const char c3 = ascii_lower(name.data()[len - 2]);
        const char c4 = ascii_lower(name.data()[len - 1]);
        return c0 == '.' && c1 == 'f' && c2 == 'l' && c3 == 'a' && c4 == 'c';
    }

    struct FindAudioCtx {
        const char* prefix{nullptr};
        char path[256]{};
        bool found{false};
    };

    fs::Status find_first_mp3(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* out = static_cast<FindAudioCtx*>(ctx);
        if (!out) return fs::Status{fs::Errc::inval};
        if (out->found || entry.type != fs::NodeType::file) return fs::Status{fs::Errc::ok};
        if (!is_mp3_name(entry.name)) return fs::Status{fs::Errc::ok};
        const auto len = name_len(entry.name);
        const char* prefix = out->prefix ? out->prefix : "/";
        std::size_t pos = 0;
        for (std::size_t i = 0; prefix[i] != '\0' && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = prefix[i];
        }
        for (std::size_t i = 0; i < len && pos + 1 < sizeof(out->path); ++i) {
            out->path[pos++] = entry.name.data()[i];
        }
        out->path[pos] = '\0';
        out->found = true;
        return fs::Status{fs::Errc::ok};
    }

    struct FileSource {
        fs::File* file{nullptr};
        util::i64 base_offset{0};

        media::Result<util::usize> read(std::span<std::byte> out) noexcept {
            if (!file) return util::unexpected(media::Errc::bad_state);
            static int debug_read = 0;
            if (file->node.offset < base_offset) {
                (void)fs::vfs_seek(*file, base_offset);
            }
            std::size_t to_read = out.size();
            if (file->node.size > 0) {
                const auto remaining = file->node.size - file->node.offset;
                if (remaining <= 0) return static_cast<util::usize>(0);
                to_read = static_cast<std::size_t>(
                    std::min<util::i64>(remaining, static_cast<util::i64>(out.size())));
            }
            if (!g_allow_large_read && to_read > kMaxRead) {
                const auto req = to_read;
                to_read = kMaxRead;
                if constexpr (kOpenTrace) {
                    if (g_opening && g_read_clamp < 4) {
                        log<"mp3 demo: read clamp req={} -> {}">(req, to_read);
                        ++g_read_clamp;
                    }
                }
            }
            const auto before = file->node.offset;
            if constexpr (kOpenTrace) {
                if (g_opening && g_read_enter < 6) {
                    log<"mp3 demo: read enter#{} off={} req={} size={} base={}">(
                        g_read_enter, before, to_read, file->node.size, base_offset);
                    ++g_read_enter;
                }
            }
            const util::u32 t0 = HAL_GetTick();
            auto st = fs::vfs_read(*file, std::span<util::u8>(
                reinterpret_cast<util::u8*>(out.data()), to_read));
            const util::u32 dt = HAL_GetTick() - t0;
            if (!st) {
                ++g_read_err;
                if constexpr (kOpenTrace) {
                    log<"mp3 demo: read err={} off={} req={} errc={} ms={}">(
                        g_read_err, before, to_read, static_cast<int>(st.err), dt);
                }
                return util::unexpected(media::Errc::io_error);
            }
            const auto after = file->node.offset;
            const util::usize got = static_cast<util::usize>(after > before ? (after - before) : 0);
            if constexpr (kVerbose) {
                if (debug_read < 4) {
                    log<"mp3 demo: read before={} after={} req={}">(before, after, to_read);
                    if (before == base_offset && to_read >= 8) {
                        auto* b = reinterpret_cast<const util::u8*>(out.data());
                        log<"mp3 demo: data {} {} {} {} {} {} {} {}">(b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
                    }
                    ++debug_read;
                }
            }
            ++g_read_calls;
            if constexpr (kOpenTrace) {
                if (g_opening && dt > 50 && g_read_slow < 4) {
                    log<"mp3 demo: read slow#{} ms={} off={} req={} got={}">(
                        g_read_slow, dt, before, to_read, got);
                    ++g_read_slow;
                }
            }
            if (got == 0) {
                ++g_read_zero;
                if constexpr (kOpenTrace) {
                    log<"mp3 demo: read zero count={} off={} size={} base={} req={}">(
                        g_read_zero, before, file->node.size, base_offset, to_read);
                }
                return static_cast<util::usize>(0);
            }
            if constexpr (kOpenTrace) {
                const auto now = static_cast<util::u32>(HAL_GetTick());
                const auto interval = g_opening ? kOpenReadLogEvery : kRunReadLogEvery;
                if ((g_read_calls % interval) == 0 && (now - g_last_read_log_ms) > kReadLogMinMs) {
                    g_last_read_log_ms = now;
                    log<"mp3 demo: read#{} off {}->{} req {} got {} open={}">(
                        g_read_calls, before, after, to_read, got, static_cast<int>(g_opening));
                }
            }
            return got;
        }

        media::Result<util::i64> seek(util::i64 offset, media::SeekWhence whence) noexcept {
            if (!file) return util::unexpected(media::Errc::bad_state);
            util::i64 target = offset;
            if (whence == media::SeekWhence::cur) {
                target = (file->node.offset - base_offset) + offset + base_offset;
            } else if (whence == media::SeekWhence::end) {
                target = (file->node.size - base_offset) + offset + base_offset;
            } else {
                target = base_offset + offset;
            }
            if constexpr (kVerbose) {
                static int debug_seek = 0;
                if (debug_seek < 4) {
                    log<"mp3 demo: seek whence={} off={} -> {}">(static_cast<int>(whence), offset, target);
                    ++debug_seek;
                }
            }
            auto st = fs::vfs_seek(*file, target);
            if (!st) {
                if constexpr (kOpenTrace) {
                    log<"mp3 demo: seek err={} whence={} off={} -> {}">(
                        static_cast<int>(st.err), static_cast<int>(whence), offset, target);
                }
                return util::unexpected(media::Errc::io_error);
            }
            if constexpr (kOpenTrace) {
                if (g_opening && g_seek_calls < 6) {
                    log<"mp3 demo: seek#{} ok off={} now={}">(
                        g_seek_calls, target, file->node.offset);
                }
            }
            if constexpr (kOpenTrace) {
                if (g_opening && g_seek_calls < 6) {
                    log<"mp3 demo: seek#{} whence={} off={} -> {}">(
                        g_seek_calls, static_cast<int>(whence), offset, target);
                    ++g_seek_calls;
                }
            }
            return target;
        }

        media::Result<util::i64> tell() noexcept {
            if (!file) return util::unexpected(media::Errc::bad_state);
            const auto pos = file->node.offset - base_offset;
            if constexpr (kOpenTrace) {
                if (g_opening && g_tell_calls < 4) {
                    log<"mp3 demo: tell#{} -> {}">(g_tell_calls, pos);
                    ++g_tell_calls;
                }
            }
            return pos;
        }

        media::Result<util::i64> size() noexcept {
            if (!file) return util::unexpected(media::Errc::bad_state);
            const auto size = file->node.size - base_offset;
            if constexpr (kOpenTrace) {
                if (g_opening && g_size_calls < 4) {
                    log<"mp3 demo: size#{} -> {}">(g_size_calls, size);
                    ++g_size_calls;
                }
            }
            return size;
        }
    };

    struct Mp3Session {
        audio::Mp3Filter filter{};
        FileSource source{};
        bool ended{false};
    };

    struct WavInfo {
        std::uint16_t channels{0};
        std::uint32_t sample_rate{0};
        std::uint16_t bits_per_sample{0};
        std::uint32_t data_offset{0};
        std::uint32_t data_size{0};
    };

    struct WavState {
        WavInfo info{};
        std::uint32_t remaining{0};
    };

    struct FlacState {
        std::uint16_t channels{0};
        std::uint32_t sample_rate{0};
        bool opened{false};
        bool ended{false};
    };

    std::uint16_t read_u16_le(const std::uint8_t* data) noexcept {
        return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
    }

    std::uint32_t read_u32_le(const std::uint8_t* data) noexcept {
        return static_cast<std::uint32_t>(data[0])
            | (static_cast<std::uint32_t>(data[1]) << 8)
            | (static_cast<std::uint32_t>(data[2]) << 16)
            | (static_cast<std::uint32_t>(data[3]) << 24);
    }

    bool read_wav_header(fs::File& f, WavInfo& info) noexcept {
        std::array<std::uint8_t, 12> riff{};
        auto st = fs::vfs_read(f, std::span<std::uint8_t>{riff});
        if (!st) return false;
        if (std::memcmp(riff.data(), "RIFF", 4) != 0 || std::memcmp(riff.data() + 8, "WAVE", 4) != 0) {
            return false;
        }
        std::uint32_t cursor = 12;
        bool fmt_ok = false;
        bool data_ok = false;
        while (!fmt_ok || !data_ok) {
            if (cursor + 8 > f.node.size) return false;
            std::array<std::uint8_t, 8> chunk{};
            st = fs::vfs_read(f, std::span<std::uint8_t>{chunk});
            if (!st) return false;
            cursor += 8;
            const std::uint32_t size = read_u32_le(chunk.data() + 4);
            std::uint32_t next = cursor + size;
            if (size & 1u) {
                ++next;
            }
            if (std::memcmp(chunk.data(), "fmt ", 4) == 0) {
                if (size < 16) return false;
                std::array<std::uint8_t, 16> fmt{};
                st = fs::vfs_read(f, std::span<std::uint8_t>{fmt});
                if (!st) return false;
                cursor += 16;
                const std::uint16_t format = read_u16_le(fmt.data());
                info.channels = read_u16_le(fmt.data() + 2);
                info.sample_rate = read_u32_le(fmt.data() + 4);
                info.bits_per_sample = read_u16_le(fmt.data() + 14);
                if (format != 1 && format != 0xFFFEu) return false;
                fmt_ok = true;
                if (size > 16) {
                    cursor += (size - 16);
                    st = fs::vfs_seek(f, static_cast<std::int64_t>(cursor));
                    if (!st) return false;
                }
            } else if (std::memcmp(chunk.data(), "data", 4) == 0) {
                info.data_offset = cursor;
                info.data_size = size;
                data_ok = true;
            }
            if (!fmt_ok || !data_ok) {
                cursor = next;
                st = fs::vfs_seek(f, static_cast<std::int64_t>(cursor));
                if (!st) return false;
            }
        }
        return true;
    }

    enum class AudioKind : std::uint8_t { none, mp3, wav, flac };

    struct Mp3Player {
        fs::File file{};
        Mp3Session session{};
        WavState wav{};
        FlacState flac{};
        FileSource flac_source{};
        audio::FlacFilter flac_filter{};
        std::size_t period_words{0};
        AudioKind kind{AudioKind::none};
        bool active{false};
        bool started{false};
        bool file_open{false};
    };

    static Mp3Player g_player{};
    static std::array<std::uint8_t, kMaxRead> g_wav_read_buf CHARM_DMA_BUFFER{};
    static std::array<std::int32_t, kI2sBufFrames * 2 * 2> g_pcm32_buffer CHARM_DMA_BUFFER{};
    constexpr std::uintptr_t kSdramBase = 0xD0000000u;
    constexpr std::size_t kMp3ArenaSize = 128 * 1024;
    constexpr std::size_t kFlacArenaSize = 512 * 1024;
    constexpr std::uintptr_t kFlacArenaBase = kSdramBase + kMp3ArenaSize;
#ifndef CHARM_USE_SDRAM_ARENA
#define CHARM_USE_SDRAM_ARENA 1
#endif
#if !CHARM_USE_SDRAM_ARENA
    alignas(32) static std::array<std::uint8_t, kMp3ArenaSize> g_mp3_arena{};
    alignas(32) static std::array<std::uint8_t, kFlacArenaSize> g_flac_arena{};
#endif

    bool mp3_fill_block(
        Mp3Session& s,
        std::span<std::int16_t> pcm,
        std::span<std::uint16_t> out_words,
        std::uint8_t channels) noexcept {
        static int debug_blocks = 0;
        static int energy_blocks = 0;
        static std::uint32_t block_count = 0;
        if (s.ended) {
            std::memset(out_words.data(), 0, out_words.size_bytes());
            return false;
        }
        auto dst = std::span<std::byte>(
            reinterpret_cast<std::byte*>(pcm.data()),
            pcm.size() * sizeof(std::int16_t));
        auto res = s.filter.process({}, dst);
        if constexpr (kVerbose) {
            if (debug_blocks < 3) {
            log<"mp3 demo: produced={} eos={}">(res ? res->produced : 0, res ? res->end_of_stream : 0);
            ++debug_blocks;
            }
        }
        if (!res) {
            s.ended = true;
            std::memset(out_words.data(), 0, out_words.size_bytes());
            return false;
        }
        if (res->produced < dst.size()) {
            auto* tail = dst.data() + res->produced;
            const auto tail_size = dst.size() - res->produced;
            for (std::size_t i = 0; i < tail_size; ++i) {
                tail[i] = std::byte{0};
            }
        }
        if (res->produced == 0 && res->end_of_stream) {
            s.ended = true;
            return false;
        }
        const auto samples = dst.size() / sizeof(std::int16_t);
        std::uint32_t nonzero = 0;
        std::uint64_t abs_sum = 0;
        std::int16_t pre_min = 32767;
        std::int16_t pre_max = -32768;
        std::int16_t post_min = 32767;
        std::int16_t post_max = -32768;
        for (std::size_t i = 0; i < samples; ++i) {
            auto v = static_cast<std::int32_t>(pcm[i]);
            if (v != 0) ++nonzero;
            abs_sum += static_cast<std::uint64_t>(v < 0 ? -v : v);
            if (v < pre_min) pre_min = static_cast<std::int16_t>(v);
            if (v > pre_max) pre_max = static_cast<std::int16_t>(v);
            v <<= kGainShift;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            if (v < post_min) post_min = static_cast<std::int16_t>(v);
            if (v > post_max) post_max = static_cast<std::int16_t>(v);
            pcm[i] = static_cast<std::int16_t>(v);
        }
        const auto frames = static_cast<std::size_t>(samples / channels);
          for (std::size_t i = 0; i < frames; ++i) {
              const std::int16_t l = pcm[i * channels];
              const std::int16_t r = (channels > 1) ? pcm[i * channels + 1] : l;
              const std::size_t o = i * kI2sWordsPerFrame;
              out_words[o + 0] = static_cast<std::uint16_t>(l);
              out_words[o + 1] = static_cast<std::uint16_t>(r);
          }
        if constexpr (kVerbose) {
            if (energy_blocks < 3) {
            log<"mp3 demo: energy nonzero={} sum={}">(nonzero, abs_sum);
            ++energy_blocks;
            }
        }
        if constexpr (kVerbose) {
            if ((block_count % 100) == 0) {
            log<"mp3 demo: block#{} pre=[{},{}] post=[{},{}]">(
                block_count, pre_min, pre_max, post_min, post_max);
            }
        }
        if constexpr (kVerbose) {
            if (debug_blocks <= 2) {
            std::int16_t min_v = 32767;
            std::int16_t max_v = -32768;
            for (std::size_t i = 0; i < samples; ++i) {
                const auto v = pcm[i];
                if (v < min_v) min_v = v;
                if (v > max_v) max_v = v;
            }
            log<"mp3 demo: block min={} max={} s0={} s1={} s2={} s3={} s4={} s5={}">(min_v, max_v,
                pcm[0], pcm[1], pcm[2], pcm[3], pcm[4], pcm[5]);
            }
        }
        ++block_count;
        return true;
    }

    bool wav_fill_block(WavState& wav,
                        std::span<std::uint16_t> out_words) noexcept {
        if (wav.remaining == 0) {
            std::memset(out_words.data(), 0, out_words.size_bytes());
            return false;
        }
        const std::size_t frames = out_words.size() / 2;
        const std::size_t sample_bytes = wav.info.bits_per_sample / 8;
        const std::size_t frame_bytes = wav.info.channels * sample_bytes;
        std::size_t want = frames * frame_bytes;
        if (want > g_wav_read_buf.size()) want = g_wav_read_buf.size();
        if (want > wav.remaining) want = wav.remaining;
        want -= (want % frame_bytes);
        if (want == 0) {
            std::memset(out_words.data(), 0, out_words.size_bytes());
            wav.remaining = 0;
            return false;
        }
        auto st = fs::vfs_read(g_player.file,
            std::span<std::uint8_t>(g_wav_read_buf.data(), want));
        if (!st) {
            std::memset(out_words.data(), 0, out_words.size_bytes());
            wav.remaining = 0;
            return false;
        }
        wav.remaining -= static_cast<std::uint32_t>(want);

        if (wav.info.bits_per_sample == 16) {
            const auto* samples = reinterpret_cast<const std::uint16_t*>(g_wav_read_buf.data());
            const std::size_t sample_count = want / sizeof(std::uint16_t);
            if (wav.info.channels == 2) {
                const std::size_t copy_words = std::min(out_words.size(), sample_count);
                std::memcpy(out_words.data(), samples, copy_words * sizeof(std::uint16_t));
                if (copy_words < out_words.size()) {
                    std::memset(out_words.data() + copy_words, 0,
                        (out_words.size() - copy_words) * sizeof(std::uint16_t));
                }
            } else {
                const std::size_t frames_count = sample_count;
                const std::size_t max_frames = std::min(frames, frames_count);
                for (std::size_t i = 0; i < max_frames; ++i) {
                    const std::uint16_t s = samples[i];
                    out_words[i * 2] = s;
                    out_words[i * 2 + 1] = s;
                }
                if (max_frames < frames) {
                    std::memset(out_words.data() + (max_frames * 2), 0,
                        (frames - max_frames) * 2 * sizeof(std::uint16_t));
                }
            }
        } else if (wav.info.bits_per_sample == 24) {
            const std::size_t sample_count = want / 3;
            const std::size_t frames_count = sample_count / (wav.info.channels ? wav.info.channels : 1);
            const std::size_t max_frames = std::min(frames, frames_count);
            const auto* src = g_wav_read_buf.data();
            for (std::size_t i = 0; i < max_frames; ++i) {
                const std::size_t base = i * wav.info.channels * 3;
                auto read_sample = [&](std::size_t idx) -> std::uint16_t {
                    const std::size_t off = base + idx * 3;
                    std::int32_t v = static_cast<std::int32_t>(src[off])
                        | (static_cast<std::int32_t>(src[off + 1]) << 8)
                        | (static_cast<std::int32_t>(src[off + 2]) << 16);
                    if (v & 0x00800000) v |= static_cast<std::int32_t>(0xFF000000);
                    v >>= 8;
                    if (v > 32767) v = 32767;
                    if (v < -32768) v = -32768;
                    return static_cast<std::uint16_t>(v);
                };
                const std::uint16_t l = read_sample(0);
                const std::uint16_t r = (wav.info.channels > 1) ? read_sample(1) : l;
                out_words[i * 2] = l;
                out_words[i * 2 + 1] = r;
            }
            if (max_frames < frames) {
                std::memset(out_words.data() + (max_frames * 2), 0,
                    (frames - max_frames) * 2 * sizeof(std::uint16_t));
            }
        } else if (wav.info.bits_per_sample == 32) {
            const auto* samples = reinterpret_cast<const std::int32_t*>(g_wav_read_buf.data());
            const std::size_t sample_count = want / sizeof(std::int32_t);
            const std::size_t frames_count = sample_count / (wav.info.channels ? wav.info.channels : 1);
            const std::size_t max_frames = std::min(frames, frames_count);
            for (std::size_t i = 0; i < max_frames; ++i) {
                const std::size_t base = i * wav.info.channels;
                auto conv = [&](std::size_t idx) -> std::uint16_t {
                    std::int32_t v = samples[base + idx] >> 16;
                    if (v > 32767) v = 32767;
                    if (v < -32768) v = -32768;
                    return static_cast<std::uint16_t>(v);
                };
                const std::uint16_t l = conv(0);
                const std::uint16_t r = (wav.info.channels > 1) ? conv(1) : l;
                out_words[i * 2] = l;
                out_words[i * 2 + 1] = r;
            }
            if (max_frames < frames) {
                std::memset(out_words.data() + (max_frames * 2), 0,
                    (frames - max_frames) * 2 * sizeof(std::uint16_t));
            }
        } else {
            std::memset(out_words.data(), 0, out_words.size_bytes());
            wav.remaining = 0;
            return false;
        }
        return true;
    }

    bool flac_fill_block(FlacState& flac,
                         std::span<std::uint16_t> out_words) noexcept {
        if (flac.ended || !flac.opened) {
            std::memset(out_words.data(), 0, out_words.size_bytes());
            return false;
        }
        auto dst = std::span<std::byte>(
            reinterpret_cast<std::byte*>(g_pcm32_buffer.data()),
            g_pcm32_buffer.size() * sizeof(std::int32_t));
        auto res = g_player.flac_filter.process({}, dst);
        if (!res) {
            flac.ended = true;
            std::memset(out_words.data(), 0, out_words.size_bytes());
            return false;
        }
        if (res->produced == 0 && res->end_of_stream) {
            flac.ended = true;
            return false;
        }
        const std::size_t frame_bytes = static_cast<std::size_t>(flac.channels) * sizeof(std::int32_t);
        const std::size_t frames = frame_bytes ? (res->produced / frame_bytes) : 0;
        const std::size_t samples = frames * flac.channels;
        if (frames == 0) {
            return true;
        }
        if (flac.channels == 2) {
            const std::size_t out_samples = std::min(out_words.size(), samples);
            for (std::size_t i = 0; i < out_samples; ++i) {
                std::int32_t v = g_pcm32_buffer[i] >> 16;
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                out_words[i] = static_cast<std::uint16_t>(v);
            }
            if (out_samples < out_words.size()) {
                std::memset(out_words.data() + out_samples, 0,
                    (out_words.size() - out_samples) * sizeof(std::uint16_t));
            }
        } else {
            const std::size_t max_frames = std::min<std::size_t>(frames, out_words.size() / 2);
            for (std::size_t i = 0; i < max_frames; ++i) {
                std::int32_t v = g_pcm32_buffer[i] >> 16;
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                out_words[i * 2] = static_cast<std::uint16_t>(v);
                out_words[i * 2 + 1] = static_cast<std::uint16_t>(v);
            }
            if (max_frames * 2 < out_words.size()) {
                std::memset(out_words.data() + (max_frames * 2), 0,
                    (out_words.size() - (max_frames * 2)) * sizeof(std::uint16_t));
            }
        }
        return true;
    }
} // namespace

  static volatile bool g_half_ready = false;
  static volatile bool g_full_ready = false;
  static volatile std::uint32_t g_underruns = 0;
  static volatile std::uint32_t g_i2s_half_count = 0;
  static volatile std::uint32_t g_i2s_full_count = 0;
  static volatile std::uint32_t g_dma_irq_count = 0;
  static volatile std::uint32_t g_dma_irq_last_ms = 0;
  static util::u32 g_poll_start_ms = 0;
  static bool g_use_dma_poll = false;
  static util::u32 g_poll_last_fill_ms = 0;
  static int g_poll_last_phase = -1;
  static util::u32 g_ndtr_last_ms = 0;
  static int g_ndtr_last = -1;
  static util::u32 g_dma_restart_ms = 0;
    alignas(32) static std::array<std::int16_t, kI2sBufFrames * 2 * 2> g_pcm_buffer{};
    static std::array<std::uint16_t, kI2sBufFrames * 2 * kI2sWordsPerFrame> g_i2s_buffer CHARM_DMA_BUFFER{};

      extern "C" void charm_audio_i2s_half_notify() {
          g_half_ready = true;
          g_i2s_half_count = g_i2s_half_count + 1;
      }

      extern "C" void charm_audio_i2s_full_notify() {
          g_full_ready = true;
          g_i2s_full_count = g_i2s_full_count + 1;
      }

      extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
          if (hi2s == &hi2s1) {
              charm_audio_i2s_half_notify();
          }
      }

      extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s) {
          if (hi2s == &hi2s1) {
              charm_audio_i2s_full_notify();
          }
      }

      extern "C" void charm_audio_dma_irq_notify() noexcept {
          g_dma_irq_count = g_dma_irq_count + 1;
          g_dma_irq_last_ms = HAL_GetTick();
      }

  export void audio_i2s_selftest(util::u32 duration_ms) noexcept {
      constexpr util::u32 kToneHz = 1000;
      constexpr util::u32 kSampleRate = 44100;
      constexpr std::int16_t kAmp = 12000;

      const util::u32 half_period = kSampleRate / (kToneHz * 2);
      const std::size_t total_frames = kI2sBufFrames * 2;
      for (std::size_t i = 0; i < total_frames; ++i) {
          const bool high = ((i / half_period) % 2) == 0;
          const std::int16_t v = high ? kAmp : static_cast<std::int16_t>(-kAmp);
          const std::size_t o = i * kI2sWordsPerFrame;
          g_i2s_buffer[o + 0] = static_cast<std::uint16_t>(v);
          g_i2s_buffer[o + 1] = static_cast<std::uint16_t>(v);
      }
      clean_dcache(g_i2s_buffer.data(), g_i2s_buffer.size() * sizeof(g_i2s_buffer[0]));

      HAL_I2S_DMAStop(&hi2s1);
      g_half_ready = false;
      g_full_ready = false;
      g_i2s_half_count = 0;
      g_i2s_full_count = 0;

      if (HAL_I2S_Transmit_DMA(&hi2s1,
              g_i2s_buffer.data(),
              static_cast<uint16_t>(g_i2s_buffer.size())) != HAL_OK) {
          log<"i2s test: dma start failed state={} err={}">(
              static_cast<int>(hi2s1.State),
              static_cast<int>(HAL_I2S_GetError(&hi2s1)));
          return;
      }

      const util::u32 start = HAL_GetTick();
      while ((HAL_GetTick() - start) < duration_ms) {
          HAL_Delay(10);
      }
      HAL_I2S_DMAStop(&hi2s1);
      log<"i2s test: ms={} half={} full={} state={} err={}">(
          duration_ms,
          g_i2s_half_count,
          g_i2s_full_count,
          static_cast<int>(hi2s1.State),
          static_cast<int>(HAL_I2S_GetError(&hi2s1)));
  }

  export void audio_mp3_demo_run() noexcept {
    HAL_I2S_DMAStop(&hi2s1);
    allow_unaligned_access();
    std::string_view open_path{};
    if constexpr (kUseFixedPath) {
        open_path = kFixedPath;
        if constexpr (kStartupLog) {
            log<"mp3 demo: fixed path {}">(open_path);
  }

    } else {
        FindAudioCtx ctx{};
        ctx.prefix = "/MUSIC/";
        auto st = fs::vfs_list("/MUSIC", &ctx, &find_first_mp3);
        if (!st || !ctx.found) {
            if constexpr (kStartupLog) {
                log<"mp3 demo: /MUSIC scan failed {}">(static_cast<int>(st.err));
            }
            FindAudioCtx root_ctx{};
            root_ctx.prefix = "/";
            st = fs::vfs_list("/", &root_ctx, &find_first_mp3);
            if (!st || !root_ctx.found) {
                log<"mp3 demo: no mp3 found">();
                return;
            }
            ctx = root_ctx;
        }
        open_path = ctx.path;
        if constexpr (kStartupLog) {
            log<"mp3 demo: open {}">(open_path);
        }
    }
    (void)audio_mp3_play_path(open_path);
  }

  namespace {
      bool play_mp3_path(std::string_view open_path) noexcept {
        allow_unaligned_access();
        if (open_path.empty()) {
            log<"mp3 demo: empty path">();
            return false;
        }
        if (!is_mp3_name(open_path)) {
            log<"mp3 demo: not mp3 {}">(open_path);
            return false;
        }
        fs::File f{};
        auto st = fs::vfs_open(open_path, f);
        if (!st) {
            log<"mp3 demo: open failed {}">(static_cast<int>(st.err));
            return false;
        }
        if constexpr (kStartupLog) {
            log<"mp3 demo: file size={}">(
                f.node.size);
        }
        {
            std::array<util::u8, 16> head{};
            (void)fs::vfs_seek(f, 0);
            auto st_read = fs::vfs_read(f, head);
            (void)st_read;
            if constexpr (kStartupLog) {
                log<"mp3 demo: head {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                    head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7],
                    head[8], head[9], head[10], head[11], head[12], head[13], head[14], head[15]);
            }
            (void)fs::vfs_seek(f, 0);
        }
        (void)fs::vfs_seek(f, 0);

        Mp3Session session{};
        session.source = FileSource{&f};
        {
            std::array<util::u8, 10> id3{};
            (void)fs::vfs_seek(f, 0);
            (void)fs::vfs_read(f, id3);
            util::i64 base = 0;
            if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
                const std::uint32_t sz =
                    (static_cast<std::uint32_t>(id3[6] & 0x7f) << 21) |
                    (static_cast<std::uint32_t>(id3[7] & 0x7f) << 14) |
                    (static_cast<std::uint32_t>(id3[8] & 0x7f) << 7) |
                    (static_cast<std::uint32_t>(id3[9] & 0x7f));
                base = static_cast<util::i64>(10 + sz);
            }
            session.source.base_offset = base;
            if constexpr (kStartupLog) {
                log<"mp3 demo: base={}">(base);
            }
            if constexpr (kVerbose) {
                std::array<util::u8, 512> peek{};
                (void)fs::vfs_seek(f, base);
                const auto before = f.node.offset;
                (void)fs::vfs_read(f, peek);
                const auto after = f.node.offset;
                const auto read_bytes = static_cast<util::usize>(after > before ? (after - before) : 0);
                std::size_t nonzero = 0;
                std::size_t zeros = 0;
                std::size_t ff = 0;
                for (auto b : peek) {
                    if (b == 0) ++zeros;
                    if (b == 0xFF) ++ff;
                    if (b != 0) ++nonzero;
                }
                log<"mp3 demo: data nonzero {} zeros {} ff {} read {}">(
                    nonzero, zeros, ff, read_bytes);
                log<"mp3 demo: data16 {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                    peek[0], peek[1], peek[2], peek[3], peek[4], peek[5], peek[6], peek[7],
                    peek[8], peek[9], peek[10], peek[11], peek[12], peek[13], peek[14], peek[15]);
            }
            (void)fs::vfs_seek(f, base);
        }
    g_read_calls = 0;
    g_read_zero = 0;
    g_read_err = 0;
    g_read_enter = 0;
    g_read_slow = 0;
    g_seek_calls = 0;
    g_tell_calls = 0;
    g_size_calls = 0;
    g_last_read_log_ms = 0;
    if constexpr (kStartupLog) {
        log<"mp3 demo: filter open begin off={} size={} base={}">(
            f.node.offset, f.node.size, session.source.base_offset);
    }
    g_opening = true;
    const util::u32 open_start = HAL_GetTick();
    auto ref = media::make_stream_source_ref(session.source);
    auto rst = session.filter.open(ref);
    const util::u32 open_ms = HAL_GetTick() - open_start;
    g_opening = false;
    if constexpr (kStartupLog) {
        log<"mp3 demo: filter open end ok={} ms={}">(
            static_cast<int>(static_cast<bool>(rst)), open_ms);
    }
    if (!rst) {
        log<"mp3 demo: decoder open failed">();
        (void)fs::vfs_close(f);
        return false;
    }
    const auto fmt = session.filter.format();
    if (fmt.channels != 1 && fmt.channels != 2) {
        log<"mp3 demo: channels {} not supported">(fmt.channels);
        session.filter.close();
        (void)fs::vfs_close(f);
        return false;
    }
    if constexpr (kStartupLog) {
        log<"mp3 demo: rate={} ch={}">(fmt.rate, fmt.channels);
        log<"mp3 demo: size={}">(f.node.size);
    }

    const std::size_t period_words = kI2sBufFrames * kI2sWordsPerFrame;
    if (period_words == 0) {
        log<"mp3 demo: invalid period">();
        session.filter.close();
        (void)fs::vfs_close(f);
        return false;
    }

    auto pcm_first = std::span<std::int16_t>(g_pcm_buffer.data(), kI2sBufFrames * 2);
    auto pcm_second = std::span<std::int16_t>(g_pcm_buffer.data() + kI2sBufFrames * 2, kI2sBufFrames * 2);
    auto out_first = std::span<std::uint16_t>(g_i2s_buffer.data(), period_words);
    auto out_second = std::span<std::uint16_t>(g_i2s_buffer.data() + period_words, period_words);
    mp3_fill_block(session, pcm_first, out_first, fmt.channels);
    mp3_fill_block(session, pcm_second, out_second, fmt.channels);
    {
        auto* pcm = g_pcm_buffer.data();
        const std::size_t samples = kI2sBufFrames * 2;
        std::int16_t min_v = 32767;
        std::int16_t max_v = -32768;
        for (std::size_t i = 0; i < samples; ++i) {
            const auto v = pcm[i];
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
        if constexpr (kStartupLog) {
            log<"mp3 demo: pcm min={} max={} s0={} s1={} s2={} s3={}">(min_v, max_v, pcm[0], pcm[1], pcm[2], pcm[3]);
        }
    }

    const auto dma_state = HAL_DMA_GetState(&hdma_spi1_tx);
    if (dma_state != HAL_DMA_STATE_READY) {
        log<"mp3 demo: i2s dma not ready state={} err={}">(
            static_cast<int>(dma_state),
            static_cast<int>(hdma_spi1_tx.ErrorCode));
        (void)HAL_DMA_Abort(&hdma_spi1_tx);
        (void)HAL_DMA_DeInit(&hdma_spi1_tx);
        if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK) {
            log<"mp3 demo: i2s dma reinit failed err={}">(
                static_cast<int>(hdma_spi1_tx.ErrorCode));
        }
    }

    if (HAL_I2S_Transmit_DMA(&hi2s1,
            g_i2s_buffer.data(),
            static_cast<uint16_t>(g_i2s_buffer.size())) != HAL_OK) {
        log<"mp3 demo: dma start failed">();
        log<"mp3 demo: i2s state={} err={}">(static_cast<int>(hi2s1.State),
            static_cast<int>(HAL_I2S_GetError(&hi2s1)));
        session.filter.close();
        (void)fs::vfs_close(f);
        return false;
    }
    if constexpr (kStartupLog) {
        log<"mp3 demo: i2s state={} err={}">(static_cast<int>(hi2s1.State),
            static_cast<int>(HAL_I2S_GetError(&hi2s1)));
    }

    std::size_t idle_ticks = 0;
    std::size_t tick = 0;
    util::u32 last_stat_ms = HAL_GetTick();
    bool half_filled = true;
    bool full_filled = true;
    while (true) {
        if (g_half_ready) {
            g_half_ready = false;
            if (!mp3_fill_block(session, pcm_first, out_first, fmt.channels)) {
                g_underruns = g_underruns + 1;
                half_filled = false;
            } else {
                half_filled = true;
            }
        }
        if (g_full_ready) {
            g_full_ready = false;
            if (!mp3_fill_block(session, pcm_second, out_second, fmt.channels)) {
                g_underruns = g_underruns + 1;
                full_filled = false;
            } else {
                full_filled = true;
            }
        }
        if (session.ended) {
            if (idle_ticks == 0) {
                log<"mp3 demo: eos read_calls={} zero={} err={}">(
                    g_read_calls, g_read_zero, g_read_err);
            }
            if (++idle_ticks > 50) break;
        }
        if constexpr (kRuntimeLog || kStatLog) {
            if ((++tick % 50000) == 0) {
                const auto* dma_inst = static_cast<DMA_Stream_TypeDef*>(hdma_spi1_tx.Instance);
                const auto ndtr = dma_inst ? static_cast<int>(dma_inst->NDTR) : -1;
                const auto dstate = static_cast<int>(hdma_spi1_tx.State);
                const auto derr = static_cast<int>(hdma_spi1_tx.ErrorCode);
                log<"mp3 demo: playing... ndtr={} dma_state={} dma_err={} i2s_state={} underrun={} half_ok={} full_ok={}">(
                    ndtr, dstate, derr, static_cast<int>(hi2s1.State), g_underruns,
                    static_cast<int>(half_filled), static_cast<int>(full_filled));
            }
        }
        if constexpr (kStatLog) {
            const auto now = HAL_GetTick();
            if ((now - last_stat_ms) >= 1000) {
                last_stat_ms = now;
                auto pos = session.source.tell();
                auto size = session.source.size();
                const auto pos_v = pos ? pos.value() : -1;
                const auto size_v = size ? size.value() : -1;
                log<"mp3 demo: stat ms={} pos={} size={} ended={} i2s_state={} i2s_err={}">(
                    now, pos_v, size_v, static_cast<int>(session.ended),
                    static_cast<int>(hi2s1.State),
                    static_cast<int>(HAL_I2S_GetError(&hi2s1)));
            }
        }
    }
        log<"mp3 demo: end">();
        HAL_I2S_DMAStop(&hi2s1);
        session.filter.close();
        (void)fs::vfs_close(f);
        return true;
      }
  }

  export bool audio_mp3_play_path(std::string_view open_path) noexcept {
      return play_mp3_path(open_path);
  }

  namespace {
      bool start_playback(std::string_view open_path) noexcept {
          if (open_path.empty()) {
              log<"mp3 demo: empty path">();
              return false;
          }
          const bool is_mp3 = is_mp3_name(open_path);
          const bool is_wav = is_wav_name(open_path);
          const bool is_flac = is_flac_name(open_path);
          if (!is_mp3 && !is_wav && !is_flac) {
              log<"audio: unsupported {}">(open_path);
              return false;
          }
          audio_mp3_stop();
          g_player.active = false;
          g_player.started = false;
          g_player.file_open = false;
          g_player.period_words = 0;
          g_player.kind = AudioKind::none;
          g_player.session.filter.close();
          g_player.session.filter.~Mp3Filter();
          new (&g_player.session.filter) audio::Mp3Filter{};
          g_player.flac_filter.close();
          g_player.flac.opened = false;
          g_player.flac.ended = false;
          g_player.flac.channels = 0;
          g_player.flac.sample_rate = 0;
          g_allow_large_read = false;
          g_player.session.ended = false;
          g_player.session.source.file = nullptr;
          g_player.session.source.base_offset = 0;
          g_player.file = fs::File{};
          auto st = fs::vfs_open(open_path, g_player.file);
          if (!st) {
              log<"mp3 demo: open failed {}">(static_cast<int>(st.err));
              return false;
          }
          g_player.file_open = true;
          if constexpr (kStartupLog) {
              log<"mp3 demo: file size={}">(g_player.file.node.size);
          }
          {
              std::array<util::u8, 16> head{};
              (void)fs::vfs_seek(g_player.file, 0);
              (void)fs::vfs_read(g_player.file, head);
              if constexpr (kStartupLog) {
                  log<"mp3 demo: head {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                      head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7],
                      head[8], head[9], head[10], head[11], head[12], head[13], head[14], head[15]);
              }
              (void)fs::vfs_seek(g_player.file, 0);
          }
          if (is_wav) {
              g_allow_large_read = false;
              WavInfo info{};
              (void)fs::vfs_seek(g_player.file, 0);
              if (!read_wav_header(g_player.file, info)) {
                  log<"wav: invalid header">();
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              log<"wav: rate={} ch={} bits={} data={}">(
                  info.sample_rate, info.channels, info.bits_per_sample, info.data_size);
              if ((info.channels != 1 && info.channels != 2) ||
                  (info.bits_per_sample != 16 && info.bits_per_sample != 24 && info.bits_per_sample != 32)) {
                  log<"wav: only 16/24/32-bit mono/stereo supported">();
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              if (info.sample_rate != 44100) {
                  log<"wav: unsupported sample rate {}">(info.sample_rate);
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              g_player.kind = AudioKind::wav;
              g_player.wav.info = info;
              g_player.wav.remaining = info.data_size;
              g_player.period_words = kI2sBufFrames * kI2sWordsPerFrame;
              (void)fs::vfs_seek(g_player.file, static_cast<std::int64_t>(info.data_offset));

              auto out_first = std::span<std::uint16_t>(g_i2s_buffer.data(), g_player.period_words);
              auto out_second = std::span<std::uint16_t>(g_i2s_buffer.data() + g_player.period_words, g_player.period_words);
              wav_fill_block(g_player.wav, out_first);
              wav_fill_block(g_player.wav, out_second);
              flush_i2s_block(out_first);
              flush_i2s_block(out_second);

              if (HAL_I2S_Transmit_DMA(&hi2s1,
                      g_i2s_buffer.data(),
                      static_cast<uint16_t>(g_i2s_buffer.size())) != HAL_OK) {
                  log<"wav: dma start failed">();
                  log<"wav: i2s state={} err={}">(static_cast<int>(hi2s1.State),
                      static_cast<int>(HAL_I2S_GetError(&hi2s1)));
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              g_half_ready = false;
              g_full_ready = false;
              g_ndtr_last = -1;
              g_ndtr_last_ms = HAL_GetTick();
              g_dma_restart_ms = 0;
              g_player.active = true;
              g_player.started = true;
              return true;
          }
          if (is_flac) {
              g_allow_large_read = true;
              g_read_calls = 0;
              g_read_zero = 0;
              g_read_err = 0;
              g_read_enter = 0;
              g_read_slow = 0;
              g_seek_calls = 0;
              g_tell_calls = 0;
              g_size_calls = 0;
              g_last_read_log_ms = 0;
              g_opening = true;
              const util::u32 flac_open_start = HAL_GetTick();
              g_player.flac_source.file = &g_player.file;
              g_player.flac_source.base_offset = 0;
              auto ref = media::make_stream_source_ref(g_player.flac_source);
              auto rst = g_player.flac_filter.open(ref);
              const util::u32 flac_open_ms = HAL_GetTick() - flac_open_start;
              g_opening = false;
              if (!rst) {
                  log<"flac: open failed ms={}">(
                      flac_open_ms);
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              const auto fmt = g_player.flac_filter.format();
              if (fmt.channels != 1 && fmt.channels != 2) {
                  log<"flac: channels {} not supported">(fmt.channels);
                  g_player.flac_filter.close();
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              if (fmt.rate != 44100) {
                  log<"flac: unsupported sample rate {}">(fmt.rate);
                  g_player.flac_filter.close();
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              log<"flac: rate={} ch={}">(fmt.rate, fmt.channels);
              g_player.kind = AudioKind::flac;
              g_player.flac.channels = static_cast<std::uint16_t>(fmt.channels);
              g_player.flac.sample_rate = fmt.rate;
              g_player.flac.opened = true;
              g_player.flac.ended = false;
              g_player.period_words = kI2sBufFrames * kI2sWordsPerFrame;

              auto out_first = std::span<std::uint16_t>(g_i2s_buffer.data(), g_player.period_words);
              auto out_second = std::span<std::uint16_t>(g_i2s_buffer.data() + g_player.period_words, g_player.period_words);
              flac_fill_block(g_player.flac, out_first);
              flac_fill_block(g_player.flac, out_second);
              flush_i2s_block(out_first);
              flush_i2s_block(out_second);

              if (HAL_I2S_Transmit_DMA(&hi2s1,
                      g_i2s_buffer.data(),
                      static_cast<uint16_t>(g_i2s_buffer.size())) != HAL_OK) {
                  log<"flac: dma start failed">();
                  log<"flac: i2s state={} err={}">(static_cast<int>(hi2s1.State),
                      static_cast<int>(HAL_I2S_GetError(&hi2s1)));
                  g_player.flac_filter.close();
                  if (g_player.file_open) {
                      (void)fs::vfs_close(g_player.file);
                  }
                  g_player.file_open = false;
                  return false;
              }
              g_half_ready = false;
              g_full_ready = false;
              g_ndtr_last = -1;
              g_ndtr_last_ms = HAL_GetTick();
              g_dma_restart_ms = 0;
              g_player.active = true;
              g_player.started = true;
              return true;
          }
          g_allow_large_read = false;
          g_player.kind = AudioKind::mp3;
          g_player.session.ended = false;
          g_player.session.source.file = &g_player.file;
          g_player.session.source.base_offset = 0;
          {
              std::array<util::u8, 10> id3{};
              (void)fs::vfs_seek(g_player.file, 0);
              (void)fs::vfs_read(g_player.file, id3);
              util::i64 base = 0;
              if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
                  const std::uint32_t sz =
                      (static_cast<std::uint32_t>(id3[6] & 0x7f) << 21) |
                      (static_cast<std::uint32_t>(id3[7] & 0x7f) << 14) |
                      (static_cast<std::uint32_t>(id3[8] & 0x7f) << 7) |
                      (static_cast<std::uint32_t>(id3[9] & 0x7f));
                  base = static_cast<util::i64>(10 + sz);
              }
              g_player.session.source.base_offset = base;
              if constexpr (kStartupLog) {
                  log<"mp3 demo: base={}">(base);
              }
              (void)fs::vfs_seek(g_player.file, base);
          }

          g_read_calls = 0;
          g_read_zero = 0;
          g_read_err = 0;
          g_read_enter = 0;
          g_read_slow = 0;
          g_seek_calls = 0;
          g_tell_calls = 0;
          g_size_calls = 0;
          g_last_read_log_ms = 0;
          if constexpr (kStartupLog) {
              log<"mp3 demo: filter open begin off={} size={} base={}">(
                  g_player.file.node.offset, g_player.file.node.size, g_player.session.source.base_offset);
          }
          g_allow_large_read = true;
          g_opening = true;
          const util::u32 open_start = HAL_GetTick();
          auto ref = media::make_stream_source_ref(g_player.session.source);
          auto rst = g_player.session.filter.open(ref);
          const util::u32 open_ms = HAL_GetTick() - open_start;
          g_opening = false;
          g_allow_large_read = false;
          if constexpr (kStartupLog) {
              log<"mp3 demo: filter open end ok={} ms={}">(
                  static_cast<int>(static_cast<bool>(rst)), open_ms);
          }
          if (!rst) {
              log<"mp3 demo: decoder open failed">();
              if (g_player.file_open) {
                  (void)fs::vfs_close(g_player.file);
              }
              g_player.file_open = false;
              return false;
          }
          const auto fmt = g_player.session.filter.format();
          if (fmt.channels != 1 && fmt.channels != 2) {
              log<"mp3 demo: channels {} not supported">(fmt.channels);
              g_player.session.filter.close();
              if (g_player.file_open) {
                  (void)fs::vfs_close(g_player.file);
              }
              g_player.file_open = false;
              return false;
          }
          if constexpr (kStartupLog) {
              log<"mp3 demo: rate={} ch={}">(fmt.rate, fmt.channels);
              log<"mp3 demo: size={}">(g_player.file.node.size);
          }
          g_player.period_words = kI2sBufFrames * kI2sWordsPerFrame;
          if (g_player.period_words == 0) {
              log<"mp3 demo: invalid period">();
              g_player.session.filter.close();
              if (g_player.file_open) {
                  (void)fs::vfs_close(g_player.file);
              }
              g_player.file_open = false;
              return false;
          }
          auto pcm_first = std::span<std::int16_t>(g_pcm_buffer.data(), kI2sBufFrames * 2);
          auto pcm_second = std::span<std::int16_t>(g_pcm_buffer.data() + kI2sBufFrames * 2, kI2sBufFrames * 2);
          auto out_first = std::span<std::uint16_t>(g_i2s_buffer.data(), g_player.period_words);
          auto out_second = std::span<std::uint16_t>(g_i2s_buffer.data() + g_player.period_words, g_player.period_words);
          log<"mp3 demo: fill0 begin">();
          mp3_fill_block(g_player.session, pcm_first, out_first, fmt.channels);
          log<"mp3 demo: fill0 end">();
          log<"mp3 demo: fill1 begin">();
          if (!mp3_fill_block(g_player.session, pcm_second, out_second, fmt.channels)) {
              std::memset(out_second.data(), 0, out_second.size_bytes());
          }
          log<"mp3 demo: fill1 end">();
          flush_i2s_block(out_first);
          flush_i2s_block(out_second);
          {
              auto* pcm = g_pcm_buffer.data();
              const std::size_t samples = kI2sBufFrames * 2;
              std::int16_t min_v = 32767;
              std::int16_t max_v = -32768;
              for (std::size_t i = 0; i < samples; ++i) {
                  const auto v = pcm[i];
                  if (v < min_v) min_v = v;
                  if (v > max_v) max_v = v;
              }
              if constexpr (kStartupLog) {
                  log<"mp3 demo: pcm min={} max={} s0={} s1={} s2={} s3={}">(
                      min_v, max_v, pcm[0], pcm[1], pcm[2], pcm[3]);
              }
          }
          if constexpr (kStartupLog) {
              log<"mp3 demo: i2s buf=0x{:08X}">(
                  static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_i2s_buffer.data())));
          }

          const auto dma_state = HAL_DMA_GetState(&hdma_spi1_tx);
          if (dma_state != HAL_DMA_STATE_READY) {
              log<"mp3 demo: i2s dma not ready state={} err={}">(
                  static_cast<int>(dma_state),
                  static_cast<int>(hdma_spi1_tx.ErrorCode));
              (void)HAL_DMA_Abort(&hdma_spi1_tx);
              (void)HAL_DMA_DeInit(&hdma_spi1_tx);
              if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK) {
                  log<"mp3 demo: i2s dma reinit failed err={}">(
                      static_cast<int>(hdma_spi1_tx.ErrorCode));
              }
          }
          if (HAL_I2S_Transmit_DMA(&hi2s1,
                  g_i2s_buffer.data(),
                  static_cast<uint16_t>(g_i2s_buffer.size())) != HAL_OK) {
              log<"mp3 demo: dma start failed">();
              log<"mp3 demo: i2s state={} err={}">(static_cast<int>(hi2s1.State),
                  static_cast<int>(HAL_I2S_GetError(&hi2s1)));
              g_player.session.filter.close();
              if (g_player.file_open) {
                  (void)fs::vfs_close(g_player.file);
              }
              g_player.file_open = false;
              return false;
          }
          __HAL_DMA_ENABLE_IT(&hdma_spi1_tx, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME);
          HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
          HAL_NVIC_SetPendingIRQ(DMA1_Stream0_IRQn);
          __HAL_I2S_ENABLE(&hi2s1);
          SET_BIT(hi2s1.Instance->CFG1, SPI_CFG1_TXDMAEN);
          if constexpr (kStartupLog) {
              const auto* dma_inst = static_cast<DMA_Stream_TypeDef*>(hdma_spi1_tx.Instance);
              const std::uint32_t dma_cr = dma_inst ? dma_inst->CR : 0u;
              const std::uint32_t dma_ndtr = dma_inst ? dma_inst->NDTR : 0u;
              const std::uint32_t spi_cfg1 = hi2s1.Instance->CFG1;
              const std::uint32_t spi_cfg2 = hi2s1.Instance->CFG2;
              const std::uint32_t spi_sr = hi2s1.Instance->SR;
              const std::uint32_t spi_cr1 = hi2s1.Instance->CR1;
              const std::uint32_t dmamux_ccr = DMAMUX1_Channel0->CCR;
              #if defined(RCC_PERIPHCLK_SPI123)
              const std::uint32_t i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
              #elif defined(RCC_PERIPHCLK_SPI1)
              const std::uint32_t i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI1);
              #else
              const std::uint32_t i2s_clk = 0u;
              #endif
              log<"mp3 demo: i2s regs cfg1=0x{:08X} cfg2=0x{:08X} cr1=0x{:08X} sr=0x{:08X}">(spi_cfg1, spi_cfg2, spi_cr1, spi_sr);
              log<"mp3 demo: dma regs cr=0x{:08X} ndtr={} req={} dmamux_ccr=0x{:08X}">(
                  static_cast<unsigned int>(dma_cr),
                  static_cast<unsigned int>(dma_ndtr),
                  static_cast<unsigned int>(hdma_spi1_tx.Init.Request),
                  static_cast<unsigned int>(dmamux_ccr));
              log<"mp3 demo: i2s clk={}Hz">(static_cast<unsigned int>(i2s_clk));
          }
          if constexpr (kStartupLog) {
              log<"mp3 demo: i2s state={} err={}">(static_cast<int>(hi2s1.State),
                  static_cast<int>(HAL_I2S_GetError(&hi2s1)));
          }
          g_half_ready = false;
          g_full_ready = false;
          g_underruns = 0;
          g_i2s_half_count = 0;
          g_i2s_full_count = 0;
          g_poll_start_ms = HAL_GetTick();
          g_use_dma_poll = false;
          g_poll_last_fill_ms = 0;
          g_poll_last_phase = -1;
          g_ndtr_last = -1;
          g_ndtr_last_ms = g_poll_start_ms;
          g_dma_restart_ms = 0;
          g_player.active = true;
          g_player.started = true;
          return true;
      }
  }

  export bool audio_mp3_start(std::string_view open_path) noexcept {
      return start_playback(open_path);
  }

  export bool audio_mp3_update() noexcept {
      if (!g_player.active) return false;
      poll_i2s_dma_flags();
      static util::u32 last_log_ms = 0;
      static util::u32 last_half = 0;
      static util::u32 last_full = 0;
      const util::u32 now_ms = HAL_GetTick();
      if (!g_use_dma_poll && (now_ms - g_poll_start_ms) > 200u
          && g_i2s_half_count == 0 && g_i2s_full_count == 0) {
          g_use_dma_poll = true;
      }
      const auto* dma_inst = static_cast<DMA_Stream_TypeDef*>(hdma_spi1_tx.Instance);
      const int ndtr_now = dma_inst ? static_cast<int>(dma_inst->NDTR) : -1;
      if (ndtr_now >= 0) {
          if (g_ndtr_last < 0 || ndtr_now != g_ndtr_last) {
              g_ndtr_last = ndtr_now;
              g_ndtr_last_ms = now_ms;
          } else if ((now_ms - g_ndtr_last_ms) > 40u && (now_ms - g_dma_restart_ms) > 250u) {
              g_dma_restart_ms = now_ms;
              log<"mp3 demo: dma stall ndtr={} restart">(
                  static_cast<int>(ndtr_now));
              HAL_I2S_DMAStop(&hi2s1);
              if (HAL_I2S_Transmit_DMA(&hi2s1,
                      g_i2s_buffer.data(),
                      static_cast<uint16_t>(g_i2s_buffer.size())) == HAL_OK) {
                  SET_BIT(hi2s1.Instance->CFG1, SPI_CFG1_TXDMAEN);
                  g_ndtr_last = ndtr_now;
                  g_ndtr_last_ms = now_ms;
              } else {
                  log<"mp3 demo: dma restart failed state={} err={}">(
                      static_cast<int>(hi2s1.State),
                      static_cast<int>(HAL_I2S_GetError(&hi2s1)));
              }
          }
      }
      auto out_first = std::span<std::uint16_t>(g_i2s_buffer.data(), g_player.period_words);
      auto out_second = std::span<std::uint16_t>(g_i2s_buffer.data() + g_player.period_words, g_player.period_words);
      if (g_use_dma_poll) {
          poll_i2s_ndtr(g_player.period_words);
          const auto ndtr = dma_inst ? static_cast<std::size_t>(dma_inst->NDTR) : 0u;
          const bool dma_in_first = (ndtr > g_player.period_words);
          const int phase = dma_in_first ? 0 : 1;
          const util::u32 now_fill = HAL_GetTick();
          if (g_poll_last_phase != phase || (now_fill - g_poll_last_fill_ms) > 5u) {
              g_poll_last_phase = phase;
              g_poll_last_fill_ms = now_fill;
              if (g_player.kind == AudioKind::wav) {
                  if (dma_in_first) {
                      if (!wav_fill_block(g_player.wav, out_second)) {
                          g_underruns = g_underruns + 1;
                      }
                      flush_i2s_block(out_second);
                  } else {
                      if (!wav_fill_block(g_player.wav, out_first)) {
                          g_underruns = g_underruns + 1;
                      }
                      flush_i2s_block(out_first);
                  }
              } else if (g_player.kind == AudioKind::flac) {
                  if (dma_in_first) {
                      if (!flac_fill_block(g_player.flac, out_second)) {
                          g_underruns = g_underruns + 1;
                      }
                      flush_i2s_block(out_second);
                  } else {
                      if (!flac_fill_block(g_player.flac, out_first)) {
                          g_underruns = g_underruns + 1;
                      }
                      flush_i2s_block(out_first);
                  }
              } else {
                  auto& session = g_player.session;
                  auto pcm_first = std::span<std::int16_t>(g_pcm_buffer.data(), kI2sBufFrames * 2);
                  auto pcm_second = std::span<std::int16_t>(g_pcm_buffer.data() + kI2sBufFrames * 2, kI2sBufFrames * 2);
                  if (dma_in_first) {
                      if (!mp3_fill_block(session, pcm_second, out_second, session.filter.format().channels)) {
                          g_underruns = g_underruns + 1;
                      }
                      flush_i2s_block(out_second);
                  } else {
                      if (!mp3_fill_block(session, pcm_first, out_first, session.filter.format().channels)) {
                          g_underruns = g_underruns + 1;
                      }
                      flush_i2s_block(out_first);
                  }
              }
          }
      }
      if (g_player.kind == AudioKind::wav) {
          if (g_half_ready) {
              g_half_ready = false;
              if (!wav_fill_block(g_player.wav, out_first)) {
                  g_underruns = g_underruns + 1;
              }
              flush_i2s_block(out_first);
          }
          if (g_full_ready) {
              g_full_ready = false;
              if (!wav_fill_block(g_player.wav, out_second)) {
                  g_underruns = g_underruns + 1;
              }
              flush_i2s_block(out_second);
          }
          if (g_player.wav.remaining == 0) {
              audio_mp3_stop();
              return false;
          }
          return true;
      }
      if (g_player.kind == AudioKind::flac) {
          g_allow_large_read = true;
          if (g_half_ready) {
              g_half_ready = false;
              if (!flac_fill_block(g_player.flac, out_first)) {
                  g_underruns = g_underruns + 1;
              }
              flush_i2s_block(out_first);
          }
          if (g_full_ready) {
              g_full_ready = false;
              if (!flac_fill_block(g_player.flac, out_second)) {
                  g_underruns = g_underruns + 1;
              }
              flush_i2s_block(out_second);
          }
          if (g_player.flac.ended) {
              audio_mp3_stop();
              return false;
          }
          return true;
      }
      g_allow_large_read = false;

      auto& session = g_player.session;
      auto pcm_first = std::span<std::int16_t>(g_pcm_buffer.data(), kI2sBufFrames * 2);
      auto pcm_second = std::span<std::int16_t>(g_pcm_buffer.data() + kI2sBufFrames * 2, kI2sBufFrames * 2);
      if (g_half_ready) {
          g_half_ready = false;
          if (!mp3_fill_block(session, pcm_first, out_first, session.filter.format().channels)) {
              g_underruns = g_underruns + 1;
          }
          flush_i2s_block(out_first);
      }
      if (g_full_ready) {
          g_full_ready = false;
          if (!mp3_fill_block(session, pcm_second, out_second, session.filter.format().channels)) {
              g_underruns = g_underruns + 1;
          }
          flush_i2s_block(out_second);
      }
      if (session.ended) {
          audio_mp3_stop();
          return false;
      }
      if constexpr (kUpdateLog) {
          if ((now_ms - last_log_ms) >= 1000u) {
              last_log_ms = now_ms;
              const auto* dma_inst = static_cast<DMA_Stream_TypeDef*>(hdma_spi1_tx.Instance);
              const auto ndtr = dma_inst ? static_cast<int>(dma_inst->NDTR) : -1;
              const auto cr = dma_inst ? static_cast<std::uint32_t>(dma_inst->CR) : 0u;
              const auto dma_lisr = static_cast<std::uint32_t>(DMA1->LISR);
              const auto dma_hisr = static_cast<std::uint32_t>(DMA1->HISR);
              const auto spi_sr = static_cast<std::uint32_t>(SPI1->SR);
              const auto i2s_cfgr = static_cast<std::uint32_t>(SPI1->I2SCFGR);
              const util::u32 half = g_i2s_half_count;
              const util::u32 full = g_i2s_full_count;
              const util::u32 dh = half - last_half;
              const util::u32 df = full - last_full;
              last_half = half;
              last_full = full;
              log<"mp3 demo: run ms={} half={} full={} dh={} df={} underrun={} active={} kind={} ended={} i2s_state={} i2s_err={} ndtr={} dma_state={} dma_err={}">(
                  now_ms, half, full, dh, df, g_underruns,
                  static_cast<int>(g_player.active),
                  static_cast<int>(g_player.kind),
                  static_cast<int>(session.ended),
                  static_cast<int>(hi2s1.State),
                  static_cast<int>(HAL_I2S_GetError(&hi2s1)),
                  ndtr,
                  static_cast<int>(hdma_spi1_tx.State),
                  static_cast<int>(hdma_spi1_tx.ErrorCode));
              log<"mp3 demo: dma irq cnt={} last_ms={} cr=0x{:08X}">(
                  g_dma_irq_count,
                  g_dma_irq_last_ms,
                  static_cast<unsigned int>(cr));
              log<"mp3 demo: dma lisr=0x{:08X} hisr=0x{:08X} spi_sr=0x{:08X} i2s_cfgr=0x{:08X}">(
                  static_cast<unsigned int>(dma_lisr),
                  static_cast<unsigned int>(dma_hisr),
                  static_cast<unsigned int>(spi_sr),
                  static_cast<unsigned int>(i2s_cfgr));
          }
      }
      return true;
  }

export void audio_mp3_stop() noexcept {
      if (!g_player.active && !g_player.file_open) return;
      HAL_I2S_DMAStop(&hi2s1);
      g_player.session.filter.close();
      g_player.flac_filter.close();
      if (g_player.file_open) {
          (void)fs::vfs_close(g_player.file);
      }
      g_player.file_open = false;
      g_player.kind = AudioKind::none;
      g_player.wav.remaining = 0;
      g_player.flac.opened = false;
      g_player.flac.ended = false;
      g_player.active = false;
      g_player.started = false;
}

export bool audio_mp3_is_active() noexcept {
    return g_player.active;
}

export void audio_mp3_set_log_suppressed(bool enabled) noexcept {
    g_log_suppressed = enabled;
}

  export bool audio_mp3_decode_selftest() noexcept {
      std::string_view open_path{};
      if constexpr (kUseFixedPath) {
          open_path = kFixedPath;
          if constexpr (kStartupLog) {
              log<"mp3 test: fixed path {}">(open_path);
          }
      } else {
          FindAudioCtx ctx{};
          ctx.prefix = "/MUSIC/";
          auto st = fs::vfs_list("/MUSIC", &ctx, &find_first_mp3);
          if (!st || !ctx.found) {
              if constexpr (kStartupLog) {
                  log<"mp3 test: /MUSIC scan failed {}">(static_cast<int>(st.err));
              }
              FindAudioCtx root_ctx{};
              root_ctx.prefix = "/";
              st = fs::vfs_list("/", &root_ctx, &find_first_mp3);
              if (!st || !root_ctx.found) {
                  log<"mp3 test: no mp3 found">();
                  return false;
              }
              ctx = root_ctx;
          }
          open_path = ctx.path;
          if constexpr (kStartupLog) {
              log<"mp3 test: open {}">(open_path);
          }
      }
      fs::File f{};
      auto st = fs::vfs_open(open_path, f);
      if (!st) {
          log<"mp3 test: open failed {}">(static_cast<int>(st.err));
          return false;
      }
      if constexpr (kStartupLog) {
          log<"mp3 test: file size={}">(f.node.size);
      }
      (void)fs::vfs_seek(f, 0);

      Mp3Session session{};
      session.source = FileSource{&f};
      {
          std::array<util::u8, 10> id3{};
          (void)fs::vfs_seek(f, 0);
          (void)fs::vfs_read(f, id3);
          util::i64 base = 0;
          if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
              const std::uint32_t sz =
                  (static_cast<std::uint32_t>(id3[6] & 0x7f) << 21) |
                  (static_cast<std::uint32_t>(id3[7] & 0x7f) << 14) |
                  (static_cast<std::uint32_t>(id3[8] & 0x7f) << 7) |
                  (static_cast<std::uint32_t>(id3[9] & 0x7f));
              base = static_cast<util::i64>(10 + sz);
          }
          session.source.base_offset = base;
          if constexpr (kStartupLog) {
              log<"mp3 test: base={}">(base);
          }
          (void)fs::vfs_seek(f, base);
      }

      auto ref = media::make_stream_source_ref(session.source);
      auto rst = session.filter.open(ref);
      if (!rst) {
          log<"mp3 test: decoder open failed">();
          (void)fs::vfs_close(f);
          return false;
      }
      const auto fmt = session.filter.format();
      if constexpr (kStartupLog) {
          log<"mp3 test: rate={} ch={}">(fmt.rate, fmt.channels);
      }
      const std::size_t frame_bytes = static_cast<std::size_t>(fmt.channels) * sizeof(std::int16_t);
      util::u64 frames = 0;
      util::u32 crc = 0;
      const util::u32 t0 = HAL_GetTick();
      while (true) {
          auto dst = std::span<std::byte>(
              reinterpret_cast<std::byte*>(g_pcm_buffer.data()),
              g_pcm_buffer.size() * sizeof(std::int16_t));
          auto res = session.filter.process({}, dst);
          if (!res) {
              log<"mp3 test: decode error">();
              break;
          }
          if (res->produced > 0) {
              crc = boot::crc32_update(
                  crc,
                  reinterpret_cast<const util::u8*>(dst.data()),
                  static_cast<util::usize>(res->produced));
              frames += static_cast<util::u64>(res->produced / frame_bytes);
          }
          if (res->produced == 0 && res->end_of_stream) {
              break;
          }
      }
      const util::u32 ms = HAL_GetTick() - t0;
      log<"mp3 test: frames={} crc=0x{:08X} ms={}">(
          frames, crc, ms);
      session.filter.close();
      (void)fs::vfs_close(f);
      return true;
  }

export void audio_set_console_sink(out::channel_sink& sink) noexcept {
    g_sink = &sink;
    audio::mp3_set_debug_sink(sink);
#if CHARM_USE_SDRAM_ARENA
    audio::mp3_set_arena(reinterpret_cast<void*>(kSdramBase), kMp3ArenaSize);
    audio::flac_set_arena(reinterpret_cast<void*>(kFlacArenaBase), kFlacArenaSize);
#else
    audio::mp3_set_arena(g_mp3_arena.data(), g_mp3_arena.size());
    audio::flac_set_arena(g_flac_arena.data(), g_flac_arena.size());
#endif
}

