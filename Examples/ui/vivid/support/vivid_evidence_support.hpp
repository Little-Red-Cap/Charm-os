#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace vivid::evidence {
    struct RenderEvidence {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::size_t exec_cmds{0};
        std::size_t failed_cmds{0};
        std::size_t dirty_count{0};
        std::uint32_t dirty_hash{0};
        std::uint32_t cmd_hash{0};
        std::uint32_t pixel_hash{0};
    };

    struct StateDeltaEvidence {
        const char* id{""};
        const char* key{""};
        const char* source{""};
        int old_value{0};
        int new_value{0};
        const char* reason{""};

        [[nodiscard]] bool changed() const noexcept {
            return old_value != new_value;
        }
    };

    class RunLog {
    public:
        constexpr RunLog(const char* tag, const char* run) noexcept
            : tag_(tag), run_(run) {}

        void begin() const noexcept {
            std::printf("[%s] run=%s phase=begin\n", tag_, run_);
        }

        void end(bool ok) const noexcept {
            std::printf("[%s] run=%s phase=end result=%s cases=%u\n",
                        tag_,
                        run_,
                        ok ? "ok" : "fail",
                        case_count_);
        }

        void case_begin(const char* name) noexcept {
            ++case_count_;
            std::printf("[%s] case=%s", tag_, name);
        }

        [[nodiscard]] unsigned case_count() const noexcept {
            return case_count_;
        }

    private:
        const char* tag_;
        const char* run_;
        unsigned case_count_{0};
    };

    [[nodiscard]] inline bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] inline std::uint32_t hash_mix(std::uint32_t hash, std::uint32_t value) noexcept {
        hash ^= value;
        hash *= 16777619u;
        return hash;
    }

    inline void print_state_delta(const StateDeltaEvidence& delta) noexcept {
        std::printf(" state_delta=%d id=%s key=%s old=%d new=%d changed=%d source=%s",
                    delta.changed() ? 1 : 0,
                    delta.id ? delta.id : "",
                    delta.key ? delta.key : "",
                    delta.old_value,
                    delta.new_value,
                    delta.changed() ? 1 : 0,
                    delta.source ? delta.source : "");
        if (delta.reason && delta.reason[0] != '\0') {
            std::printf(" reason=%s", delta.reason);
        }
    }

    inline void print_named_state_delta(const char* name,
                                        const StateDeltaEvidence& delta) noexcept {
        const char* prefix = name ? name : "delta";
        std::printf(" %s_state_delta=%d %s_id=%s %s_key=%s %s_old=%d %s_new=%d %s_changed=%d %s_source=%s",
                    prefix,
                    delta.changed() ? 1 : 0,
                    prefix,
                    delta.id ? delta.id : "",
                    prefix,
                    delta.key ? delta.key : "",
                    prefix,
                    delta.old_value,
                    prefix,
                    delta.new_value,
                    prefix,
                    delta.changed() ? 1 : 0,
                    prefix,
                    delta.source ? delta.source : "");
        if (delta.reason && delta.reason[0] != '\0') {
            std::printf(" %s_reason=%s", prefix, delta.reason);
        }
    }

    [[nodiscard]] inline std::uint32_t hash_bytes(const std::byte* data, std::size_t len) noexcept {
        std::uint32_t hash = 2166136261u;
        if (!data) return hash;
        for (std::size_t index = 0; index < len; ++index) {
            hash ^= static_cast<std::uint8_t>(data[index]);
            hash *= 16777619u;
        }
        return hash;
    }

    [[nodiscard]] inline std::uint32_t hash_dirty(const DefaultCanvas& canvas) noexcept {
        std::uint32_t hash = 2166136261u;
        const auto& dirty = canvas.dirty_list();
        hash = hash_mix(hash, static_cast<std::uint32_t>(dirty.full() ? 1u : 0u));
        hash = hash_mix(hash, static_cast<std::uint32_t>(dirty.size()));
        for (std::size_t index = 0; index < dirty.size(); ++index) {
            const Rect rect = dirty[index];
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.x));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.y));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.w));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.h));
        }
        return hash;
    }

    [[nodiscard]] inline std::uint32_t hash_cmd_stats(const ui::scene::CmdStats& cmd,
                                                      const ui::scene::ExecStats& exec) noexcept {
        std::uint32_t hash = 2166136261u;
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.cmd_count));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.cmd_bytes));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.text_used));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.blob_used));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_count));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_rect));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_text));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_other));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.failed_cmds));
        return hash;
    }

    [[nodiscard]] inline bool rect_contains(Rect outer, Rect inner) noexcept {
        return inner.x >= outer.x
            && inner.y >= outer.y
            && inner.x + inner.w <= outer.x + outer.w
            && inner.y + inner.h <= outer.y + outer.h;
    }

    [[nodiscard]] inline bool dirty_stays_inside(const DefaultCanvas& canvas, Rect bounds) noexcept {
        const auto& dirty = canvas.dirty_list();
        if (dirty.full()) return false;
        for (std::size_t index = 0; index < dirty.size(); ++index) {
            if (!rect_contains(bounds, dirty[index])) return false;
        }
        return true;
    }

    inline void prepare_style_sheet() noexcept {
        apply_baseline_theme_preset(make_style_from_tokens(Theme::instance().get_tokens()));
    }

    [[nodiscard]] inline RenderEvidence render_scene(::ui::scene::Scene& scene,
                                                     DefaultCanvas& canvas,
                                                     Rect dirty_hint) noexcept {
        canvas.begin_frame();
        canvas.mark_dirty(dirty_hint);
        scene.render();
        const auto cmd = scene.last_cmd_stats();
        const auto exec = scene.last_exec_stats();
        return RenderEvidence{
            .cmd_count = cmd.cmd_count,
            .cmd_bytes = cmd.cmd_bytes,
            .exec_cmds = exec.cmd_count,
            .failed_cmds = exec.failed_cmds,
            .dirty_count = canvas.dirty_list().size(),
            .dirty_hash = hash_dirty(canvas),
            .cmd_hash = hash_cmd_stats(cmd, exec),
            .pixel_hash = hash_bytes(canvas.data(),
                                     static_cast<std::size_t>(canvas.height()) * canvas.stride_bytes()),
        };
    }
}
