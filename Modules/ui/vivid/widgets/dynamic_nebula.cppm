module;
#include <cstddef>
#include <cstdint>
#include <span>
#include "vivid_features.generated.hpp"
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#include <cmath>
#endif
export module charm.widgets.dynamic_nebula;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;

using namespace ui::render;

// Dynamic nebula (ARM-2D dynamic_nebula inspired)
export
class DynamicNebula : public WidgetBase<DynamicNebula> {
public:
    static constexpr std::size_t kMaxParticles = 64;

    class ParticleWorkspace {
    public:
        ParticleWorkspace(std::span<float> angles, std::span<float> offsets) noexcept {
            std::size_t capacity = angles.size();
            if (offsets.size() < capacity) capacity = offsets.size();
            if (kMaxParticles < capacity) capacity = kMaxParticles;
            angles_ = angles.first(capacity);
            offsets_ = offsets.first(capacity);
        }

        ~ParticleWorkspace() noexcept;
        ParticleWorkspace(const ParticleWorkspace&) = delete;
        ParticleWorkspace& operator=(const ParticleWorkspace&) = delete;
        ParticleWorkspace(ParticleWorkspace&&) = delete;
        ParticleWorkspace& operator=(ParticleWorkspace&&) = delete;

        [[nodiscard]] std::size_t capacity() const noexcept {
            return angles_.size();
        }

    private:
        friend class DynamicNebula;

        std::span<float> angles_{};
        std::span<float> offsets_{};
        DynamicNebula* owner_{nullptr};
    };

    DynamicNebula() {
        set_size(160, 160);
    }

    ~DynamicNebula() noexcept {
        detach_particle_workspace();
    }

    DynamicNebula(const DynamicNebula&) = delete;
    DynamicNebula& operator=(const DynamicNebula&) = delete;
    DynamicNebula(DynamicNebula&&) = delete;
    DynamicNebula& operator=(DynamicNebula&&) = delete;

    [[nodiscard]] bool attach_particle_workspace(ParticleWorkspace& workspace) noexcept {
        if (workspace.owner_ != nullptr && workspace.owner_ != this) return false;
        if (particle_workspace_ == &workspace) return true;
        detach_particle_workspace();
        particle_workspace_ = &workspace;
        workspace.owner_ = this;
        reset_particles();
        return true;
    }

    void detach_particle_workspace() noexcept {
        if (!particle_workspace_) return;
        if (particle_workspace_->owner_ == this) particle_workspace_->owner_ = nullptr;
        particle_workspace_ = nullptr;
    }

    [[nodiscard]] bool has_particle_workspace() const noexcept {
        return particle_workspace_ != nullptr;
    }

    void set_radius(int r) noexcept {
        radius_ = (r > 0) ? r : 0;
    }

    void set_visible_ring_width(int w) noexcept {
        visible_ring_ = (w > 0) ? w : 1;
    }

    void set_fully_visible_width(int w) noexcept {
        fully_visible_ = (w >= 0) ? w : 0;
    }

    void set_fade_edge_width(int w) noexcept {
        fade_edge_ = (w >= 0) ? w : 0;
    }

    void set_particle_count(int count) noexcept {
        const int max_particles = static_cast<int>(kMaxParticles);
        particle_count_ = (count < 1) ? 1 : (count > max_particles ? max_particles : count);
        reset_particles();
    }

    void set_speed(float speed) noexcept { speed_ = (speed > 0.0f) ? speed : 0.0f; }
    void set_outward(bool on) noexcept { outward_ = on; }
    void set_color(const rgba& c) noexcept { color_ = c; }
    void set_particle_size(int px) noexcept { particle_size_ = (px > 0) ? px : 1; }

