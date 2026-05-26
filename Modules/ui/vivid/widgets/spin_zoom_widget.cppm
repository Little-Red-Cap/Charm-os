module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "vivid_features.generated.hpp"
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#include <cmath>
#endif
export module charm.widgets.spin_zoom_widget;

import charm.core.object;
import charm.core.event;
import charm.core.input_interaction;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.image;
import charm.gfx.render_style;
import charm.gfx.pixel_ops;

using namespace ui::render;

// Spin/zoom image widget (ARM-2D spin_zoom_widget inspired)
export
class SpinZoomWidget : public WidgetBase<SpinZoomWidget> {
public:
    SpinZoomWidget() {
        set_size(180, 180);
        set_focusable(true);
        double_tap_.set_callback(DoubleTapRestoreStrategy::callback_delegate::bind<&SpinZoomWidget::on_double_tap>(*this));
        double_tap_.set_threshold(double_tap_ms_, double_tap_radius_);
        enable_interaction(&double_tap_, InteractionList<>::mask(Event::Type::Click));
    }

    void set_image(const ImageView& img) noexcept { image_ = img; }
    const ImageView& image() const noexcept { return image_; }

    void set_zoom(float z) noexcept {
        zoom_ = clamp_zoom(z);
    }

    void set_zoom_limits(float min_zoom, float max_zoom) noexcept {
        min_zoom_ = (min_zoom > 0.0f) ? min_zoom : 0.1f;
        max_zoom_ = (max_zoom > min_zoom_) ? max_zoom : min_zoom_;
        zoom_ = clamp_zoom(zoom_);
    }

    void set_rotation_deg(float deg) noexcept { rotation_deg_ = deg; }
    float rotation_deg() const noexcept { return rotation_deg_; }

    void set_spin_speed(float deg_per_frame) noexcept { spin_speed_ = deg_per_frame; }
    void set_auto_spin(bool on) noexcept { auto_spin_ = on; }

    void set_wheel_zoom_step(float step) noexcept { wheel_step_ = (step > 0.0f) ? step : 0.1f; }
    void set_drag_rotate_scale(float deg_per_px) noexcept { drag_rotate_ = deg_per_px; }

    void set_pinch_enabled(bool on) noexcept { pinch_enabled_ = on; }
    void set_inertia_enabled(bool on) noexcept { inertia_enabled_ = on; }
    void set_inertia_decay(float decay) noexcept {
        if (decay <= 0.0f) decay = 0.0f;
        if (decay > 0.98f) decay = 0.98f;
        inertia_decay_ = decay;
    }
    void set_double_tap_restore(bool on) noexcept {
        double_tap_.set_enabled(on);
        if (on) {
            enable_interaction(&double_tap_, InteractionList<>::mask(Event::Type::Click));
        } else {
            disable_interaction(&double_tap_);
        }
    }
    void set_double_tap_ms(int ms) noexcept {
        double_tap_ms_ = (ms > 0) ? ms : 0;
        double_tap_.set_threshold(double_tap_ms_, double_tap_radius_);
    }
    void set_double_tap_radius(int px) noexcept {
        if (px < 0) px = 0;
        double_tap_radius_ = px;
        double_tap_.set_threshold(double_tap_ms_, double_tap_radius_);
    }

    bool on_event(const Event& e) {
#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        (void)e;
        return false;
#else
        if (dispatch_interactions(e)) return true;
        const auto r = get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) return false;
            dragging_ = true;
            last_x_ = e.x;
            rotation_velocity_ = 0.0f;
            return true;
        }
        if (e.type == Event::Type::DragStart || e.type == Event::Type::DragMove) {
            if (!dragging_) return false;
            const int dx = (e.dx != 0) ? e.dx : (e.x - last_x_);
            last_x_ = e.x;
            const float delta = static_cast<float>(dx) * drag_rotate_;
            rotation_deg_ += delta;
            rotation_velocity_ = delta;
            return true;
        }
        if (e.type == Event::Type::DragEnd || e.type == Event::Type::MouseUp) {
            if (!dragging_) return false;
            dragging_ = false;
            return true;
        }
        if (e.type == Event::Type::MouseWheel) {
            if (!r.contains(e.x, e.y)) return false;
            set_zoom(zoom_ + static_cast<float>(e.wheel_y) * wheel_step_);
            zoom_velocity_ = 0.0f;
            return true;
        }
        if (!pinch_enabled_) return false;
        if (e.type == Event::Type::GesturePinch) {
            if (e.gesture_phase == Event::GesturePhase::Begin) {
                pinch_base_zoom_ = zoom_;
                pinch_active_ = true;
                zoom_velocity_ = 0.0f;
            } else if (e.gesture_phase == Event::GesturePhase::Update) {
                const float next = clamp_zoom(pinch_base_zoom_ * e.scale);
                zoom_velocity_ = next - zoom_;
                zoom_ = next;
            } else if (e.gesture_phase == Event::GesturePhase::End) {
                pinch_active_ = false;
            }
            return true;
        }
        return false;
#endif
    }

    void draw(CanvasBase& cvs) {
#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered),
                                                  has_state(State::Pressed), has_state(State::Focused),
                                                  style_variant());
        const Style& base = Theme::instance().get<SpinZoomWidget>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::SpinZoomWidget, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        draw_focus_ring(cvs, r, st, has_state(State::Focused));
        return;
#else
        if (!image_) return;
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<SpinZoomWidget>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::SpinZoomWidget, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (!pinch_active_ && inertia_enabled_ && !dragging_) {
            if (std::fabs(rotation_velocity_) > 0.001f) {
                rotation_deg_ += rotation_velocity_;
                rotation_velocity_ *= inertia_decay_;
            }
            if (std::fabs(zoom_velocity_) > 0.0005f) {
                zoom_ = clamp_zoom(zoom_ + zoom_velocity_);
                zoom_velocity_ *= inertia_decay_;
            }
        }

        if (auto_spin_) {
            rotation_deg_ += spin_speed_;
        }

        draw_image_rotated(cvs, r, image_, zoom_, rotation_deg_);
        draw_focus_ring(cvs, r, st, has_state(State::Focused));
