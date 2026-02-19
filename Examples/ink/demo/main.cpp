#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <span>
#include <thread>
#include <utility>
#include <SDL3/SDL_log.h>

import gui.core;
import gui.canvas_1bpp;
import gui.renderer;
import gui.widgets;
import gui.motion;
import gui.theme;
import gui.layout;
import gui.font;
import gui.image_1bpp;
import gui.qr_widget;
import gui.perf;
import backend.sdl3;
import out.api;
import alg_dither;
import service_trace;
import trace_core;
import util.core;

using Canvas = gui::Canvas1bpp<128, 64>;
using Renderer = gui::Renderer<Canvas>;

namespace demo {

constexpr float kPi = 3.14159265358979323846f;

inline void draw_text(Renderer& r, const char* s) noexcept {
    r.drawText(2, 2, s, true);
}

inline void draw_line_demo(Renderer& r, float t) noexcept {
    const int x0 = 0;
    const int y0 = 0;
    const int x1 = 127;
    const int y1 = (int)(t * 63.0f);
    gui::draw_line(r, x0, y0, x1, y1, true);
}

inline void draw_rect_demo(Renderer& r, float t) noexcept {
    const int w = 20 + (int)(t * 80.0f);
    const int h = 12 + (int)(t * 40.0f);
    const int x = (128 - w) / 2;
    const int y = (64 - h) / 2;
    r.drawRect(gui::Rect{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h}, true);
}

inline void fill_rect_demo(Renderer& r, float t) noexcept {
    const int w = 16 + (int)(t * 90.0f);
    const int h = 10 + (int)(t * 40.0f);
    const int x = (128 - w) / 2;
    const int y = (64 - h) / 2;
    r.fillRect(gui::Rect{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h}, true);
}

inline void invert_demo(Renderer& r, float t) noexcept {
    (void)t;
    r.set_invert(true);
    r.fillRect(gui::Rect{8, 12, 112, 40}, true);
    r.set_invert(false);
    r.drawText(20, 26, "Invert", true);
}

inline void round_rect_demo(Renderer& r, float t) noexcept {
    const int w = 30 + (int)(t * 70.0f);
    const int h = 16 + (int)(t * 30.0f);
    const int x = (128 - w) / 2;
    const int y = (64 - h) / 2;
    gui::draw_round_rect(r, gui::Rect{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h}, true);
}

inline void fill_round_rect_demo(Renderer& r, float t) noexcept {
    const int w = 30 + (int)(t * 70.0f);
    const int h = 16 + (int)(t * 30.0f);
    const int x = (128 - w) / 2;
    const int y = (64 - h) / 2;
    gui::fill_round_rect(r, gui::Rect{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h});
}

inline void dither_demo(Renderer& r, float t) noexcept {
    (void)t;
    constexpr int w = 96;
    constexpr int h = 32;
    constexpr int stride = (w + 7) / 8;
    static std::uint8_t gray[w * h]{};
    static std::uint8_t bits[stride * h]{};
    static bool init = false;
    if (!init) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                gray[y * w + x] = (std::uint8_t)((x * 255) / (w - 1));
            }
        }
        init = true;
    }
    alg::DitherConfig cfg{};
    cfg.op = alg::DitherOperator::ordered;
    cfg.ordered_mode = alg::DitherMode::bayer4;
    cfg.params.threshold = 128;
    gui::dither_gray_to_1bpp(std::span<const util::u8>{gray, (std::size_t)(w * h)},
                             w, h, cfg,
                             std::span<util::u8>{bits, (std::size_t)(stride * h)});
    const gui::Image1bpp img{
        (std::int16_t)w,
        (std::int16_t)h,
        (std::int16_t)stride,
        bits,
        gui::ImageLayout::RowMajorMsb
    };
    r.drawText(6, 2, "Dither", true);
    gui::draw_image_1bpp(r, 16, 18, img, true);
}

inline void draw_circle(Renderer& r, int cx, int cy, int rad, bool on) noexcept {
    int x = rad;
    int y = 0;
    int err = 0;
    while (x >= y) {
        r.setPixel(cx + x, cy + y, on);
        r.setPixel(cx + y, cy + x, on);
        r.setPixel(cx - y, cy + x, on);
        r.setPixel(cx - x, cy + y, on);
        r.setPixel(cx - x, cy - y, on);
        r.setPixel(cx - y, cy - x, on);
        r.setPixel(cx + y, cy - x, on);
        r.setPixel(cx + x, cy - y, on);
        if (err <= 0) {
            ++y;
            err += 2 * y + 1;
        }
        if (err > 0) {
            --x;
            err -= 2 * x + 1;
        }
    }
}

