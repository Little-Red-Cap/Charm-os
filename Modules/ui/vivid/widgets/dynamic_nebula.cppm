module;
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
export module charm.widgets.dynamic_nebula;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

// Dynamic nebula (ARM-2D dynamic_nebula inspired)
export
class DynamicNebula : public WidgetBase<DynamicNebula> {
public:
    DynamicNebula() {
        set_size(160, 160);
        reset_particles();
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
        particle_count_ = (count < 1) ? 1 : (count > kMaxParticles ? kMaxParticles : count);
        reset_particles();
    }

    void set_speed(float speed) noexcept { speed_ = (speed > 0.0f) ? speed : 0.0f; }
    void set_outward(bool on) noexcept { outward_ = on; }
    void set_color(const rgba& c) noexcept { color_ = c; }
    void set_particle_size(int px) noexcept { particle_size_ = (px > 0) ? px : 1; }

    void draw(CanvasBase& cvs) {
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

        for (int i = 0; i < particle_count_; ++i) {
            auto& p = particles_[i];
            if (outward_) {
                p.offset += speed_;
                if (p.offset >= static_cast<float>(vis)) {
                    p.offset = 0.0f;
                    p.angle = random_angle();
                }
            } else {
                p.offset -= speed_;
                if (p.offset <= 0.0f) {
                    p.offset = static_cast<float>(vis);
                    p.angle = random_angle();
                }
            }

            const float radius = p.offset + invisible;
            const float sx = std::cos(p.angle) * radius;
            const float sy = std::sin(p.angle) * radius;
            const int px = cx + static_cast<int>(std::lround(sx));
            const int py = cy + static_cast<int>(std::lround(sy));

            const float offset = p.offset;
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
    }

private:
    struct Particle {
        float angle{};
        float offset{};
    };

    void reset_particles() noexcept {
        for (int i = 0; i < particle_count_; ++i) {
            particles_[i].angle = random_angle();
            particles_[i].offset = random_offset();
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

    static constexpr int kMaxParticles = 64;
    std::array<Particle, kMaxParticles> particles_{};
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