#endif
    }

private:
    static rgba decode_pixel(const ImageView& img, int sx, int sy) noexcept {
        if (!img.data || sx < 0 || sy < 0 || sx >= img.w || sy >= img.h) {
            return {0, 0, 0, 0};
        }
        const int bpp = bytes_per_pixel(img.format);
        const std::byte* row = img.data + sy * img.stride_bytes;
        const std::byte* p = row + sx * bpp;
        if (img.format == PixelFormat::RGB565) {
            uint16_t px{};
            std::memcpy(&px, p, sizeof(px));
            const rgb rgbv = unpack_rgb565(px);
            return rgba{rgbv.r, rgbv.g, rgbv.b, 255};
        }
        if (img.format == PixelFormat::RGB888) {
            return rgba{
                static_cast<std::uint8_t>(p[0]),
                static_cast<std::uint8_t>(p[1]),
                static_cast<std::uint8_t>(p[2]),
                255
            };
        }
        rgba src{
            static_cast<std::uint8_t>(p[1]),
            static_cast<std::uint8_t>(p[2]),
            static_cast<std::uint8_t>(p[3]),
            static_cast<std::uint8_t>(p[0])
        };
        if (img.force_opaque) {
            src.a = 255;
        }
        return src;
    }

    static void blend_pixel(CanvasBase& cvs, int x, int y,
                            const rgba& src, bool premultiplied) noexcept {
        if (src.a == 255) {
            cvs.set_pixel(x, y, src);
            return;
        }
        if (src.a == 0) return;
        const rgba dst = cvs.get_pixel(x, y);
        const int ia = 255 - src.a;
        rgba out{};
        if (premultiplied) {
            out = rgba{
                static_cast<std::uint8_t>(src.r + (dst.r * ia) / 255),
                static_cast<std::uint8_t>(src.g + (dst.g * ia) / 255),
                static_cast<std::uint8_t>(src.b + (dst.b * ia) / 255),
                255
            };
        } else {
            out = rgba{
                static_cast<std::uint8_t>((src.r * src.a + dst.r * ia) / 255),
                static_cast<std::uint8_t>((src.g * src.a + dst.g * ia) / 255),
                static_cast<std::uint8_t>((src.b * src.a + dst.b * ia) / 255),
                255
            };
        }
        cvs.set_pixel(x, y, out);
    }

    static int bytes_per_pixel(PixelFormat fmt) noexcept {
        switch (fmt) {
        case PixelFormat::RGB565: return 2;
        case PixelFormat::RGB888: return 3;
        case PixelFormat::ARGB8888: return 4;
        default: return 4;
        }
    }

    static void draw_image_rotated(CanvasBase& cvs,
                                   const Rect& r,
                                   const ImageView& img,
                                   float zoom,
                                   float rotation_deg) noexcept {
        if (!img || r.w <= 0 || r.h <= 0) return;
        const float angle = rotation_deg * 3.1415926f / 180.0f;
        const float cosv = std::cos(angle);
        const float sinv = std::sin(angle);

        const float cx = static_cast<float>(r.x + r.w / 2);
        const float cy = static_cast<float>(r.y + r.h / 2);
        const float src_cx = static_cast<float>(img.w) * 0.5f;
        const float src_cy = static_cast<float>(img.h) * 0.5f;

        const float scale = zoom;
        if (scale <= 0.0f) return;

        for (int y = r.y; y < r.y + r.h; ++y) {
            for (int x = r.x; x < r.x + r.w; ++x) {
                if (!cvs.in_clip(x, y)) continue;
                const float dx = static_cast<float>(x) - cx;
                const float dy = static_cast<float>(y) - cy;
                const float rx = (dx * cosv + dy * sinv) / scale;
                const float ry = (-dx * sinv + dy * cosv) / scale;
                const int sx = static_cast<int>(std::lround(rx + src_cx));
                const int sy = static_cast<int>(std::lround(ry + src_cy));
                const rgba src = decode_pixel(img, sx, sy);
                if (src.a == 0) continue;
                blend_pixel(cvs, x, y, src, img.premultiplied_alpha);
            }
        }
    }

    float clamp_zoom(float value) const noexcept {
        if (value < min_zoom_) return min_zoom_;
        if (value > max_zoom_) return max_zoom_;
        return value;
    }

    void reset_transform() noexcept {
        zoom_ = 1.0f;
        rotation_deg_ = 0.0f;
        rotation_velocity_ = 0.0f;
        zoom_velocity_ = 0.0f;
        pinch_active_ = false;
    }

    ImageView image_{};
    float zoom_{1.0f};
    float min_zoom_{0.5f};
    float max_zoom_{4.0f};
    float rotation_deg_{0.0f};
    float spin_speed_{0.6f};
    bool auto_spin_{false};
    float wheel_step_{0.1f};
    float drag_rotate_{0.6f};
    bool pinch_enabled_{true};
    bool inertia_enabled_{true};
    float inertia_decay_{0.85f};
    bool dragging_{false};
    int last_x_{0};
    float pinch_base_zoom_{1.0f};
    bool pinch_active_{false};
    float rotation_velocity_{0.0f};
    float zoom_velocity_{0.0f};
    int double_tap_ms_{280};
    int double_tap_radius_{12};
    DoubleTapRestoreStrategy double_tap_{};

    void on_double_tap() noexcept {
        reset_transform();
    }
};