inline void fill_circle(Renderer& r, int cx, int cy, int rad, bool on) noexcept {
    for (int y = -rad; y <= rad; ++y) {
        const int w = (int)std::sqrt((float)(rad * rad - y * y));
        for (int x = -w; x <= w; ++x) {
            r.setPixel(cx + x, cy + y, on);
        }
    }
}

inline void circle_demo(Renderer& r, float t) noexcept {
    const int rad = 6 + (int)(t * 24.0f);
    draw_circle(r, 64, 32, rad, true);
}

inline void fill_circle_demo(Renderer& r, float t) noexcept {
    const int rad = 6 + (int)(t * 20.0f);
    fill_circle(r, 64, 32, rad, true);
}

inline void draw_ellipse(Renderer& r, int cx, int cy, int rx, int ry, bool on) noexcept {
    for (int a = 0; a < 360; a += 3) {
        const float rad = a * (kPi / 180.0f);
        const int x = cx + (int)(std::cos(rad) * rx);
        const int y = cy + (int)(std::sin(rad) * ry);
        r.setPixel(x, y, on);
    }
}

inline void fill_ellipse(Renderer& r, int cx, int cy, int rx, int ry, bool on) noexcept {
    for (int y = -ry; y <= ry; ++y) {
        const float t = 1.0f - (float)(y * y) / (float)(ry * ry);
        const int w = (t > 0.0f) ? (int)(rx * std::sqrt(t)) : 0;
        for (int x = -w; x <= w; ++x) {
            r.setPixel(cx + x, cy + y, on);
        }
    }
}

inline void ellipse_demo(Renderer& r, float t) noexcept {
    const int rx = 10 + (int)(t * 40.0f);
    const int ry = 6 + (int)(t * 20.0f);
    draw_ellipse(r, 64, 32, rx, ry, true);
}

inline void fill_ellipse_demo(Renderer& r, float t) noexcept {
    const int rx = 8 + (int)(t * 40.0f);
    const int ry = 6 + (int)(t * 18.0f);
    fill_ellipse(r, 64, 32, rx, ry, true);
}

inline void triangle_demo(Renderer& r, float t) noexcept {
    const int x0 = 64;
    const int y0 = 10 + (int)(t * 20.0f);
    const int x1 = 20;
    const int y1 = 52;
    const int x2 = 108;
    const int y2 = 52;
    gui::draw_line(r, x0, y0, x1, y1, true);
    gui::draw_line(r, x1, y1, x2, y2, true);
    gui::draw_line(r, x2, y2, x0, y0, true);
}

inline void fill_triangle(Renderer& r, int x0, int y0, int x1, int y1, int x2, int y2) noexcept {
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }

    auto edge = [](int x0, int y0, int x1, int y1, int y) -> int {
        if (y1 == y0) return x0;
        return x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    };

    for (int y = y0; y <= y2; ++y) {
        int xa = (y < y1) ? edge(x0, y0, x1, y1, y) : edge(x1, y1, x2, y2, y);
        int xb = edge(x0, y0, x2, y2, y);
        if (xa > xb) std::swap(xa, xb);
        for (int x = xa; x <= xb; ++x) r.setPixel(x, y, true);
    }
}

inline void fill_triangle_demo(Renderer& r, float t) noexcept {
    const int x0 = 64;
    const int y0 = 10 + (int)(t * 20.0f);
    const int x1 = 20;
    const int y1 = 52;
    const int x2 = 108;
    const int y2 = 52;
    fill_triangle(r, x0, y0, x1, y1, x2, y2);
}

inline void arc_demo(Renderer& r, float t) noexcept {
    const int cx = 64;
    const int cy = 32;
    const int rad = 24;
    const float end = t * 2.0f * kPi;
    for (int i = 0; i <= 80; ++i) {
        const float a = end * (float)i / 80.0f;
        const int x = cx + (int)(std::cos(a) * rad);
        const int y = cy + (int)(std::sin(a) * rad);
        r.setPixel(x, y, true);
    }
}

