module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

export module audio.pump;

import audio.fifo;
import audio.format;
import audio.pcm_buffer;
import media.stream.sink;

export namespace audio {
    using FillCallback = media::FillCallback;

    struct PumpStats {
        std::uint64_t callback_count{0};
        std::uint64_t underrun_count{0};
        std::size_t last_request_bytes{0};
        std::size_t water_min{0};
        std::size_t water_max{0};
        bool has_water{false};
    };

    class AudioPump {
    public:
        void bind(PcmFifo& fifo, const AudioFormat& fmt) noexcept {
            fifo_ = &fifo;
            frame_size_ = fmt.frame_size();
            reset_stats();
        }

        void set_frame_size(std::size_t frame_size) noexcept {
            frame_size_ = frame_size;
        }

        std::size_t fill(std::span<std::byte> dst) noexcept {
            return on_fill(dst);
        }

        FillCallback fill_callback() const noexcept { return &AudioPump::on_fill_static; }

        PumpStats snapshot() const noexcept {
            PumpStats out{};
            out.callback_count = callback_count_.load(std::memory_order_relaxed);
            out.underrun_count = underrun_count_.load(std::memory_order_relaxed);
            out.last_request_bytes = last_request_bytes_.load(std::memory_order_relaxed);
            out.water_min = water_min_.load(std::memory_order_relaxed);
            out.water_max = water_max_.load(std::memory_order_relaxed);
            out.has_water = has_water_.load(std::memory_order_relaxed) != 0;
            return out;
        }

        void reset_stats() noexcept {
            callback_count_.store(0, std::memory_order_relaxed);
            underrun_count_.store(0, std::memory_order_relaxed);
            last_request_bytes_.store(0, std::memory_order_relaxed);
            water_min_.store(0, std::memory_order_relaxed);
            water_max_.store(0, std::memory_order_relaxed);
            has_water_.store(0, std::memory_order_relaxed);
        }

    private:
        static std::size_t on_fill_static(std::span<std::byte> dst, void* user) noexcept {
            auto* self = static_cast<AudioPump*>(user);
            if (!self) return 0;
            return self->on_fill(dst);
        }

        void update_water(std::size_t water) noexcept {
            if (has_water_.exchange(1, std::memory_order_relaxed) == 0) {
                water_min_.store(water, std::memory_order_relaxed);
                water_max_.store(water, std::memory_order_relaxed);
                return;
            }

            auto cur_min = water_min_.load(std::memory_order_relaxed);
            while (water < cur_min &&
                   !water_min_.compare_exchange_weak(cur_min, water, std::memory_order_relaxed)) {
            }

            auto cur_max = water_max_.load(std::memory_order_relaxed);
            while (water > cur_max &&
                   !water_max_.compare_exchange_weak(cur_max, water, std::memory_order_relaxed)) {
            }
        }

        std::size_t on_fill(std::span<std::byte> dst) noexcept {
            callback_count_.fetch_add(1, std::memory_order_relaxed);
            last_request_bytes_.store(dst.size(), std::memory_order_relaxed);

            if (!fifo_ || frame_size_ == 0) {
                underrun_count_.fetch_add(1, std::memory_order_relaxed);
                return 0;
            }

            update_water(fifo_->size_bytes());
            const std::size_t filled = read_pcm_fifo(*fifo_, dst, frame_size_);
            if (filled < dst.size()) {
                underrun_count_.fetch_add(1, std::memory_order_relaxed);
            }
            return filled;
        }

        PcmFifo* fifo_{nullptr};
        std::size_t frame_size_{0};

        std::atomic<std::uint64_t> callback_count_{0};
        std::atomic<std::uint64_t> underrun_count_{0};
        std::atomic<std::size_t> last_request_bytes_{0};
        std::atomic<std::size_t> water_min_{0};
        std::atomic<std::size_t> water_max_{0};
        std::atomic<std::uint8_t> has_water_{0};
    };
}
