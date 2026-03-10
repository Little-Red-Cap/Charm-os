module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#ifndef CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#define CHARM_VIVID_ENABLE_FLOAT_WIDGETS 1
#endif
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#include <cmath>
#endif
export module charm.widgets.image;

import charm.core.object;
import charm.core.geometry;
import charm.core.event;
import charm.core.input_interaction;
import charm.gfx.image;
import charm.gfx.render_style;
import charm.gfx.pixel_ops;

using namespace ui::render;

export
class Image : public WidgetBase<Image> {
public:
    enum class AlignH {
        Left,
        Center,
        Right
    };

    enum class AlignV {
        Top,
        Center,
        Bottom
    };

    enum class ScaleMode {
        None,
        Stretch,
        Fit,
        Fill
    };

    enum class Rotation {
        None,
        Rotate90,
        Rotate180,
        Rotate270
    };

    enum class Sampling {
        Nearest,
        Bilinear
    };

    enum class CropMode {
        Clamp,
        Transparent
    };

    enum class EdgeMode {
        KeepInside,
        AllowOutside
    };

    Image() {
        double_tap_.set_callback(&Image::on_double_tap, this);
        double_tap_.set_threshold(double_tap_ms_, double_tap_radius_);
        enable_interaction(&double_tap_, InteractionList<>::mask(Event::Type::Click));
    }

    void set_image(const ImageView& img) noexcept {
        image_ = img;
        if (image_) {
            set_size(image_.w, image_.h);
        }
    }

    const ImageView& image() const noexcept { return image_; }

    void set_scale_mode(ScaleMode mode) noexcept { scale_mode_ = mode; }
    ScaleMode scale_mode() const noexcept { return scale_mode_; }

    void set_rotation(Rotation r) noexcept { rotation_ = r; }
    Rotation rotation() const noexcept { return rotation_; }

    void set_sampling(Sampling s) noexcept { sampling_ = s; }
    Sampling sampling() const noexcept { return sampling_; }

    void set_crop_mode(CropMode m) noexcept { crop_mode_ = m; }
    CropMode crop_mode() const noexcept { return crop_mode_; }

    void set_edge_mode(EdgeMode m) noexcept { edge_mode_ = m; }
    EdgeMode edge_mode() const noexcept { return edge_mode_; }

    void set_anchor(float x, float y) noexcept {
        anchor_x_ = (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x);
        anchor_y_ = (y < 0.0f) ? 0.0f : ((y > 1.0f) ? 1.0f : y);
    }

    void set_alignment(AlignH h, AlignV v) noexcept {
        align_h_ = h;
        align_v_ = v;
        anchor_x_ = (h == AlignH::Left) ? 0.0f : (h == AlignH::Right ? 1.0f : 0.5f);
        anchor_y_ = (v == AlignV::Top) ? 0.0f : (v == AlignV::Bottom ? 1.0f : 0.5f);
    }

    void set_crop(const Rect& r) noexcept {
        crop_ = r;
        has_crop_ = true;
    }

    void clear_crop() noexcept { has_crop_ = false; }

    void set_zoom(float zoom) noexcept {
        zoom_ = clamp_zoom(zoom);
    }