inline void rotate_demo(Renderer& r, float t) noexcept {
    const int cx = 64;
    const int cy = 32;
    const float a = t * 2.0f * kPi;
    const int x = cx + (int)(std::cos(a) * 26.0f);
    const int y = cy + (int)(std::sin(a) * 26.0f);
    gui::draw_line(r, cx, cy, x, y, true);
}

inline void polygon_demo(Renderer& r, float t) noexcept {
    const int n = 6;
    const int cx = 64;
    const int cy = 32;
    const float rot = t * 2.0f * kPi;
    const int rad = 22;
    int px = cx + (int)(std::cos(rot) * rad);
    int py = cy + (int)(std::sin(rot) * rad);
    for (int i = 1; i <= n; ++i) {
        const float a = rot + (2.0f * kPi * i) / n;
        const int x = cx + (int)(std::cos(a) * rad);
        const int y = cy + (int)(std::sin(a) * rad);
        gui::draw_line(r, px, py, x, y, true);
        px = x;
        py = y;
    }
}

inline void text_demo(Renderer& r, float t) noexcept {
    (void)t;
    r.drawText(10, 10, "Text Demo", true);
    r.drawText(10, 24, "1234567890", true);
    r.drawText(10, 38, "Hello", true);
    r.drawText(10, 52, "\xe4\xbd\xa0\xe5\xa5\xbd", true); // UTF-8 "你好"
}

inline void material_demo(Renderer& r, float t) noexcept {
    (void)t;
    r.drawText(6, 2, "Materials", true);

    const gui::Rect card{8, 10, 112, 44};
    r.drawRect(card, true);
    gui::fill_pattern(r, gui::Rect{(std::int16_t)(card.x + 1), (std::int16_t)(card.y + 1),
                                   (std::int16_t)(card.w - 2), (std::int16_t)(card.h - 2)},
                      gui::PatternKind::Noise25, true);

    const gui::Rect header{(std::int16_t)(card.x + 2), (std::int16_t)(card.y + 2), (std::int16_t)(card.w - 4), 10};
    r.fillRect(header, true);
    r.drawText((std::int16_t)(header.x + 4), (std::int16_t)(header.y + 2), "Texture UI", false);

    const int sw = 28;
    const int sh = 18;
    const int gap = 6;
    const int sx = card.x + 6;
    const int sy = card.y + 18;
    const gui::Rect a{(std::int16_t)sx, (std::int16_t)sy, (std::int16_t)sw, (std::int16_t)sh};
    const gui::Rect b{(std::int16_t)(sx + sw + gap), (std::int16_t)sy, (std::int16_t)sw, (std::int16_t)sh};
    const gui::Rect c{(std::int16_t)(sx + (sw + gap) * 2), (std::int16_t)sy, (std::int16_t)sw, (std::int16_t)sh};

    r.drawRect(a, true);
    r.drawRect(b, true);
    r.drawRect(c, true);
    gui::fill_pattern(r, gui::Rect{(std::int16_t)(a.x + 1), (std::int16_t)(a.y + 1),
                                   (std::int16_t)(a.w - 2), (std::int16_t)(a.h - 2)},
                      gui::PatternKind::Hatch45, true);
    gui::fill_pattern(r, gui::Rect{(std::int16_t)(b.x + 1), (std::int16_t)(b.y + 1),
                                   (std::int16_t)(b.w - 2), (std::int16_t)(b.h - 2)},
                      gui::PatternKind::Dots, true);
    gui::fill_pattern(r, gui::Rect{(std::int16_t)(c.x + 1), (std::int16_t)(c.y + 1),
                                   (std::int16_t)(c.w - 2), (std::int16_t)(c.h - 2)},
                      gui::PatternKind::Cross, true);
}

