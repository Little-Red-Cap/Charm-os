module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

export module audio.dsp_graph;

export namespace audio {
    class DspGraph {
    public:
        void reset(std::uint16_t channels) noexcept {
            channels_ = channels;
            fade_total_frames_ = 0;
            fade_remaining_frames_ = 0;
            gain_ = 1.0f;
            init_nodes();
        }

        void set_gain(float gain) noexcept {
            gain_ = std::clamp(gain, 0.0f, 1.0f);
        }

        void reset_fade(std::uint64_t frames) noexcept {
            fade_total_frames_ = frames;
            fade_remaining_frames_ = frames;
        }

        void process(std::span<std::int32_t> samples, std::size_t frames) noexcept {
            if (channels_ == 0 || frames == 0) return;
            const std::size_t total_samples = frames * channels_;
            if (samples.size() < total_samples) {
                frames = samples.size() / channels_;
            }
            if (frames == 0) return;

            for (std::size_t i = 0; i < node_count_; ++i) {
                auto& node = nodes_[i];
                if (!node.enabled || !node.fn) continue;
                node.fn(*this, samples, frames);
            }
        }

    private:
        enum class NodeKind : std::uint8_t {
            fade,
            gain,
            clip
        };

        using NodeFn = void(*)(DspGraph&, std::span<std::int32_t>, std::size_t);

        struct Node {
            NodeKind kind{};
            bool enabled{false};
            NodeFn fn{nullptr};
        };

        static constexpr std::size_t kMaxNodes = 4;

        void init_nodes() noexcept {
            node_count_ = 0;
            add_node(NodeKind::fade, &DspGraph::node_fade);
            add_node(NodeKind::gain, &DspGraph::node_gain);
            add_node(NodeKind::clip, &DspGraph::node_clip);
        }

        void add_node(NodeKind kind, NodeFn fn) noexcept {
            if (node_count_ >= nodes_.size()) return;
            nodes_[node_count_] = Node{kind, true, fn};
            ++node_count_;
        }

        static void node_fade(DspGraph& self, std::span<std::int32_t> samples, std::size_t frames) noexcept {
            const std::uint64_t fade_total = self.fade_total_frames_;
            const std::uint64_t fade_remaining = self.fade_remaining_frames_;
            if (self.channels_ == 0 || frames == 0) return;
            if (fade_total == 0 || fade_remaining == 0) return;

            std::size_t sample_index = 0;
            for (std::size_t frame = 0; frame < frames; ++frame) {
                const std::uint64_t done = fade_total - fade_remaining + frame + 1;
                const std::uint64_t scale = std::min(done, fade_total);
                const std::int64_t scale_num = static_cast<std::int64_t>(scale);
                const std::int64_t scale_den = static_cast<std::int64_t>(fade_total == 0 ? 1 : fade_total);
                for (std::size_t ch = 0; ch < self.channels_; ++ch, ++sample_index) {
                    const std::int64_t v = samples[sample_index];
                    samples[sample_index] = static_cast<std::int32_t>((v * scale_num) / scale_den);
                }
            }

            self.fade_remaining_frames_ = (frames >= fade_remaining)
                ? 0
                : (fade_remaining - frames);
        }

        static void node_gain(DspGraph& self, std::span<std::int32_t> samples, std::size_t frames) noexcept {
            if (self.channels_ == 0 || frames == 0) return;
            if (self.gain_ == 1.0f) return;
            const std::size_t total_samples = frames * self.channels_;
            for (std::size_t i = 0; i < total_samples; ++i) {
                const float v = static_cast<float>(samples[i]) * self.gain_;
                samples[i] = static_cast<std::int32_t>(v);
            }
        }

        static void node_clip(DspGraph& self, std::span<std::int32_t> samples, std::size_t frames) noexcept {
            if (self.channels_ == 0 || frames == 0) return;
            const std::size_t total_samples = frames * self.channels_;
            const std::int64_t min_v = std::numeric_limits<std::int32_t>::min();
            const std::int64_t max_v = std::numeric_limits<std::int32_t>::max();
            for (std::size_t i = 0; i < total_samples; ++i) {
                const std::int64_t v = samples[i];
                samples[i] = static_cast<std::int32_t>(std::clamp(v, min_v, max_v));
            }
        }

        std::uint16_t channels_{0};
        float gain_{1.0f};
        std::uint64_t fade_total_frames_{0};
        std::uint64_t fade_remaining_frames_{0};
        std::array<Node, kMaxNodes> nodes_{};
        std::size_t node_count_{0};
    };
}