    void draw(CanvasBase& cvs) {
#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        (void)cvs;
        return;
#else
        const int active_count = active_particle_count();
        if (active_count <= 0) return;
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<DynamicNebula>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::DynamicNebula, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;

        const int vis = (visible_ring_ > radius_) ? radius_ : visible_ring_;
        const int fade_edge = (fade_edge_ > vis) ? vis : fade_edge_;
        const int solid = (fully_visible_ > vis) ? vis : fully_visible_;
        const float invisible = static_cast<float>(radius_ - vis);
        const rgba col = color_.a ? color_ : accent;

        for (int i = 0; i < active_count; ++i) {
            float& angle = particle_workspace_->angles_[static_cast<std::size_t>(i)];
            float& offset = particle_workspace_->offsets_[static_cast<std::size_t>(i)];
            if (outward_) {
                offset += speed_;
                if (offset >= static_cast<float>(vis)) {
                    offset = 0.0f;
                    angle = random_angle();
                }
            } else {
                offset -= speed_;
                if (offset <= 0.0f) {
                    offset = static_cast<float>(vis);
                    angle = random_angle();
                }
            }

            const float radius = offset + invisible;
            const float sx = std::cos(angle) * radius;
            const float sy = std::sin(angle) * radius;
            const int px = cx + static_cast<int>(std::lround(sx));
            const int py = cy + static_cast<int>(std::lround(sy));

            std::uint8_t alpha = 255;
            const float fade_in_end = static_cast<float>(vis - fade_edge - solid);
            if (offset < fade_in_end && fade_in_end > 0.0f) {
                alpha = static_cast<std::uint8_t>(255.0f * (offset / fade_in_end));
            } else if (fade_edge > 0) {
                const float fade_out_start = static_cast<float>(vis - fade_edge);
                if (offset > fade_out_start) {
                    const float t = (offset - fade_out_start) / static_cast<float>(fade_edge);
                    alpha = static_cast<std::uint8_t>(255.0f * (1.0f - t));
                }
            }

            rgba draw = col;
            draw.a = static_cast<std::uint8_t>((draw.a * alpha) / 255);
            draw_circle(cvs, px, py, particle_size_, draw, true);
        }
#endif
    }

private:
    [[nodiscard]] int active_particle_count() const noexcept {
        if (!particle_workspace_) return 0;
        const auto capacity = static_cast<int>(particle_workspace_->capacity());
        return (particle_count_ < capacity) ? particle_count_ : capacity;
    }

    void reset_particles() noexcept {
        const int active_count = active_particle_count();
        for (int i = 0; i < active_count; ++i) {
            particle_workspace_->angles_[static_cast<std::size_t>(i)] = random_angle();
            particle_workspace_->offsets_[static_cast<std::size_t>(i)] = random_offset();
        }
    }

    float random_angle() noexcept {
        return random_unit() * 6.2831853f;
    }

    float random_offset() noexcept {
        const float vis = static_cast<float>((visible_ring_ > 0) ? visible_ring_ : 1);
        return random_unit() * vis;
    }

    float random_unit() noexcept {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ & 0xFFFF) / 65535.0f;
    }

    ParticleWorkspace* particle_workspace_{nullptr};
    int particle_count_{24};
    int radius_{60};
    int visible_ring_{24};
    int fully_visible_{8};
    int fade_edge_{8};
    int particle_size_{2};
    float speed_{0.6f};
    bool outward_{true};
    rgba color_{0, 0, 0, 0};
    std::uint32_t rng_{0x12345678u};
};

inline DynamicNebula::ParticleWorkspace::~ParticleWorkspace() noexcept {
    if (owner_) owner_->detach_particle_workspace();
}

static_assert(sizeof(DynamicNebula)
              <= sizeof(ObjectBase) + sizeof(DynamicNebula::ParticleWorkspace*)
                   + sizeof(int) * 6 + sizeof(float) + sizeof(bool) + sizeof(rgba)
                   + sizeof(std::uint32_t) + alignof(DynamicNebula) * 3,
              "DynamicNebula must not regain inline particle storage");
static_assert(sizeof(DynamicNebula::ParticleWorkspace)
              <= sizeof(std::span<float>) * 2 + sizeof(DynamicNebula*)
                   + alignof(DynamicNebula::ParticleWorkspace),
              "DynamicNebula workspace must remain a non-owning bounded view");