inline void physics_demo(Renderer& r, float t) noexcept {
    constexpr int kSamples = 96;
    static gui::motion::Spring1D spring{};
    static float set_hist[kSamples]{};
    static float val_hist[kSamples]{};
    static int head = 0;
    static bool init = false;
    static float last_t = 0.0f;

    auto reset = [&]() noexcept {
        for (int i = 0; i < kSamples; ++i) {
            set_hist[i] = 0.0f;
            val_hist[i] = 0.0f;
        }
        head = 0;
        spring.reset(0.0f);
        spring.set_params(12.0f, 0.7f);
        init = true;
        last_t = 0.0f;
    };

    if (!init || t < last_t) {
        reset();
    }

    constexpr float duration = 3.0f;
    const float time = t * duration;
    float dt = (t >= last_t) ? (t - last_t) * duration : 0.0f;
    last_t = t;

    float set = 0.0f;
    if (time >= 1.0f && time < 2.0f) {
        set = 1.0f;
    }
    spring.target = set;
    spring.step(dt);

    set_hist[head] = set;
    val_hist[head] = spring.value;
    head = (head + 1) % kSamples;

    r.drawText(6, 2, "Set", true);
    r.drawText(32, 2, "Track", true);

    const gui::Rect plot{6, 12, 116, 38};
    r.drawRect(plot, true);

    auto sample_at = [&](const float* arr, int x, int w) noexcept -> float {
        const int offset = w - 1 - x;
        int idx = head - 1 - offset;
        while (idx < 0) idx += kSamples;
        return arr[idx % kSamples];
    };

    const int w = (plot.w < kSamples) ? plot.w : kSamples;
    const int h = plot.h;
    const int x0 = plot.x;
    const int y0 = plot.y;

    for (int x = 0; x < w; ++x) {
        const float v = sample_at(set_hist, x, w);
        const int y = y0 + h - 1 - (int)(v * (float)(h - 1));
        if ((x & 1) == 0) {
            r.setPixel((std::int16_t)(x0 + x), (std::int16_t)y, true);
        }
    }

    int prev_y = y0 + h - 1 - (int)(sample_at(val_hist, 0, w) * (float)(h - 1));
    for (int x = 1; x < w; ++x) {
        const float v = sample_at(val_hist, x, w);
        const int y = y0 + h - 1 - (int)(v * (float)(h - 1));
        gui::draw_line(r, x0 + x - 1, prev_y, x0 + x, y, true);
        prev_y = y;
    }
}

inline void gauge_demo(Renderer& r, float t) noexcept {
    const float v = 0.5f + 0.5f * std::sin(t * 2.0f * kPi);
    const std::uint8_t value = (std::uint8_t)(v * 100.0f);
    const gui::Rect rc{12, 10, 104, 44};
    gui::draw_gauge(r, rc, value, true);
    r.drawText(6, 2, "Gauge", true);
}

inline void segment_demo(Renderer& r, float t) noexcept {
    const int value = (int)(t * 999.0f);
    const int d0 = (value / 100) % 10;
    const int d1 = (value / 10) % 10;
    const int d2 = value % 10;
    const int w = 18;
    const int h = 28;
    const int gap = 6;
    const int x0 = (128 - (w * 3 + gap * 2)) / 2;
    const int y0 = 18;
    gui::draw_segment_digit(r, gui::Rect{(std::int16_t)x0, (std::int16_t)y0, (std::int16_t)w, (std::int16_t)h}, d0, true);
    gui::draw_segment_digit(r, gui::Rect{(std::int16_t)(x0 + w + gap), (std::int16_t)y0, (std::int16_t)w, (std::int16_t)h}, d1, true);
    gui::draw_segment_digit(r, gui::Rect{(std::int16_t)(x0 + (w + gap) * 2), (std::int16_t)y0, (std::int16_t)w, (std::int16_t)h}, d2, true);
    r.drawText(6, 2, "Segment", true);
}

inline void sparkline_demo(Renderer& r, float t) noexcept {
    constexpr int kN = 64;
    static std::uint8_t data[kN]{};
    const float phase = t * 2.0f * kPi;
    for (int i = 0; i < kN; ++i) {
        const float a = phase + (float)i * 0.18f;
        const float s = 0.5f + 0.5f * std::sin(a);
        data[i] = (std::uint8_t)(s * 100.0f);
    }
    gui::SparklineView view{};
    view.data = data;
    view.count = kN;
    view.min_v = 0;
    view.max_v = 100;
    view.draw_border = true;
    const gui::Rect rc{6, 18, 116, 34};
    gui::draw_sparkline(r, rc, view, true);
    r.drawText(6, 2, "Sparkline", true);
}

inline void tag_demo(Renderer& r, float t) noexcept {
    (void)t;
    const auto& font = *gui::theme::current().font_default;
    r.drawText(6, 2, "Tags", true);
    const gui::Rect a{12, 16, 44, 14};
    const gui::Rect b{64, 16, 44, 14};
    const gui::Rect c{12, 36, 96, 14};
    gui::draw_tag(r, a, font, "LIVE", true);
    gui::draw_tag(r, b, font, "SYNC", true);
    gui::draw_tag(r, c, font, "MONOCHROME", true);
}