    void set_zoom_limits(float min_zoom, float max_zoom) noexcept {
        min_zoom_ = (min_zoom > 0.0f) ? min_zoom : 0.1f;
        max_zoom_ = (max_zoom > min_zoom_) ? max_zoom : min_zoom_;
        zoom_ = clamp_zoom(zoom_);
    }

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
        double_tap_.set_threshold((ms > 0) ? ms : 0, double_tap_radius_);
    }
    void set_double_tap_radius(int px) noexcept {
        if (px < 0) px = 0;
        double_tap_radius_ = px;
        double_tap_.set_threshold(double_tap_ms_, double_tap_radius_);
    }

    Rect paint_bounds() const noexcept {
        const auto r = get_rect();
        if (!image_) return Rect{};
        if (edge_mode_ == EdgeMode::KeepInside) return r;
#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        Rect src{0, 0, image_.w, image_.h};
        if (has_crop_) {
            src.x = (crop_.x < 0) ? 0 : crop_.x;
            src.y = (crop_.y < 0) ? 0 : crop_.y;
            src.w = crop_.w;
            src.h = crop_.h;
            if (src.x + src.w > image_.w) src.w = image_.w - src.x;
            if (src.y + src.h > image_.h) src.h = image_.h - src.y;
            if (src.w <= 0 || src.h <= 0) return Rect{};
        }
        const int dst_w = (scale_mode_ == ScaleMode::Stretch) ? r.w : src.w;
        const int dst_h = (scale_mode_ == ScaleMode::Stretch) ? r.h : src.h;
        if (dst_w <= 0 || dst_h <= 0) return Rect{};
        return Rect{r.x, r.y, dst_w, dst_h};
#else
        Rect src{0, 0, image_.w, image_.h};
        if (has_crop_) {
            src.x = (crop_.x < 0) ? 0 : crop_.x;
            src.y = (crop_.y < 0) ? 0 : crop_.y;
            src.w = crop_.w;
            src.h = crop_.h;
            if (src.x + src.w > image_.w) src.w = image_.w - src.x;
            if (src.y + src.h > image_.h) src.h = image_.h - src.y;
            if (src.w <= 0 || src.h <= 0) return Rect{};
        }
        const ImageView src_view = make_subview(image_, src);
        if (!src_view) return Rect{};
        const bool swap = (rotation_ == Rotation::Rotate90 || rotation_ == Rotation::Rotate270);
        const int src_w = swap ? src_view.h : src_view.w;
        const int src_h = swap ? src_view.w : src_view.h;
        int dst_w = src_w;
        int dst_h = src_h;
        if (scale_mode_ == ScaleMode::Stretch) {
            dst_w = r.w;
            dst_h = r.h;
        } else if (scale_mode_ == ScaleMode::Fit) {
            if (src_w <= 0 || src_h <= 0) return Rect{};
            const float sx = static_cast<float>(r.w) / static_cast<float>(src_w);
            const float sy = static_cast<float>(r.h) / static_cast<float>(src_h);
            const float s = (sx < sy) ? sx : sy;
            dst_w = static_cast<int>(src_w * s);
            dst_h = static_cast<int>(src_h * s);
        } else if (scale_mode_ == ScaleMode::Fill) {
            if (src_w <= 0 || src_h <= 0) return Rect{};
            const float sx = static_cast<float>(r.w) / static_cast<float>(src_w);
            const float sy = static_cast<float>(r.h) / static_cast<float>(src_h);
            const float s = (sx > sy) ? sx : sy;
            dst_w = static_cast<int>(src_w * s);
            dst_h = static_cast<int>(src_h * s);
        }
        if (zoom_ != 1.0f) {
            dst_w = static_cast<int>(static_cast<float>(dst_w) * zoom_);
            dst_h = static_cast<int>(static_cast<float>(dst_h) * zoom_);
        }
        if (dst_w <= 0 || dst_h <= 0) return Rect{};
        const int dst_x = r.x + static_cast<int>((r.w - dst_w) * anchor_x_);
        const int dst_y = r.y + static_cast<int>((r.h - dst_h) * anchor_y_);
        return Rect{dst_x, dst_y, dst_w, dst_h};
#endif
    }

    bool on_event(const Event& e) {
#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        (void)e;
        return false;
#else
        if (dispatch_interactions(e)) return true;
        if (!pinch_enabled_) return false;
        if (e.type != Event::Type::GesturePinch) return false;
        if (e.gesture_phase == Event::GesturePhase::Begin) {
            pinch_active_ = true;
            pinch_base_zoom_ = zoom_;
            zoom_velocity_ = 0.0f;
        } else if (e.gesture_phase == Event::GesturePhase::Update) {
            if (!pinch_active_) return false;
            const float next = clamp_zoom(pinch_base_zoom_ * e.scale);
            zoom_velocity_ = next - zoom_;
            zoom_ = next;
        } else if (e.gesture_phase == Event::GesturePhase::End) {
            pinch_active_ = false;
        }
        return true;
#endif
    }

    void draw(CanvasBase& cvs) {
        // Intentional: image widget bypasses theme/style rendering; container handles background/border.
        if (!image_) return;
#if !CHARM_VIVID_ENABLE_FLOAT_WIDGETS
        const auto r = get_rect();
        Rect src{0, 0, image_.w, image_.h};
        if (has_crop_) {
            src.x = (crop_.x < 0) ? 0 : crop_.x;
            src.y = (crop_.y < 0) ? 0 : crop_.y;
            src.w = crop_.w;
            src.h = crop_.h;
            if (src.x + src.w > image_.w) src.w = image_.w - src.x;
            if (src.y + src.h > image_.h) src.h = image_.h - src.y;
            if (src.w <= 0 || src.h <= 0) return;
        }
        const ImageView src_view = make_subview(image_, src);
        if (!src_view) return;
        auto clip_state = cvs.save_clip();
        if (edge_mode_ == EdgeMode::KeepInside) {
            cvs.set_clip(r);
        }
        if (scale_mode_ == ScaleMode::Stretch) {
            draw_image_scaled(cvs, r.x, r.y, r.w, r.h, src_view);
        } else {
            draw_image(cvs, r.x, r.y, src_view);
        }
        cvs.restore_clip(clip_state);
        return;
#else
        if (!pinch_active_ && inertia_enabled_ && std::fabs(zoom_velocity_) > 0.0001f) {
            zoom_ = clamp_zoom(zoom_ + zoom_velocity_);
            zoom_velocity_ *= inertia_decay_;
            if (std::fabs(zoom_velocity_) < 0.0001f) {
                zoom_velocity_ = 0.0f;
            }
        }
        const auto r = get_rect();
        Rect src{0, 0, image_.w, image_.h};
        if (has_crop_) {
            src.x = (crop_.x < 0) ? 0 : crop_.x;
            src.y = (crop_.y < 0) ? 0 : crop_.y;
            src.w = crop_.w;
            src.h = crop_.h;
            if (src.x + src.w > image_.w) src.w = image_.w - src.x;
            if (src.y + src.h > image_.h) src.h = image_.h - src.y;
            if (src.w <= 0 || src.h <= 0) return;
        }

        const ImageView src_view = make_subview(image_, src);
        if (!src_view) return;

        const bool swap = (rotation_ == Rotation::Rotate90 || rotation_ == Rotation::Rotate270);
        const int src_w = swap ? src_view.h : src_view.w;
        const int src_h = swap ? src_view.w : src_view.h;

        int dst_w = src_w;
        int dst_h = src_h;
        if (scale_mode_ == ScaleMode::Stretch) {
            dst_w = r.w;
            dst_h = r.h;
        } else if (scale_mode_ == ScaleMode::Fit) {
            if (src_w <= 0 || src_h <= 0) return;
            const float sx = static_cast<float>(r.w) / static_cast<float>(src_w);
            const float sy = static_cast<float>(r.h) / static_cast<float>(src_h);
            const float s = (sx < sy) ? sx : sy;
            dst_w = static_cast<int>(src_w * s);
            dst_h = static_cast<int>(src_h * s);
        } else if (scale_mode_ == ScaleMode::Fill) {
            if (src_w <= 0 || src_h <= 0) return;
            const float sx = static_cast<float>(r.w) / static_cast<float>(src_w);
            const float sy = static_cast<float>(r.h) / static_cast<float>(src_h);
            const float s = (sx > sy) ? sx : sy;
            dst_w = static_cast<int>(src_w * s);
            dst_h = static_cast<int>(src_h * s);
        }

        if (zoom_ != 1.0f) {
            dst_w = static_cast<int>(static_cast<float>(dst_w) * zoom_);
            dst_h = static_cast<int>(static_cast<float>(dst_h) * zoom_);
        }

        int dst_x = r.x + static_cast<int>((r.w - dst_w) * anchor_x_);
        int dst_y = r.y + static_cast<int>((r.h - dst_h) * anchor_y_);

        auto clip_state = cvs.save_clip();
        if (edge_mode_ == EdgeMode::KeepInside) {
            cvs.set_clip(r);
        }
        if (rotation_ == Rotation::None && sampling_ == Sampling::Nearest) {
            if (dst_w != src_view.w || dst_h != src_view.h) {
                draw_image_scaled(cvs, dst_x, dst_y, dst_w, dst_h, src_view);
            } else {
                draw_image(cvs, dst_x, dst_y, src_view);
            }
        } else {
            draw_image_transformed(cvs, dst_x, dst_y, dst_w, dst_h, src_view, rotation_, sampling_, crop_mode_);
        }
        cvs.restore_clip(clip_state);
#endif
    }