inline void knob_demo(Renderer& r, float t) noexcept {
    const float v = 0.5f + 0.5f * std::sin(t * 2.0f * kPi);
    const std::uint8_t value = (std::uint8_t)(v * 100.0f);
    const gui::Rect rc{32, 10, 64, 44};
    gui::draw_knob(r, rc, value, true);
    r.drawText(6, 2, "Knob", true);
}

inline void marquee_demo(Renderer& r, float t) noexcept {
    const char* text = "Charm-ink marquee text demo";
    const auto& font = *gui::theme::current().font_default;
    r.drawText(6, 2, "Marquee", true);
    const gui::Rect rc{6, 24, 116, 12};
    r.drawRect(rc, true);
    const int w = gui::measure_text(font, text);
    const std::uint32_t now_ms = (std::uint32_t)(t * 4000.0f);
    const int dx = gui::marquee_offset((std::int16_t)w, rc.w - 2, now_ms, 24, 400);
    const int base = gui::layout::baseline_from_top(font, rc.y + 2);
    r.drawText((std::int16_t)(rc.x + 1 - dx), (std::int16_t)base, text, true);
}

inline void icon_row_demo(Renderer& r, float t) noexcept {
    (void)t;
    static constexpr std::uint8_t kIconData[] = {
        0x00, 0x00,
        0x18, 0x00,
        0x3C, 0x00,
        0x7E, 0x00,
        0xFF, 0x00,
        0x7E, 0x00,
        0x3C, 0x00,
        0x18, 0x00,
        0x18, 0x00,
        0x3C, 0x00,
        0x7E, 0x00,
        0xFF, 0x00,
        0x7E, 0x00,
        0x3C, 0x00,
        0x18, 0x00,
        0x00, 0x00,
    };
    static constexpr gui::Image1bpp kIcon{
        16, 16, 2,
        kIconData,
        gui::ImageLayout::RowMajorMsb
    };
    r.drawText(6, 2, "Icon Row", true);
    gui::draw_icon_label_badge(r, gui::Rect{6, 14, 116, 14}, &kIcon, "Wireless", "ON", false);
    gui::draw_icon_label_badge(r, gui::Rect{6, 30, 116, 14}, &kIcon, "Battery", "78%", true);
    gui::draw_icon_label_badge(r, gui::Rect{6, 46, 116, 14}, &kIcon, "Sync", "NEW", false);
}

inline void qr_demo(Renderer& r, float t) noexcept {
    (void)t;
    r.drawText(6, 2, "QR", true);
    static gui::qr::QrCode code{};
    if (!code.valid) {
        (void)code.encode("https://www.baidu.com/");
    }
    const gui::Rect rc{12, 12, 104, 48};
    code.draw(r, rc, true);
}

struct DemoItem {
    const char* name;
    std::uint32_t duration_ms;
    void (*draw)(Renderer&, float t) noexcept;
};

static constexpr DemoItem kDemos[] = {
    {"Line", 1200, &draw_line_demo},
    {"Rect", 1200, &draw_rect_demo},
    {"FillRect", 1200, &fill_rect_demo},
    {"Invert", 1200, &invert_demo},
    {"RoundRect", 1200, &round_rect_demo},
    {"FillRound", 1200, &fill_round_rect_demo},
    {"Dither", 1600, &dither_demo},
    {"Circle", 1200, &circle_demo},
    {"FillCircle", 1200, &fill_circle_demo},
    {"Ellipse", 1200, &ellipse_demo},
    {"FillEllipse", 1200, &fill_ellipse_demo},
    {"Triangle", 1200, &triangle_demo},
    {"FillTri", 1200, &fill_triangle_demo},
    {"Arc", 1200, &arc_demo},
    {"Rotate", 1200, &rotate_demo},
    {"Polygon", 1200, &polygon_demo},
    {"Text", 1600, &text_demo},
    {"Materials", 1600, &material_demo},
    {"Physics", 2400, &physics_demo},
    {"Gauge", 1600, &gauge_demo},
    {"Segment", 1600, &segment_demo},
    {"Sparkline", 1600, &sparkline_demo},
    {"Tags", 1600, &tag_demo},
    {"Knob", 1600, &knob_demo},
    {"Marquee", 1600, &marquee_demo},
    {"IconRow", 1600, &icon_row_demo},
    {"QR", 1600, &qr_demo},
};

} // namespace demo