private:
    static rgba decode_pixel(const ImageView& img, int sx, int sy, CropMode mode) noexcept {
        if (!img.data || sx < 0 || sy < 0 || sx >= img.w || sy >= img.h) {
            if (mode == CropMode::Clamp) {
                const int cx = (sx < 0) ? 0 : (sx >= img.w ? (img.w - 1) : sx);
                const int cy = (sy < 0) ? 0 : (sy >= img.h ? (img.h - 1) : sy);
                sx = cx;
                sy = cy;
            } else {
                return {0, 0, 0, 0};
            }
        }
        if (sx < 0 || sy < 0 || sx >= img.w || sy >= img.h) {
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

    static rgba sample_bilinear(const ImageView& img, float fx, float fy, CropMode mode) noexcept {
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const int x1 = (x0 + 1 < img.w) ? (x0 + 1) : x0;
        const int y1 = (y0 + 1 < img.h) ? (y0 + 1) : y0;
        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);

        const rgba c00 = decode_pixel(img, x0, y0, mode);
        const rgba c10 = decode_pixel(img, x1, y0, mode);
        const rgba c01 = decode_pixel(img, x0, y1, mode);
        const rgba c11 = decode_pixel(img, x1, y1, mode);

        auto lerp = [](std::uint8_t a, std::uint8_t b, float t) -> float {
            return static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
        };

        const float r0 = lerp(c00.r, c10.r, tx);
        const float r1 = lerp(c01.r, c11.r, tx);
        const float g0 = lerp(c00.g, c10.g, tx);
        const float g1 = lerp(c01.g, c11.g, tx);
        const float b0 = lerp(c00.b, c10.b, tx);
        const float b1 = lerp(c01.b, c11.b, tx);
        const float a0 = lerp(c00.a, c10.a, tx);
        const float a1 = lerp(c01.a, c11.a, tx);

        rgba out{};
        out.r = static_cast<std::uint8_t>(lerp(static_cast<std::uint8_t>(r0), static_cast<std::uint8_t>(r1), ty));
        out.g = static_cast<std::uint8_t>(lerp(static_cast<std::uint8_t>(g0), static_cast<std::uint8_t>(g1), ty));
        out.b = static_cast<std::uint8_t>(lerp(static_cast<std::uint8_t>(b0), static_cast<std::uint8_t>(b1), ty));
        out.a = static_cast<std::uint8_t>(lerp(static_cast<std::uint8_t>(a0), static_cast<std::uint8_t>(a1), ty));
        return out;
    }

    static void draw_image_transformed(CanvasBase& cvs,
                                       int dst_x, int dst_y, int dst_w, int dst_h,
                                       const ImageView& img,
                                       Rotation rot,
                                       Sampling sampling,
                                       CropMode crop_mode) noexcept {
        if (!img || dst_w <= 0 || dst_h <= 0) return;
        const int sw = img.w;
        const int sh = img.h;
        if (sw <= 0 || sh <= 0) return;
        for (int y = 0; y < dst_h; ++y) {
            for (int x = 0; x < dst_w; ++x) {
                const int px = dst_x + x;
                const int py = dst_y + y;
                if (!cvs.in_clip(px, py)) continue;

                float fx = 0.0f;
                float fy = 0.0f;
                const float u = (dst_w > 1) ? static_cast<float>(x) / static_cast<float>(dst_w - 1) : 0.0f;
                const float v = (dst_h > 1) ? static_cast<float>(y) / static_cast<float>(dst_h - 1) : 0.0f;
                switch (rot) {
                case Rotation::Rotate90:
                    fx = (1.0f - v) * static_cast<float>(sw - 1);
                    fy = u * static_cast<float>(sh - 1);
                    break;
                case Rotation::Rotate180:
                    fx = (1.0f - u) * static_cast<float>(sw - 1);
                    fy = (1.0f - v) * static_cast<float>(sh - 1);
                    break;
                case Rotation::Rotate270:
                    fx = v * static_cast<float>(sw - 1);
                    fy = (1.0f - u) * static_cast<float>(sh - 1);
                    break;
                default:
                    fx = u * static_cast<float>(sw - 1);
                    fy = v * static_cast<float>(sh - 1);
                    break;
                }

                rgba src{};
                if (sampling == Sampling::Bilinear) {
                    src = sample_bilinear(img, fx, fy, crop_mode);
                } else {
                    const int sx = static_cast<int>(fx + 0.5f);
                    const int sy = static_cast<int>(fy + 0.5f);
                    src = decode_pixel(img, sx, sy, crop_mode);
                }
                blend_pixel(cvs, px, py, src, img.premultiplied_alpha);
            }
        }
    }
    static int bytes_per_pixel(PixelFormat fmt) noexcept {
        switch (fmt) {
        case PixelFormat::RGB565: return 2;
        case PixelFormat::RGB888: return 3;
        case PixelFormat::ARGB8888: return 4;
        default: return 4;
        }
    }

    static ImageView make_subview(const ImageView& img, const Rect& r) noexcept {
        if (!img) return {};
        const int x = (r.x < 0) ? 0 : r.x;
        const int y = (r.y < 0) ? 0 : r.y;
        const int w = r.w;
        const int h = r.h;
        if (w <= 0 || h <= 0) return {};
        const int bpp = bytes_per_pixel(img.format);
        const std::byte* data = img.data + y * img.stride_bytes + x * bpp;
        return make_image_view(img.format, w, h, img.stride_bytes, data, img.premultiplied_alpha, img.force_opaque);
    }

    void reset_zoom() noexcept {
        zoom_ = 1.0f;
        zoom_velocity_ = 0.0f;
        pinch_active_ = false;
    }

    float clamp_zoom(float value) const noexcept {
        if (value < min_zoom_) return min_zoom_;
        if (value > max_zoom_) return max_zoom_;
        return value;
    }

    ImageView image_{};
    ScaleMode scale_mode_{ScaleMode::Stretch};
    Rotation rotation_{Rotation::None};
    Sampling sampling_{Sampling::Nearest};
    CropMode crop_mode_{CropMode::Clamp};
    EdgeMode edge_mode_{EdgeMode::KeepInside};
    AlignH align_h_{AlignH::Center};
    AlignV align_v_{AlignV::Center};
    float anchor_x_{0.5f};
    float anchor_y_{0.5f};
    Rect crop_{};
    bool has_crop_{false};
    float zoom_{1.0f};
    float min_zoom_{0.5f};
    float max_zoom_{4.0f};
    float pinch_base_zoom_{1.0f};
    float zoom_velocity_{0.0f};
    float inertia_decay_{0.85f};
    bool pinch_enabled_{true};
    bool pinch_active_{false};
    bool inertia_enabled_{true};
    int double_tap_ms_{300};
    int double_tap_radius_{12};
    DoubleTapRestoreStrategy double_tap_{};

    static void on_double_tap(void* ctx) {
        auto* self = static_cast<Image*>(ctx);
        if (self) self->reset_zoom();
    }
};