int main() try {
    constexpr int kScale = 8;
    const char* kTitle = "Charm-ink Demo";

    Canvas canvas;
    Renderer renderer(canvas);
    backend::SDL3Backend<128, 64> sdl(kTitle, kScale);

    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();

    constexpr std::uint32_t kTraceFpsId = 1001;
    constexpr std::uint32_t kTraceFrameId = 1002;
    constexpr util::usize kTraceCap = 128;

    struct TraceSink {
        service::TraceBuffer<util::u32, kTraceCap> buffer{};
        util::u32 now_ms{0};

        void emit(trace::TraceKind kind, std::uint32_t id, std::uint64_t payload) noexcept {
            service::TraceRecord<util::u32, kTraceCap> rec{};
            rec.time = now_ms;
            rec.id = id;
            rec.payload = payload;
            rec.count = 1;
            rec.kind = kind;
            buffer.push(rec);
        }
    };

    auto trace_emit = [](void* ctx, trace::TraceKind kind, std::uint32_t id, std::uint64_t payload) noexcept {
        auto* sink = static_cast<TraceSink*>(ctx);
        if (!sink) return;
        sink->emit(kind, id, payload);
    };

    auto trace_latest = [](const TraceSink& sink,
                           std::uint32_t id,
                           trace::TraceKind kind,
                           std::uint64_t& out) noexcept -> bool {
        const auto total = sink.buffer.size();
        if (total == 0) return false;
        const auto cap = sink.buffer.capacity();
        const auto head = sink.buffer.head();
        const auto& data = sink.buffer.data();
        for (util::usize i = 0; i < total; ++i) {
            const auto idx = (head + cap - 1 - i) % cap;
            const auto& rec = data[idx];
            if (rec.id == id && rec.kind == kind) {
                out = rec.payload;
                return true;
            }
        }
        return false;
    };

    TraceSink trace{};
    gui::perf::FpsCounter fps{};
    fps.set_trace_hook(gui::perf::TraceHook{&trace, trace_emit, kTraceFpsId, kTraceFrameId});

    std::size_t index = 0;
    while (true) {
        const auto now = clock::now();
        const auto ms = (std::uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        trace.now_ms = ms;
        if (fps.update(ms)) {
            std::uint64_t fps_x1000 = 0;
            std::uint64_t frames = 0;
            (void)trace_latest(trace, kTraceFpsId, trace::TraceKind::counter, fps_x1000);
            (void)trace_latest(trace, kTraceFrameId, trace::TraceKind::counter_delta, frames);
            (void)out::raw().template try_println<"trace_fps,{},{}">(fps_x1000, frames);
        }

        const auto& demo = demo::kDemos[index];
        const std::uint32_t t_in = ms % demo.duration_ms;
        const float t = (demo.duration_ms > 0) ? ((float)t_in / (float)demo.duration_ms) : 0.0f;

        renderer.clear(false);
        demo.draw(renderer, t);
        renderer.drawText(2, 54, demo.name, true);
        if (canvas.dirty_count() > 0) {
            constexpr int kDirtyMaxRects = 4;
            constexpr int kDirtyAreaLimit = (Canvas::kWidth * Canvas::kHeight) / 2;
            const auto stats = canvas.dirty_stats();
            const bool too_many = (stats.count > kDirtyMaxRects);
            const bool too_big = (stats.area > kDirtyAreaLimit);
            const bool full = stats.full || too_many || too_big;
            if (full) {
                sdl.update_texture(canvas, nullptr);
            } else {
                const int n = canvas.dirty_count();
                for (int i = 0; i < n; ++i) {
                    const auto dr = canvas.dirty_rect_at(i);
                    sdl.update_texture(canvas, &dr);
                }
            }
            sdl.present_frame();
            canvas.clear_dirty();
        }

        if (t_in + 16 >= demo.duration_ms) {
            index = (index + 1) % (sizeof(demo::kDemos) / sizeof(demo::kDemos[0]));
        }

        if (sdl.pump_quit()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
} catch (const std::exception& e) {
    SDL_Log("Fatal: %s", e.what());
    return 1;
}
