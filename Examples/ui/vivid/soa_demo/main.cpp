#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cmath>

import charm.ui.vivid_internal;
import charm.ui.scene;
import charm.core.event;
import charm.core.config;
import charm.core.geometry;
import charm.core.style;
import charm.core.style_evidence;
import charm.core.soa_registry;
import charm.gfx.snapshot;
import charm.core.theme_preset;
import charm.core.widget_registry;
import charm.gfx.canvas;
import charm.gfx.display_policy;
import charm.gfx.image;
import charm.gfx.snapshot;
import charm.gfx.pixel_ops;
import charm.gfx.svg;
import charm.gfx.text_box;
import charm.font.typography;
import charm.widgets.menu_tree;
import charm.widgets.perf_overlay;
import out.api;

namespace {
    struct StdioSink {
        out::result<std::size_t> write(out::bytes b) noexcept {
            if (b.size() == 0) return out::ok<std::size_t>(0u);
            (void)std::fwrite(b.data(), 1, b.size(), stdout);
            return out::ok(b.size());
        }

        out::result<std::size_t> flush() noexcept {
            std::fflush(stdout);
            return out::ok<std::size_t>(0u);
        }
    };

    StdioSink g_console{};

    struct Viewport {
        int x{0};
        int y{0};
        int w{0};
        int h{0};
        float scale{1.0f};
    };

    Viewport compute_viewport(int win_w, int win_h, int canvas_w, int canvas_h) noexcept {
        const float sx = static_cast<float>(win_w) / static_cast<float>(canvas_w);
        const float sy = static_cast<float>(win_h) / static_cast<float>(canvas_h);
        const float scale = (sx < sy) ? sx : sy;
        const int w = static_cast<int>(static_cast<float>(canvas_w) * scale);
        const int h = static_cast<int>(static_cast<float>(canvas_h) * scale);
        const int x = (win_w - w) / 2;
        const int y = (win_h - h) / 2;
        return Viewport{x, y, w, h, scale};
    }

    bool map_mouse(const Viewport& vp, int wx, int wy, int& out_x, int& out_y) noexcept {
        if (wx < vp.x || wy < vp.y || wx >= vp.x + vp.w || wy >= vp.y + vp.h) return false;
        out_x = static_cast<int>((wx - vp.x) / vp.scale);
        out_y = static_cast<int>((wy - vp.y) / vp.scale);
        return true;
    }

    constexpr int kTileWidth = 64;
    constexpr int kTileHeight = 64;
    constexpr std::size_t kTileStride = static_cast<std::size_t>(kTileWidth) * DefaultFrameBuffer::bytes_per_pixel;
    constexpr std::size_t kTileBytes = kTileStride * static_cast<std::size_t>(kTileHeight);

    std::uint32_t hash_bytes(const std::byte* data, std::size_t len) noexcept {
        std::uint32_t hash = 2166136261u;
        if (!data) return hash;
        for (std::size_t i = 0; i < len; ++i) {
            hash ^= static_cast<std::uint8_t>(data[i]);
            hash *= 16777619u;
        }
        return hash;
    }

    bool run_svg_workspace_regression() {
        constexpr int kRasterWidth = 24;
        constexpr int kRasterHeight = 24;
        constexpr ::ui::gfx::svg::RasterConfig kConfig{
            .width = kRasterWidth,
            .height = kRasterHeight,
            .view = {68.0f, 24.0f},
        };
        constexpr std::string_view kSquare = "M2 2 L22 2 L22 22 L2 22 Z";
        static ::ui::gfx::svg::RasterWorkspace workspace{};
        static std::array<std::byte,
                          static_cast<std::size_t>(kRasterWidth * kRasterHeight * 4)> pixels{};

        const auto raster = [&](std::string_view path) {
            return ::ui::gfx::svg::rasterize_path(
                workspace,
                path,
                kConfig,
                std::span<std::byte>{pixels.data(), pixels.size()},
                rgba{32, 120, 220, 255},
                true);
        };
        if (!raster(kSquare)) return false;
        if (std::none_of(pixels.begin(), pixels.end(), [](std::byte value) {
                return value != std::byte{0};
            })) {
            return false;
        }

        std::string intersection_overflow{"M0 0"};
        intersection_overflow.reserve(512);
        for (int i = 1; i <= 66; ++i) {
            intersection_overflow += " L";
            intersection_overflow += std::to_string(i);
            intersection_overflow += (i % 2 == 0) ? " 0" : " 24";
        }
        if (raster(intersection_overflow)) return false;

        return raster(kSquare);
    }

    bool run_perf_overlay_runtime_regression() noexcept {
        using namespace ui::perf_overlay_runtime;

        clear();
        clear_perf_overlay_debug_channels();

        constexpr std::string_view overlong_name{"123456789012345678901234"};
        static_assert(overlong_name.size() == debug_channel_name_capacity);
        if (perf_overlay_debug_channel(overlong_name) != debug_line_count) return false;
        if (perf_overlay_debug_channel(overlong_name) != debug_line_count) return false;

        constexpr std::array<std::string_view, debug_line_count> names{
            "slot.0", "slot.1", "slot.2", "slot.3", "slot.4", "slot.5"
        };
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (perf_overlay_debug_channel(names[i]) != i) return false;
            if (perf_overlay_debug_channel(names[i]) != i) return false;
        }
        if (perf_overlay_debug_channel("slot.overflow") != debug_line_count) return false;

        set_perf_overlay_debug_channel(0, "stale");
        if (debug_line(0) != "stale") return false;
        clear_perf_overlay_debug_channels();
        if (!debug_line(0).empty() || !perf_overlay_debug_channel_name(0).empty()) return false;

        constexpr std::string_view max_name{"12345678901234567890123"};
        static_assert(max_name.size() + 1u == debug_channel_name_capacity);
        const std::size_t max_slot = perf_overlay_debug_channel(max_name);
        if (max_slot != 0 || perf_overlay_debug_channel(max_name) != max_slot) return false;

        set({.dispatch_groups = 7});
        if (!valid() || get().dispatch_groups != 7) return false;
        clear();
        clear_perf_overlay_debug_channels();
        return !valid();
    }


    constexpr int kTestIconWidth = 8;
    constexpr int kTestIconHeight = 8;
    constexpr std::uint16_t kTestIconOn = 0xFFE0;
    constexpr std::uint16_t kTestIconOff = 0x0000;
    constexpr int kSliceWidth = 6;
    constexpr int kSliceHeight = 6;
    constexpr std::uint16_t kSliceBorder = 0x4208;
    constexpr std::uint16_t kSliceCenter = 0xFFFF;
    constexpr rgba kDemoBg{246, 248, 252, 255};
    constexpr rgba kDemoPanel{232, 236, 244, 255};
    constexpr rgba kDemoPanelBorder{176, 184, 196, 255};
    constexpr rgba kDemoPath{32, 120, 220, 255};
    constexpr std::array<std::uint16_t, kTestIconWidth * kTestIconHeight> kTestIconData{
        kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOn,
        kTestIconOff, kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOff,
        kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOff, kTestIconOff,
        kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOff,
        kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOff,
        kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOff, kTestIconOff,
        kTestIconOff, kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOn,  kTestIconOff,
        kTestIconOn,  kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOff, kTestIconOn
    };
    constexpr std::array<std::uint16_t, kSliceWidth * kSliceHeight> kSliceData{
        kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder,
        kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder,
        kSliceBorder, kSliceBorder, kSliceCenter, kSliceCenter, kSliceBorder, kSliceBorder,
        kSliceBorder, kSliceBorder, kSliceCenter, kSliceCenter, kSliceBorder, kSliceBorder,
        kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder,
        kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder, kSliceBorder
    };
    std::array<Point, 4> g_demo_path_points{};

    const ImageView kTestIconView = make_image_view(PixelFormat::RGB565,
                                                    kTestIconWidth,
                                                    kTestIconHeight,
                                                    kTestIconWidth * static_cast<int>(sizeof(std::uint16_t)),
                                                    reinterpret_cast<const std::byte*>(kTestIconData.data()),
                                                    false,
                                                    false);
    const ImageView kSliceView = make_image_view(PixelFormat::RGB565,
                                                 kSliceWidth,
                                                 kSliceHeight,
                                                 kSliceWidth * static_cast<int>(sizeof(std::uint16_t)),
                                                 reinterpret_cast<const std::byte*>(kSliceData.data()),
                                                 false,
                                                 false);

    ui::draw_cmd::ImageId g_test_icon_id{};
    ui::draw_cmd::ImageId g_slice_id{};
    bool g_demo_images_ready{false};
    bool g_selftest_dedup{false};

    void ensure_demo_images() noexcept {
        if (g_demo_images_ready) return;
        std::array<ui::draw_cmd::ImageAsset, 2> assets{
            ui::draw_cmd::ImageAsset{kTestIconView, ui::draw_cmd::invalid_image_id(), "demo_images"},
            ui::draw_cmd::ImageAsset{kSliceView, ui::draw_cmd::invalid_image_id(), "demo_images"}
        };
        (void)ui::draw_cmd::register_image_bundle(assets,
                                                  ui::draw_cmd::ImageRegisterReason::Init,
                                                  true,
                                                  false);
        g_test_icon_id = assets[0].id;
        g_slice_id = assets[1].id;
        if (g_selftest_dedup) {
            (void)ui::draw_cmd::register_image_dedup(
                kTestIconView,
                ui::draw_cmd::ImageRegisterReason::SelfTest,
                "selftest_dedup");
            (void)ui::draw_cmd::register_image_dedup(
                kSliceView,
                ui::draw_cmd::ImageRegisterReason::SelfTest,
                "selftest_dedup");
        }
        if (!ui::draw_cmd::image_registry_locked()) {
            ui::draw_cmd::set_image_registry_locked(true);
        }
        g_demo_images_ready = true;
    }

    struct ListViewTextSource {
        const char* const* items{nullptr};
        std::uint16_t count{0};
    };

    struct ListViewRowFlagsSource {
        std::uint16_t disabled_index{0xFFFF};
        std::uint16_t group_index{0xFFFF};
    };

    const char* list_view_text_at(const void* ctx, std::uint16_t index) noexcept {
        const auto* src = static_cast<const ListViewTextSource*>(ctx);
        if (!src || !src->items || src->count == 0) return "";
        return src->items[index % src->count];
    }

    std::uint8_t list_view_row_flags_at(const void* ctx, std::uint16_t index) noexcept {
        const auto* src = static_cast<const ListViewRowFlagsSource*>(ctx);
        if (!src) return 0;
        std::uint8_t flags = 0;
        if (index == src->group_index) {
            flags |= soa_detail::kListViewRowFlagGroup;
        }
        if (index == src->disabled_index) {
            flags |= soa_detail::kListViewRowFlagDisabled;
        }
        return flags;
    }

    struct ListViewIconSource {
        ui::draw_cmd::ImageId icon{};
        ui::draw_cmd::ImageId alt{};
    };

    ui::draw_cmd::ImageId list_view_icon_at(const void* ctx, std::uint16_t index) noexcept {
        const auto* src = static_cast<const ListViewIconSource*>(ctx);
        if (!src) return ui::draw_cmd::invalid_image_id();
        if (index & 1u) return src->alt;
        return src->icon;
    }

    struct TableViewTextSource {
        const char* const* rows{nullptr};
        std::uint16_t row_count{0};
        const char* const* cols{nullptr};
        std::uint8_t col_count{0};
    };

    struct TableViewColWidthSource {
        const int* widths{nullptr};
        std::uint8_t count{0};
    };

    struct TextProbe {
        Rect rect{};
        std::string_view text{};
    };

    const std::array<int, 8> kTableColWidths{
        64, 96, 72, 120, 80, 88, 68, 104
    };
    const TableViewColWidthSource kTableColWidthSource{
        kTableColWidths.data(),
        static_cast<std::uint8_t>(kTableColWidths.size())
    };

    const char* table_view_text_at(const void* ctx, std::uint16_t row, std::uint8_t col) noexcept {
        const auto* src = static_cast<const TableViewTextSource*>(ctx);
        if (!src || !src->rows || src->row_count == 0) return "";
        if (col == 0) {
            return src->rows[row % src->row_count];
        }
        if (src->cols && col < src->col_count) {
            return src->cols[col];
        }
        return src->rows[row % src->row_count];
    }

    const char* table_view_header_at(const void* ctx, std::uint8_t col) noexcept {
        const auto* src = static_cast<const TableViewTextSource*>(ctx);
        if (!src || !src->cols || src->col_count == 0) return "";
        if (col >= src->col_count) return "";
        return src->cols[col];
    }

    int table_view_col_width_at(const void* ctx, std::uint8_t col) noexcept {
        const auto* src = static_cast<const TableViewColWidthSource*>(ctx);
        if (!src || !src->widths || src->count == 0) return 0;
        return src->widths[col % src->count];
    }

    bool rects_intersect(const Rect& a, const Rect& b) noexcept {
        if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) return false;
        const int a_right = a.x + a.w;
        const int a_bottom = a.y + a.h;
        const int b_right = b.x + b.w;
        const int b_bottom = b.y + b.h;
        return a.x < b_right && a_right > b.x && a.y < b_bottom && a_bottom > b.y;
    }

    bool collect_text_probes(const ui::draw_cmd::DefaultDrawCmdBuffer& buf,
                             std::vector<TextProbe>& out) noexcept {
        out.clear();
        out.reserve(buf.size());
        ui::draw_cmd::DrawCmd cmd{};
        const std::size_t cmd_count = buf.size();
        for (std::size_t i = 0; i < cmd_count; ++i) {
            if (!buf.read_cmd(i, cmd)) return false;
            switch (cmd.type) {
            case ui::draw_cmd::CmdType::DrawTextBox: {
                if (cmd.text.length == 0) break;
                const char* text = buf.text_at(cmd.text.offset);
                if (!text) return false;
                out.push_back(TextProbe{cmd.rect, std::string_view(text, cmd.text.length)});
                break;
            }
            case ui::draw_cmd::CmdType::GlyphRun: {
                if (cmd.p0 <= 0) return false;
                const std::size_t count = static_cast<std::size_t>(cmd.p0);
                const auto blob_span = buf.blob_at(cmd.blob);
                if (blob_span.size() < count * sizeof(ui::draw_cmd::GlyphRunItem)) return false;
                for (std::size_t j = 0; j < count; ++j) {
                    ui::draw_cmd::GlyphRunItem item{};
                    std::memcpy(&item,
                                blob_span.data() + (j * sizeof(ui::draw_cmd::GlyphRunItem)),
                                sizeof(item));
                    if (item.text.length == 0) continue;
                    const char* text = buf.text_at(item.text.offset);
                    if (!text) return false;
                    out.push_back(TextProbe{item.rect, std::string_view(text, item.text.length)});
                }
                break;
            }
            default:
                break;
            }
        }
        return true;
    }

    bool collect_fill_rects(const ui::draw_cmd::DefaultDrawCmdBuffer& buf,
                            std::vector<Rect>& out) noexcept {
        out.clear();
        out.reserve(buf.size());
        ui::draw_cmd::DrawCmd cmd{};
        const std::size_t cmd_count = buf.size();
        for (std::size_t i = 0; i < cmd_count; ++i) {
            if (!buf.read_cmd(i, cmd)) return false;
            switch (cmd.type) {
            case ui::draw_cmd::CmdType::FillRect:
                out.push_back(cmd.rect);
                break;
            case ui::draw_cmd::CmdType::FillRectBatch: {
                if (cmd.p0 <= 0) return false;
                const std::size_t count = static_cast<std::size_t>(cmd.p0);
                const auto blob_span = buf.blob_at(cmd.blob);
                if (blob_span.size() < count * sizeof(ui::draw_cmd::RectBatchItem)) return false;
                for (std::size_t j = 0; j < count; ++j) {
                    ui::draw_cmd::RectBatchItem item{};
                    std::memcpy(&item,
                                blob_span.data() + (j * sizeof(ui::draw_cmd::RectBatchItem)),
                                sizeof(item));
                    out.push_back(item.rect);
                }
                break;
            }
            default:
                break;
            }
        }
        return true;
    }

    struct TreeViewTextSource {
        const char* const* items{nullptr};
        std::uint16_t count{0};
    };

    const char* tree_view_text_at(const void* ctx, std::uint16_t index) noexcept {
        const auto* src = static_cast<const TreeViewTextSource*>(ctx);
        if (!src || !src->items || src->count == 0) return "";
        return src->items[index % src->count];
    }

    struct TreeViewIndentSource {
        std::uint8_t mod{0};
    };

    std::uint8_t tree_view_indent_at(const void* ctx, std::uint16_t index) noexcept {
        const auto* src = static_cast<const TreeViewIndentSource*>(ctx);
        if (!src || src->mod == 0) return 0;
        return static_cast<std::uint8_t>(index % src->mod);
    }

    struct RollerTextSource {
        const char* const* items{nullptr};
        std::uint16_t count{0};
    };

    const char* roller_text_at(const void* ctx, std::uint16_t index) noexcept {
        const auto* src = static_cast<const RollerTextSource*>(ctx);
        if (!src || !src->items || src->count == 0) return "";
        return src->items[index % src->count];
    }

    void append_path_icon(ui::draw_cmd::DefaultDrawCmdBuffer& buf, int screen_width) noexcept {
        ensure_demo_images();
        const int path_y = 24;
        const int path_w = 120;
        const int path_h = 60;
        const int path_x = screen_width - path_w - 16;
        const Rect panel_rect{path_x - 8, path_y - 8, path_w + 16, path_h + 16};
        const Rect slice_rect{path_x - 6, path_y + path_h + 6, path_w + 12, 18};
        buf.fill_round_rect(panel_rect, 10, kDemoPanel);
        buf.stroke_round_rect(panel_rect, 10, kDemoPanelBorder);
        g_demo_path_points[0] = Point{path_x, path_y + path_h};
        g_demo_path_points[1] = Point{path_x + path_w / 2, path_y};
        g_demo_path_points[2] = Point{path_x + path_w, path_y + path_h};
        g_demo_path_points[3] = Point{path_x, path_y + path_h};
        buf.draw_path(g_demo_path_points.data(), 4, false, kDemoPath);
        buf.draw_icon(Rect{path_x + 44, path_y + 16, 24, 24}, g_test_icon_id);
        buf.draw_image_nine_slice(slice_rect, g_slice_id, 2, 2, 2, 2);
    }

    void append_compaction_probe(ui::draw_cmd::DefaultDrawCmdBuffer& buf,
                                 int screen_width,
                                 int screen_height) noexcept {
        const int size = 8;
        const int gap = 3;
        const int count = 6;
        int x = 8;
        int y = 36;
        if (screen_height > 120) {
            y = screen_height - (size + gap) * 5 - 12;
        }
        if (screen_width < 200) {
            x = 4;
        }

        for (int i = 0; i < count; ++i) {
            const Rect r{x + i * (size + gap), y, size, size};
            buf.fill_round_rect(r, 3, kDemoPanel);
        }
        y += size + gap;
        for (int i = 0; i < count; ++i) {
            const Rect r{x + i * (size + gap), y, size, size};
            buf.stroke_round_rect(r, 3, kDemoPanelBorder);
        }
        y += size + gap;
        for (int i = 0; i < count; ++i) {
            const int cx = x + i * (size + gap) + size / 2;
            const int cy = y + size / 2;
            buf.fill_circle(cx, cy, size / 2, kDemoPath);
        }
        y += size + gap;
        for (int i = 0; i < count; ++i) {
            const int cx = x + i * (size + gap) + size / 2;
            const int cy = y + size / 2;
            buf.stroke_circle(cx, cy, size / 2, kDemoPath);
        }
        y += size + gap;
        for (int i = 0; i < count; ++i) {
            const Rect r{x + i * (size + gap), y, size, size};
            buf.stroke_rect(r, kDemoPanelBorder);
        }
        y += size + gap;
        for (int i = 0; i < count; ++i) {
            const int x0 = x + i * (size + gap);
            const int y0 = y;
            buf.draw_line(x0, y0 + size, x0 + size, y0, kDemoPanelBorder);
        }
        y += size + gap;
        for (int i = 0; i < count; ++i) {
            const int x0 = x + i * (size + gap);
            const int y0 = y;
            const Point tri[3]{
                Point{x0, y0 + size},
                Point{x0 + size / 2, y0},
                Point{x0 + size, y0 + size}
            };
            buf.draw_path(tri, 3, false, kDemoPanelBorder);
        }
    }

    void apply_demo_theme() noexcept {
        ThemeTokens tokens = Theme::instance().get_tokens();
        tokens.surface = kDemoBg;
        tokens.surface_variant = kDemoPanel;
        tokens.outline = kDemoPanelBorder;
        tokens.accent = kDemoPath;
        tokens.on_surface = rgba{24, 28, 36, 255};
        tokens.on_surface_muted = rgba{92, 100, 112, 255};
        tokens.on_accent = rgba{255, 255, 255, 255};
        tokens.focus_ring = kDemoPath;
        Theme::instance().set_tokens_unsafe(tokens);
        apply_baseline_theme_preset(make_style_from_tokens(tokens));
    }

    struct SdlTileBackend {
        enum class RefreshKind : std::uint8_t {
            None,
            Partial,
            Full
        };

        DefaultFrameBuffer& fb;
        PixelFormat src_format{screen_pixel_format};
        std::size_t src_bpp{DefaultFrameBuffer::bytes_per_pixel};
        ui::gfx::DisplayConfig display{};
        ui::gfx::EinkPolicy eink_policy{};
        RefreshKind last_refresh{RefreshKind::None};
        int partial_count{0};
        int total_partial_count{0};
        int total_full_count{0};
        std::uint32_t last_full_ms{0};
        int last_dirty_pct{0};
        std::array<std::uint8_t, 256> gray2_lut{};
        ui::gfx::Gray2Curve gray2_curve_cached{ui::gfx::Gray2Curve::Linear};
        bool gray2_lut_ready{false};
        bool dirty_set{false};
        int dirty_left{0};
        int dirty_top{0};
        int dirty_right{0};
        int dirty_bottom{0};

        int width() const noexcept { return screen_width; }
        int height() const noexcept { return screen_height; }

        void begin_frame() noexcept { dirty_set = false; }
        void end_frame() noexcept {}

        void update_gray2_lut() noexcept {
            if (gray2_lut_ready && gray2_curve_cached == display.gray2_curve) return;
            gray2_curve_cached = display.gray2_curve;
            gray2_lut_ready = true;
            float gamma = 1.0f;
            switch (display.gray2_curve) {
            case ui::gfx::Gray2Curve::Soft: gamma = 1.25f; break;
            case ui::gfx::Gray2Curve::Contrast: gamma = 0.85f; break;
            default: break;
            }
            for (int i = 0; i < 256; ++i) {
                const float x = static_cast<float>(i) / 255.0f;
                const float y = std::pow(x, gamma);
                int v = static_cast<int>(y * 255.0f + 0.5f);
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                gray2_lut[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v);
            }
        }

        void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
            if (!src || bytes == 0) return;
            if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) return;
            const std::size_t bpp = DefaultFrameBuffer::bytes_per_pixel;
            const std::size_t stride = DefaultFrameBuffer::stride_bytes;
            const std::size_t max_bytes = static_cast<std::size_t>(screen_width - x) * bpp;
            if (bytes > max_bytes) bytes = max_bytes;
            auto* dst = fb.data() + static_cast<std::size_t>(y) * stride
                + static_cast<std::size_t>(x) * bpp;
            if (display.mode == ui::gfx::DisplayMode::Color) {
                std::memcpy(dst, src, bytes);
                return;
            }
            update_gray2_lut();
            if (src_bpp == 0) return;
            const std::size_t count = bytes / src_bpp;
            const auto read_rgb = [&](const std::byte* p) noexcept -> rgb {
                switch (src_format) {
                case PixelFormat::RGB565: {
                    std::uint16_t px{};
                    std::memcpy(&px, p, sizeof(px));
                    return unpack_rgb565(px);
                }
                case PixelFormat::RGB888:
                    return rgb{
                        static_cast<std::uint8_t>(p[0]),
                        static_cast<std::uint8_t>(p[1]),
                        static_cast<std::uint8_t>(p[2])
                    };
                case PixelFormat::ARGB8888: {
                    std::uint32_t px{};
                    std::memcpy(&px, p, sizeof(px));
                    const rgba c = unpack_argb8888(px);
                    return rgb{c.r, c.g, c.b};
                }
                default:
                    return rgb{0, 0, 0};
                }
            };
            const auto write_gray = [&](std::byte* p, std::uint8_t v) noexcept {
                switch (screen_pixel_format) {
                case PixelFormat::RGB565: {
                    const std::uint16_t px = pack_rgb565(rgb{v, v, v});
                    std::memcpy(p, &px, sizeof(px));
                    break;
                }
                case PixelFormat::RGB888:
                    p[0] = std::byte{v};
                    p[1] = std::byte{v};
                    p[2] = std::byte{v};
                    break;
                case PixelFormat::ARGB8888: {
                    const std::uint32_t px = pack_argb8888(rgba{v, v, v, 255});
                    std::memcpy(p, &px, sizeof(px));
                    break;
                }
                }
            };
            static constexpr std::uint8_t kBayer4[4][4] = {
                { 0,  8,  2, 10},
                {12,  4, 14,  6},
                { 3, 11,  1,  9},
                {15,  7, 13,  5}
            };
            const std::byte* s = src;
            std::byte* d = dst;
            for (std::size_t i = 0; i < count; ++i) {
                const rgb c = read_rgb(s);
                const std::uint8_t lum = static_cast<std::uint8_t>(
                    (static_cast<std::uint32_t>(c.r) * 77u
                        + static_cast<std::uint32_t>(c.g) * 150u
                        + static_cast<std::uint32_t>(c.b) * 29u) >> 8u);
                std::uint8_t v = 0u;
                if (display.mode == ui::gfx::DisplayMode::BW1) {
                    v = (lum >= display.bw1_threshold) ? 255u : 0u;
                } else {
                    const int px_x = x + static_cast<int>(i);
                    const std::uint8_t dither = kBayer4[y & 3][px_x & 3];
                    const int strength = static_cast<int>(display.gray2_strength);
                    const std::uint8_t mapped = gray2_lut[static_cast<std::size_t>(lum)];
                    int lum_d = static_cast<int>(mapped) + (static_cast<int>(dither) - 8) * strength;
                    if (lum_d < 0) lum_d = 0;
                    if (lum_d > 255) lum_d = 255;
                    const std::uint8_t level = static_cast<std::uint8_t>((lum_d * 4) >> 8);
                    v = static_cast<std::uint8_t>(level * 85u);
                }
                write_gray(d, v);
                s += src_bpp;
                d += bpp;
            }
        }

        void decide_refresh(int dirty_area, int screen_area, std::uint32_t now_ms) noexcept {
            if (dirty_area <= 0 || screen_area <= 0) {
                last_refresh = RefreshKind::None;
                last_dirty_pct = 0;
                return;
            }
            const int dirty_pct = static_cast<int>(
                (static_cast<std::uint64_t>(dirty_area) * 100u)
                / static_cast<std::uint64_t>(screen_area));
            last_dirty_pct = dirty_pct;
            const bool want_full = dirty_pct >= eink_policy.partial_area_ratio_pct
                || partial_count >= eink_policy.max_partial_count;
            const std::uint32_t since_full = now_ms - last_full_ms;
            if (want_full && since_full >= static_cast<std::uint32_t>(eink_policy.min_full_interval_ms)) {
                last_refresh = RefreshKind::Full;
                partial_count = 0;
                last_full_ms = now_ms;
                ++total_full_count;
            } else {
                last_refresh = RefreshKind::Partial;
                ++partial_count;
                ++total_partial_count;
            }
        }

        const char* last_refresh_name() const noexcept {
            switch (last_refresh) {
            case RefreshKind::Partial: return "partial";
            case RefreshKind::Full: return "full";
            default: return "none";
            }
        }

        const char* display_mode_name() const noexcept {
            return ui::gfx::display_mode_name(display.mode);
        }

        const char* gray2_curve_name() const noexcept {
            return ui::gfx::gray2_curve_name(display.gray2_curve);
        }

        void mark_dirty(int x, int y, int w, int h) noexcept {
            if (w <= 0 || h <= 0) return;
            const int left = x;
            const int top = y;
            const int right = x + w;
            const int bottom = y + h;
            if (!dirty_set) {
                dirty_set = true;
                dirty_left = left;
                dirty_top = top;
                dirty_right = right;
                dirty_bottom = bottom;
                return;
            }
            if (left < dirty_left) dirty_left = left;
            if (top < dirty_top) dirty_top = top;
            if (right > dirty_right) dirty_right = right;
            if (bottom > dirty_bottom) dirty_bottom = bottom;
        }

        bool dirty_rect(Rect& out) const noexcept {
            if (!dirty_set) return false;
            out = Rect{dirty_left, dirty_top, dirty_right - dirty_left, dirty_bottom - dirty_top};
            return out.w > 0 && out.h > 0;
        }
    };

    void append_display_overlay(ui::draw_cmd::DefaultDrawCmdBuffer& buf,
                                const SdlTileBackend& backend) noexcept {
        char label[96]{};
        Rect panel{8, 8, 160, 18};
        if (backend.display.mode == ui::gfx::DisplayMode::BW1) {
            const auto threshold = static_cast<unsigned>(backend.display.bw1_threshold);
            (void)std::snprintf(label, sizeof(label), "bw1 thr=%u", threshold);
        } else if (backend.display.mode == ui::gfx::DisplayMode::Gray2) {
            const auto strength = static_cast<unsigned>(backend.display.gray2_strength);
            (void)std::snprintf(label, sizeof(label), "gray2 str=%u curve=%s",
                                strength, backend.gray2_curve_name());
            panel.w = 200;
        } else if (backend.display.mode == ui::gfx::DisplayMode::Eink) {
            const auto ratio = static_cast<unsigned>(backend.eink_policy.partial_area_ratio_pct);
            const auto max_partial = static_cast<unsigned>(backend.eink_policy.max_partial_count);
            const auto min_full = static_cast<unsigned>(backend.eink_policy.min_full_interval_ms);
            (void)std::snprintf(label, sizeof(label), "eink pct=%u max=%u min=%ums",
                                ratio, max_partial, min_full);
            panel.w = 220;
        } else {
            return;
        }
        buf.fill_round_rect(panel, 4, kDemoPanel);
        buf.stroke_round_rect(panel, 4, kDemoPanelBorder);
        buf.draw_text_box(panel, label, kDemoPath,
                          get_font(FontId::Normal),
                          TextAlignH::Left, TextAlignV::Center,
                          TextWrap::None, TextEllipsis::None);
    }

    void append_perf_overlay(ui::draw_cmd::DefaultDrawCmdBuffer& buf,
                             int screen_w,
                             const ui::draw_cmd::DrawCmdTileStats& tile_stats,
                             const ui::draw_cmd::DrawCmdExecStats& exec_stats,
                             bool use_tiles,
                             bool has_stats) noexcept {
        if (!has_stats) return;
        const std::size_t group_rect = use_tiles ? tile_stats.group_rect : exec_stats.group_rect;
        const std::size_t group_text = use_tiles ? tile_stats.group_text : exec_stats.group_text;
        const std::size_t group_image = use_tiles ? tile_stats.group_image : exec_stats.group_image;
        const std::size_t group_line = use_tiles ? tile_stats.group_line : exec_stats.group_line;
        const std::size_t group_path = use_tiles ? tile_stats.group_path : exec_stats.group_path;
        const std::size_t group_other = use_tiles ? tile_stats.group_other : exec_stats.group_other;
        const std::size_t cmd_rect = use_tiles ? tile_stats.cmd_rect : exec_stats.cmd_rect;
        const std::size_t cmd_text = use_tiles ? tile_stats.cmd_text : exec_stats.cmd_text;
        const std::size_t cmd_image = use_tiles ? tile_stats.cmd_image : exec_stats.cmd_image;
        const std::size_t cmd_line = use_tiles ? tile_stats.cmd_line : exec_stats.cmd_line;
        const std::size_t cmd_path = use_tiles ? tile_stats.cmd_path : exec_stats.cmd_path;
        const std::size_t cmd_other = use_tiles ? tile_stats.cmd_other : exec_stats.cmd_other;
        const std::size_t dispatch_groups = use_tiles ? tile_stats.dispatch_groups : exec_stats.dispatch_groups;
        const std::size_t batch_flushes = use_tiles ? tile_stats.batch_flushes : exec_stats.batch_flushes;
        const std::size_t failed_cmds = use_tiles ? tile_stats.failed_cmds : exec_stats.failed_cmds;

        const Font& font = get_font(FontId::Normal);
        const int line_h = (font.line_height > 0) ? font.line_height : 12;
        const int padding = 6;
        const int lines = 3;
        const int panel_w = 320;
        const int panel_h = padding * 2 + line_h * lines;
        const int panel_x = (screen_w > (panel_w + 8)) ? (screen_w - panel_w - 8) : 8;
        const int panel_y = 8;
        Rect panel{panel_x, panel_y, panel_w, panel_h};

        buf.fill_round_rect(panel, 4, kDemoPanel);
        buf.stroke_round_rect(panel, 4, kDemoPanelBorder);

        char line[96]{};
        Rect line_rect{panel_x + padding, panel_y + padding, panel_w - padding * 2, line_h};
        (void)std::snprintf(line, sizeof(line),
                            "grp r/t/i/l/p/o: %zu/%zu/%zu/%zu/%zu/%zu",
                            group_rect, group_text, group_image, group_line, group_path, group_other);
        buf.draw_text_box(line_rect, line, kDemoPath, font,
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
        line_rect.y += line_h;
        (void)std::snprintf(line, sizeof(line),
                            "cmd r/t/i/l/p/o: %zu/%zu/%zu/%zu/%zu/%zu",
                            cmd_rect, cmd_text, cmd_image, cmd_line, cmd_path, cmd_other);
        buf.draw_text_box(line_rect, line, kDemoPath, font,
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
        line_rect.y += line_h;
        (void)std::snprintf(line, sizeof(line),
                            "dispatch/batch/failed: %zu/%zu/%zu",
                            dispatch_groups, batch_flushes, failed_cmds);
        buf.draw_text_box(line_rect, line, kDemoPath, font,
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
    }

    constexpr std::uint32_t vcmd_magic() noexcept {
        return static_cast<std::uint32_t>('V')
            | (static_cast<std::uint32_t>('C') << 8)
            | (static_cast<std::uint32_t>('M') << 16)
            | (static_cast<std::uint32_t>('D') << 24);
    }

    constexpr std::uint32_t kVcmdVersion = 4;
    constexpr std::uint32_t kVcmdEndian = 0x01020304u;
    constexpr std::uint32_t kVcmdFlagHasImages = 1u << 0;
    constexpr std::uint32_t kVcmdFlagHasDisplayConfig = 1u << 1;
    constexpr std::uint32_t kVcmdFlagCmdCountOptional = 1u << 2;

    struct VcmdHeader {
        std::uint32_t magic{vcmd_magic()};
        std::uint32_t version{kVcmdVersion};
        std::uint32_t flags{0};
        std::uint32_t endian{kVcmdEndian};
        std::uint32_t screen_w{0};
        std::uint32_t screen_h{0};
        std::uint32_t pixel_format{0};
        std::uint32_t cmd_struct_size{0};
        std::uint32_t cmd_struct_version{0};
        std::uint32_t cmd_count{0};
        std::uint32_t cmd_bytes{0};
        std::uint32_t text_bytes{0};
        std::uint32_t blob_bytes{0};
        std::uint32_t image_count{0};
        std::uint32_t font_count{0};
    };

    struct VcmdImageHeader {
        std::uint16_t slot{0xFFFF};
        std::uint16_t generation{0};
        std::uint16_t w{0};
        std::uint16_t h{0};
        std::uint32_t stride_bytes{0};
        std::uint32_t format{0};
        std::uint8_t premultiplied{0};
        std::uint8_t force_opaque{0};
        std::uint16_t reserved{0};
        std::uint32_t data_bytes{0};
    };

    struct VcmdDisplayConfig {
        std::uint8_t display_mode{0};
        std::uint8_t bw1_threshold{128};
        std::uint8_t gray2_strength{8};
        std::uint8_t gray2_curve{0};
        std::int32_t eink_max_partial{20};
        std::int32_t eink_min_full_ms{30000};
        std::int32_t eink_partial_ratio_pct{35};
        std::int32_t reserved2{0};
    };

    bool write_block(std::FILE* file, const void* data, std::size_t bytes) noexcept {
        if (!file) return false;
        if (bytes == 0) return true;
        return std::fwrite(data, 1, bytes, file) == bytes;
    }

    bool read_block(std::FILE* file, void* data, std::size_t bytes) noexcept {
        if (!file) return false;
        if (bytes == 0) return true;
        return std::fread(data, 1, bytes, file) == bytes;
    }

    bool dump_cmd_file(const char* path,
                       const ui::draw_cmd::DefaultDrawCmdBuffer& buf,
                       const SdlTileBackend* backend) noexcept {
        if (!path || !path[0]) return false;
        std::FILE* file = std::fopen(path, "wb");
        if (!file) return false;
        const auto stats = buf.stats();

        std::vector<VcmdImageHeader> image_headers;
        std::vector<std::vector<std::byte>> image_bytes;
        const std::size_t cap = ui::draw_cmd::image_registry_capacity();
        for (std::size_t i = 0; i < cap; ++i) {
            ui::draw_cmd::ImageRegistryEntry entry{};
            if (!ui::draw_cmd::image_registry_entry(i, entry)) continue;
            const ImageView& view = entry.view;
            if (!view) continue;
            const std::uint32_t data_bytes =
                static_cast<std::uint32_t>(view.stride_bytes * view.h);
            VcmdImageHeader header{};
            header.slot = entry.id.slot;
            header.generation = entry.id.generation;
            header.w = static_cast<std::uint16_t>(view.w);
            header.h = static_cast<std::uint16_t>(view.h);
            header.stride_bytes = static_cast<std::uint32_t>(view.stride_bytes);
            header.format = static_cast<std::uint32_t>(view.format);
            header.premultiplied = view.premultiplied_alpha ? 1u : 0u;
            header.force_opaque = view.force_opaque ? 1u : 0u;
            header.data_bytes = data_bytes;
            image_headers.push_back(header);
            image_bytes.emplace_back(view.data, view.data + data_bytes);
        }

        VcmdHeader header{};
        header.flags = image_headers.empty() ? 0u : kVcmdFlagHasImages;
        header.screen_w = static_cast<std::uint32_t>(screen_width);
        header.screen_h = static_cast<std::uint32_t>(screen_height);
        header.pixel_format = static_cast<std::uint32_t>(screen_pixel_format);
        header.cmd_struct_size = ui::draw_cmd::draw_cmd_binary_size();
        header.cmd_struct_version = ui::draw_cmd::kDrawCmdBinaryVersion;
        header.cmd_count = 0;
        header.cmd_bytes = static_cast<std::uint32_t>(stats.cmd_bytes);
        header.text_bytes = static_cast<std::uint32_t>(stats.text_used);
        header.blob_bytes = static_cast<std::uint32_t>(stats.blob_used);
        header.image_count = static_cast<std::uint32_t>(image_headers.size());
        header.font_count = 0;
        header.flags |= kVcmdFlagCmdCountOptional;

        VcmdDisplayConfig config{};
        if (backend) {
            config.display_mode = static_cast<std::uint8_t>(backend->display.mode);
            config.bw1_threshold = backend->display.bw1_threshold;
            config.gray2_strength = backend->display.gray2_strength;
            config.gray2_curve = static_cast<std::uint8_t>(backend->display.gray2_curve);
            config.eink_max_partial = backend->eink_policy.max_partial_count;
            config.eink_min_full_ms = backend->eink_policy.min_full_interval_ms;
            config.eink_partial_ratio_pct = backend->eink_policy.partial_area_ratio_pct;
            header.flags |= kVcmdFlagHasDisplayConfig;
        }

        bool ok = write_block(file, &header, sizeof(header));
        if (ok && (header.flags & kVcmdFlagHasDisplayConfig) != 0u) {
            ok = write_block(file, &config, sizeof(config));
        }
        ok = ok && write_block(file, buf.cmd_data(), header.cmd_bytes);
        ok = ok && write_block(file, buf.text_data(), header.text_bytes);
        ok = ok && write_block(file, buf.blob_data(), header.blob_bytes);
        for (std::size_t i = 0; ok && i < image_headers.size(); ++i) {
            ok = ok && write_block(file, &image_headers[i], sizeof(VcmdImageHeader));
            ok = ok && write_block(file, image_bytes[i].data(), image_bytes[i].size());
        }

        std::fclose(file);
        return ok;
    }

    bool replay_cmd_file(const char* path,
                         DefaultFrameBuffer& fb,
                         DefaultCanvas& canvas,
                         SdlTileBackend& tile_backend,
                         const FrameBufferView& tile_view,
                         const ui::draw_cmd::DrawCmdTileConfig& tile_config,
                         bool use_tiles,
                         std::uint32_t* out_hash = nullptr) noexcept {
        if (!path || !path[0]) return false;
        std::FILE* file = std::fopen(path, "rb");
        if (!file) return false;

        VcmdHeader header{};
        if (!read_block(file, &header, sizeof(header))) {
            std::fclose(file);
            return false;
        }
        if (header.magic != vcmd_magic()
            || (header.version != 2 && header.version != 3 && header.version != kVcmdVersion)) {
            std::fclose(file);
            return false;
        }
        if (header.endian != kVcmdEndian) {
            std::fclose(file);
            return false;
        }
        if (header.screen_w != static_cast<std::uint32_t>(screen_width)
            || header.screen_h != static_cast<std::uint32_t>(screen_height)) {
            std::fclose(file);
            return false;
        }
        if (header.pixel_format != static_cast<std::uint32_t>(screen_pixel_format)) {
            std::fclose(file);
            return false;
        }
        if (header.cmd_bytes != 0
            && header.cmd_struct_size != ui::draw_cmd::draw_cmd_binary_size()
            && header.version >= kVcmdVersion) {
            std::fclose(file);
            return false;
        }
        if ((header.flags & kVcmdFlagHasDisplayConfig) != 0u) {
            VcmdDisplayConfig config{};
            if (!read_block(file, &config, sizeof(config))) {
                std::fclose(file);
                return false;
            }
            tile_backend.display.mode = static_cast<ui::gfx::DisplayMode>(config.display_mode);
            tile_backend.display.bw1_threshold = config.bw1_threshold;
            tile_backend.display.gray2_strength = config.gray2_strength;
            tile_backend.display.gray2_curve = static_cast<ui::gfx::Gray2Curve>(config.gray2_curve);
            tile_backend.eink_policy.max_partial_count = config.eink_max_partial;
            tile_backend.eink_policy.min_full_interval_ms = config.eink_min_full_ms;
            tile_backend.eink_policy.partial_area_ratio_pct = config.eink_partial_ratio_pct;
        }
        std::vector<std::byte> cmd_bytes(header.cmd_bytes);
        std::vector<char> text(header.text_bytes);
        std::vector<std::byte> blob(header.blob_bytes);

        if (!read_block(file, cmd_bytes.data(), header.cmd_bytes)) {
            std::fclose(file);
            return false;
        }
        if (!read_block(file, text.data(), header.text_bytes)) {
            std::fclose(file);
            return false;
        }
        if (!read_block(file, blob.data(), header.blob_bytes)) {
            std::fclose(file);
            return false;
        }

        ui::draw_cmd::clear_image_registry();
        static std::vector<std::vector<std::byte>> g_image_store;
        g_image_store.clear();
        for (std::uint32_t i = 0; i < header.image_count; ++i) {
            VcmdImageHeader img{};
            if (!read_block(file, &img, sizeof(img))) {
                std::fclose(file);
                return false;
            }
            std::vector<std::byte> pixels(img.data_bytes);
            if (!read_block(file, pixels.data(), pixels.size())) {
                std::fclose(file);
                return false;
            }
            g_image_store.emplace_back(std::move(pixels));
            const auto& stored = g_image_store.back();
            ImageView view = make_image_view(
                static_cast<PixelFormat>(img.format),
                img.w,
                img.h,
                static_cast<int>(img.stride_bytes),
                stored.data(),
                img.premultiplied != 0,
                img.force_opaque != 0);
            ui::draw_cmd::ImageId id{img.slot, img.generation};
            const auto reg = ui::draw_cmd::register_image_with_id(
                id,
                view,
                ui::draw_cmd::ImageRegisterReason::DumpReplay,
                "vcmd_replay");
            if (!reg.ok()) {
                std::fclose(file);
                return false;
            }
        }
        if (!ui::draw_cmd::image_registry_locked()) {
            ui::draw_cmd::set_image_registry_locked(true);
        }

        std::fclose(file);

        ui::draw_cmd::DefaultDrawCmdBuffer buf{};
        const std::size_t cmd_count = ((header.flags & kVcmdFlagCmdCountOptional) != 0u)
            ? 0u
            : header.cmd_count;
        if (!buf.load(cmd_bytes.data(),
                      cmd_bytes.size(),
                      cmd_count,
                      text.data(),
                      text.size(),
                      blob.data(),
                      blob.size())) {
            return false;
        }
        ui::draw_cmd::DrawCmd cmd{};
        const std::size_t cmd_bytes_size = buf.cmd_bytes();
        std::size_t offset = 0;
        while (offset < cmd_bytes_size) {
            std::size_t stride = 0;
            if (!buf.read_cmd_at_offset(offset, cmd, stride)) return false;
            const auto blob_span = buf.blob_at(cmd.blob);
            if (cmd.type == ui::draw_cmd::CmdType::DrawTextBox) {
                const std::size_t end = static_cast<std::size_t>(cmd.text.offset) + cmd.text.length;
                if (end > text.size()) {
                    return false;
                }
            }
            if (cmd.type == ui::draw_cmd::CmdType::DrawPath) {
                const std::size_t end = static_cast<std::size_t>(cmd.blob.offset) + cmd.blob.length;
                if (end > blob.size()) {
                    return false;
                }
            }
            if ((cmd.type == ui::draw_cmd::CmdType::DrawImage
                || cmd.type == ui::draw_cmd::CmdType::DrawImageNineSlice)
                && !ui::draw_cmd::image_id_valid(cmd.image)) {
                return false;
            }
            switch (cmd.type) {
            case ui::draw_cmd::CmdType::FillRectBatch:
            case ui::draw_cmd::CmdType::StrokeRectBatch: {
                const int count = cmd.p0;
                if (count <= 0) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::RectBatchItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::FillRoundRectBatch:
            case ui::draw_cmd::CmdType::StrokeRoundRectBatch:
            case ui::draw_cmd::CmdType::FillCircleBatch:
            case ui::draw_cmd::CmdType::StrokeCircleBatch: {
                const int count = cmd.p1;
                if (count <= 0) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::RectBatchItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::FocusRingBatch: {
                const int count = cmd.p3;
                if (count <= 0) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::RectBatchItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::DrawLineBatch: {
                const int count = cmd.p0;
                if (count <= 0) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::LineBatchItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::DrawPathBatch: {
                const int count = cmd.p0;
                if (count <= 0) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::PathBatchItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::GlyphRun: {
                const int count = cmd.p0;
                if (count <= 0) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::GlyphRunItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::DrawImageBatch: {
                const int count = cmd.p0;
                if (count <= 0) return false;
                if (!ui::draw_cmd::image_id_valid(cmd.image)) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::ImageBatchItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::DrawImageRoundRectBatch: {
                const int count = cmd.p1;
                if (count <= 0) return false;
                if (!ui::draw_cmd::image_id_valid(cmd.image)) return false;
                if (blob_span.size() < static_cast<std::size_t>(count) * sizeof(ui::draw_cmd::ImageBatchItem)) {
                    return false;
                }
                break;
            }
            case ui::draw_cmd::CmdType::DrawImageNineSliceBatch: {
                if (!ui::draw_cmd::image_id_valid(cmd.image)) return false;
                if (blob_span.empty()) return false;
                if ((blob_span.size() % sizeof(ui::draw_cmd::ImageBatchItem)) != 0) return false;
                break;
            }
            default:
                break;
            }
            offset += stride;
        }

        ui::draw_cmd::DrawCmdExecutor exec{};
        fb.clear(kDemoBg);
        ui::draw_cmd::DrawCmdExecStats exec_stats{};
        if (use_tiles) {
            canvas.begin_frame();
            exec_stats = exec.execute(canvas, buf);
            canvas.end_frame();
            fb.clear(kDemoBg);
            (void)exec.execute_tiles(tile_backend, tile_view, buf, tile_config);
        } else {
            canvas.begin_frame();
            exec_stats = exec.execute(canvas, buf);
            canvas.end_frame();
        }
        if (exec_stats.failed_cmds != 0) return false;
        const std::uint32_t hash = hash_bytes(fb.data(), DefaultFrameBuffer::buffer_bytes);
        if (out_hash) {
            *out_hash = hash;
        }
        (void)out::println<"[soa] replay hash=0x{:08X} backend={}">(
            g_console,
            static_cast<unsigned>(hash),
            use_tiles ? "tile" : "full");
        return true;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    constexpr std::size_t kMaxStyleTableBytes = 11u * 1024u;
    std::FILE* g_regress_log = nullptr;
    bool g_payload_stats_dumped = false;

    const char* event_type_name(Event::Type type) noexcept {
        switch (type) {
            case Event::Type::HoverEnter: return "HoverEnter";
            case Event::Type::HoverLeave: return "HoverLeave";
            case Event::Type::MouseDown: return "MouseDown";
            case Event::Type::MouseUp: return "MouseUp";
            case Event::Type::MouseMove: return "MouseMove";
            case Event::Type::MouseWheel: return "MouseWheel";
            case Event::Type::Click: return "Click";
            case Event::Type::DragStart: return "DragStart";
            case Event::Type::DragMove: return "DragMove";
            case Event::Type::DragEnd: return "DragEnd";
            case Event::Type::GestureSwipe: return "GestureSwipe";
            case Event::Type::GesturePinch: return "GesturePinch";
            case Event::Type::FocusIn: return "FocusIn";
            case Event::Type::FocusOut: return "FocusOut";
            case Event::Type::KeyDown: return "KeyDown";
            case Event::Type::KeyUp: return "KeyUp";
            case Event::Type::Cancel: return "Cancel";
        }
        return "Unknown";
    }

    bool same_handle(WidgetHandle a, WidgetHandle b) noexcept {
        return a.kind == b.kind && a.index == b.index && a.generation == b.generation;
    }

    void dump_payload_stats(const soa_detail::PayloadStats& stats);

    int find_event_index(const SoaKernel& kernel, Event::Type type, WidgetHandle target = {}) noexcept {
        const std::size_t count = kernel.input_event_count();
        for (std::size_t i = 0; i < count; ++i) {
            const auto& item = kernel.input_event(i);
            if (item.event.type != type) continue;
            if (target && !same_handle(item.target, target)) continue;
            return static_cast<int>(i);
        }
        return -1;
    }

    int count_event(const SoaKernel& kernel, Event::Type type, WidgetHandle target = {}) noexcept {
        int total = 0;
        const std::size_t count = kernel.input_event_count();
        for (std::size_t i = 0; i < count; ++i) {
            const auto& item = kernel.input_event(i);
            if (item.event.type != type) continue;
            if (target && !same_handle(item.target, target)) continue;
            ++total;
        }
        return total;
    }

    bool expect_true(bool cond, const char* label, int& fails) noexcept {
        if (cond) return true;
        (void)out::println<"[soa][fail] {}">(g_console, label);
        if (g_regress_log) {
            std::fprintf(g_regress_log, "[soa][fail] %s\n", label);
        }
        ++fails;
        return false;
    }

    struct ScrollbarTestInfo {
        ScrollBarOrientation orient{ScrollBarOrientation::Vertical};
        Rect world{};
        int track_start{0};
        int track_len{0};
        int thumb_start{0};
        int thumb_len{0};
        int max_thumb{0};
        int max_scroll{0};
        int page{0};
    };

    int clamp_int(int v, int lo, int hi) noexcept {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    bool build_scrollbar_info(SoaKernel& kernel, WidgetHandle bar, const Rect& root_r,
        ScrollbarTestInfo& info) noexcept {
        const WidgetHandle target = kernel.scrollbar_target(bar);
        const ScrollBarOrientation orient = kernel.scrollbar_orientation(bar);
        const Rect r = kernel.rect(bar);
        Rect world{root_r.x + r.x, root_r.y + r.y, r.w, r.h};

        const StyleState state = make_style_state(true, false, false, false);
        const ResolvedStyleView view = StyleSheet::instance().lookup(WidgetKind::ScrollBar, state);
        const ResolvedMetrics* metrics = view.metrics;
        int margin = metrics ? metrics->scrollbar_margin : 0;
        if (margin < 0) margin = 0;
        int track_len = (orient == ScrollBarOrientation::Vertical)
            ? (world.h - margin * 2)
            : (world.w - margin * 2);
        if (track_len <= 0) return false;

        int max_scroll = 0;
        if (target) {
            max_scroll = (orient == ScrollBarOrientation::Vertical)
                ? kernel.max_scroll(target)
                : kernel.max_scroll_x(target);
        }
        if (max_scroll < 0) max_scroll = 0;

        int page = kernel.scrollbar_page_size(bar);
        if (page <= 0) {
            if (target) {
                const Rect tr = kernel.rect(target);
                page = (orient == ScrollBarOrientation::Vertical) ? tr.h : tr.w;
            } else {
                page = (orient == ScrollBarOrientation::Vertical) ? world.h : world.w;
            }
        }
        if (page <= 0) page = 1;

        int thumb_min = metrics ? metrics->scrollbar_thumb_min : 0;
        if (thumb_min <= 0) thumb_min = 12;
        int content_len = page + max_scroll;
        if (content_len <= 0) content_len = track_len;
        int thumb_len = (track_len * page) / content_len;
        if (thumb_len < thumb_min) thumb_len = thumb_min;
        if (thumb_len > track_len) thumb_len = track_len;
        int max_thumb = track_len - thumb_len;

        int scroll = 0;
        if (target) {
            scroll = (orient == ScrollBarOrientation::Vertical)
                ? kernel.scroll_y(target)
                : kernel.table_view_scroll_x(target);
        }
        scroll = clamp_int(scroll, 0, max_scroll);
        const int track_start = (orient == ScrollBarOrientation::Vertical)
            ? (world.y + margin)
            : (world.x + margin);
        const int thumb_start = track_start
            + ((max_scroll > 0 && max_thumb > 0) ? (max_thumb * scroll) / max_scroll : 0);

        info.orient = orient;
        info.world = world;
        info.track_start = track_start;
        info.track_len = track_len;
        info.thumb_start = thumb_start;
        info.thumb_len = thumb_len;
        info.max_thumb = max_thumb;
        info.max_scroll = max_scroll;
        info.page = page;
        return true;
    }

    void trace_input_events(SoaKernel& kernel) noexcept {
        const std::size_t count = kernel.input_event_count();
        if (count == 0 && !kernel.input_events_overflowed()) return;
        if (kernel.input_events_overflowed()) {
            (void)out::println<"[soa] input overflow">(g_console);
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto& item = kernel.input_event(i);
            (void)out::println<"[soa] ev: kind={} idx={} gen={} type={} x={} y={} dx={} dy={}">(
                g_console,
                widget_kind_name(item.target.kind),
                static_cast<int>(item.target.index),
                static_cast<int>(item.target.generation),
                event_type_name(item.event.type),
                item.event.x,
                item.event.y,
                item.event.dx,
                item.event.dy
            );
        }
    }

    bool run_input_regression(SoaGui& gui, SoaKernel& kernel, SoaFactory& factory, WidgetHandle root) noexcept {
        int fails = 0;
        kernel.input_clear_events();
#if defined(VIVID_SOA_TRACE_INPUT)
        const std::uint32_t guard_before = kernel.input_guard_state_write_violations();
#endif

        auto test_root = factory.create_container();
        auto sc = factory.create_scroll_container();
        factory.link(root, test_root);
        factory.link(test_root, sc);
        kernel.set_rect(test_root, {40, 40, 220, 180});
        kernel.set_rect(sc, {10, 10, 160, 100});

        const int x = 60;
        const int y = 60;
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, x, y, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, x + 20, y + 20, 0));

        expect_true(kernel.input_dragging(), "regress: dragging not started", fails);

        kernel.input_clear_events();
        kernel.destroy(test_root);

        const int idx_drag_end = find_event_index(kernel, Event::Type::DragEnd);
        const int idx_cancel = find_event_index(kernel, Event::Type::Cancel);
        const int idx_hover = find_event_index(kernel, Event::Type::HoverLeave);
        const int idx_focus = find_event_index(kernel, Event::Type::FocusOut);
        expect_true(idx_drag_end >= 0, "regress: destroy missing DragEnd", fails);
        expect_true(idx_cancel >= 0, "regress: destroy missing Cancel", fails);
        expect_true(idx_hover >= 0, "regress: destroy missing HoverLeave", fails);
        expect_true(idx_focus >= 0, "regress: destroy missing FocusOut", fails);
        if (idx_drag_end >= 0 && idx_cancel >= 0) {
            expect_true(idx_drag_end < idx_cancel, "regress: DragEnd order", fails);
        }
        if (idx_cancel >= 0 && idx_hover >= 0) {
            expect_true(idx_cancel < idx_hover, "regress: Cancel order", fails);
        }
        if (idx_hover >= 0 && idx_focus >= 0) {
            expect_true(idx_hover < idx_focus, "regress: HoverLeave order", fails);
        }

        expect_true(!kernel.input_pressed(), "regress: pressed not cleared", fails);
        expect_true(!kernel.input_captured(), "regress: captured not cleared", fails);
        expect_true(!kernel.input_hovered(), "regress: hovered not cleared", fails);
        expect_true(!kernel.input_focused(), "regress: focused not cleared", fails);
        expect_true(!kernel.input_dragging(), "regress: dragging not cleared", fails);

        kernel.destroy(sc);
        kernel.destroy(test_root);

        auto test_root2 = factory.create_container();
        auto a = factory.create_button("A");
        auto b = factory.create_checkbox("B");
        factory.link(root, test_root2);
        factory.link(test_root2, a);
        factory.link(test_root2, b);
        kernel.set_rect(test_root2, {300, 40, 220, 120});
        kernel.set_rect(a, {10, 10, 120, 32});
        kernel.set_rect(b, {10, 50, 120, 32});

        kernel.input_clear_events();
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 320, 60, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 320, 60, 1));
        expect_true(count_event(kernel, Event::Type::MouseDown, a) > 0,
                    "regress: click MouseDown(A) missing", fails);
        expect_true(static_cast<bool>(kernel.input_pressed()), "regress: click pressed missing", fails);
        expect_true(!kernel.input_captured(), "regress: click captured unexpectedly", fails);
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 320, 60, 1));
        expect_true(count_event(kernel, Event::Type::MouseUp, a) > 0,
                    "regress: click MouseUp(A) missing", fails);
        expect_true(!kernel.input_pressed(), "regress: click pressed not cleared", fails);
        expect_true(!kernel.input_captured(), "regress: click captured not cleared", fails);
        expect_true(!kernel.input_dragging(), "regress: click dragging unexpectedly", fails);
        expect_true(count_event(kernel, Event::Type::Cancel, a) == 0,
                    "regress: click emitted Cancel(A)", fails);

        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 320, 60, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 320, 60, 1));
        kernel.input_test_request_capture(b);

        kernel.input_clear_events();
        kernel.destroy(b);
        const int cancel_b = count_event(kernel, Event::Type::Cancel, b);
        expect_true(cancel_b > 0, "regress: Cancel(B) missing", fails);
        expect_true(!kernel.input_captured(), "regress: captured not cleared", fails);

        kernel.input_clear_events();
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 320, 60, 1));

        kernel.destroy(a);
        kernel.destroy(test_root2);
        expect_true(!kernel.input_pressed(), "regress: capture pressed not cleared", fails);
        expect_true(!kernel.input_captured(), "regress: capture captured not cleared", fails);
        expect_true(!kernel.input_hovered(), "regress: capture hovered not cleared", fails);
        expect_true(!kernel.input_focused(), "regress: capture focused not cleared", fails);
        expect_true(!kernel.input_dragging(), "regress: capture dragging not cleared", fails);

        auto test_root3 = factory.create_container();
        auto c = factory.create_checkbox("C");
        factory.link(root, test_root3);
        factory.link(test_root3, c);
        kernel.set_rect(test_root3, {560, 40, 200, 120});
        kernel.set_rect(c, {10, 10, 120, 32});

        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 580, 60, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 580, 60, 1));
        kernel.input_test_force_overflow();
        expect_true(kernel.input_events_overflowed(), "regress: overflow flag missing", fails);
        expect_true(!kernel.input_pressed(), "regress: overflow pressed not cleared", fails);
        expect_true(!kernel.input_captured(), "regress: overflow captured not cleared", fails);
        expect_true(!kernel.input_hovered(), "regress: overflow hovered not cleared", fails);
        expect_true(!kernel.input_focused(), "regress: overflow focused not cleared", fails);
        expect_true(!kernel.input_dragging(), "regress: overflow dragging not cleared", fails);

        kernel.destroy(c);
        kernel.destroy(test_root3);

        {
            struct MenuData {
                static std::uint16_t count(const void* ctx, int menu_id) noexcept {
                    (void)ctx;
                    if (menu_id == 0) return 2;
                    if (menu_id == 1) return 2;
                    return 0;
                }
                static const char* label(const void* ctx, int menu_id, std::uint16_t index) noexcept {
                    (void)ctx;
                    static const char* root_items[] = {"File", "Edit"};
                    static const char* file_items[] = {"Open", "Exit"};
                    if (menu_id == 0 && index < 2) return root_items[index];
                    if (menu_id == 1 && index < 2) return file_items[index];
                    return "";
                }
                static bool has_children(const void* ctx, int menu_id, std::uint16_t index) noexcept {
                    (void)ctx;
                    return menu_id == 0 && index == 0;
                }
                static int child_menu(const void* ctx, int menu_id, std::uint16_t index) noexcept {
                    (void)ctx;
                    return (menu_id == 0 && index == 0) ? 1 : -1;
                }
            };
            struct MenuSelection {
                int root_sel{0};
                int file_sel{0};
                static int get_selected(const void* ctx, int menu_id) noexcept {
                    auto* self = static_cast<const MenuSelection*>(ctx);
                    if (!self) return -1;
                    return (menu_id == 0) ? self->root_sel : self->file_sel;
                }
                static void set_selected(const void* ctx, int menu_id, int index) noexcept {
                    auto* self = static_cast<MenuSelection*>(const_cast<void*>(ctx));
                    if (!self) return;
                    if (menu_id == 0) self->root_sel = index;
                    else self->file_sel = index;
                }
            };

            MenuSelection menu_sel{};
            MenuTree menu{};
            auto menu_root = factory.create_container();
            auto menu_host = factory.create_button("Menu");
            factory.link(root, menu_root);
            factory.link(menu_root, menu_host);
            kernel.set_rect(menu_root, {760, 40, 200, 200});
            kernel.set_rect(menu_host, {10, 10, 80, 24});

            menu.init(factory, menu_root);
            menu.set_root_menu(0);
            menu.set_provider(MenuTree::MenuProvider{
                &menu_sel,
                &MenuData::count,
                &MenuData::label,
                nullptr,
                &MenuData::has_children,
                &MenuData::child_menu
            });
            menu.set_selection_model(MenuTree::MenuSelectionModel{
                &menu_sel,
                &MenuSelection::get_selected,
                &MenuSelection::set_selected,
                nullptr
            });

            menu.open(menu_host);
            kernel.input_test_request_capture(menu.panel_handle());

            const int menu_x = 780;
            const int menu_y = 74;
            menu.handle_event(Event::mouse(Event::Type::MouseMove, menu_x, menu_y, 0));
            menu.handle_event(Event::mouse(Event::Type::MouseDown, 20, 20, 1));
            menu.handle_event(Event::mouse(Event::Type::Click, 20, 20, 1));

            expect_true(!menu.is_open(), "regress: menu_tree not closed", fails);
            expect_true(!kernel.input_captured(), "regress: menu_tree captured not cleared", fails);
            expect_true(!kernel.input_pressed(), "regress: menu_tree pressed not cleared", fails);

            kernel.destroy(menu_host);
            kernel.destroy(menu_root);
        }

#if defined(VIVID_SOA_TRACE_INPUT)
        const std::uint32_t guard_after = kernel.input_guard_state_write_violations();
        expect_true(guard_after == guard_before, "regress: input state write guard tripped", fails);
        if (g_regress_log) {
            std::fprintf(g_regress_log, "[soa] input_guard_state_write_violations=%u\n",
                         static_cast<unsigned>(guard_after - guard_before));
        }
#endif

        expect_true(!kernel.payload_overflowed(), "regress: payload pool overflowed", fails);
        if (g_regress_log) {
            std::fprintf(g_regress_log, "[soa] payload_overflowed=%u\n",
                         kernel.payload_overflowed() ? 1u : 0u);
        }
        expect_true(!kernel.text_overflowed(), "regress: text arena overflowed", fails);
        if (g_regress_log) {
            std::fprintf(g_regress_log, "[soa] text_overflowed=%u\n",
                         kernel.text_overflowed() ? 1u : 0u);
        }

        const auto payload_stats = kernel.payload_stats();
        const std::uint32_t total_peak =
            payload_stats.label.peak
            + payload_stats.button.peak
            + payload_stats.image.peak
            + payload_stats.checkbox.peak
            + payload_stats.radio.peak
            + payload_stats.list_item.peak
            + payload_stats.text_list.peak
            + payload_stats.list_view.peak
            + payload_stats.table_view.peak
            + payload_stats.tree_view.peak
            + payload_stats.stepper.peak
            + payload_stats.number_list.peak
            + payload_stats.roller.peak
            + payload_stats.switcher.peak
            + payload_stats.slider.peak
            + payload_stats.progress.peak
            + payload_stats.scrollbar.peak
            + payload_stats.list.peak
            + payload_stats.scroll_container.peak
            + payload_stats.spinner.peak;
        const std::uint32_t total_fail =
            payload_stats.label.alloc_fail
            + payload_stats.button.alloc_fail
            + payload_stats.image.alloc_fail
            + payload_stats.checkbox.alloc_fail
            + payload_stats.radio.alloc_fail
            + payload_stats.list_item.alloc_fail
            + payload_stats.text_list.alloc_fail
            + payload_stats.list_view.alloc_fail
            + payload_stats.table_view.alloc_fail
            + payload_stats.tree_view.alloc_fail
            + payload_stats.stepper.alloc_fail
            + payload_stats.number_list.alloc_fail
            + payload_stats.roller.alloc_fail
            + payload_stats.switcher.alloc_fail
            + payload_stats.slider.alloc_fail
            + payload_stats.progress.alloc_fail
            + payload_stats.scrollbar.alloc_fail
            + payload_stats.list.alloc_fail
            + payload_stats.scroll_container.alloc_fail
            + payload_stats.spinner.alloc_fail;
        expect_true(total_peak > 0, "regress: payload peak all zero", fails);
        expect_true(total_fail == 0, "regress: payload alloc failed", fails);
        dump_payload_stats(payload_stats);

        if (fails == 0) {
            (void)out::println<"[soa] input regression OK">(g_console);
        }
        return fails == 0;
    }

    bool run_layout_regression(SoaGui& gui, SoaKernel& kernel, SoaFactory& factory, WidgetHandle root) noexcept {
        int fails = 0;

        auto layout_root = factory.create_container();
        auto layout_box = factory.create_checkbox("Layout");
        factory.link(root, layout_root);
        factory.link(layout_root, layout_box);
        kernel.set_rect(layout_root, {40, 260, 220, 120});
        kernel.set_rect(layout_box, {10, 10, 180, 32});

        gui.render();

        kernel.set_layout_state_influence(false);
        gui.render();
        kernel.layout_trace_reset();

        gui.dispatch_event(Event::mouse(Event::Type::Cancel, 0, 0, 0));
        gui.render();
        kernel.layout_trace_reset();

        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 60, 280, 0));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: hover invalidated with influence off", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: hover pass with influence off", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: hover missing paint invalidation", fails);
        kernel.layout_trace_reset();

        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 60, 280, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 90, 300, 0));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: drag invalidated with influence off", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: drag pass with influence off", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: drag missing paint invalidation", fails);
        kernel.layout_trace_reset();

        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 90, 300, 1));
        gui.dispatch_event(Event::wheel(60, 280, 1));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: wheel invalidated with influence off", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: wheel pass with influence off", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: wheel missing paint invalidation", fails);

        kernel.set_layout_state_influence(true);
        gui.render();
        kernel.set_focused(layout_box, false);
        kernel.layout_trace_reset();

        kernel.set_focused(layout_box, true);
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: focus invalidated but mask forbids", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: focus pass but mask forbids", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: focus missing paint invalidation", fails);

        gui.dispatch_event(Event::mouse(Event::Type::Cancel, 0, 0, 0));
        gui.render();
        kernel.layout_trace_reset();
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 60, 280, 0));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: hover invalidated but mask forbids", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: hover pass but mask forbids", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: hover missing paint invalidation", fails);

        auto scroll_root = factory.create_container();
        auto scroll = factory.create_scroll_container();
        auto scroll_a = factory.create_button("Scroll A");
        auto scroll_b = factory.create_button("Scroll B");
        auto scroll_c = factory.create_button("Scroll C");
        factory.link(root, scroll_root);
        factory.link(scroll_root, scroll);
        factory.link(scroll, scroll_a);
        factory.link(scroll, scroll_b);
        factory.link(scroll, scroll_c);
        kernel.set_rect(scroll_root, {280, 260, 220, 120});
        kernel.set_rect(scroll, {10, 10, 180, 80});
        kernel.set_rect(scroll_a, {0, 0, 160, 30});
        kernel.set_rect(scroll_b, {0, 35, 160, 30});
        kernel.set_rect(scroll_c, {0, 70, 160, 30});
        kernel.set_scroll_step(scroll, 12);
        gui.render();

        kernel.layout_trace_reset();
        const int scroll_before = kernel.scroll_y(scroll);
        gui.dispatch_event(Event::wheel(300, 280, -1));
        gui.render();
        const int scroll_after = kernel.scroll_y(scroll);
        expect_true(scroll_after != scroll_before, "layout: scroll container did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "layout: scroll container invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: scroll container pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: scroll container missing paint invalidation", fails);

        kernel.layout_trace_reset();
        const int drag_before = kernel.scroll_y(scroll);
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 300, 280, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 300, 240, 0));
        gui.render();
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 300, 240, 1));
        const int drag_after = kernel.scroll_y(scroll);
        expect_true(drag_after != drag_before, "layout: scroll drag did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "layout: scroll drag invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: scroll drag pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: scroll drag missing paint invalidation", fails);

        auto list_root = factory.create_container();
        auto list = factory.create_list();
        auto list_item_a = factory.create_list_item("Item A");
        auto list_item_b = factory.create_list_item("Item B");
        auto list_item_c = factory.create_list_item("Item C");
        auto list_item_d = factory.create_list_item("Item D");
        auto list_item_e = factory.create_list_item("Item E");
        auto list_item_f = factory.create_list_item("Item F");
        factory.link(root, list_root);
        factory.link(list_root, list);
        factory.link(list, list_item_a);
        factory.link(list, list_item_b);
        factory.link(list, list_item_c);
        factory.link(list, list_item_d);
        factory.link(list, list_item_e);
        factory.link(list, list_item_f);
        kernel.set_rect(list_root, {40, 420, 220, 120});
        kernel.set_rect(list, {10, 10, 180, 80});
        kernel.set_list_row_height(list, 24);
        kernel.set_scroll_step(list, 12);
        gui.render();

        kernel.layout_trace_reset();
        const int list_before = kernel.scroll_y(list);
        gui.dispatch_event(Event::wheel(60, 440, -1));
        gui.render();
        const int list_after = kernel.scroll_y(list);
        expect_true(list_after != list_before, "layout: list did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "layout: list invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: list pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: list missing paint invalidation", fails);

        kernel.layout_trace_reset();
        const int list_drag_before = kernel.scroll_y(list);
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 60, 440, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 60, 410, 0));
        gui.render();
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 60, 410, 1));
        const int list_drag_after = kernel.scroll_y(list);
        expect_true(list_drag_after != list_drag_before, "layout: list drag did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "layout: list drag invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: list drag pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: list drag missing paint invalidation", fails);

        kernel.layout_trace_reset();
        kernel.set_text(layout_box, "Layout Updated");
        gui.render();
        expect_true(kernel.layout_invalidated_count() > 0, "layout: text change did not invalidate", fails);
        expect_true(kernel.layout_pass_count() > 0, "layout: text change did not run pass", fails);

        kernel.destroy(list_item_f);
        kernel.destroy(list_item_e);
        kernel.destroy(list_item_d);
        kernel.destroy(list_item_c);
        kernel.destroy(list_item_b);
        kernel.destroy(list_item_a);
        kernel.destroy(list);
        kernel.destroy(list_root);
        kernel.destroy(scroll_c);
        kernel.destroy(scroll_b);
        kernel.destroy(scroll_a);
        kernel.destroy(scroll);
        kernel.destroy(scroll_root);
        kernel.destroy(layout_box);
        kernel.destroy(layout_root);

        if (fails == 0) {
            (void)out::println<"[soa] layout regression OK">(g_console);
        }
        return fails == 0;
    }

    bool run_ui_regression(SoaGui& gui, SoaKernel& kernel, SoaFactory& factory, WidgetHandle root) noexcept {
        int fails = 0;
        auto tab_root = factory.create_container();
        auto nav_bar = factory.create_navigation_bar();
        auto tab_bar = factory.create_tab_bar();
        auto menu = factory.create_menu();
        auto menu_item_a = factory.create_menu_item("New");
        auto menu_item_b = factory.create_menu_item("Open");
        auto menu_item_c = factory.create_menu_item("Save");
        auto console_box = factory.create_console_box();
        factory.link(root, tab_root);
        factory.link(tab_root, nav_bar);
        factory.link(tab_root, menu);
        factory.link(root, console_box);
        factory.link(root, tab_bar);
        factory.link(menu, menu_item_a);
        factory.link(menu, menu_item_b);
        factory.link(menu, menu_item_c);

        kernel.set_rect(tab_root, {240, 60, 200, 160});
        kernel.set_rect(nav_bar, {10, 10, 180, 28});
        kernel.set_rect(menu, {10, 48, 180, 80});
        kernel.set_rect(menu_item_a, {0, 0, 180, 24});
        kernel.set_rect(menu_item_b, {0, 28, 180, 24});
        kernel.set_rect(menu_item_c, {0, 56, 180, 24});
        kernel.set_rect(console_box, {10, 420, 180, 56});
        kernel.set_rect(tab_bar, {240, 20, 200, 24});
        factory.set_navigation_bar_label(nav_bar, 0, "Home");
        factory.set_navigation_bar_label(nav_bar, 1, "Stats");
        factory.set_navigation_bar_label(nav_bar, 2, "Setup");
        factory.set_navigation_bar_selected(nav_bar, 0);
        factory.set_tab_bar_label(tab_bar, 0, "Home");
        factory.set_tab_bar_label(tab_bar, 1, "Stats");
        factory.set_tab_bar_label(tab_bar, 2, "Setup");
        factory.set_tab_bar_selected(tab_bar, 0);
        kernel.set_checked(menu_item_a, true);
        auto progress = factory.create_progress();
        auto progress_wheel = factory.create_progress_wheel();
        auto progress_simple = factory.create_progress_bar_simple();
        auto progress_round = factory.create_progress_bar_round();
        auto progress_flowing = factory.create_progress_flowing();
        auto stepper = factory.create_stepper();
        auto number_list = factory.create_number_list();
        auto roller = factory.create_roller();
        factory.link(tab_root, progress);
        factory.link(tab_root, progress_wheel);
        factory.link(tab_root, progress_simple);
        factory.link(tab_root, progress_round);
        factory.link(tab_root, progress_flowing);
        factory.link(root, stepper);
        factory.link(root, number_list);
        factory.link(root, roller);
        kernel.set_rect(progress, {10, 110, 180, 12});
        kernel.set_rect(progress_wheel, {150, 78, 32, 32});
        kernel.set_rect(progress_simple, {10, 130, 180, 8});
        kernel.set_rect(progress_round, {10, 142, 180, 8});
        kernel.set_rect(progress_flowing, {10, 152, 180, 6});
        kernel.set_rect(stepper, {20, 340, 200, 44});
        kernel.set_rect(number_list, {20, 388, 90, 92});
        kernel.set_rect(roller, {120, 388, 90, 92});
        kernel.set_range(progress, 0, 100);
        kernel.set_value(progress, 10);
        kernel.set_range(progress_wheel, 0, 100);
        kernel.set_value(progress_wheel, 20);
        kernel.set_range(progress_simple, 0, 100);
        kernel.set_value(progress_simple, 30);
        kernel.set_range(progress_round, 0, 100);
        kernel.set_value(progress_round, 35);
        kernel.set_range(progress_flowing, 0, 100);
        kernel.set_value(progress_flowing, 45);
        factory.set_stepper_count(stepper, 4);
        factory.set_stepper_label(stepper, 0, "Low");
        factory.set_stepper_label(stepper, 1, "Med");
        factory.set_stepper_label(stepper, 2, "High");
        factory.set_stepper_label(stepper, 3, "Max");
        factory.set_stepper_current(stepper, 0);
        factory.set_number_list_count(number_list, 10);
        factory.set_number_list_range(number_list, 0, 1);
        factory.set_number_list_selected(number_list, 0);
        factory.set_number_list_row_height(number_list, 22);
        factory.set_number_list_wheel_step(number_list, 22);
        static const char* roller_items[] = {
            "One", "Two", "Three", "Four",
            "Five", "Six"
        };
        static const RollerTextSource roller_source{
            roller_items,
            static_cast<std::uint16_t>(sizeof(roller_items) / sizeof(roller_items[0]))
        };
        factory.set_roller_source(roller, roller_source.count, &roller_source, &roller_text_at);
        factory.set_roller_selected(roller, 0);
        factory.set_roller_row_height(roller, 22);
        factory.set_roller_wheel_step(roller, 22);
        gui.render();

        const Rect tab_root_r = kernel.rect(tab_root);
        const Rect tab_r = kernel.rect(nav_bar);
        const int tab_abs_x = tab_root_r.x + tab_r.x;
        const int tab_abs_y = tab_root_r.y + tab_r.y;
        const int seg_w = (tab_r.w > 0) ? (tab_r.w / 3) : 0;
        kernel.layout_trace_reset();
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, tab_abs_x + seg_w + 6, tab_abs_y + 8, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, tab_abs_x + seg_w + 6, tab_abs_y + 8, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, tab_abs_x + seg_w + 6, tab_abs_y + 8, 1));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: tab select invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: tab select pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "ui: tab select missing paint", fails);
    expect_true(kernel.segmented_selected(nav_bar) == 1, "ui: tab select not applied", fails);

    const Rect tab2_r = kernel.rect(tab_bar);
    const int tab2_abs_x = tab2_r.x;
    const int tab2_abs_y = tab2_r.y;
    const int tab2_seg_w = (tab2_r.w > 0) ? (tab2_r.w / 3) : 0;
    kernel.layout_trace_reset();
    gui.dispatch_event(Event::mouse(Event::Type::MouseMove, tab2_abs_x + tab2_seg_w * 2 + 4, tab2_abs_y + 6, 0));
    gui.dispatch_event(Event::mouse(Event::Type::MouseDown, tab2_abs_x + tab2_seg_w * 2 + 4, tab2_abs_y + 6, 1));
    gui.dispatch_event(Event::mouse(Event::Type::MouseUp, tab2_abs_x + tab2_seg_w * 2 + 4, tab2_abs_y + 6, 1));
    gui.render();
    expect_true(kernel.layout_invalidated_count() == 0, "ui: tab bar invalidated layout", fails);
    expect_true(kernel.layout_pass_count() == 0, "ui: tab bar pass", fails);
    expect_true(kernel.paint_invalidated_count() > 0, "ui: tab bar missing paint", fails);
    expect_true(kernel.segmented_selected(tab_bar) == 2, "ui: tab bar not applied", fails);

        const Rect menu_r = kernel.rect(menu);
        const Rect item_b_r = kernel.rect(menu_item_b);
        const Rect item_c_r = kernel.rect(menu_item_c);
        const int menu_abs_x = tab_root_r.x + menu_r.x;
        const int menu_abs_y = tab_root_r.y + menu_r.y;
        kernel.layout_trace_reset();
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove,
                                        menu_abs_x + item_b_r.x + 6,
                                        menu_abs_y + item_b_r.y + 8,
                                        0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown,
                                        menu_abs_x + item_b_r.x + 6,
                                        menu_abs_y + item_b_r.y + 8,
                                        1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp,
                                        menu_abs_x + item_b_r.x + 6,
                                        menu_abs_y + item_b_r.y + 8,
                                        1));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: menu select invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: menu select pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "ui: menu select missing paint", fails);
        expect_true(kernel.checked(menu_item_b), "ui: menu select not applied", fails);
        expect_true(!kernel.checked(menu_item_a), "ui: menu select not exclusive", fails);

        kernel.layout_trace_reset();
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown,
                                        menu_abs_x + item_c_r.x + 6,
                                        menu_abs_y + item_c_r.y + 8,
                                        1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp,
                                        menu_abs_x + item_c_r.x + 6,
                                        menu_abs_y + item_c_r.y + 8,
                                        1));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: menu switch invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: menu switch pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "ui: menu switch missing paint", fails);
        expect_true(kernel.checked(menu_item_c), "ui: menu switch not applied", fails);
        expect_true(!kernel.checked(menu_item_b), "ui: menu switch not exclusive", fails);

        kernel.layout_trace_reset();
        const std::uint32_t paint_before = kernel.paint_invalidated_count();
        kernel.set_value(progress, 60);
        gui.render();
        const std::uint32_t paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: progress invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: progress pass", fails);
        expect_true(paint_after > paint_before, "ui: progress missing paint", fails);

        kernel.layout_trace_reset();
        const std::uint32_t wheel_paint_before = kernel.paint_invalidated_count();
        kernel.set_value(progress_wheel, 80);
        gui.render();
        const std::uint32_t wheel_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: progress wheel invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: progress wheel pass", fails);
        expect_true(wheel_paint_after > wheel_paint_before, "ui: progress wheel missing paint", fails);

        kernel.layout_trace_reset();
        const std::uint32_t simple_paint_before = kernel.paint_invalidated_count();
        kernel.set_value(progress_simple, 90);
        gui.render();
        const std::uint32_t simple_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: progress simple invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: progress simple pass", fails);
        expect_true(simple_paint_after > simple_paint_before, "ui: progress simple missing paint", fails);

        kernel.layout_trace_reset();
        const std::uint32_t round_paint_before = kernel.paint_invalidated_count();
        kernel.set_value(progress_round, 60);
        gui.render();
        const std::uint32_t round_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: progress round invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: progress round pass", fails);
        expect_true(round_paint_after > round_paint_before, "ui: progress round missing paint", fails);

        kernel.layout_trace_reset();
        const std::uint32_t flow_paint_before = kernel.paint_invalidated_count();
        kernel.set_value(progress_flowing, 80);
        gui.render();
        const std::uint32_t flow_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: progress flowing invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: progress flowing pass", fails);
        expect_true(flow_paint_after > flow_paint_before, "ui: progress flowing missing paint", fails);

        kernel.layout_trace_reset();
        const std::uint32_t log_paint_before = kernel.paint_invalidated_count();
        factory.console_append(console_box, "log: one");
        factory.console_append(console_box, "log: two");
        gui.render();
        const std::uint32_t log_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: log append invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: log append pass", fails);
        expect_true(log_paint_after > log_paint_before, "ui: log append missing paint", fails);

        kernel.layout_trace_reset();
        const std::uint32_t step_paint_before = kernel.paint_invalidated_count();
        const Rect step_r = kernel.rect(stepper);
        const int step_x = step_r.x + (step_r.w * 2) / 3;
        const int step_y = step_r.y + step_r.h / 2;
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, step_x, step_y, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, step_x, step_y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, step_x, step_y, 1));
        gui.render();
        const std::uint32_t step_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: stepper invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: stepper pass", fails);
        expect_true(step_paint_after > step_paint_before, "ui: stepper missing paint", fails);
        expect_true(kernel.stepper_current(stepper) == 2, "ui: stepper not applied", fails);

        kernel.layout_trace_reset();
        const std::uint32_t number_paint_before = kernel.paint_invalidated_count();
        const Rect number_r = kernel.rect(number_list);
        const int number_x = number_r.x + number_r.w / 2;
        const int number_y = number_r.y + number_r.h / 2 + kernel.number_list_row_height(number_list);
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, number_x, number_y, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, number_x, number_y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, number_x, number_y, 1));
        gui.render();
        const std::uint32_t number_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: number list invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: number list pass", fails);
        expect_true(number_paint_after > number_paint_before, "ui: number list missing paint", fails);
        expect_true(kernel.number_list_selected(number_list) == 1, "ui: number list not applied", fails);

        kernel.layout_trace_reset();
        const std::uint32_t roller_paint_before = kernel.paint_invalidated_count();
        const Rect roller_r = kernel.rect(roller);
        const int roller_x = roller_r.x + roller_r.w / 2;
        const int roller_y = roller_r.y + roller_r.h / 2 + kernel.roller_row_height(roller);
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, roller_x, roller_y, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, roller_x, roller_y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, roller_x, roller_y, 1));
        gui.render();
        const std::uint32_t roller_paint_after = kernel.paint_invalidated_count();
        expect_true(kernel.layout_invalidated_count() == 0, "ui: roller invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "ui: roller pass", fails);
        expect_true(roller_paint_after > roller_paint_before, "ui: roller missing paint", fails);
        expect_true(kernel.roller_selected(roller) == 1, "ui: roller not applied", fails);

    kernel.destroy(progress_flowing);
    kernel.destroy(progress_round);
    kernel.destroy(progress_simple);
    kernel.destroy(progress_wheel);
    kernel.destroy(progress);
    kernel.destroy(roller);
    kernel.destroy(number_list);
    kernel.destroy(stepper);
    kernel.destroy(console_box);
    kernel.destroy(tab_bar);
        kernel.destroy(menu_item_c);
        kernel.destroy(menu_item_b);
        kernel.destroy(menu_item_a);
        kernel.destroy(menu);
        kernel.destroy(nav_bar);
        kernel.destroy(tab_root);

        if (fails == 0) {
            (void)out::println<"[soa] ui regression OK">(g_console);
        }
        return fails == 0;
    }

    bool run_list_view_regression(SoaGui& gui,
                                  SoaKernel& kernel,
                                  SoaFactory& factory,
                                  WidgetHandle root,
                                  DefaultFrameBuffer& fb,
                                  DefaultCanvas& canvas,
                                  SdlTileBackend& tile_backend,
                                  const FrameBufferView& tile_view,
                                  const ui::draw_cmd::DrawCmdTileConfig& tile_config) noexcept {
        int fails = 0;
        const auto stats_before = kernel.payload_stats();
        const std::uint16_t list_item_peak_before = stats_before.list_item.peak;
        const std::uint32_t list_item_fail_before = stats_before.list_item.alloc_fail;
        const std::uint32_t list_view_fail_before = stats_before.list_view.alloc_fail;

        ensure_demo_images();
        auto list_root = factory.create_container();
        auto list_view = factory.create_list_view();
        factory.link(root, list_root);
        factory.link(list_root, list_view);

        kernel.set_rect(list_root, {screen_width - 260, 60, 220, 200});
        kernel.set_rect(list_view, {0, 0, 200, 200});
        kernel.set_list_row_height(list_view, 24);
        kernel.set_scroll_step(list_view, 24);

        static const char* list_items[] = {
            "Alpha", "Beta", "Gamma", "Delta",
            "Epsilon", "Zeta", "Eta", "Theta"
        };
        static const ListViewTextSource list_source{
            list_items,
            static_cast<std::uint16_t>(sizeof(list_items) / sizeof(list_items[0]))
        };
        static const ListViewRowFlagsSource row_flags_source{
            .disabled_index = 1,
            .group_index = 0,
        };
        static const ListViewIconSource icon_source{
            g_test_icon_id,
            g_slice_id
        };
        factory.set_list_view_source(list_view, 1000, &list_source, &list_view_text_at);
        factory.set_list_view_row_flags_source(list_view, &row_flags_source, &list_view_row_flags_at);
        factory.set_list_view_icon_source(list_view, &icon_source, &list_view_icon_at, 18);
        gui.render();
        expect_true(gui.last_exec_stats().failed_cmds == 0, "listview: failed_cmds", fails);
        expect_true((kernel.list_view_item_row_flags(list_view, 0) & soa_detail::kListViewRowFlagGroup) != 0,
            "listview: group row flag missing", fails);
        expect_true((kernel.list_view_item_row_flags(list_view, 1) & soa_detail::kListViewRowFlagDisabled) != 0,
            "listview: disabled row flag missing", fails);

        const auto stats_after_create = kernel.payload_stats();
        const std::uint16_t expected_peak =
            static_cast<std::uint16_t>(stats_before.list_view.peak + 1u);
        expect_true(stats_after_create.list_view.peak == expected_peak,
            "listview: peak count mismatch", fails);
        expect_true(stats_after_create.list_item.peak == list_item_peak_before,
            "listview: list_item peak changed", fails);
        expect_true(stats_after_create.list_item.alloc_fail == list_item_fail_before,
            "listview: list_item alloc failed", fails);
        expect_true(stats_after_create.list_view.alloc_fail == list_view_fail_before,
            "listview: list_view alloc failed", fails);
        expect_true(!stats_after_create.overflowed, "listview: payload overflowed", fails);
        expect_true(!stats_after_create.text_overflowed, "listview: text overflowed", fails);

        const Rect root_rect = kernel.rect(list_root);
        const Rect view_rect = kernel.rect(list_view);
        const int hit_x = root_rect.x + view_rect.x + 8;
        const int hit_y = root_rect.y + view_rect.y + 8;
        const int row_h = kernel.list_row_height(list_view);

        const int before_scroll = kernel.scroll_y(list_view);
        kernel.layout_trace_reset();
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, hit_x, hit_y, 0));
        gui.dispatch_event(Event::wheel(hit_x, hit_y, -6));
        gui.render();
        const int after_scroll = kernel.scroll_y(list_view);
        expect_true(after_scroll != before_scroll, "listview: wheel did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "listview: wheel invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "listview: wheel pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "listview: wheel missing paint", fails);

        const auto cmd_stats = gui.last_cmd_stats();
        expect_true(!cmd_stats.cmd_overflowed, "listview: cmd overflowed", fails);
        expect_true(!cmd_stats.text_overflowed, "listview: text overflowed", fails);
        expect_true(!cmd_stats.blob_overflowed, "listview: blob overflowed", fails);

        kernel.layout_trace_reset();
        const int drag_before = kernel.scroll_y(list_view);
        const int drag_y = hit_y + row_h;
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, hit_x, drag_y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, hit_x, drag_y - row_h * 4, 0));
        gui.render();
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, hit_x, drag_y - row_h * 4, 1));
        const int drag_after = kernel.scroll_y(list_view);
        expect_true(drag_after != drag_before, "listview: drag did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "listview: drag invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "listview: drag pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "listview: drag missing paint", fails);

        kernel.set_scroll_y_clamped(list_view, 0);
        gui.render();
        kernel.set_list_view_selected(list_view, -1);
        kernel.layout_trace_reset();
        const int disabled_y = hit_y + row_h;
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, hit_x, disabled_y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, hit_x, disabled_y, 1));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "listview: disabled click invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "listview: disabled click pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "listview: disabled click missing paint", fails);
        expect_true(kernel.list_view_selected(list_view) == -1, "listview: disabled click changed selection", fails);

        kernel.layout_trace_reset();
        const int select_y = hit_y + row_h * 2;
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, hit_x, select_y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, hit_x, select_y, 1));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "listview: select invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "listview: select pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "listview: select missing paint", fails);
        expect_true(kernel.list_view_selected(list_view) >= 0, "listview: select not applied", fails);

        kernel.layout_trace_reset();
        const int far_scroll = row_h * 200;
        kernel.set_scroll_y_clamped(list_view, far_scroll);
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "listview: far scroll invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "listview: far scroll pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "listview: far scroll missing paint", fails);

        const auto stats_after_scroll = kernel.payload_stats();
        expect_true(stats_after_scroll.list_view.peak == expected_peak,
            "listview: peak changed on scroll", fails);
        expect_true(stats_after_scroll.list_item.peak == list_item_peak_before,
            "listview: list_item peak changed on scroll", fails);
        expect_true(stats_after_scroll.list_item.alloc_fail == list_item_fail_before,
            "listview: list_item alloc failed on scroll", fails);
        expect_true(stats_after_scroll.list_view.alloc_fail == list_view_fail_before,
            "listview: list_view alloc failed on scroll", fails);
        expect_true(!stats_after_scroll.overflowed, "listview: payload overflowed on scroll", fails);
        expect_true(!stats_after_scroll.text_overflowed, "listview: text overflowed on scroll", fails);

        kernel.destroy(list_view);
        kernel.destroy(list_root);

        if (fails == 0) {
            ui::draw_cmd::DefaultDrawCmdBuffer dump_buf{};
            gui.record_commands(dump_buf);
            const char* temp_dir = std::getenv("TEMP");
            char dump_path[512]{};
            if (temp_dir && temp_dir[0]) {
                std::snprintf(dump_path, sizeof(dump_path), "%s\\soa_iconlist.vcmd", temp_dir);
            } else {
                std::snprintf(dump_path, sizeof(dump_path), "soa_iconlist.vcmd");
            }
            if (!dump_cmd_file(dump_path, dump_buf, &tile_backend)) {
                expect_true(false, "listview: dump_cmd_file failed", fails);
            } else if (!replay_cmd_file(dump_path, fb, canvas, tile_backend, tile_view, tile_config, false, nullptr)) {
                expect_true(false, "listview: replay full failed", fails);
            } else if (!replay_cmd_file(dump_path, fb, canvas, tile_backend, tile_view, tile_config, true, nullptr)) {
                expect_true(false, "listview: replay tile failed", fails);
            }
        }

        if (fails == 0) {
            (void)out::println<"[soa] listview regression OK">(g_console);
        }
        return fails == 0;
    }

    bool run_table_tree_regression(SoaGui& gui,
                                   SoaKernel& kernel,
                                   SoaFactory& factory,
                                   WidgetHandle root) noexcept {
        int fails = 0;
        const auto stats_before = kernel.payload_stats();
        const std::uint16_t table_peak_before = stats_before.table_view.peak;
        const std::uint16_t tree_peak_before = stats_before.tree_view.peak;
        const std::uint16_t list_item_peak_before = stats_before.list_item.peak;
        const std::uint32_t table_fail_before = stats_before.table_view.alloc_fail;
        const std::uint32_t tree_fail_before = stats_before.tree_view.alloc_fail;
        const std::uint32_t list_item_fail_before = stats_before.list_item.alloc_fail;

        auto table_root = factory.create_container();
        auto table_view = factory.create_table_view();
        auto table_scroll_x = factory.create_scrollbar_for(table_view);
        auto tree_root = factory.create_container();
        auto tree_view = factory.create_tree_view();
        factory.link(root, table_root);
        factory.link(table_root, table_view);
        factory.link(table_root, table_scroll_x);
        factory.link(root, tree_root);
        factory.link(tree_root, tree_view);

        kernel.set_rect(table_root, {screen_width - 260, 280, 220, 200});
        kernel.set_rect(table_view, {0, 0, 200, 184});
        kernel.set_rect(table_scroll_x, {0, 184, 200, 16});
        kernel.set_scrollbar_orientation(table_scroll_x, ScrollBarOrientation::Horizontal);
        kernel.set_scrollbar_page_size(table_scroll_x, 200);
        kernel.set_rect(tree_root, {screen_width - 520, 280, 220, 200});
        kernel.set_rect(tree_view, {0, 0, 200, 200});
        kernel.set_list_row_height(table_view, 24);
        kernel.set_list_row_height(tree_view, 24);
        kernel.set_scroll_step(table_view, 24);
        kernel.set_scroll_step(tree_view, 24);

        static const char* table_rows[] = {
            "Alpha", "Beta", "Gamma", "Delta",
            "Epsilon", "Zeta", "Eta", "Theta"
        };
        static const char* table_cols[] = {
            "ID", "Name", "State", "Value"
        };
        static const TableViewTextSource table_source{
            table_rows,
            static_cast<std::uint16_t>(sizeof(table_rows) / sizeof(table_rows[0])),
            table_cols,
            static_cast<std::uint8_t>(sizeof(table_cols) / sizeof(table_cols[0]))
        };
        factory.set_table_view_source(table_view, 1000, 32, &table_source, &table_view_text_at);
        factory.set_table_view_header(table_view, &table_source, &table_view_header_at);
        factory.set_table_view_header_height(table_view, 20);
        factory.set_table_view_header_padding(table_view, 6);
        factory.set_table_view_header_style(table_view, TableViewHeaderStyle::Accent);
        factory.set_table_view_header_divider(table_view, true);
        factory.set_table_view_col_divider_style(table_view, TableViewColDividerStyle::HeaderOnly);
        factory.set_table_view_col_width_fn(table_view, &kTableColWidthSource, &table_view_col_width_at);

        static const char* tree_items[] = {
            "Root", "Alpha", "Beta", "Gamma",
            "Delta", "Epsilon", "Zeta", "Eta"
        };
        static const TreeViewTextSource tree_source{
            tree_items,
            static_cast<std::uint16_t>(sizeof(tree_items) / sizeof(tree_items[0]))
        };
        static const TreeViewIndentSource tree_indent{20};
        factory.set_tree_view_source(tree_view, 1000, &tree_source, &tree_view_text_at,
                                     &tree_indent, &tree_view_indent_at);
        factory.set_tree_view_indent_px(tree_view, 12);
        factory.set_tree_view_max_indent_px(tree_view, 140);
        factory.set_tree_view_min_text_avail_px(tree_view, 64);

        gui.render();
        expect_true(gui.last_exec_stats().failed_cmds == 0, "tabletree: failed_cmds", fails);

        const auto stats_after_create = kernel.payload_stats();
        expect_true(stats_after_create.table_view.peak == static_cast<std::uint16_t>(table_peak_before + 1u),
            "tableview: peak count mismatch", fails);
        expect_true(stats_after_create.tree_view.peak == static_cast<std::uint16_t>(tree_peak_before + 1u),
            "treeview: peak count mismatch", fails);
        expect_true(stats_after_create.list_item.peak == list_item_peak_before,
            "tabletree: list_item peak changed", fails);
        expect_true(stats_after_create.table_view.alloc_fail == table_fail_before,
            "tableview: alloc failed", fails);
        expect_true(stats_after_create.tree_view.alloc_fail == tree_fail_before,
            "treeview: alloc failed", fails);
        expect_true(stats_after_create.list_item.alloc_fail == list_item_fail_before,
            "tabletree: list_item alloc failed", fails);
        expect_true(!stats_after_create.overflowed, "tabletree: payload overflowed", fails);
        expect_true(!stats_after_create.text_overflowed, "tabletree: text overflowed", fails);

        const Rect table_root_r = kernel.rect(table_root);
        const Rect table_r = kernel.rect(table_view);
        const Rect table_world{table_root_r.x + table_r.x, table_root_r.y + table_r.y, table_r.w, table_r.h};
        const int table_hit_x = table_world.x + 8;
        const int header_h = kernel.table_view_header_height(table_view);
        const int header_hit_y = table_world.y + ((header_h > 0) ? (header_h / 2) : 0);
        const int table_hit_y = table_world.y + header_h + 8;

        const TableViewHeaderStyle header_style_cases[] = {
            TableViewHeaderStyle::Default,
            TableViewHeaderStyle::Muted,
            TableViewHeaderStyle::Accent
        };
        for (const TableViewHeaderStyle style : header_style_cases) {
            kernel.layout_trace_reset();
            factory.set_table_view_header_style(table_view, style);
            gui.render();
            expect_true(gui.last_exec_stats().failed_cmds == 0, "tableview: header style failed_cmds", fails);
            expect_true(kernel.layout_invalidated_count() == 0,
                        "tableview: header style invalidated layout", fails);
            expect_true(kernel.layout_pass_count() == 0, "tableview: header style pass", fails);
            expect_true(kernel.paint_invalidated_count() > 0, "tableview: header style missing paint", fails);

            if (style == TableViewHeaderStyle::Muted) {
                ui::draw_cmd::DefaultDrawCmdBuffer muted_buf{};
                gui.record_commands(muted_buf);
                std::vector<Rect> fill_rects{};
                const bool fill_decode_ok = collect_fill_rects(muted_buf, fill_rects);
                expect_true(fill_decode_ok, "tableview: muted fill decode", fails);
                if (fill_decode_ok) {
                    std::vector<Rect> table_fills{};
                    for (const Rect& fill : fill_rects) {
                        if (rects_intersect(fill, table_world)) {
                            table_fills.push_back(fill);
                        }
                    }
                    expect_true(table_fills.size() >= 3, "tableview: muted fill sequence", fails);
                    if (table_fills.size() >= 3) {
                        const Rect& header_fill = table_fills[1];
                        const Rect& next_fill = table_fills[2];
                        const bool has_inset_fill =
                            next_fill.x == header_fill.x + 1
                            && next_fill.y == header_fill.y + 1
                            && next_fill.w == header_fill.w - 2
                            && next_fill.h == header_fill.h - 2;
                        expect_true(!has_inset_fill, "tableview: muted header inset fill", fails);
                    }
                }
            }
        }

        kernel.layout_trace_reset();
        const int table_before = kernel.scroll_y(table_view);
        const int table_x_before_body_wheel = kernel.table_view_scroll_x(table_view);
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, table_hit_x, table_hit_y, 0));
        gui.dispatch_event(Event::wheel(table_hit_x, table_hit_y, -6));
        gui.render();
        const int table_after = kernel.scroll_y(table_view);
        const int table_x_after_body_wheel = kernel.table_view_scroll_x(table_view);
        expect_true(table_after != table_before, "tableview: wheel did not scroll", fails);
        expect_true(table_x_after_body_wheel == table_x_before_body_wheel,
                    "tableview: body wheel moved x", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "tableview: wheel invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "tableview: wheel pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "tableview: wheel missing paint", fails);

        kernel.layout_trace_reset();
        const int table_x_before = kernel.table_view_scroll_x(table_view);
        kernel.set_table_view_scroll_x(table_view, table_x_before + 120);
        gui.render();
        const int table_x_after = kernel.table_view_scroll_x(table_view);
        expect_true(table_x_after != table_x_before, "tableview: horiz scroll did not move", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "tableview: horiz invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "tableview: horiz pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "tableview: horiz missing paint", fails);

        kernel.set_scroll_y(table_view, 0);
        gui.render();
        ui::draw_cmd::DefaultDrawCmdBuffer align_buf{};
        gui.record_commands(align_buf);
        std::vector<TextProbe> text_probes{};
        const bool text_probe_ok = collect_text_probes(align_buf, text_probes);
        expect_true(text_probe_ok, "tableview: align text decode", fails);
        if (text_probe_ok) {
            const auto probe_in_table = [&](const TextProbe& probe) noexcept {
                return rects_intersect(probe.rect, table_world)
                    && probe.rect.y >= table_world.y
                    && probe.rect.y < table_world.y + table_world.h;
            };
            std::vector<int> row_ys{};
            for (const TextProbe& probe : text_probes) {
                if (probe_in_table(probe)) {
                    row_ys.push_back(probe.rect.y);
                }
            }
            std::sort(row_ys.begin(), row_ys.end());
            row_ys.erase(std::unique(row_ys.begin(), row_ys.end()), row_ys.end());
            expect_true(row_ys.size() >= 2, "tableview: align row bands", fails);
            if (row_ys.size() >= 2) {
                const auto find_text_at = [&](std::string_view text, int y) noexcept -> const TextProbe* {
                    for (const TextProbe& probe : text_probes) {
                        if (probe.rect.y != y) continue;
                        if (!probe_in_table(probe)) continue;
                        if (probe.text == text) return &probe;
                    }
                    return nullptr;
                };
                const auto row_has_columns = [&](int y) noexcept {
                    return find_text_at("Name", y) && find_text_at("State", y) && find_text_at("Value", y);
                };
                int header_y = -1;
                int body_y = -1;
                const int header_band_bottom = table_world.y + header_h;
                for (const int y : row_ys) {
                    if (!row_has_columns(y)) continue;
                    if (header_y < 0 && y < header_band_bottom) {
                        header_y = y;
                    } else if (body_y < 0 && y >= header_band_bottom) {
                        body_y = y;
                        break;
                    }
                }
                expect_true(header_y >= 0 && body_y >= 0, "tableview: align row selection", fails);
                const TextProbe* header_name = find_text_at("Name", header_y);
                const TextProbe* header_state = find_text_at("State", header_y);
                const TextProbe* header_value = find_text_at("Value", header_y);
                const TextProbe* body_name = find_text_at("Name", body_y);
                const TextProbe* body_state = find_text_at("State", body_y);
                const TextProbe* body_value = find_text_at("Value", body_y);
                const bool aligned =
                    header_name && header_state && header_value
                    && body_name && body_state && body_value
                    && (header_state->rect.x - header_name->rect.x) == (body_state->rect.x - body_name->rect.x)
                    && (header_value->rect.x - header_name->rect.x) == (body_value->rect.x - body_name->rect.x);
                expect_true(aligned, "tableview: header/body columns misaligned", fails);
            }
        }

        kernel.set_table_view_scroll_x(table_view, 0);
        kernel.layout_trace_reset();
        const int table_x_before_header_wheel = kernel.table_view_scroll_x(table_view);
        const int table_y_before_header_wheel = kernel.scroll_y(table_view);
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, table_hit_x, header_hit_y, 0));
        gui.dispatch_event(Event::wheel(table_hit_x, header_hit_y, -6));
        gui.render();
        const int table_x_after_header_wheel = kernel.table_view_scroll_x(table_view);
        const int table_y_after_header_wheel = kernel.scroll_y(table_view);
        expect_true(table_x_after_header_wheel != table_x_before_header_wheel,
                    "tableview: header wheel did not scroll x", fails);
        expect_true(table_y_after_header_wheel == table_y_before_header_wheel,
                    "tableview: header wheel moved y", fails);
        expect_true(kernel.layout_invalidated_count() == 0,
                    "tableview: header wheel invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "tableview: header wheel pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "tableview: header wheel missing paint", fails);

        kernel.layout_trace_reset();
        const Rect hscroll_r = kernel.rect(table_scroll_x);
        const int hscroll_hit_x = table_root_r.x + hscroll_r.x + hscroll_r.w / 2;
        const int hscroll_hit_y = table_root_r.y + hscroll_r.y + hscroll_r.h / 2;
        const int table_x_before_wheel = kernel.table_view_scroll_x(table_view);
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, hscroll_hit_x, hscroll_hit_y, 0));
        gui.dispatch_event(Event::wheel(hscroll_hit_x, hscroll_hit_y, -6));
        gui.render();
        const int table_x_after_wheel = kernel.table_view_scroll_x(table_view);
        expect_true(table_x_after_wheel != table_x_before_wheel, "tableview: wheel did not scroll x", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "tableview: wheel x invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "tableview: wheel pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "tableview: wheel missing paint", fails);

        ScrollbarTestInfo hinfo{};
        const bool hinfo_ok = build_scrollbar_info(kernel, table_scroll_x, table_root_r, hinfo);
        expect_true(hinfo_ok, "tableview: hscroll info", fails);
        if (hinfo_ok && hinfo.max_scroll > 0) {
            kernel.set_table_view_scroll_x(table_view, 0);
            kernel.layout_trace_reset();
            gui.dispatch_event(Event::mouse(Event::Type::MouseMove, hinfo.track_start + hinfo.track_len - 1,
                                            hscroll_hit_y, 0));
            gui.dispatch_event(Event::mouse(Event::Type::MouseDown, hinfo.track_start + hinfo.track_len - 1,
                                            hscroll_hit_y, 1));
            gui.dispatch_event(Event::mouse(Event::Type::MouseUp, hinfo.track_start + hinfo.track_len - 1,
                                            hscroll_hit_y, 1));
            gui.render();
            const int table_x_page = kernel.table_view_scroll_x(table_view);
            expect_true(table_x_page > 0, "tableview: hscroll page click", fails);
            expect_true(kernel.layout_invalidated_count() == 0, "tableview: hscroll page invalidated layout", fails);
            expect_true(kernel.layout_pass_count() == 0, "tableview: hscroll page pass", fails);
            expect_true(kernel.paint_invalidated_count() > 0, "tableview: hscroll page missing paint", fails);

            kernel.set_table_view_scroll_x(table_view, hinfo.max_scroll);
            kernel.layout_trace_reset();
            gui.dispatch_event(Event::mouse(Event::Type::MouseMove, hinfo.track_start + 1, hscroll_hit_y, 0));
            gui.dispatch_event(Event::mouse(Event::Type::MouseDown, hinfo.track_start + 1, hscroll_hit_y, 1));
            gui.dispatch_event(Event::mouse(Event::Type::MouseUp, hinfo.track_start + 1, hscroll_hit_y, 1));
            gui.render();
            const int table_x_back = kernel.table_view_scroll_x(table_view);
            expect_true(table_x_back < hinfo.max_scroll, "tableview: hscroll page back", fails);
            expect_true(kernel.layout_invalidated_count() == 0, "tableview: hscroll back invalidated layout", fails);
            expect_true(kernel.layout_pass_count() == 0, "tableview: hscroll back pass", fails);
            expect_true(kernel.paint_invalidated_count() > 0, "tableview: hscroll back missing paint", fails);

            kernel.set_table_view_scroll_x(table_view, hinfo.max_scroll);
            kernel.layout_trace_reset();
            gui.dispatch_event(Event::mouse(Event::Type::MouseMove, hinfo.track_start + hinfo.track_len - 1,
                                            hscroll_hit_y, 0));
            gui.dispatch_event(Event::mouse(Event::Type::MouseDown, hinfo.track_start + hinfo.track_len - 1,
                                            hscroll_hit_y, 1));
            gui.dispatch_event(Event::mouse(Event::Type::MouseUp, hinfo.track_start + hinfo.track_len - 1,
                                            hscroll_hit_y, 1));
            gui.render();
            const int table_x_clamp = kernel.table_view_scroll_x(table_view);
            expect_true(table_x_clamp == hinfo.max_scroll, "tableview: hscroll clamp max", fails);
            expect_true(kernel.layout_invalidated_count() == 0, "tableview: hscroll clamp invalidated layout", fails);
            expect_true(kernel.layout_pass_count() == 0, "tableview: hscroll clamp pass", fails);

            const int mid_scroll = hinfo.max_scroll / 2;
            kernel.set_table_view_scroll_x(table_view, mid_scroll);
            build_scrollbar_info(kernel, table_scroll_x, table_root_r, hinfo);
            kernel.layout_trace_reset();
            const int thumb_center = hinfo.thumb_start + hinfo.thumb_len / 2;
            gui.dispatch_event(Event::mouse(Event::Type::MouseMove, thumb_center, hscroll_hit_y, 0));
            gui.dispatch_event(Event::mouse(Event::Type::MouseDown, thumb_center, hscroll_hit_y, 1));
            gui.dispatch_event(Event::mouse(Event::Type::MouseMove, thumb_center + 20, hscroll_hit_y, 1));
            gui.dispatch_event(Event::mouse(Event::Type::MouseUp, thumb_center + 20, hscroll_hit_y, 1));
            gui.render();
            const int table_x_drag = kernel.table_view_scroll_x(table_view);
            expect_true(table_x_drag != mid_scroll, "tableview: hscroll drag", fails);
            expect_true(kernel.layout_invalidated_count() == 0, "tableview: hscroll drag invalidated layout", fails);
            expect_true(kernel.layout_pass_count() == 0, "tableview: hscroll drag pass", fails);
            expect_true(kernel.paint_invalidated_count() > 0, "tableview: hscroll drag missing paint", fails);
        }
        const int hscroll_x = table_root_r.x + hscroll_r.x + hscroll_r.w - 2;
        const int hscroll_y = table_root_r.y + hscroll_r.y + hscroll_r.h / 2;
        kernel.layout_trace_reset();
        const int table_x_before_bar = kernel.table_view_scroll_x(table_view);
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, hscroll_x, hscroll_y, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, hscroll_x, hscroll_y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, hscroll_x, hscroll_y, 1));
        gui.render();
        const int table_x_after_bar = kernel.table_view_scroll_x(table_view);
        expect_true(table_x_after_bar != table_x_before_bar, "tableview: scrollbar horiz did not move", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "tableview: scrollbar invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "tableview: scrollbar pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "tableview: scrollbar missing paint", fails);

        kernel.set_table_view_col_width(table_view, 72);
        expect_true(!kernel.table_view_has_col_width_fn(table_view), "tableview: fixed width still has fn", fails);
        kernel.layout_trace_reset();
        const int table_x_before_fixed = kernel.table_view_scroll_x(table_view);
        kernel.set_table_view_scroll_x(table_view, table_x_before_fixed + 64);
        gui.render();
        const int table_x_after_fixed = kernel.table_view_scroll_x(table_view);
        expect_true(table_x_after_fixed != table_x_before_fixed, "tableview: fixed width scroll did not move", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "tableview: fixed width invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "tableview: fixed width pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "tableview: fixed width missing paint", fails);

        kernel.layout_trace_reset();
        const Rect tree_root_r = kernel.rect(tree_root);
        const Rect tree_r = kernel.rect(tree_view);
        const int tree_hit_x = tree_root_r.x + tree_r.x + 8;
        const int tree_hit_y = tree_root_r.y + tree_r.y + 8;
        const int tree_before = kernel.scroll_y(tree_view);
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, tree_hit_x, tree_hit_y, 0));
        gui.dispatch_event(Event::wheel(tree_hit_x, tree_hit_y, -6));
        gui.render();
        const int tree_after = kernel.scroll_y(tree_view);
        expect_true(tree_after != tree_before, "treeview: wheel did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "treeview: wheel invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "treeview: wheel pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "treeview: wheel missing paint", fails);

        const auto stats_after_scroll = kernel.payload_stats();
        expect_true(stats_after_scroll.table_view.peak == stats_after_create.table_view.peak,
            "tableview: peak changed on scroll", fails);
        expect_true(stats_after_scroll.tree_view.peak == stats_after_create.tree_view.peak,
            "treeview: peak changed on scroll", fails);
        expect_true(stats_after_scroll.list_item.peak == list_item_peak_before,
            "tabletree: list_item peak changed on scroll", fails);
        expect_true(stats_after_scroll.table_view.alloc_fail == table_fail_before,
            "tableview: alloc failed on scroll", fails);
        expect_true(stats_after_scroll.tree_view.alloc_fail == tree_fail_before,
            "treeview: alloc failed on scroll", fails);
        expect_true(!stats_after_scroll.overflowed, "tabletree: payload overflowed on scroll", fails);
        expect_true(!stats_after_scroll.text_overflowed, "tabletree: text overflowed on scroll", fails);

        kernel.destroy(table_view);
        kernel.destroy(table_root);
        kernel.destroy(tree_view);
        kernel.destroy(tree_root);

        if (fails == 0) {
            (void)out::println<"[soa] table/tree regression OK">(g_console);
        }
        return fails == 0;
    }

    bool run_style_regression(SoaGui& gui) noexcept {
        int fails = 0;
        auto& sheet = StyleSheet::instance();
        sheet.style_trace_reset();
        const std::uint32_t role_before = sheet.role_palette_compile_count();
        const std::uint32_t table_before = sheet.style_table_compile_count();

        gui.render();
        expect_true(sheet.role_palette_compile_count() == role_before,
                    "style: role palette compiled without token change", fails);
        expect_true(sheet.style_table_compile_count() == table_before,
                    "style: style table compiled without token change", fails);

        ThemeTokens tokens = Theme::instance().get_tokens();
        tokens.accent = adjust_by_luma(tokens.accent, 12);
        Theme::instance().set_tokens_unsafe(tokens);
        apply_baseline_theme_preset(make_style_from_tokens(tokens));
        const std::uint32_t role_after = sheet.role_palette_compile_count();
        const std::uint32_t table_after = sheet.style_table_compile_count();
        expect_true(role_after == role_before + 1u, "style: role palette not rebuilt", fails);
        expect_true(table_after == table_before + 1u, "style: style table not rebuilt", fails);

        const StyleStats stats = sheet.style_stats();
        (void)out::println<"[soa] style bytes: colors={} metrics_id={} pool={} total={} pool_size={}">(
            g_console,
            static_cast<std::uint32_t>(stats.style_colors_bytes),
            static_cast<std::uint32_t>(stats.style_metrics_id_bytes),
            static_cast<std::uint32_t>(stats.metrics_pool_bytes),
            static_cast<std::uint32_t>(stats.style_table_total_bytes),
            static_cast<std::uint32_t>(stats.metrics_pool_size));
        expect_true(!stats.metrics_overflowed, "style: metrics pool overflowed", fails);
        expect_true(stats.style_table_total_bytes <= kMaxStyleTableBytes, "style: table bytes too large", fails);

        if (g_regress_log) {
            std::fprintf(g_regress_log,
                         "[soa] style bytes: colors=%u metrics_id=%u pool=%u total=%u pool_size=%u\n",
                         static_cast<unsigned>(stats.style_colors_bytes),
                         static_cast<unsigned>(stats.style_metrics_id_bytes),
                         static_cast<unsigned>(stats.metrics_pool_bytes),
                         static_cast<unsigned>(stats.style_table_total_bytes),
                         static_cast<unsigned>(stats.metrics_pool_size));
            std::fprintf(g_regress_log, "[soa] metrics_overflowed=%u\n",
                         stats.metrics_overflowed ? 1u : 0u);
        }

        const std::uint8_t interactive_mask =
            static_cast<std::uint8_t>(StyleStateFlag::Hovered)
            | static_cast<std::uint8_t>(StyleStateFlag::Pressed)
            | static_cast<std::uint8_t>(StyleStateFlag::Disabled);
        const std::uint8_t press_only_mask =
            static_cast<std::uint8_t>(StyleStateFlag::Pressed)
            | static_cast<std::uint8_t>(StyleStateFlag::Disabled);

        const StyleStateEvidence stepper_evidence = make_style_state_evidence(WidgetKind::Stepper);
        expect_true(style_state_evidence_matches_interactive_law(stepper_evidence),
                    "style: stepper interactive law failed", fails);
        expect_true(stepper_evidence.mask == interactive_mask, "style: stepper mask mismatch", fails);
        expect_true(stepper_evidence.state_count == 8, "style: stepper state count mismatch", fails);

        const StyleStateEvidence number_list_evidence = make_style_state_evidence(WidgetKind::NumberList);
        expect_true(number_list_evidence.mask == press_only_mask, "style: number list mask mismatch", fails);
        expect_true(number_list_evidence.state_count == 4, "style: number list state count mismatch", fails);
        expect_true(number_list_evidence.includes_pressed, "style: number list missing pressed state", fails);
        expect_true(number_list_evidence.includes_disabled, "style: number list missing disabled state", fails);
        expect_true(!number_list_evidence.includes_hovered, "style: number list unexpectedly hovered", fails);
        expect_true(!number_list_evidence.includes_focused, "style: number list unexpectedly focused", fails);

        const StyleStateEvidence roller_evidence = make_style_state_evidence(WidgetKind::Roller);
        expect_true(roller_evidence.mask == press_only_mask, "style: roller mask mismatch", fails);
        expect_true(roller_evidence.state_count == 4, "style: roller state count mismatch", fails);
        expect_true(roller_evidence.includes_pressed, "style: roller missing pressed state", fails);
        expect_true(roller_evidence.includes_disabled, "style: roller missing disabled state", fails);
        expect_true(!roller_evidence.includes_hovered, "style: roller unexpectedly hovered", fails);
        expect_true(!roller_evidence.includes_focused, "style: roller unexpectedly focused", fails);

        for (WidgetKind kind : enabled_widget_kinds) {
            const StyleKindStateInfo info = sheet.style_kind_state_info(kind);
            (void)out::println<"[soa] style kind={} mask=0x{:02X} states={} offset={}">(
                g_console,
                widget_kind_name(kind),
                static_cast<int>(info.mask),
                static_cast<int>(info.state_count),
                static_cast<int>(info.state_offset));
            if (g_regress_log) {
                std::fprintf(g_regress_log,
                             "[soa] style kind=%s mask=0x%02X states=%u offset=%u\n",
                             widget_kind_name(kind),
                             static_cast<unsigned>(info.mask),
                             static_cast<unsigned>(info.state_count),
                             static_cast<unsigned>(info.state_offset));
            }
        }

        if (fails == 0) {
            (void)out::println<"[soa] style regression OK">(g_console);
        }
        return fails == 0;
    }

    bool run_style_patch_pool_regression() noexcept {
        if constexpr (SoaKernel::kStylePatchCapacity >= SoaKernel::kMaxNodes) {
            (void)out::println<"[soa] style_patch_pool cap={} max_nodes={} overflow_test=skipped result=ok">(
                g_console,
                static_cast<unsigned>(SoaKernel::kStylePatchCapacity),
                static_cast<unsigned>(SoaKernel::kMaxNodes));
            return true;
        }

        int fails = 0;
        static SoaKernel probe{};
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = 3;

        WidgetHandle first{};
        for (std::size_t i = 0; i < SoaKernel::kStylePatchCapacity; ++i) {
            const WidgetHandle node = probe.create(WidgetKind::Container);
            expect_true(static_cast<bool>(node), "style patch pool: node allocation failed", fails);
            if (!node) break;
            if (i == 0) first = node;
            probe.set_style_adjust(node, patch);
            expect_true(probe.has_style_patch(node), "style patch pool: admitted patch missing", fails);
        }

        expect_true(probe.style_patch_live_count() == SoaKernel::kStylePatchCapacity,
                    "style patch pool: live count mismatch", fails);
        expect_true(probe.style_patch_peak_count() == SoaKernel::kStylePatchCapacity,
                    "style patch pool: peak count mismatch", fails);
        expect_true(probe.style_patch_alloc_fail() == 0,
                    "style patch pool: unexpected allocation failure", fails);

        probe.set_style_override(first, patch);
        expect_true(probe.style_patch_kind(first) == StylePatchKind::Override,
                    "style patch pool: existing slot update changed allocation", fails);
        expect_true(probe.style_patch_live_count() == SoaKernel::kStylePatchCapacity,
                    "style patch pool: existing slot update changed live count", fails);

        const WidgetHandle overflow_node = probe.create(WidgetKind::Container);
        expect_true(static_cast<bool>(overflow_node), "style patch pool: overflow node allocation failed", fails);
        probe.set_style_adjust(overflow_node, patch);
        expect_true(!probe.has_style_patch(overflow_node),
                    "style patch pool: overflow silently installed patch", fails);
        expect_true(probe.style_patch_overflowed() && probe.style_patch_alloc_fail() == 1,
                    "style patch pool: overflow evidence missing", fails);

        probe.clear_style_patch(first);
        probe.set_style_override(overflow_node, patch);
        expect_true(!probe.has_style_patch(first)
                        && probe.has_style_patch(overflow_node)
                        && probe.style_patch_kind(overflow_node) == StylePatchKind::Override,
                    "style patch pool: released slot was not reused", fails);
        expect_true(probe.style_patch_live_count() == SoaKernel::kStylePatchCapacity,
                    "style patch pool: reused slot changed live count", fails);
        expect_true(probe.style_patch_overflowed() && probe.style_patch_alloc_fail() == 1,
                    "style patch pool: sticky overflow evidence was lost", fails);

        (void)out::println<"[soa] style_patch_pool live={} peak={} cap={} fail={} overflow={} result={}">(
            g_console,
            static_cast<unsigned>(probe.style_patch_live_count()),
            static_cast<unsigned>(probe.style_patch_peak_count()),
            static_cast<unsigned>(SoaKernel::kStylePatchCapacity),
            static_cast<unsigned>(probe.style_patch_alloc_fail()),
            probe.style_patch_overflowed() ? 1u : 0u,
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_semantic_pool_regression() noexcept {
        if constexpr (SoaKernel::kSemanticCapacity >= SoaKernel::kMaxNodes) {
            (void)out::println<"[soa] semantic_pool cap={} max_nodes={} overflow_test=skipped result=ok">(
                g_console,
                static_cast<unsigned>(SoaKernel::kSemanticCapacity),
                static_cast<unsigned>(SoaKernel::kMaxNodes));
            return true;
        }

        int fails = 0;
        static SoaKernel probe{};
        WidgetHandle first{};
        for (std::size_t i = 0; i < SoaKernel::kSemanticCapacity; ++i) {
            const WidgetHandle node = probe.create(WidgetKind::Container);
            expect_true(static_cast<bool>(node), "semantic pool: node allocation failed", fails);
            if (!node) break;
            if (i == 0) first = node;
            probe.set_semantic(node, SemanticRole::Container, "s", "l");
            expect_true(probe.semantic_snapshot(node).found,
                        "semantic pool: admitted entry missing", fails);
        }

        expect_true(probe.semantic_live_count() == SoaKernel::kSemanticCapacity,
                    "semantic pool: live count mismatch", fails);
        expect_true(probe.semantic_peak_count() == SoaKernel::kSemanticCapacity,
                    "semantic pool: peak count mismatch", fails);
        expect_true(probe.semantic_alloc_fail() == 0,
                    "semantic pool: unexpected allocation failure", fails);

        probe.set_semantic(first, SemanticRole::Button, "updated", "Updated");
        probe.set_semantic_actions(first, 0);
        const auto updated = probe.semantic_snapshot(first);
        expect_true(updated.found && updated.actions == 0,
                    "semantic pool: existing slot update failed", fails);
        expect_true(probe.semantic_live_count() == SoaKernel::kSemanticCapacity,
                    "semantic pool: existing slot update changed live count", fails);

        const WidgetHandle overflow_node = probe.create(WidgetKind::Container);
        expect_true(static_cast<bool>(overflow_node),
                    "semantic pool: overflow node allocation failed", fails);
        probe.set_semantic_actions(
            overflow_node,
            semantic_action_mask(SemanticAction::Activate));
        expect_true(probe.semantic_live_count() == SoaKernel::kSemanticCapacity
                        && probe.semantic_alloc_fail() == 0,
                    "semantic pool: action-only write allocated a slot", fails);
        probe.set_semantic(overflow_node, SemanticRole::Container, "overflow", "Overflow");
        const auto rejected = probe.semantic_snapshot(overflow_node);
        expect_true(same_handle(rejected.handle, overflow_node) && !rejected.found,
                    "semantic pool: overflow silently installed entry", fails);
        const auto preserved = probe.semantic_snapshot(first);
        expect_true(preserved.found && preserved.actions == 0
                        && std::strcmp(preserved.id, "updated") == 0,
                    "semantic pool: overflow replaced an existing entry", fails);
        expect_true(probe.semantic_overflowed() && probe.semantic_alloc_fail() == 1,
                    "semantic pool: overflow evidence missing", fails);

        probe.clear_semantic(first);
        const auto cleared = probe.semantic_snapshot(first);
        expect_true(same_handle(cleared.handle, first) && !cleared.found,
                    "semantic pool: clear did not remove entry", fails);
        probe.set_semantic(overflow_node, SemanticRole::Container, "reused", "Reused");
        expect_true(probe.semantic_snapshot(overflow_node).found
                        && probe.semantic_live_count() == SoaKernel::kSemanticCapacity,
                    "semantic pool: cleared slot was not reused", fails);

        probe.destroy(overflow_node);
        const WidgetHandle replacement = probe.create(WidgetKind::Container);
        expect_true(static_cast<bool>(replacement),
                    "semantic pool: replacement node allocation failed", fails);
        probe.set_semantic(replacement, SemanticRole::Container, "replacement", "Replacement");
        expect_true(probe.semantic_snapshot(replacement).found
                        && probe.semantic_live_count() == SoaKernel::kSemanticCapacity,
                    "semantic pool: destroyed node did not release slot", fails);
        expect_true(probe.semantic_overflowed() && probe.semantic_alloc_fail() == 1,
                    "semantic pool: sticky overflow evidence was lost", fails);

        (void)out::println<"[soa] semantic_pool live={} peak={} cap={} bytes={} fail={} overflow={} result={}">(
            g_console,
            static_cast<unsigned>(probe.semantic_live_count()),
            static_cast<unsigned>(probe.semantic_peak_count()),
            static_cast<unsigned>(SoaKernel::kSemanticCapacity),
            static_cast<unsigned>(SoaKernel::kSemanticPoolBytes),
            static_cast<unsigned>(probe.semantic_alloc_fail()),
            probe.semantic_overflowed() ? 1u : 0u,
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_payload_owner_regression() noexcept {
        int fails = 0;

        soa_detail::PayloadPool<soa_detail::LabelPayload, 1> pool{};
        pool.reset();
        constexpr std::uint16_t first_owner = 7;
        constexpr std::uint16_t second_owner = 9;
        const auto stale_slot = pool.alloc(first_owner, WidgetKind::Label);
        expect_true(soa_detail::payload_slot_valid(stale_slot),
                    "payload owner: initial allocation failed", fails);
        expect_true(pool.get(stale_slot, first_owner, WidgetKind::Label) != nullptr,
                    "payload owner: initial owner lookup failed", fails);

        pool.free(stale_slot, first_owner, WidgetKind::Label);
        const auto active_slot = pool.alloc(second_owner, WidgetKind::Label);
        expect_true(active_slot == stale_slot,
                    "payload owner: fixed slot was not reused", fails);
        expect_true(pool.get(stale_slot, first_owner, WidgetKind::Label) == nullptr,
                    "payload owner: stale owner reached reused slot", fails);
        pool.free(stale_slot, first_owner, WidgetKind::Label);
        expect_true(pool.get(active_slot, second_owner, WidgetKind::Label) != nullptr,
                    "payload owner: stale free released current owner", fails);
        pool.free(active_slot, second_owner, WidgetKind::Label);

        static SoaKernel probe{};
        const WidgetHandle first = probe.create(WidgetKind::Label);
        expect_true(static_cast<bool>(first),
                    "payload owner: first node allocation failed", fails);
        probe.set_text(first, "old");
        probe.destroy(first);
        const WidgetHandle replacement = probe.create(WidgetKind::Label);
        expect_true(static_cast<bool>(replacement)
                        && replacement.index == first.index
                        && replacement.generation != first.generation,
                    "payload owner: node slot generation did not advance", fails);
        expect_true(std::strcmp(probe.text(replacement), "") == 0,
                    "payload owner: replacement retained stale payload", fails);
        probe.set_text(first, "stale");
        expect_true(std::strcmp(probe.text(replacement), "") == 0,
                    "payload owner: stale widget handle changed replacement", fails);
        probe.set_text(replacement, "new");
        expect_true(std::strcmp(probe.text(replacement), "new") == 0,
                    "payload owner: replacement payload write failed", fails);
        probe.destroy(replacement);

        (void)out::println<"[soa] payload_owner slot_bytes={} stale_read=0 stale_free=0 node_reuse=1 result={}">(
            g_console,
            static_cast<unsigned>(SoaKernel::kNodeStorageSlotBytes),
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_node_runtime_state_regression() noexcept {
        int fails = 0;
        static SoaKernel probe{};
        WidgetHandle node = probe.create(WidgetKind::TabView);
        expect_true(static_cast<bool>(node),
                    "node runtime state: allocation failed", fails);
        expect_true(!probe.hovered(node) && !probe.pressed(node) && !probe.focused(node)
                        && !probe.segmented_underline(node),
                    "node runtime state: defaults mismatch", fails);

        probe.set_segmented_underline(node, true);
        std::size_t cases = 0;
        for (std::uint8_t value = 0; value < 8; ++value) {
            const bool hovered = (value & 1u) != 0;
            const bool pressed = (value & 2u) != 0;
            const bool focused = (value & 4u) != 0;
            probe.set_hovered(node, hovered);
            probe.set_pressed(node, pressed);
            probe.set_focused(node, focused);

            std::uint8_t expected = static_cast<std::uint8_t>(SoaStateMask::Enabled);
            if (hovered) expected = static_cast<std::uint8_t>(expected | static_cast<std::uint8_t>(SoaStateMask::Hovered));
            if (pressed) expected = static_cast<std::uint8_t>(expected | static_cast<std::uint8_t>(SoaStateMask::Pressed));
            if (focused) expected = static_cast<std::uint8_t>(expected | static_cast<std::uint8_t>(SoaStateMask::Focused));
            expect_true(probe.state_compact(node).bits == expected,
                        "node runtime state: interaction combination mismatch", fails);
            expect_true(probe.segmented_underline(node),
                        "node runtime state: interaction write changed presentation", fails);
            ++cases;
        }

        const auto interaction_before_lifecycle = probe.state_compact(node).bits;
        probe.set_visible(node, false);
        probe.set_focusable(node, true);
        probe.set_hit_testable(node, false);
        probe.set_clip_children(node, true);
        expect_true(!probe.visible(node)
                        && probe.focusable(node)
                        && !probe.hit_testable(node)
                        && probe.clip_children(node)
                        && probe.state_compact(node).bits == interaction_before_lifecycle
                        && probe.segmented_underline(node),
                    "node runtime state: lifecycle write changed interaction or presentation", fails);

        probe.set_visible(node, true);
        probe.set_focusable(node, false);
        probe.set_hit_testable(node, true);
        probe.set_clip_children(node, false);
        probe.set_enabled(node, false);
        const auto enabled_mask = static_cast<std::uint8_t>(SoaStateMask::Enabled);
        expect_true(probe.visible(node)
                        && !probe.focusable(node)
                        && probe.hit_testable(node)
                        && !probe.clip_children(node)
                        && probe.state_compact(node).bits
                            == static_cast<std::uint8_t>(interaction_before_lifecycle & ~enabled_mask)
                        && probe.hovered(node)
                        && probe.pressed(node)
                        && probe.focused(node)
                        && probe.segmented_underline(node),
                    "node runtime state: enabled write changed unrelated packed state", fails);
        probe.set_enabled(node, true);
        expect_true(probe.state_compact(node).bits == interaction_before_lifecycle,
                    "node runtime state: enabled bit did not restore", fails);

        const auto before_mode_clear = probe.state_compact(node).bits;
        probe.set_segmented_underline(node, false);
        expect_true(!probe.segmented_underline(node)
                        && probe.state_compact(node).bits == before_mode_clear,
                    "node runtime state: presentation write changed interaction", fails);

        probe.destroy(node);
        node = probe.create(WidgetKind::TabView);
        expect_true(static_cast<bool>(node)
                        && !probe.hovered(node)
                        && !probe.pressed(node)
                        && !probe.focused(node)
                        && !probe.segmented_underline(node),
                    "node runtime state: reused node retained stale bits", fails);
        probe.destroy(node);

        SoaFactory factory{probe};
        const WidgetHandle navigation = factory.create_navigation_bar();
        expect_true(static_cast<bool>(navigation) && probe.segmented_underline(navigation),
                    "node runtime state: navigation factory lost underline mode", fails);
        probe.destroy(navigation);

        (void)out::println<"[soa] node_runtime_state bytes={} segmented_payload_bytes={} cases={} lifecycle=1 isolation=1 reuse=1 navigation=1 result={}">(
            g_console,
            static_cast<unsigned>(SoaKernel::kNodeRuntimeStateBytes),
            static_cast<unsigned>(sizeof(soa_detail::SegmentedControlPayload)),
            static_cast<unsigned>(cases),
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_style_class_id_regression() noexcept {
        int fails = 0;
        static SoaKernel probe{};
        WidgetHandle node = probe.create(WidgetKind::Button);
        expect_true(static_cast<bool>(node)
                        && probe.style_class(node) == kStyleClassInvalid,
                    "style class id: allocation or default mismatch", fails);

        if constexpr (kStyleClassMax > 1) {
            const auto max_id = static_cast<StyleClassId>(kStyleClassMax - 1u);
            probe.set_style_class(node, max_id);
            expect_true(probe.style_class(node) == max_id,
                        "style class id: maximum admitted id mismatch", fails);
            probe.clear_style_class(node);
            expect_true(probe.style_class(node) == kStyleClassInvalid,
                        "style class id: clear mismatch", fails);

            probe.set_style_class(node, static_cast<StyleClassId>(1));
            const WidgetHandle stale = node;
            probe.destroy(node);
            node = probe.create(WidgetKind::Button);
            expect_true(static_cast<bool>(node)
                            && node.index == stale.index
                            && node.generation != stale.generation
                            && probe.style_class(node) == kStyleClassInvalid,
                        "style class id: reused node retained stale class", fails);
        }
        probe.destroy(node);

        (void)out::println<"[soa] style_class_id bytes={} max={} invalid=0 reuse=1 result={}">(
            g_console,
            static_cast<unsigned>(SoaKernel::kNodeStyleClassBytes),
            static_cast<unsigned>(kStyleClassMax),
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_layout_text_state_regression() noexcept {
        int fails = 0;
        static SoaKernel probe{};
        WidgetHandle node = probe.create(WidgetKind::Container);
        expect_true(static_cast<bool>(node), "layout text state: node allocation failed", fails);
        expect_true(probe.layout_kind(node) == SoaLayoutKind::None
                        && probe.text_align_h(node) == TextAlignH::Left
                        && probe.text_align_v(node) == TextAlignV::Center,
                    "layout text state: defaults mismatch", fails);

        constexpr std::array layouts{SoaLayoutKind::None, SoaLayoutKind::List};
        constexpr std::array align_h{TextAlignH::Left, TextAlignH::Center, TextAlignH::Right};
        constexpr std::array align_v{TextAlignV::Top, TextAlignV::Center, TextAlignV::Bottom};
        std::size_t cases = 0;
        for (const auto layout : layouts) {
            for (const auto h : align_h) {
                for (const auto v : align_v) {
                    probe.set_layout_kind(node, layout);
                    probe.set_text_align(node, h, v);
                    expect_true(probe.layout_kind(node) == layout
                                    && probe.text_align_h(node) == h
                                    && probe.text_align_v(node) == v,
                                "layout text state: combination mismatch", fails);
                    ++cases;
                }
            }
        }

        probe.set_text_align(node, TextAlignH::Right, TextAlignV::Bottom);
        probe.set_layout_kind(node, SoaLayoutKind::List);
        expect_true(probe.text_align_h(node) == TextAlignH::Right
                        && probe.text_align_v(node) == TextAlignV::Bottom,
                    "layout text state: layout update changed alignment", fails);
        probe.set_text_align(node, TextAlignH::Center, TextAlignV::Top);
        expect_true(probe.layout_kind(node) == SoaLayoutKind::List,
                    "layout text state: alignment update changed layout", fails);

        probe.destroy(node);
        node = probe.create(WidgetKind::Container);
        expect_true(static_cast<bool>(node)
                        && probe.layout_kind(node) == SoaLayoutKind::None
                        && probe.text_align_h(node) == TextAlignH::Left
                        && probe.text_align_v(node) == TextAlignV::Center,
                    "layout text state: reused node retained stale bits", fails);

        (void)out::println<"[soa] layout_text_state bytes={} cases={} defaults=1 isolation=1 result={}">(
            g_console,
            static_cast<unsigned>(SoaKernel::kNodeLayoutTextStateBytes),
            static_cast<unsigned>(cases),
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_child_topology_regression() noexcept {
        int fails = 0;
        static SoaKernel probe{};
        const WidgetHandle parent_a = probe.create(WidgetKind::Container);
        const WidgetHandle parent_b = probe.create(WidgetKind::Container);
        const WidgetHandle child_a = probe.create(WidgetKind::Container);
        const WidgetHandle child_b = probe.create(WidgetKind::Container);
        const WidgetHandle child_c = probe.create(WidgetKind::Container);
        const bool allocated = parent_a && parent_b && child_a && child_b && child_c;
        expect_true(allocated, "child topology: node allocation failed", fails);

        if (allocated) {
            expect_true(probe.child_count(parent_a) == 0 && probe.child_count(parent_b) == 0,
                        "child topology: initial count mismatch", fails);
            expect_true(probe.link(parent_a, child_a)
                            && probe.link(parent_a, child_b)
                            && probe.link(parent_a, child_c),
                        "child topology: initial link failed", fails);
            expect_true(probe.child_count(parent_a) == 3
                            && same_handle(probe.first_child(parent_a), child_a)
                            && same_handle(probe.last_child(parent_a), child_c),
                        "child topology: linked count or endpoints mismatch", fails);

            expect_true(probe.unlink(parent_a, child_b),
                        "child topology: unlink failed", fails);
            expect_true(probe.child_count(parent_a) == 2
                            && !probe.parent(child_b)
                            && same_handle(probe.next_sibling(child_a), child_c)
                            && same_handle(probe.prev_sibling(child_c), child_a),
                        "child topology: unlink did not preserve sibling truth", fails);

            expect_true(probe.link(parent_b, child_b),
                        "child topology: second-parent link failed", fails);
            expect_true(probe.link(parent_b, child_c),
                        "child topology: reparent failed", fails);
            expect_true(probe.child_count(parent_a) == 1
                            && probe.child_count(parent_b) == 2
                            && same_handle(probe.parent(child_c), parent_b)
                            && same_handle(probe.first_child(parent_b), child_b)
                            && same_handle(probe.last_child(parent_b), child_c),
                        "child topology: reparent count mismatch", fails);

            probe.destroy(child_a);
            expect_true(probe.child_count(parent_a) == 0
                            && !probe.first_child(parent_a)
                            && !probe.last_child(parent_a),
                        "child topology: child destroy did not detach", fails);

            const WidgetHandle stale_parent = parent_b;
            probe.destroy(parent_b);
            expect_true(probe.child_count(stale_parent) == 0
                            && !probe.parent(child_b)
                            && !probe.parent(child_c),
                        "child topology: parent destroy did not detach children", fails);

            const WidgetHandle replacement = probe.create(WidgetKind::Container);
            expect_true(replacement
                            && replacement.index == stale_parent.index
                            && replacement.generation != stale_parent.generation
                            && probe.child_count(replacement) == 0
                            && !probe.first_child(replacement)
                            && !probe.last_child(replacement),
                        "child topology: reused node retained stale links", fails);
            probe.destroy(replacement);
        }

        probe.destroy(child_c);
        probe.destroy(child_b);
        probe.destroy(child_a);
        probe.destroy(parent_b);
        probe.destroy(parent_a);

        (void)out::println<"[soa] child_topology storage=derived link=1 unlink=1 reparent=1 destroy=1 reuse=1 result={}">(
            g_console,
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_traversal_workspace_regression() noexcept {
        int fails = 0;
        static SoaKernel probe{};
        const WidgetHandle root = probe.create(WidgetKind::Container);
        expect_true(static_cast<bool>(root), "traversal workspace: root allocation failed", fails);

        {
            const auto held = probe.acquire_traversal(SoaKernel::TraversalPhase::Render);
            expect_true(static_cast<bool>(held), "traversal workspace: initial lease failed", fails);
            if (held) {
                expect_true(held.stack().size() == SoaKernel::kMaxNodes,
                            "traversal workspace: capacity mismatch", fails);
            }
            const auto blocked = probe.semantic_tree_snapshot(root);
            expect_true(blocked.overflowed && blocked.visited_count == 0,
                        "traversal workspace: conflicting phase was not rejected", fails);
            expect_true(probe.traversal_phase_conflicted() && probe.workspace_overflowed(),
                        "traversal workspace: conflict evidence missing", fails);
        }

        const auto recovered = probe.semantic_tree_snapshot(root);
        expect_true(!recovered.overflowed && recovered.visited_count == 1,
                    "traversal workspace: lease release did not restore traversal", fails);

        (void)out::println<"[soa] traversal_workspace frame={} bytes={} cap={} conflict={} overflow={} result={}">(
            g_console,
            static_cast<unsigned>(sizeof(SoaKernel::TraversalFrame)),
            static_cast<unsigned>(SoaKernel::kTraversalWorkspaceBytes),
            static_cast<unsigned>(SoaKernel::kMaxNodes),
            probe.traversal_phase_conflicted() ? 1u : 0u,
            probe.workspace_overflowed() ? 1u : 0u,
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_rect_truth_regression() noexcept {
        int fails = 0;
        static SoaKernel probe{};
        const WidgetHandle node = probe.create(WidgetKind::Container);
        expect_true(static_cast<bool>(node), "rect truth: node allocation failed", fails);
        probe.set_input_root(node);
        probe.set_visible(node, true);
        probe.set_enabled(node, true);
        probe.set_hit_testable(node, true);

        probe.set_rect(node, {10, 10, 20, 20});
        const WidgetHandle initial_hit = probe.input_hit_test(15, 15);
        expect_true(same_handle(initial_hit, node), "rect truth: initial bounds not hittable", fails);

        probe.set_rect(node, {100, 100, 20, 20});
        const WidgetHandle stale_hit = probe.input_hit_test(15, 15);
        const WidgetHandle moved_hit = probe.input_hit_test(105, 105);
        expect_true(!stale_hit, "rect truth: old bounds remained hittable after move", fails);
        expect_true(same_handle(moved_hit, node), "rect truth: moved bounds not hittable", fails);
        expect_true(probe.rect(node).x == 100 && probe.rect(node).y == 100,
                    "rect truth: current geometry was not retained", fails);
        expect_true(!probe.workspace_overflowed() && !probe.traversal_phase_conflicted(),
                    "rect truth: hit-test workspace evidence failed", fails);

        probe.destroy(node);
        (void)out::println<"[soa] rect_truth initial={} stale={} moved={} result={}">(
            g_console,
            initial_hit ? 1u : 0u,
            stale_hit ? 1u : 0u,
            moved_hit ? 1u : 0u,
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    bool run_workspace_regression(SoaGui& gui,
                                  SoaKernel& kernel,
                                  SoaFactory& factory,
                                  WidgetHandle root,
                                  ui::draw_cmd::DefaultDrawCmdBuffer& probe_buffer) noexcept {
        int fails = 0;
        static std::array<WidgetHandle, SoaKernel::kMaxNodes> chain{};
        chain.fill({});

        WidgetHandle parent = root;
        std::size_t chain_count = 0;
        while (chain_count < chain.size()) {
            const WidgetHandle node = factory.create_container();
            if (!node) break;
            if (!factory.link(parent, node)) {
                kernel.destroy(node);
                expect_true(false, "workspace: failed to link deep chain", fails);
                break;
            }
            kernel.set_rect(node, {0, 0, 2, 2});
            chain[chain_count++] = node;
            parent = node;
        }

        expect_true(chain_count >= SoaKernel::kMaxNodes / 2,
                    "workspace: chain did not approach soa_max_nodes", fails);
        if constexpr (SoaKernel::kMaxNodes > 256) {
            expect_true(chain_count > 256,
                        "workspace: chain did not cross legacy 256-node limit", fails);
        }

        if (chain_count > 0) {
            constexpr const char* kLeafId = "workspace.deep.leaf";
            const WidgetHandle leaf = chain[chain_count - 1];
            kernel.set_hit_testable(leaf, true);
            kernel.set_focusable(leaf, true);
            kernel.set_semantic(leaf, SemanticRole::Container, kLeafId, "Deep workspace leaf");
            kernel.set_semantic_actions(leaf, semantic_action_mask(SemanticAction::Activate));

            const auto cmd_stats = gui.record_commands(probe_buffer);
            const WidgetHandle hit = gui.hit_test(1, 1);
            const auto semantic = kernel.resolve_semantic_intent(root, kLeafId, SemanticAction::Activate);

            expect_true(!cmd_stats.cmd_overflowed, "workspace: draw command overflow", fails);
            expect_true(!cmd_stats.text_overflowed, "workspace: draw text overflow", fails);
            expect_true(!cmd_stats.blob_overflowed, "workspace: draw blob overflow", fails);
            expect_true(same_handle(hit, leaf), "workspace: deep hit-test truncated", fails);
            expect_true(semantic.found && same_handle(semantic.handle, leaf),
                        "workspace: deep semantic traversal truncated", fails);
            expect_true(!kernel.workspace_overflowed(), "workspace: traversal overflowed", fails);
        }

        for (std::size_t i = chain_count; i > 0; --i) {
            kernel.destroy(chain[i - 1]);
        }

        (void)out::println<"[soa] workspace chain={} max={} overflow={} conflict={} result={}">(
            g_console,
            static_cast<unsigned>(chain_count),
            static_cast<unsigned>(SoaKernel::kMaxNodes),
            kernel.workspace_overflowed() ? 1u : 0u,
            kernel.traversal_phase_conflicted() ? 1u : 0u,
            fails == 0 ? "ok" : "fail");
        return fails == 0;
    }

    void dump_payload_stats(const soa_detail::PayloadStats& stats) {
        if (g_payload_stats_dumped) return;
        g_payload_stats_dumped = true;
        auto dump = [&](const char* name, const soa_detail::PayloadPoolStats& s) {
            (void)out::println<"[soa] payload {} cap={} peak={} fail={}">(
                g_console,
                name,
                static_cast<std::uint32_t>(s.cap),
                static_cast<std::uint32_t>(s.peak),
                static_cast<std::uint32_t>(s.alloc_fail));
        };
        dump("Label", stats.label);
        dump("Button", stats.button);
        dump("Image", stats.image);
        dump("TextInput", stats.text_input);
        dump("TextArea", stats.text_area);
        dump("NumberInput", stats.number_input);
        dump("SegmentedControl", stats.segmented);
        dump("ToggleGroup", stats.toggle_group);
        dump("Checkbox", stats.checkbox);
        dump("Radio", stats.radio);
        dump("ListItem", stats.list_item);
        dump("TextList", stats.text_list);
        dump("ListView", stats.list_view);
        dump("TableView", stats.table_view);
        dump("TreeView", stats.tree_view);
        dump("Stepper", stats.stepper);
        dump("NumberList", stats.number_list);
        dump("Roller", stats.roller);
        dump("Switch", stats.switcher);
        dump("Slider", stats.slider);
        dump("Progress", stats.progress);
        dump("ScrollBar", stats.scrollbar);
        dump("List", stats.list);
        dump("ScrollContainer", stats.scroll_container);
        dump("Spinner", stats.spinner);
        if (g_regress_log) {
            std::fprintf(g_regress_log,
                         "[soa] payload_overflowed=%u text_overflowed=%u\n",
                         stats.overflowed ? 1u : 0u,
                         stats.text_overflowed ? 1u : 0u);
        }
    }
#endif
}

int main(int argc, char** argv) {
    bool run_regress = false;
    bool run_regress_layout = false;
    bool run_regress_ui = false;
    bool use_tiles = false;
    bool use_bw1 = false;
    bool use_gray2 = false;
    bool use_eink = false;
    bool print_stats = false;
    bool show_perf_overlay = false;
    bool run_compare = false;
    bool run_dump = false;
    bool run_replay = false;
    bool run_ci = false;
    bool selftest_dedup = false;
    bool require_font_provider = false;
    bool require_fallback_font = false;
    bool require_utf8_replacement_disabled = false;
    int max_missing_glyphs = 0;
    int max_fallback_glyphs = 0;
    int max_utf8_replacements = 0;
    int max_text_draw = -1;
    int max_text_glyphs = -1;
    int max_text_pixels = -1;
    bool replay_use_tiles = false;
    bool replay_backend_set = false;
    bool run_screenshot = false;
    bool run_gif = false;
    int bw1_threshold = 128;
    int gray2_strength = 8;
    int compaction_union_factor = 8;
    ui::gfx::Gray2Curve gray2_curve = ui::gfx::Gray2Curve::Linear;
    int gif_frames = 1;
    std::uint16_t gif_delay_cs = 4;
    std::string dump_cmd_path{};
    std::string replay_cmd_path{};
    std::string screenshot_path{};
    std::string gif_path{};
#if defined(VIVID_SOA_TRACE_INPUT)
    char log_path[512]{};
    const char* temp_dir = std::getenv("TEMP");
    if (temp_dir && temp_dir[0]) {
        std::snprintf(log_path, sizeof(log_path), "%s\\soa_regress.log", temp_dir);
    } else {
        std::snprintf(log_path, sizeof(log_path), "soa_regress.log");
    }
    g_regress_log = std::fopen(log_path, "wb");
#endif
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--soa-tile") {
            use_tiles = true;
        } else if (arg == "--soa-stats") {
            print_stats = true;
            show_perf_overlay = true;
        } else if (arg == "--soa-compare") {
            run_compare = true;
        } else if (arg == "--soa-ci") {
            run_ci = true;
            run_compare = true;
            run_dump = true;
#if defined(VIVID_SOA_TRACE_INPUT)
            run_regress = true;
            run_regress_layout = true;
#endif
        } else if (arg == "--regress-ui") {
            run_regress_ui = true;
        } else if (arg == "--selftest-dedup") {
            selftest_dedup = true;
        } else if (arg == "--require-font-provider") {
            require_font_provider = true;
        } else if (arg == "--require-fallback-font") {
            require_fallback_font = true;
        } else if (arg == "--require-utf8-replace-disabled") {
            require_utf8_replacement_disabled = true;
        } else if (arg.rfind("--dump-cmd=", 0) == 0) {
            run_dump = true;
            dump_cmd_path = std::string(arg.substr(11));
        } else if (arg.rfind("--replay-cmd=", 0) == 0) {
            run_replay = true;
            replay_cmd_path = std::string(arg.substr(13));
        } else if (arg.rfind("--screenshot=", 0) == 0) {
            run_screenshot = true;
            screenshot_path = std::string(arg.substr(13));
        } else if (arg.rfind("--gif=", 0) == 0) {
            run_gif = true;
            gif_path = std::string(arg.substr(6));
        } else if (arg.rfind("--gif-frames=", 0) == 0) {
            gif_frames = std::atoi(std::string(arg.substr(13)).c_str());
        } else if (arg.rfind("--gif-delay=", 0) == 0) {
            gif_delay_cs = static_cast<std::uint16_t>(std::atoi(std::string(arg.substr(12)).c_str()));
        } else if (arg == "--bw1") {
            use_bw1 = true;
            use_tiles = true;
        } else if (arg.rfind("--bw1-threshold=", 0) == 0) {
            const int value = std::atoi(std::string(arg.substr(17)).c_str());
            bw1_threshold = (value < 0) ? 0 : (value > 255) ? 255 : value;
        } else if (arg == "--perf-overlay") {
            show_perf_overlay = true;
        } else if (arg == "--gray2") {
            use_gray2 = true;
            use_tiles = true;
        } else if (arg.rfind("--gray2-strength=", 0) == 0) {
            const int value = std::atoi(std::string(arg.substr(17)).c_str());
            gray2_strength = (value < 0) ? 0 : (value > 64) ? 64 : value;
        } else if (arg.rfind("--compaction-union-factor=", 0) == 0) {
            const int value = std::atoi(std::string(arg.substr(27)).c_str());
            compaction_union_factor = (value < 1) ? 1 : value;
        } else if (arg.rfind("--gray2-curve=", 0) == 0) {
            const std::string_view value = std::string_view(arg).substr(14);
            if (value == "soft") {
                gray2_curve = ui::gfx::Gray2Curve::Soft;
            } else if (value == "contrast") {
                gray2_curve = ui::gfx::Gray2Curve::Contrast;
            } else {
                gray2_curve = ui::gfx::Gray2Curve::Linear;
            }
        } else if (arg.rfind("--max-missing-glyphs=", 0) == 0) {
            max_missing_glyphs = std::atoi(std::string(arg.substr(21)).c_str());
        } else if (arg.rfind("--max-fallback-glyphs=", 0) == 0) {
            max_fallback_glyphs = std::atoi(std::string(arg.substr(22)).c_str());
        } else if (arg.rfind("--max-utf8-replace=", 0) == 0) {
            max_utf8_replacements = std::atoi(std::string(arg.substr(20)).c_str());
        } else if (arg.rfind("--max-text-draw=", 0) == 0) {
            max_text_draw = std::atoi(std::string(arg.substr(16)).c_str());
        } else if (arg.rfind("--max-text-glyphs=", 0) == 0) {
            max_text_glyphs = std::atoi(std::string(arg.substr(18)).c_str());
        } else if (arg.rfind("--max-text-pixels=", 0) == 0) {
            max_text_pixels = std::atoi(std::string(arg.substr(18)).c_str());
        } else if (arg == "--eink") {
            use_eink = true;
            use_tiles = true;
        } else if (arg == "--backend=tile") {
            replay_use_tiles = true;
            replay_backend_set = true;
        } else if (arg == "--backend=full") {
            replay_use_tiles = false;
            replay_backend_set = true;
        }
#if defined(VIVID_SOA_TRACE_INPUT)
        else if (arg == "--soa-regress") {
            run_regress = true;
            run_regress_layout = true;
        } else if (arg == "--soa-regress-layout") {
            run_regress_layout = true;
        }
#endif
    }
    if (run_ci && dump_cmd_path.empty()) {
        const char* temp_dir = std::getenv("TEMP");
        if (temp_dir && temp_dir[0]) {
            dump_cmd_path = std::string(temp_dir) + "\\soa_ci.vcmd";
        } else {
            dump_cmd_path = "soa_ci.vcmd";
        }
    }
    if (run_replay && !replay_backend_set) {
        replay_use_tiles = use_tiles;
    }
#if !defined(VIVID_SOA_TRACE_INPUT)
    if (run_ci) {
        (void)out::println<"[soa-ci] ok=0 reason=trace_disabled">(g_console);
        return 1;
    }
#endif
    if (run_ci) {
        (void)out::println<"[soa] abi style_patch={} soa_kernel={} scene={} nodes={} node_storage_slot={} node_runtime_state={} node_style_class={} node_style_patch_slot={} node_semantic_slot={} layout_text_state={} semantic_slots={} semantic_pool={} style_patch_slots={} style_patch_pool={} traversal_frame={} traversal_workspace={}">(
            g_console,
            static_cast<unsigned long long>(sizeof(StylePatch)),
            static_cast<unsigned long long>(sizeof(SoaKernel)),
            static_cast<unsigned long long>(sizeof(::ui::scene::Scene)),
            static_cast<unsigned>(SoaKernel::kMaxNodes),
            static_cast<unsigned>(SoaKernel::kNodeStorageSlotBytes),
            static_cast<unsigned>(SoaKernel::kNodeRuntimeStateBytes),
            static_cast<unsigned>(SoaKernel::kNodeStyleClassBytes),
            static_cast<unsigned>(SoaKernel::kNodeStylePatchSlotBytes),
            static_cast<unsigned>(SoaKernel::kNodeSemanticSlotBytes),
            static_cast<unsigned>(SoaKernel::kNodeLayoutTextStateBytes),
            static_cast<unsigned>(SoaKernel::kSemanticCapacity),
            static_cast<unsigned>(SoaKernel::kSemanticPoolBytes),
            static_cast<unsigned>(SoaKernel::kStylePatchCapacity),
            static_cast<unsigned>(SoaKernel::kStylePatchPoolBytes),
            static_cast<unsigned>(sizeof(SoaKernel::TraversalFrame)),
            static_cast<unsigned>(SoaKernel::kTraversalWorkspaceBytes));
    }
#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_regress || run_regress_layout || run_regress_ui) {
        run_compare = true;
    }
#endif
#if defined(VIVID_SOA_TRACE_INPUT)
    if (g_regress_log) {
        std::fprintf(g_regress_log, "[soa] log_path=%s\n", log_path);
        std::fprintf(g_regress_log, "[soa] regress=%u layout=%u ui=%u\n",
                     run_regress ? 1u : 0u,
                     run_regress_layout ? 1u : 0u,
                     run_regress_ui ? 1u : 0u);
    }
    auto close_regress_log = []() noexcept {
        if (g_regress_log) {
            std::fclose(g_regress_log);
            g_regress_log = nullptr;
        }
    };
#endif
    g_selftest_dedup = selftest_dedup;
    apply_demo_theme();
    if (gif_frames <= 0) gif_frames = 1;
    const bool run_headless =
        run_regress || run_regress_layout || run_regress_ui || run_compare || run_dump || run_replay
        || run_screenshot || run_gif;
    if (run_headless && use_bw1) {
        (void)out::println<"[soa] bw1 disabled for headless runs">(g_console);
        use_bw1 = false;
    }

    ui::draw_cmd::set_compaction_union_factor(compaction_union_factor);
    if (run_headless && use_gray2) {
        (void)out::println<"[soa] gray2 disabled for headless runs">(g_console);
        use_gray2 = false;
    }
    if (run_headless && use_eink) {
        (void)out::println<"[soa] eink disabled for headless runs">(g_console);
        use_eink = false;
    }

    // Keep the large framebuffer off the stack to avoid stack overflow.
    static DefaultFrameBuffer fb{};
    DefaultCanvas canvas{fb};
    static std::array<std::byte, kTileBytes> tile_storage{};
    FrameBufferView tile_view{
        screen_pixel_format,
        tile_storage.data(),
        static_cast<std::size_t>(kTileWidth),
        static_cast<std::size_t>(kTileHeight),
        kTileStride
    };
    SdlTileBackend tile_backend{fb};
    tile_backend.src_format = tile_view.format;
    tile_backend.src_bpp = (tile_view.format == PixelFormat::RGB565) ? 2u
        : (tile_view.format == PixelFormat::RGB888) ? 3u
        : (tile_view.format == PixelFormat::ARGB8888) ? 4u
        : 0u;
    if (use_eink) {
        tile_backend.display.mode = ui::gfx::DisplayMode::Eink;
    } else if (use_gray2) {
        tile_backend.display.mode = ui::gfx::DisplayMode::Gray2;
    } else if (use_bw1) {
        tile_backend.display.mode = ui::gfx::DisplayMode::BW1;
    } else {
        tile_backend.display.mode = ui::gfx::DisplayMode::Color;
    }
    tile_backend.display.bw1_threshold = static_cast<std::uint8_t>(bw1_threshold);
    tile_backend.display.gray2_strength = static_cast<std::uint8_t>(gray2_strength);
    tile_backend.display.gray2_curve = gray2_curve;
    ui::draw_cmd::DrawCmdTileConfig tile_config{};
    tile_config.tile_width = kTileWidth;
    tile_config.tile_height = kTileHeight;
    tile_config.clear_color = kDemoBg;

    SoaKernel kernel{};
    SoaFactory factory{kernel};

    auto root = factory.create_container();
    kernel.set_rect(root, {0, 0, screen_width, screen_height});
    kernel.set_clip_children(root, true);

    auto title = factory.create_label("SoA Kernel Demo");
    auto btn = factory.create_button("Press");
    auto icon_btn = factory.create_icon_button();
    auto image_view = factory.create_image();
    auto spinner = factory.create_spinner();
    auto sw = factory.create_switch();
    auto checkbox = factory.create_checkbox("Checkbox");
    auto radio = factory.create_radio("Radio");
    auto text_box = factory.create_text_box("Read-only text box");
    auto segmented = factory.create_segmented_control();
    auto tab_bar = factory.create_tab_bar();
        auto slider = factory.create_slider();
        auto progress = factory.create_progress();
        auto progress_wheel = factory.create_progress_wheel();
        auto progress_simple = factory.create_progress_bar_simple();
        auto progress_round = factory.create_progress_bar_round();
        auto progress_flowing = factory.create_progress_flowing();
    auto toggle_group = factory.create_toggle_group();
    auto stepper = factory.create_stepper();
    auto number_list = factory.create_number_list();
    auto roller = factory.create_roller();
    auto tg_a = factory.create_checkbox("Option A");
    auto tg_b = factory.create_checkbox("Option B");
    auto tg_c = factory.create_checkbox("Option C");
    auto list_view = factory.create_icon_list();
    auto list_scroll = factory.create_scrollbar_for(list_view);
    auto text_list = factory.create_text_list();
    auto text_list_scroll = factory.create_scrollbar_for(text_list);
    auto scroll = factory.create_scroll_container();
    auto scroll_scroll = factory.create_scrollbar_for(scroll);
    auto table_view = factory.create_table_view();
    auto table_scroll_x = factory.create_scrollbar_for(table_view);
    auto tree_view = factory.create_tree_view();
    auto console_box = factory.create_console_box();
    auto menu = factory.create_menu();
    auto menu_item_a = factory.create_menu_item("New");
    auto menu_item_b = factory.create_menu_item("Open");
    auto menu_item_c = factory.create_menu_item("Save");

    ensure_demo_images();
    if (!ui::draw_cmd::image_registry_locked()) {
        ui::draw_cmd::set_image_registry_locked(true);
    }
    factory.set_button_icon(btn, g_test_icon_id);
    factory.set_button_icon_size(btn, 18);
    factory.set_button_icon(icon_btn, g_test_icon_id);
    factory.set_button_icon_size(icon_btn, 20);
    factory.set_image(image_view, g_slice_id);

    factory.link(root, title);
    factory.link(root, btn);
    factory.link(root, image_view);
    factory.link(root, spinner);
    factory.link(root, sw);
    factory.link(root, checkbox);
    factory.link(root, radio);
    factory.link(root, text_box);
    factory.link(root, segmented);
    factory.link(root, tab_bar);
    factory.link(root, toggle_group);
    factory.link(root, stepper);
    factory.link(root, number_list);
    factory.link(root, roller);
    factory.link(root, slider);
    factory.link(root, progress);
    factory.link(root, progress_wheel);
    factory.link(root, progress_simple);
    factory.link(root, progress_round);
    factory.link(root, progress_flowing);
    factory.link(root, list_view);
    factory.link(root, list_scroll);
    factory.link(root, text_list);
    factory.link(root, text_list_scroll);
    factory.link(root, scroll);
    factory.link(root, scroll_scroll);
    factory.link(root, table_view);
    factory.link(root, table_scroll_x);
    factory.link(root, tree_view);
    factory.link(root, console_box);
    factory.link(root, menu);
    factory.link(toggle_group, tg_a);
    factory.link(toggle_group, tg_b);
    factory.link(toggle_group, tg_c);
    factory.link(menu, menu_item_a);
    factory.link(menu, menu_item_b);
    factory.link(menu, menu_item_c);

    kernel.set_rect(title, {24, 16, screen_width - 48, 24});
    kernel.set_rect(btn, {24, 60, 160, 40});
    kernel.set_rect(icon_btn, {240, 60, 40, 40});
    kernel.set_rect(image_view, {200, 60, 32, 32});
    kernel.set_rect(spinner, {200, 104, 24, 24});
    kernel.set_rect(sw, {24, 112, 96, 32});
    kernel.set_rect(checkbox, {24, 160, 200, 32});
    kernel.set_rect(radio, {24, 200, 200, 32});
    kernel.set_rect(text_box, {320, 120, 160, 80});
    kernel.set_rect(segmented, {24, 240, 280, 32});
    kernel.set_rect(tab_bar, {24, 352, 280, 24});
    kernel.set_rect(slider, {24, 290, 280, 24});
    kernel.set_rect(progress, {24, 330, 280, 18});
    kernel.set_rect(progress_wheel, {320, 300, 48, 48});
    kernel.set_rect(progress_simple, {24, 356, 280, 10});
    kernel.set_rect(progress_round, {320, 360, 160, 10});
    kernel.set_rect(progress_flowing, {320, 374, 160, 8});
    kernel.set_rect(list_view, {24, 380, 200, 200});
    kernel.set_rect(list_scroll, {230, 380, 12, 200});
    kernel.set_rect(text_list, {250, 200, 200, 160});
    kernel.set_rect(text_list_scroll, {456, 200, 12, 160});
    kernel.set_rect(scroll, {250, 380, 200, 200});
    kernel.set_rect(scroll_scroll, {456, 380, 12, 200});
    kernel.set_rect(toggle_group, {250, 60, 200, 120});
    kernel.set_rect(tg_a, {0, 0, 200, 32});
    kernel.set_rect(tg_b, {0, 40, 200, 32});
    kernel.set_rect(tg_c, {0, 80, 200, 32});
    kernel.set_rect(stepper, {24, 600, 200, 48});
    kernel.set_rect(number_list, {24, 660, 120, 120});
    kernel.set_rect(roller, {160, 660, 120, 120});
    kernel.set_rect(table_view, {480, 60, 280, 144});
    kernel.set_rect(table_scroll_x, {480, 208, 280, 12});
    kernel.set_rect(tree_view, {480, 240, 280, 160});
    kernel.set_rect(console_box, {480, 420, 280, 140});
    kernel.set_rect(menu, {250, 600, 200, 120});
    kernel.set_rect(menu_item_a, {0, 0, 200, 28});
    kernel.set_rect(menu_item_b, {0, 36, 200, 28});
    kernel.set_rect(menu_item_c, {0, 72, 200, 28});

    kernel.set_range(slider, 0, 100);
    kernel.set_range(progress, 0, 100);
    kernel.set_range(progress_wheel, 0, 100);
    kernel.set_range(progress_simple, 0, 100);
    kernel.set_range(progress_round, 0, 100);
    kernel.set_range(progress_flowing, 0, 100);
    kernel.set_list_row_height(list_view, 28);
    kernel.set_list_row_height(text_list, 24);
    kernel.set_scroll_step(text_list, 24);
    kernel.set_list_row_height(table_view, 24);
    kernel.set_list_row_height(tree_view, 24);
    kernel.set_scroll_step(table_view, 24);
    kernel.set_scroll_step(tree_view, 24);
    kernel.set_scrollbar_orientation(list_scroll, ScrollBarOrientation::Vertical);
    kernel.set_scrollbar_orientation(text_list_scroll, ScrollBarOrientation::Vertical);
    kernel.set_scrollbar_orientation(scroll_scroll, ScrollBarOrientation::Vertical);
    kernel.set_scrollbar_orientation(table_scroll_x, ScrollBarOrientation::Horizontal);
    kernel.set_scrollbar_page_size(table_scroll_x, 280);
    factory.set_segmented_label(segmented, 0, "One");
    factory.set_segmented_label(segmented, 1, "Two");
    factory.set_segmented_label(segmented, 2, "Three");
    factory.set_segmented_selected(segmented, 1);
    factory.set_tab_bar_label(tab_bar, 0, "Home");
    factory.set_tab_bar_label(tab_bar, 1, "Stats");
    factory.set_tab_bar_label(tab_bar, 2, "Setup");
    factory.set_tab_bar_selected(tab_bar, 0);
    kernel.set_checked(menu_item_b, true);
    kernel.set_value(progress_round, 40);
    kernel.set_value(progress_flowing, 70);
    factory.set_stepper_count(stepper, 4);
    factory.set_stepper_label(stepper, 0, "Low");
    factory.set_stepper_label(stepper, 1, "Med");
    factory.set_stepper_label(stepper, 2, "High");
    factory.set_stepper_label(stepper, 3, "Max");
    factory.set_stepper_current(stepper, 1);
    factory.set_number_list_count(number_list, 24);
    factory.set_number_list_range(number_list, 0, 5);
    factory.set_number_list_selected(number_list, 3);
    factory.set_number_list_row_height(number_list, 24);
    factory.set_number_list_wheel_step(number_list, 24);
    const char* roller_items[] = {
        "One", "Two", "Three", "Four",
        "Five", "Six", "Seven", "Eight"
    };
    const RollerTextSource roller_source{
        roller_items,
        static_cast<std::uint16_t>(sizeof(roller_items) / sizeof(roller_items[0]))
    };
    factory.set_roller_source(roller, roller_source.count, &roller_source, &roller_text_at);
    factory.set_roller_selected(roller, 1);
    factory.set_roller_row_height(roller, 24);
    factory.set_roller_wheel_step(roller, 24);

    const char* list_view_items[] = {
        "Alpha", "Beta", "Gamma", "Delta",
        "Epsilon", "Zeta", "Eta", "Theta"
    };
    const ListViewTextSource list_view_source{
        list_view_items,
        static_cast<std::uint16_t>(sizeof(list_view_items) / sizeof(list_view_items[0]))
    };
    static const ListViewIconSource list_view_icons{
        g_test_icon_id,
        g_slice_id
    };
    factory.set_list_view_source(list_view, 1000, &list_view_source, &list_view_text_at);
    factory.set_list_view_icon_source(list_view, &list_view_icons, &list_view_icon_at, 18);
    factory.set_list_view_selected(list_view, 3);

    const char* table_rows[] = {
        "Alpha", "Beta", "Gamma", "Delta",
        "Epsilon", "Zeta", "Eta", "Theta"
    };
    const char* table_cols[] = {
        "ID", "Name", "State", "Value"
    };
    const TableViewTextSource table_source{
        table_rows,
        static_cast<std::uint16_t>(sizeof(table_rows) / sizeof(table_rows[0])),
        table_cols,
        static_cast<std::uint8_t>(sizeof(table_cols) / sizeof(table_cols[0]))
    };
    factory.set_table_view_source(table_view, 1000, 4, &table_source, &table_view_text_at);
    factory.set_table_view_header(table_view, &table_source, &table_view_header_at);
    factory.set_table_view_header_height(table_view, 20);
    factory.set_table_view_header_padding(table_view, 6);
    factory.set_table_view_header_style(table_view, TableViewHeaderStyle::Muted);
    factory.set_table_view_header_divider(table_view, true);
    factory.set_table_view_col_divider_style(table_view, TableViewColDividerStyle::Full);

    const char* tree_items[] = {
        "Root", "Alpha", "Beta", "Gamma",
        "Delta", "Epsilon", "Zeta", "Eta"
    };
    const TreeViewTextSource tree_source{
        tree_items,
        static_cast<std::uint16_t>(sizeof(tree_items) / sizeof(tree_items[0]))
    };
    const TreeViewIndentSource tree_indent{3};
    factory.set_tree_view_source(tree_view, 1000, &tree_source, &tree_view_text_at,
                                 &tree_indent, &tree_view_indent_at);

    factory.console_append(console_box, "[log] SoA kernel ready");
    factory.console_append(console_box, "[log] ui regression enabled");
    factory.console_append(console_box, "[log] cmd budget guard OK");

    const char* text_list_items[] = {
        "Alpha", "Beta", "Gamma", "Delta",
        "Epsilon", "Zeta", "Eta", "Theta",
        "Iota", "Kappa", "Lambda", "Mu"
    };
    const std::uint16_t text_list_count =
        static_cast<std::uint16_t>(sizeof(text_list_items) / sizeof(text_list_items[0]));
    factory.set_text_list_count(text_list, text_list_count);
    for (std::uint16_t i = 0; i < text_list_count; ++i) {
        factory.set_text_list_item(text_list, i, text_list_items[i]);
    }
    factory.set_text_list_selected(text_list, 2);

    const char* scroll_rows[] = {
        "Row 1", "Row 2", "Row 3", "Row 4", "Row 5", "Row 6",
        "Row 7", "Row 8", "Row 9", "Row 10", "Row 11", "Row 12"
    };
    int row_y = 0;
    for (const char* row_text : scroll_rows) {
        auto row = factory.create_label(row_text);
        factory.link(scroll, row);
        kernel.set_rect(row, {8, row_y, 160, 20});
        row_y += 22;
    }

    ui::draw_cmd::DefaultDrawCmdBuffer gui_cmd_buffer{};
    ui::draw_cmd::DefaultDrawCmdCompactionWorkspace gui_compaction_workspace{};
    ui::draw_cmd::DrawCmdExecutor gui_cmd_exec{};
    SoaGui gui(canvas,
               kernel,
               root,
               gui_cmd_buffer,
               gui_compaction_workspace,
               gui_cmd_exec);
    kernel.clear_workspace_overflow();

#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_replay) {
        const bool ok = replay_cmd_file(replay_cmd_path.c_str(),
                                        fb,
                                        canvas,
                                        tile_backend,
                                        tile_view,
                                        tile_config,
                                        replay_use_tiles,
                                        nullptr);
        close_regress_log();
        return ok ? 0 : 1;
    }
#else
    if (run_replay) {
        const bool ok = replay_cmd_file(replay_cmd_path.c_str(),
                                        fb,
                                        canvas,
                                        tile_backend,
                                        tile_view,
                                        tile_config,
                                        replay_use_tiles,
                                        nullptr);
        return ok ? 0 : 1;
    }
#endif

#if defined(VIVID_SOA_TRACE_INPUT)
    bool ci_ok = true;
    const char* ci_reason = nullptr;
    auto ci_mark_fail = [&](const char* reason) noexcept {
        if (!ci_ok) return;
        ci_ok = false;
        ci_reason = reason;
    };
    bool list_peak_ok = true;
    bool table_tree_ok = true;
    bool table_tree_ran = false;
    bool ui_ok = true;
    bool svg_workspace_ok = true;
    bool style_patch_pool_ok = true;
    bool semantic_pool_ok = true;
    bool payload_owner_ok = true;
    bool node_runtime_state_ok = true;
    bool style_class_id_ok = true;
    bool layout_text_state_ok = true;
    bool child_topology_ok = true;
    bool traversal_workspace_ok = true;
    bool rect_truth_ok = true;
    bool perf_overlay_runtime_ok = true;
    auto trace_regress_stage = [&](const char* stage) noexcept {
        (void)out::println<"[soa] regress stage={}">(g_console, stage);
        if (!g_regress_log) return;
        std::fprintf(g_regress_log, "[soa] regress_stage=%s\n", stage);
        std::fflush(g_regress_log);
    };
#endif

#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_ci) {
        svg_workspace_ok = run_svg_workspace_regression();
        (void)out::println<"[soa] svg_workspace ok={}">(
            g_console,
            svg_workspace_ok ? 1u : 0u);
        if (!svg_workspace_ok) {
            ci_mark_fail("svg_workspace");
        }
        style_patch_pool_ok = run_style_patch_pool_regression();
        if (!style_patch_pool_ok) {
            ci_mark_fail("style_patch_pool");
        }
        semantic_pool_ok = run_semantic_pool_regression();
        if (!semantic_pool_ok) {
            ci_mark_fail("semantic_pool");
        }
        payload_owner_ok = run_payload_owner_regression();
        if (!payload_owner_ok) {
            ci_mark_fail("payload_owner");
        }
        node_runtime_state_ok = run_node_runtime_state_regression();
        if (!node_runtime_state_ok) {
            ci_mark_fail("node_runtime_state");
        }
        style_class_id_ok = run_style_class_id_regression();
        if (!style_class_id_ok) {
            ci_mark_fail("style_class_id");
        }
        layout_text_state_ok = run_layout_text_state_regression();
        if (!layout_text_state_ok) {
            ci_mark_fail("layout_text_state");
        }
        child_topology_ok = run_child_topology_regression();
        if (!child_topology_ok) {
            ci_mark_fail("child_topology");
        }
        traversal_workspace_ok = run_traversal_workspace_regression();
        if (!traversal_workspace_ok) {
            ci_mark_fail("traversal_workspace");
        }
        rect_truth_ok = run_rect_truth_regression();
        if (!rect_truth_ok) {
            ci_mark_fail("rect_truth");
        }
        perf_overlay_runtime_ok = run_perf_overlay_runtime_regression();
        (void)out::println<"[soa] perf_overlay_runtime bytes={} ok={}">(
            g_console,
            static_cast<unsigned>(ui::perf_overlay_runtime::resident_bytes),
            perf_overlay_runtime_ok ? 1u : 0u);
        if (!perf_overlay_runtime_ok) {
            ci_mark_fail("perf_overlay_runtime");
        }
    }
    if (run_regress) {
        bool regress_ok = true;
        trace_regress_stage("input.begin");
        if (!run_input_regression(gui, kernel, factory, root)) {
            regress_ok = false;
        }
        trace_regress_stage("input.end");
        trace_regress_stage("style.begin");
        if (!run_style_regression(gui)) {
            regress_ok = false;
        }
        trace_regress_stage("style.end");
        trace_regress_stage("listview.begin");
        if (!run_list_view_regression(gui, kernel, factory, root, fb, canvas, tile_backend, tile_view, tile_config)) {
            regress_ok = false;
            list_peak_ok = false;
#if defined(VIVID_SOA_TRACE_INPUT)
            if (run_ci) {
                ci_mark_fail("listview");
            }
#endif
        }
        trace_regress_stage("listview.end");
        trace_regress_stage("table_tree.begin");
        if (!run_table_tree_regression(gui, kernel, factory, root)) {
            regress_ok = false;
#if defined(VIVID_SOA_TRACE_INPUT)
            table_tree_ok = false;
            if (run_ci) {
                ci_mark_fail("table_tree");
            }
#endif
        }
        trace_regress_stage("table_tree.end");
#if defined(VIVID_SOA_TRACE_INPUT)
        table_tree_ran = true;
#endif
        if (!regress_ok) {
#if defined(VIVID_SOA_TRACE_INPUT)
            if (run_ci) {
                ci_mark_fail("regress");
            } else {
                close_regress_log();
                return 1;
            }
#else
            close_regress_log();
            return 1;
#endif
        }
    }
    if (run_regress_layout) {
#if defined(VIVID_SOA_TRACE_INPUT)
        trace_regress_stage("layout.begin");
#endif
        const bool ok = run_layout_regression(gui, kernel, factory, root);
        if (!ok) {
#if defined(VIVID_SOA_TRACE_INPUT)
            if (run_ci) {
                ci_mark_fail("layout");
            } else {
                close_regress_log();
                return 1;
            }
#else
            close_regress_log();
            return 1;
#endif
        }
#if defined(VIVID_SOA_TRACE_INPUT)
        trace_regress_stage("layout.end");
#endif
    }
    if (run_regress_ui) {
#if defined(VIVID_SOA_TRACE_INPUT)
        trace_regress_stage("ui.begin");
#endif
        const bool ok = run_ui_regression(gui, kernel, factory, root);
        if (!ok) {
            ui_ok = false;
#if defined(VIVID_SOA_TRACE_INPUT)
            if (run_ci) {
                ci_mark_fail("ui");
            } else {
                close_regress_log();
                return 1;
            }
#else
            close_regress_log();
            return 1;
#endif
        }
#if defined(VIVID_SOA_TRACE_INPUT)
        if (!table_tree_ran) {
            const bool table_ok = run_table_tree_regression(gui, kernel, factory, root);
            table_tree_ran = true;
            if (!table_ok) {
                table_tree_ok = false;
                if (run_ci) {
                    ci_mark_fail("table_tree");
                } else {
                    close_regress_log();
                    return 1;
                }
            }
        }
        trace_regress_stage("ui.end");
#endif
    }
#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_regress) {
        trace_regress_stage("workspace.begin");
        if (!run_workspace_regression(gui, kernel, factory, root, gui_cmd_buffer)) {
            if (run_ci) {
                ci_mark_fail("workspace_regression");
            } else {
                close_regress_log();
                return 1;
            }
        }
        trace_regress_stage("workspace.end");
    }
#endif
#endif
    ui::draw_cmd::DefaultDrawCmdBuffer compare_buf{};
    bool has_recorded = false;
    ui::draw_cmd::ImageRegistryStats img_stats_before_record{};
    ui::draw_cmd::ImageRegistryStats img_stats_after_record{};
    bool img_stats_valid = false;
    bool img_growth_ok = true;
    std::uint32_t img_growth_count = 0;
    bool img_dedup_ok = true;
    std::uint32_t img_growth_record = 0;
    std::uint32_t img_growth_compact = 0;
    std::uint32_t img_growth_execute = 0;
    std::uint32_t missing_glyphs = 0;
    std::uint32_t missing_glyph_fallbacks = 0;
    std::uint32_t utf8_replacements = 0;
    TextProfileSample text_profile{};
    std::size_t compare_cmd_count_raw = 0;
    if (run_compare || run_dump) {
#if defined(VIVID_SOA_TRACE_INPUT)
        reset_font_ptr_map_count();
        reset_missing_glyph_stats();
        text_profile_reset();
#endif
        if (selftest_dedup) {
            ensure_demo_images();
            (void)ui::draw_cmd::register_image_dedup(
                kTestIconView,
                ui::draw_cmd::ImageRegisterReason::SelfTest,
                "selftest_dedup");
            (void)ui::draw_cmd::register_image_dedup(
                kTestIconView,
                ui::draw_cmd::ImageRegisterReason::SelfTest,
                "selftest_dedup");
            (void)ui::draw_cmd::register_image_dedup(
                kSliceView,
                ui::draw_cmd::ImageRegisterReason::SelfTest,
                "selftest_dedup");
            (void)ui::draw_cmd::register_image_dedup(
                kSliceView,
                ui::draw_cmd::ImageRegisterReason::SelfTest,
                "selftest_dedup");
        }
        img_stats_before_record = ui::draw_cmd::image_registry_stats();
        gui.record_commands(compare_buf);
        append_path_icon(compare_buf, screen_width);
        append_compaction_probe(compare_buf, screen_width, screen_height);
        compare_cmd_count_raw = compare_buf.stats().cmd_count;
        compare_buf.compact(gui_compaction_workspace);
        has_recorded = true;
        img_stats_after_record = ui::draw_cmd::image_registry_stats();
        img_stats_valid = true;
        if (img_stats_after_record.register_new_after_lock >= img_stats_before_record.register_new_after_lock) {
            img_growth_count = img_stats_after_record.register_new_after_lock
                - img_stats_before_record.register_new_after_lock;
        } else {
            img_growth_count = img_stats_after_record.register_new_after_lock;
        }
        if (img_stats_after_record.register_new_record >= img_stats_before_record.register_new_record) {
            img_growth_record = img_stats_after_record.register_new_record
                - img_stats_before_record.register_new_record;
        } else {
            img_growth_record = img_stats_after_record.register_new_record;
        }
        if (img_stats_after_record.register_new_compact >= img_stats_before_record.register_new_compact) {
            img_growth_compact = img_stats_after_record.register_new_compact
                - img_stats_before_record.register_new_compact;
        } else {
            img_growth_compact = img_stats_after_record.register_new_compact;
        }
        if (img_stats_after_record.register_new_execute >= img_stats_before_record.register_new_execute) {
            img_growth_execute = img_stats_after_record.register_new_execute
                - img_stats_before_record.register_new_execute;
        } else {
            img_growth_execute = img_stats_after_record.register_new_execute;
        }
        img_growth_ok = (img_growth_count == 0);
#if defined(VIVID_SOA_TRACE_INPUT)
        const auto font_ptr_maps = static_cast<unsigned>(font_ptr_map_count());
        (void)out::println<"[soa] font ptr map count={}">(g_console, font_ptr_maps);
        missing_glyphs = missing_glyph_count();
        missing_glyph_fallbacks = missing_glyph_fallback_count();
        utf8_replacements = utf8_replacement_count();
        (void)out::println<"[soa] glyph missing={} fallback={} utf8_replace={}">(
            g_console,
            static_cast<unsigned>(missing_glyphs),
            static_cast<unsigned>(missing_glyph_fallbacks),
            static_cast<unsigned>(utf8_replacements));
        (void)out::println<"[soa] text draw={} glyphs={} pixels={}">(
            g_console,
            static_cast<unsigned long long>(text_profile.draw_calls),
            static_cast<unsigned long long>(text_profile.glyphs),
            static_cast<unsigned long long>(text_profile.pixels));
        if (g_regress_log) {
            std::fprintf(g_regress_log, "[soa] font_ptr_map_count=%u\n", font_ptr_maps);
            std::fprintf(g_regress_log,
                         "[soa] glyph_missing=%u glyph_fallback=%u utf8_replace=%u\n",
                         static_cast<unsigned>(missing_glyphs),
                         static_cast<unsigned>(missing_glyph_fallbacks),
                         static_cast<unsigned>(utf8_replacements));
            std::fprintf(g_regress_log,
                         "[soa] text_draw=%llu text_glyphs=%llu text_pixels=%llu\n",
                         static_cast<unsigned long long>(text_profile.draw_calls),
                         static_cast<unsigned long long>(text_profile.glyphs),
                         static_cast<unsigned long long>(text_profile.pixels));
        }
#endif
    }

    std::uint32_t compare_hash_full = 0;
    std::uint32_t compare_hash_tile = 0;
    std::size_t compare_failed_cmds = 0;
    std::size_t compare_cmd_count = 0;
    std::size_t compare_cmd_capacity = 0;
    std::size_t compare_cmd_bytes = 0;
    std::size_t compare_cmd_saved = 0;
    std::uint8_t compare_cmd_saved_pct = 0;
    std::size_t compare_dispatch_groups = 0;
    std::size_t compare_batch_flushes = 0;
    std::size_t compare_batch_shrink = 0;
    std::size_t compare_batch_shrink_line = 0;
    std::size_t compare_batch_shrink_path = 0;
    std::size_t compare_batch_shrink_rect = 0;
    std::size_t compare_batch_shrink_round = 0;
    std::size_t compare_batch_shrink_image = 0;
    std::size_t compare_batch_shrink_focus = 0;
    int compare_tile_flushes = 0;
    std::uint8_t compare_tile_hit_pct = 0;
    std::size_t compare_tile_dispatch_groups = 0;
    std::size_t compare_tile_batch_flushes = 0;
    std::size_t compare_tile_failed_cmds = 0;
    std::size_t compare_group_rect = 0;
    std::size_t compare_group_text = 0;
    std::size_t compare_group_image = 0;
    std::size_t compare_group_line = 0;
    std::size_t compare_group_path = 0;
    std::size_t compare_group_other = 0;
    std::size_t compare_cmd_rect = 0;
    std::size_t compare_cmd_text = 0;
    std::size_t compare_cmd_image = 0;
    std::size_t compare_cmd_line = 0;
    std::size_t compare_cmd_path = 0;
    std::size_t compare_cmd_other = 0;
    std::size_t compare_fail_text = 0;
    std::size_t compare_fail_image = 0;
    std::size_t compare_fail_blob = 0;
    std::size_t compare_fail_path = 0;
    std::size_t compare_fail_clip = 0;
    std::size_t compare_fail_other = 0;
    std::size_t compare_clip_push_overflow = 0;
    std::size_t compare_clip_pop_underflow = 0;
    std::size_t compare_clip_invalid = 0;
    bool compare_ok = true;
    bool compact_ok = true;
    std::size_t compact_saved = 0;

    if (run_compare) {
#if defined(VIVID_SOA_TRACE_INPUT)
        trace_regress_stage("compare.begin");
#endif
        {
            ui::draw_cmd::DefaultDrawCmdBuffer compact_probe{};
            compact_probe.fill_rect({8, 8, 24, 6}, kDemoPanel);
            compact_probe.fill_rect({8, 16, 24, 6}, kDemoPanel);
            compact_probe.stroke_rect({40, 8, 24, 6}, kDemoPanelBorder);
            compact_probe.stroke_rect({40, 16, 24, 6}, kDemoPanelBorder);
            compact_probe.fill_round_rect({8, 28, 24, 8}, 4, kDemoPanel);
            compact_probe.fill_round_rect({8, 38, 24, 8}, 4, kDemoPanel);
            compact_probe.stroke_round_rect({40, 28, 24, 8}, 4, kDemoPanelBorder);
            compact_probe.stroke_round_rect({40, 38, 24, 8}, 4, kDemoPanelBorder);
            compact_probe.fill_circle(20, 60, 6, kDemoPanel);
            compact_probe.fill_circle(36, 60, 6, kDemoPanel);
            compact_probe.stroke_circle(54, 60, 6, kDemoPanelBorder);
            compact_probe.stroke_circle(70, 60, 6, kDemoPanelBorder);
            compact_probe.draw_text_box({8, 72, 64, 16}, "compact", kDemoPath,
                                        get_font(FontId::Normal),
                                        TextAlignH::Left, TextAlignV::Center,
                                        TextWrap::None, TextEllipsis::None);
            compact_probe.draw_text_box({8, 88, 64, 16}, "compact", kDemoPath,
                                        get_font(FontId::Normal),
                                        TextAlignH::Left, TextAlignV::Center,
                                        TextWrap::None, TextEllipsis::None);
            const auto before = compact_probe.stats().cmd_count;
            compact_probe.compact(gui_compaction_workspace);
            const auto after = compact_probe.stats().cmd_count;
            compact_saved = (before > after) ? (before - after) : 0;
            if (compact_saved == 0) {
                compact_ok = false;
            }
        }
        ui::draw_cmd::DrawCmdExecutor exec{};
        const auto cmp_stats = compare_buf.stats();
        compare_cmd_count = cmp_stats.cmd_count;
        compare_cmd_capacity = cmp_stats.cmd_capacity;
        compare_cmd_bytes = cmp_stats.cmd_bytes;
        compare_batch_shrink = cmp_stats.batch_shrink;
        compare_batch_shrink_line = cmp_stats.batch_shrink_line;
        compare_batch_shrink_path = cmp_stats.batch_shrink_path;
        compare_batch_shrink_rect = cmp_stats.batch_shrink_rect;
        compare_batch_shrink_round = cmp_stats.batch_shrink_round;
        compare_batch_shrink_image = cmp_stats.batch_shrink_image;
        compare_batch_shrink_focus = cmp_stats.batch_shrink_focus;
        if (compare_cmd_count_raw >= compare_cmd_count) {
            compare_cmd_saved = compare_cmd_count_raw - compare_cmd_count;
        }
        if (compare_cmd_count_raw > 0) {
            compare_cmd_saved_pct = static_cast<std::uint8_t>(
                (compare_cmd_saved * 100u) / compare_cmd_count_raw);
        }

        text_profile_reset();
        fb.clear(kDemoBg);
        canvas.begin_frame();
        const auto exec_stats = exec.execute(canvas, compare_buf);
        canvas.end_frame();
        text_profile = text_profile_sample();
        compare_hash_full = hash_bytes(fb.data(), DefaultFrameBuffer::buffer_bytes);

        fb.clear(kDemoBg);
        const auto tile_stats = exec.execute_tiles(tile_backend, tile_view, compare_buf, tile_config);
        compare_hash_tile = hash_bytes(fb.data(), DefaultFrameBuffer::buffer_bytes);
        compare_failed_cmds = exec_stats.failed_cmds;
        compare_dispatch_groups = exec_stats.dispatch_groups;
        compare_batch_flushes = exec_stats.batch_flushes;
        compare_group_rect = exec_stats.group_rect;
        compare_group_text = exec_stats.group_text;
        compare_group_image = exec_stats.group_image;
        compare_group_line = exec_stats.group_line;
        compare_group_path = exec_stats.group_path;
        compare_group_other = exec_stats.group_other;
        compare_cmd_rect = exec_stats.cmd_rect;
        compare_cmd_text = exec_stats.cmd_text;
        compare_cmd_image = exec_stats.cmd_image;
        compare_cmd_line = exec_stats.cmd_line;
        compare_cmd_path = exec_stats.cmd_path;
        compare_cmd_other = exec_stats.cmd_other;
        compare_fail_text = exec_stats.fail_text;
        compare_fail_image = exec_stats.fail_image;
        compare_fail_blob = exec_stats.fail_blob;
        compare_fail_path = exec_stats.fail_path;
        compare_fail_clip = exec_stats.fail_clip;
        compare_fail_other = exec_stats.fail_other;
        compare_clip_push_overflow = exec_stats.clip_push_overflow;
        compare_clip_pop_underflow = exec_stats.clip_pop_underflow;
        compare_clip_invalid = exec_stats.clip_invalid;
        compare_tile_flushes = tile_stats.tile_flush_count;
        compare_tile_dispatch_groups = tile_stats.dispatch_groups;
        compare_tile_batch_flushes = tile_stats.batch_flushes;
        compare_tile_failed_cmds = tile_stats.failed_cmds;
        if (tile_stats.tiles_total > 0) {
            compare_tile_hit_pct = static_cast<std::uint8_t>(
                (static_cast<std::uint32_t>(tile_stats.tiles_drawn) * 100u)
                / static_cast<std::uint32_t>(tile_stats.tiles_total));
        }

        (void)out::println<"[soa] compare full=0x{:08X} tile=0x{:08X}">(
            g_console,
            static_cast<unsigned>(compare_hash_full),
            static_cast<unsigned>(compare_hash_tile));
        if (exec_stats.failed_cmds != 0) {
            (void)out::println<"[soa][fail] compare failed_cmds={}">(g_console,
                                                                  static_cast<unsigned>(exec_stats.failed_cmds));
            compare_ok = false;
        }
        if (cmp_stats.cmd_overflowed || cmp_stats.text_overflowed || cmp_stats.blob_overflowed || cmp_stats.cmd_count == 0) {
            compare_ok = false;
        }
        if (tile_stats.tiles_total == 0 || tile_stats.tiles_drawn == 0) {
            compare_ok = false;
        }
        if (compare_hash_full != compare_hash_tile) {
            compare_ok = false;
        }
#if defined(VIVID_SOA_TRACE_INPUT)
        if (!compare_ok && run_ci) {
            ci_mark_fail("compare");
        }
#endif
        if (!compare_ok && !run_ci) {
#if defined(VIVID_SOA_TRACE_INPUT)
            close_regress_log();
#endif
            return 1;
        }
#if defined(VIVID_SOA_TRACE_INPUT)
        if (run_ci && !compact_ok) {
            ci_mark_fail("compact");
        }
#endif
#if defined(VIVID_SOA_TRACE_INPUT)
        trace_regress_stage("compare.end");
#endif
    }

    bool dump_ok = true;
    if (run_dump) {
#if defined(VIVID_SOA_TRACE_INPUT)
        trace_regress_stage("dump.begin");
#endif
        if (!has_recorded) {
            gui.record_commands(compare_buf);
            append_path_icon(compare_buf, screen_width);
            append_compaction_probe(compare_buf, screen_width, screen_height);
            compare_cmd_count_raw = compare_buf.stats().cmd_count;
            compare_buf.compact(gui_compaction_workspace);
        }
        const auto dump_stats = compare_buf.stats();
        if (dump_stats.cmd_overflowed || dump_stats.text_overflowed || dump_stats.blob_overflowed || dump_stats.cmd_count == 0) {
            dump_ok = false;
        }
        if (dump_ok && !dump_cmd_file(dump_cmd_path.c_str(), compare_buf, &tile_backend)) {
            dump_ok = false;
        }
#if defined(VIVID_SOA_TRACE_INPUT)
        if (!dump_ok && run_ci) {
            ci_mark_fail("dump");
        }
#endif
        if (!dump_ok && !run_ci) {
#if defined(VIVID_SOA_TRACE_INPUT)
            close_regress_log();
#endif
            return 1;
        }
#if defined(VIVID_SOA_TRACE_INPUT)
        trace_regress_stage("dump.end");
#endif
    }
#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_ci) {
        trace_regress_stage("replay.begin");
        std::uint32_t replay_full = 0;
        std::uint32_t replay_tile = 0;
        bool replay_ok = false;
        if (dump_ok) {
            replay_ok = replay_cmd_file(dump_cmd_path.c_str(),
                                        fb,
                                        canvas,
                                        tile_backend,
                                        tile_view,
                                        tile_config,
                                        false,
                                        &replay_full);
            if (replay_ok) {
                replay_ok = replay_cmd_file(dump_cmd_path.c_str(),
                                            fb,
                                            canvas,
                                            tile_backend,
                                            tile_view,
                                            tile_config,
                                            true,
                                            &replay_tile);
            }
        }
        if (!replay_ok) {
            ci_mark_fail("replay");
        }
        trace_regress_stage("replay.end");

        const bool payload_ok = !kernel.payload_overflowed();
        const bool text_ok = !kernel.text_overflowed();
        const bool blob_ok = !compare_buf.stats().blob_overflowed;
        const bool clip_ok = compare_clip_push_overflow == 0
            && compare_clip_pop_underflow == 0
            && compare_clip_invalid == 0;
        const bool tile_failed_ok = compare_tile_failed_cmds == 0;
        const std::size_t cmd_budget = (compare_cmd_capacity > 0)
            ? (compare_cmd_capacity * 9u) / 10u
            : 0u;
        const bool cmd_budget_ok = compare_cmd_count <= cmd_budget;
    const auto payload_stats = kernel.payload_stats();
    const std::uint32_t total_fail =
        payload_stats.label.alloc_fail
        + payload_stats.button.alloc_fail
        + payload_stats.image.alloc_fail
        + payload_stats.checkbox.alloc_fail
        + payload_stats.radio.alloc_fail
        + payload_stats.list_item.alloc_fail
        + payload_stats.text_list.alloc_fail
        + payload_stats.list_view.alloc_fail
        + payload_stats.table_view.alloc_fail
        + payload_stats.tree_view.alloc_fail
        + payload_stats.stepper.alloc_fail
        + payload_stats.number_list.alloc_fail
        + payload_stats.roller.alloc_fail
        + payload_stats.switcher.alloc_fail
        + payload_stats.slider.alloc_fail
        + payload_stats.progress.alloc_fail
        + payload_stats.scrollbar.alloc_fail
        + payload_stats.list.alloc_fail
            + payload_stats.scroll_container.alloc_fail
            + payload_stats.spinner.alloc_fail;

        const auto img_stats = img_stats_valid ? img_stats_after_record
                                               : ui::draw_cmd::image_registry_stats();
        const bool img_overflow_ok = !img_stats.overflowed;
        if (selftest_dedup) {
            img_dedup_ok = img_stats.dedup_hits > 0;
            if (!img_dedup_ok) {
                ci_mark_fail("img_dedup");
            }
        }
        if (!img_growth_ok || img_growth_record != 0 || img_growth_compact != 0 || img_growth_execute != 0) {
#if defined(VIVID_SOA_TRACE_INPUT)
            const auto reason = ui::draw_cmd::image_registry_first_after_lock_reason();
            const char* tag = ui::draw_cmd::image_registry_first_after_lock_tag();
            (void)out::println<"[soa][fail] img_growth_after_lock count={} reason={} tag={}">(
                g_console,
                static_cast<unsigned>(img_growth_count),
                ui::draw_cmd::image_register_reason_name(reason),
                tag ? tag : "-");
#endif
            ci_mark_fail("img_growth_after_lock");
        }
        if (!img_overflow_ok) {
            ci_mark_fail("img_overflow");
        }
        if (max_missing_glyphs >= 0 && static_cast<int>(missing_glyphs) > max_missing_glyphs) {
            ci_mark_fail("missing_glyphs");
        }
        if (max_fallback_glyphs >= 0 && static_cast<int>(missing_glyph_fallbacks) > max_fallback_glyphs) {
            ci_mark_fail("fallback_glyphs");
        }
        if (max_utf8_replacements >= 0 && static_cast<int>(utf8_replacements) > max_utf8_replacements) {
            ci_mark_fail("utf8_replace");
        }
        if (require_font_provider && !font_provider_bound()) {
            ci_mark_fail("font_provider_missing");
        }
        if (require_fallback_font && !font_fallback_bound()) {
            ci_mark_fail("font_fallback_missing");
        }
        if (require_utf8_replacement_disabled && utf8_replacement_enabled()) {
            ci_mark_fail("utf8_replace_enabled");
        }
        if (max_text_draw >= 0 && static_cast<int>(text_profile.draw_calls) > max_text_draw) {
            ci_mark_fail("text_draw");
        }
        if (max_text_glyphs >= 0 && static_cast<int>(text_profile.glyphs) > max_text_glyphs) {
            ci_mark_fail("text_glyphs");
        }
        if (max_text_pixels >= 0 && static_cast<int>(text_profile.pixels) > max_text_pixels) {
            ci_mark_fail("text_pixels");
        }
        if (!cmd_budget_ok) {
            ci_mark_fail("cmd_budget");
        }
        if (!clip_ok) {
            ci_mark_fail("clip_invalid");
        }
        if (!tile_failed_ok) {
            ci_mark_fail("tile_failed_cmds");
        }
        const bool workspace_ok = !kernel.workspace_overflowed()
            && !gui.last_cmd_stats().workspace_overflowed
            && !kernel.traversal_phase_conflicted()
            && !gui.last_cmd_stats().traversal_phase_conflicted;
        if (!workspace_ok) {
            ci_mark_fail("workspace_overflow");
        }
        const bool style_patch_ok = !kernel.style_patch_overflowed()
            && !gui.last_cmd_stats().style_patch_overflowed
            && kernel.style_patch_alloc_fail() == 0;
        if (!style_patch_ok) {
            ci_mark_fail("style_patch_overflow");
        }
        const bool semantic_ok = !kernel.semantic_overflowed()
            && !gui.last_cmd_stats().semantic_overflowed
            && kernel.semantic_alloc_fail() == 0;
        if (!semantic_ok) {
            ci_mark_fail("semantic_overflow");
        }
        const bool ok = ci_ok
              && compare_ok
              && dump_ok
              && replay_ok
            && payload_ok
            && text_ok
            && blob_ok
            && clip_ok
            && tile_failed_ok
            && (compare_failed_cmds == 0)
            && (compare_hash_full == compare_hash_tile)
            && (replay_full == compare_hash_full)
            && (replay_tile == compare_hash_tile)
            && (total_fail == 0)
            && list_peak_ok
            && table_tree_ok
            && ui_ok
            && svg_workspace_ok
            && img_growth_ok
            && img_overflow_ok
            && img_dedup_ok
            && cmd_budget_ok
            && workspace_ok
            && traversal_workspace_ok
            && rect_truth_ok
            && style_patch_ok
            && style_patch_pool_ok
            && semantic_ok
            && semantic_pool_ok
            && payload_owner_ok
            && node_runtime_state_ok
            && style_class_id_ok
            && layout_text_state_ok;

        (void)out::println<"[soa-ci] display mode={} bw1={} gray2={} gray2_curve={} eink_max_partial={} eink_min_full_ms={} eink_partial_pct={} missing_glyphs={} fallback_glyphs={} utf8_replace={} text_draw={} text_glyphs={} text_pixels={}">(
            g_console,
            tile_backend.display_mode_name(),
            static_cast<unsigned>(tile_backend.display.bw1_threshold),
            static_cast<unsigned>(tile_backend.display.gray2_strength),
            tile_backend.gray2_curve_name(),
            tile_backend.eink_policy.max_partial_count,
            tile_backend.eink_policy.min_full_interval_ms,
            tile_backend.eink_policy.partial_area_ratio_pct,
            static_cast<unsigned>(missing_glyphs),
            static_cast<unsigned>(missing_glyph_fallbacks),
            static_cast<unsigned>(utf8_replacements),
            static_cast<unsigned long long>(text_profile.draw_calls),
            static_cast<unsigned long long>(text_profile.glyphs),
            static_cast<unsigned long long>(text_profile.pixels));

        (void)out::println<"[soa-ci] ok={} hash=0x{:08X} replay_full=0x{:08X} replay_tile=0x{:08X} failed_cmds={} overflows(p/t/b)={}/{}/{} workspace_overflow={} traversal_conflict={} traversal_workspace_ok={} rect_truth_ok={} style_patch_overflow={} style_patch_live={} style_patch_peak={} style_patch_cap={} style_patch_fail={} style_patch_pool_ok={} semantic_overflow={} semantic_live={} semantic_peak={} semantic_cap={} semantic_fail={} semantic_pool_ok={} alloc_fail={} peak_ok={} table_tree_ok={} ui_ok={} compact_saved={} batch_shrink={} batch_shrink_line={} batch_shrink_path={} batch_shrink_rect={} batch_shrink_round={} batch_shrink_image={} batch_shrink_focus={} cmd_raw={} cmd_count={} cmd_saved={} cmd_saved_pct={} cmd_budget={} dispatch_groups={} batch_flushes={} groups(rect/text/img/line/path/other)={}/{}/{}/{}/{}/{} cmds(rect/text/img/line/path/other)={}/{}/{}/{}/{}/{} fail(text/img/blob/path/clip/other)={}/{}/{}/{}/{}/{} clip(push_over/pop_under/invalid)={}/{}/{} tile_flushes={} tile_hit_pct={} tile_dispatch_groups={} tile_batch_flushes={} tile_failed_cmds={} img_new_total={} img_new_after_lock={} img_new_record={} img_new_compact={} img_new_execute={} img_bytes={} img_reuse={} img_growth={} img_overflow={} img_dedup_ok={} img_after_lock_reason={} img_after_lock_tag={} reason={}">(
            g_console,
            ok ? 1u : 0u,
            static_cast<unsigned>(compare_hash_full),
            static_cast<unsigned>(replay_full),
            static_cast<unsigned>(replay_tile),
            static_cast<unsigned>(compare_failed_cmds),
            payload_ok ? 0u : 1u,
            text_ok ? 0u : 1u,
            blob_ok ? 0u : 1u,
            workspace_ok ? 0u : 1u,
            kernel.traversal_phase_conflicted() ? 1u : 0u,
            traversal_workspace_ok ? 1u : 0u,
            rect_truth_ok ? 1u : 0u,
            style_patch_ok ? 0u : 1u,
            static_cast<unsigned>(kernel.style_patch_live_count()),
            static_cast<unsigned>(kernel.style_patch_peak_count()),
            static_cast<unsigned>(SoaKernel::kStylePatchCapacity),
            static_cast<unsigned>(kernel.style_patch_alloc_fail()),
            style_patch_pool_ok ? 1u : 0u,
            semantic_ok ? 0u : 1u,
            static_cast<unsigned>(kernel.semantic_live_count()),
            static_cast<unsigned>(kernel.semantic_peak_count()),
            static_cast<unsigned>(SoaKernel::kSemanticCapacity),
            static_cast<unsigned>(kernel.semantic_alloc_fail()),
            semantic_pool_ok ? 1u : 0u,
            static_cast<unsigned>(total_fail),
            list_peak_ok ? 1u : 0u,
            table_tree_ok ? 1u : 0u,
            ui_ok ? 1u : 0u,
            static_cast<unsigned>(compact_saved),
            static_cast<unsigned>(compare_batch_shrink),
            static_cast<unsigned>(compare_batch_shrink_line),
            static_cast<unsigned>(compare_batch_shrink_path),
            static_cast<unsigned>(compare_batch_shrink_rect),
            static_cast<unsigned>(compare_batch_shrink_round),
            static_cast<unsigned>(compare_batch_shrink_image),
            static_cast<unsigned>(compare_batch_shrink_focus),
            static_cast<unsigned>(compare_cmd_count_raw),
            static_cast<unsigned>(compare_cmd_count),
            static_cast<unsigned>(compare_cmd_saved),
            static_cast<unsigned>(compare_cmd_saved_pct),
            static_cast<unsigned>(cmd_budget),
            static_cast<unsigned>(compare_dispatch_groups),
            static_cast<unsigned>(compare_batch_flushes),
            static_cast<unsigned>(compare_group_rect),
            static_cast<unsigned>(compare_group_text),
            static_cast<unsigned>(compare_group_image),
            static_cast<unsigned>(compare_group_line),
            static_cast<unsigned>(compare_group_path),
            static_cast<unsigned>(compare_group_other),
            static_cast<unsigned>(compare_cmd_rect),
            static_cast<unsigned>(compare_cmd_text),
            static_cast<unsigned>(compare_cmd_image),
            static_cast<unsigned>(compare_cmd_line),
            static_cast<unsigned>(compare_cmd_path),
            static_cast<unsigned>(compare_cmd_other),
            static_cast<unsigned>(compare_fail_text),
            static_cast<unsigned>(compare_fail_image),
            static_cast<unsigned>(compare_fail_blob),
            static_cast<unsigned>(compare_fail_path),
            static_cast<unsigned>(compare_fail_clip),
            static_cast<unsigned>(compare_fail_other),
            static_cast<unsigned>(compare_clip_push_overflow),
            static_cast<unsigned>(compare_clip_pop_underflow),
            static_cast<unsigned>(compare_clip_invalid),
            static_cast<unsigned>(compare_tile_flushes),
            static_cast<unsigned>(compare_tile_hit_pct),
            static_cast<unsigned>(compare_tile_dispatch_groups),
            static_cast<unsigned>(compare_tile_batch_flushes),
            static_cast<unsigned>(compare_tile_failed_cmds),
            static_cast<unsigned>(img_stats.register_new_total),
            static_cast<unsigned>(img_stats.register_new_after_lock),
            static_cast<unsigned>(img_stats.register_new_record),
            static_cast<unsigned>(img_stats.register_new_compact),
            static_cast<unsigned>(img_stats.register_new_execute),
            static_cast<unsigned>(img_stats.bytes_total),
            static_cast<unsigned>(img_stats.dedup_hits),
            static_cast<unsigned>(img_growth_count),
            img_overflow_ok ? 0u : 1u,
            img_dedup_ok ? 1u : 0u,
            ui::draw_cmd::image_register_reason_name(ui::draw_cmd::image_registry_first_after_lock_reason()),
            (ui::draw_cmd::image_registry_first_after_lock_tag()
                ? ui::draw_cmd::image_registry_first_after_lock_tag()
                : "-"),
            ci_reason ? ci_reason : "none");
        close_regress_log();
        return ok ? 0 : 1;
    }
#endif
#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_screenshot || run_gif) {
        ensure_demo_images();
        ui::draw_cmd::set_image_registry_locked(true);
        ui::draw_cmd::DefaultDrawCmdBuffer snap_buf{};
        ui::draw_cmd::DrawCmdExecutor exec{};
        std::vector<std::vector<std::uint8_t>> frames{};
        FrameBufferView snapshot_view{
            screen_pixel_format,
            fb.data(),
            static_cast<std::size_t>(screen_width),
            static_cast<std::size_t>(screen_height),
            DefaultFrameBuffer::stride_bytes
        };
        int value = 0;
        std::uint8_t spinner_phase = 0;
        const int frame_count = run_gif ? gif_frames : 1;
        frames.reserve(run_gif ? static_cast<std::size_t>(frame_count) : 0u);

        for (int i = 0; i < frame_count; ++i) {
            value = (value + 17) % 101;
            kernel.set_value(progress, value);
            kernel.set_value(progress_wheel, value);
            kernel.set_value(progress_simple, value);
            spinner_phase = static_cast<std::uint8_t>((spinner_phase + 1u) % 8u);
            kernel.set_spinner_phase(spinner, spinner_phase);
            if (!kernel.pressed(slider)) {
                kernel.set_value(slider, value);
            }

            snap_buf.clear();
            gui.record_commands(snap_buf);
            append_path_icon(snap_buf, screen_width);
            snap_buf.compact(gui_compaction_workspace);
            fb.clear(kDemoBg);
            canvas.begin_frame();
            exec.execute(canvas, snap_buf);
            canvas.end_frame();

            if (run_gif) {
                frames.push_back(charm::gfx::snapshot::capture_indexed_332(snapshot_view));
            }
        }

        if (run_screenshot) {
            if (!charm::gfx::snapshot::write_ppm(screenshot_path.c_str(), snapshot_view)) {
                (void)out::error<"[soa][fail] screenshot={}">(g_console, screenshot_path);
                close_regress_log();
                return 1;
            }
            (void)out::println<"[soa] screenshot={}">(g_console, screenshot_path);
        }
        if (run_gif) {
            if (!charm::gfx::snapshot::write_gif(
                    gif_path.c_str(),
                    screen_width,
                    screen_height,
                    frames,
                    gif_delay_cs)) {
                (void)out::error<"[soa][fail] gif={}">(g_console, gif_path);
                close_regress_log();
                return 1;
            }
            (void)out::println<"[soa] gif={} frames={} delay_cs={}">(
                g_console,
                gif_path,
                static_cast<unsigned>(frames.size()),
                static_cast<unsigned>(gif_delay_cs));
        }
    }
#endif
#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_headless) {
        close_regress_log();
        return 0;
    }
#else
    if (run_headless) {
        return 0;
    }
#endif

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        (void)out::error<"SDL_Init failed: {}">(g_console, SDL_GetError());
#if defined(VIVID_SOA_TRACE_INPUT)
        close_regress_log();
#endif
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Vivid SoA Demo", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        (void)out::error<"SDL_CreateWindow failed: {}">(g_console, SDL_GetError());
        SDL_Quit();
#if defined(VIVID_SOA_TRACE_INPUT)
        close_regress_log();
#endif
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        (void)out::error<"SDL_CreateRenderer failed: {}">(g_console, SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
#if defined(VIVID_SOA_TRACE_INPUT)
        close_regress_log();
#endif
        return 1;
    }

    auto to_sdl_format = []() noexcept {
        switch (screen_pixel_format) {
        case PixelFormat::RGB565: return SDL_PIXELFORMAT_RGB565;
        case PixelFormat::RGB888: return SDL_PIXELFORMAT_RGB24;
        case PixelFormat::ARGB8888: return SDL_PIXELFORMAT_ARGB8888;
        }
        return SDL_PIXELFORMAT_RGB24;
    };
    SDL_Texture* texture = SDL_CreateTexture(renderer, to_sdl_format(), SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        (void)out::error<"SDL_CreateTexture failed: {}">(g_console, SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
#if defined(VIVID_SOA_TRACE_INPUT)
        close_regress_log();
#endif
        return 1;
    }

    ui::draw_cmd::DefaultDrawCmdBuffer cmd_buf{};
    ui::draw_cmd::DrawCmdExecutor cmd_exec{};

    int win_w = screen_width;
    int win_h = screen_height;
    int mouse_x = 0;
    int mouse_y = 0;
    bool running = true;
    int value = 0;
    std::uint8_t spinner_phase = 0;
    int stat_frame = 0;
    const int stat_interval = 60;
    ui::draw_cmd::DrawCmdTileStats last_tile_stats{};
    ui::draw_cmd::DrawCmdExecStats last_exec_stats{};
    bool last_stats_valid = false;
    bool last_stats_tiles = false;
    const auto adjust_gray2_strength = [&](int delta) noexcept {
        if (tile_backend.display.mode != ui::gfx::DisplayMode::Gray2) return;
        int next = static_cast<int>(tile_backend.display.gray2_strength) + delta;
        next = std::clamp(next, 0, 64);
        if (next == tile_backend.display.gray2_strength) return;
        tile_backend.display.gray2_strength = static_cast<std::uint8_t>(next);
        gray2_strength = next;
        (void)out::println<"[soa] gray2 strength={}">(g_console, next);
    };
    const auto cycle_gray2_curve = [&]() noexcept {
        if (tile_backend.display.mode != ui::gfx::DisplayMode::Gray2) return;
        const int next = (static_cast<int>(tile_backend.display.gray2_curve) + 1) % 3;
        tile_backend.display.gray2_curve = static_cast<ui::gfx::Gray2Curve>(next);
        gray2_curve = tile_backend.display.gray2_curve;
        (void)out::println<"[soa] gray2 curve={}">(g_console, tile_backend.gray2_curve_name());
    };
    const auto adjust_bw1_threshold = [&](int delta) noexcept {
        if (tile_backend.display.mode != ui::gfx::DisplayMode::BW1) return;
        int next = static_cast<int>(tile_backend.display.bw1_threshold) + delta;
        next = std::clamp(next, 0, 255);
        if (next == tile_backend.display.bw1_threshold) return;
        tile_backend.display.bw1_threshold = static_cast<std::uint8_t>(next);
        bw1_threshold = next;
        (void)out::println<"[soa] bw1 threshold={}">(g_console, next);
    };
    const auto adjust_eink_ratio = [&](int delta) noexcept {
        if (tile_backend.display.mode != ui::gfx::DisplayMode::Eink) return;
        int next = tile_backend.eink_policy.partial_area_ratio_pct + delta;
        next = std::clamp(next, 1, 100);
        if (next == tile_backend.eink_policy.partial_area_ratio_pct) return;
        tile_backend.eink_policy.partial_area_ratio_pct = next;
        (void)out::println<"[soa] eink partial_pct={}">(g_console, next);
    };
    const auto adjust_eink_max_partial = [&](int delta) noexcept {
        if (tile_backend.display.mode != ui::gfx::DisplayMode::Eink) return;
        int next = tile_backend.eink_policy.max_partial_count + delta;
        next = std::clamp(next, 1, 200);
        if (next == tile_backend.eink_policy.max_partial_count) return;
        tile_backend.eink_policy.max_partial_count = next;
        (void)out::println<"[soa] eink max_partial={}">(g_console, next);
    };
    const auto adjust_eink_min_full = [&](int delta) noexcept {
        if (tile_backend.display.mode != ui::gfx::DisplayMode::Eink) return;
        int next = tile_backend.eink_policy.min_full_interval_ms + delta;
        next = std::clamp(next, 0, 120000);
        if (next == tile_backend.eink_policy.min_full_interval_ms) return;
        tile_backend.eink_policy.min_full_interval_ms = next;
        (void)out::println<"[soa] eink min_full_ms={}">(g_console, next);
    };
    const auto handle_display_hotkey = [&](SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_P:
            show_perf_overlay = !show_perf_overlay;
            (void)out::println<"[soa] perf overlay={}">(g_console, show_perf_overlay ? 1 : 0);
            break;
        case SDLK_KP_PLUS:
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_RIGHTBRACKET:
            adjust_gray2_strength(1);
            break;
        case SDLK_KP_MINUS:
        case SDLK_MINUS:
        case SDLK_LEFTBRACKET:
            adjust_gray2_strength(-1);
            break;
        case SDLK_G:
            cycle_gray2_curve();
            break;
        case SDLK_PERIOD:
            adjust_bw1_threshold(1);
            break;
        case SDLK_COMMA:
            adjust_bw1_threshold(-1);
            break;
        case SDLK_1:
            adjust_eink_ratio(-1);
            break;
        case SDLK_2:
            adjust_eink_ratio(1);
            break;
        case SDLK_3:
            adjust_eink_max_partial(-1);
            break;
        case SDLK_4:
            adjust_eink_max_partial(1);
            break;
        case SDLK_5:
            adjust_eink_min_full(-500);
            break;
        case SDLK_6:
            adjust_eink_min_full(500);
            break;
        default:
            break;
        }
    };

    while (running) {
        SDL_Event evt{};
        SDL_GetWindowSize(window, &win_w, &win_h);
        const Viewport vp = compute_viewport(win_w, win_h, screen_width, screen_height);

        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_KEY_DOWN) {
                handle_display_hotkey(evt.key.key);
            } else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
                if (map_mouse(vp, evt.motion.x, evt.motion.y, mouse_x, mouse_y)) {
                    gui.dispatch_event(Event::mouse(Event::Type::MouseMove, mouse_x, mouse_y, 0));
#if defined(VIVID_SOA_TRACE_INPUT)
                    trace_input_events(kernel);
#endif
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, mouse_x, mouse_y, 1));
#if defined(VIVID_SOA_TRACE_INPUT)
                        trace_input_events(kernel);
#endif
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, mouse_x, mouse_y, 1));
#if defined(VIVID_SOA_TRACE_INPUT)
                        trace_input_events(kernel);
#endif
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                gui.dispatch_event(Event::wheel(mouse_x, mouse_y, static_cast<int>(evt.wheel.y)));
#if defined(VIVID_SOA_TRACE_INPUT)
                trace_input_events(kernel);
#endif
            }
        }

        value = (value + 1) % 101;
        kernel.set_value(progress, value);
        kernel.set_value(progress_wheel, value);
        kernel.set_value(progress_simple, value);
        spinner_phase = static_cast<std::uint8_t>((spinner_phase + 1u) % 8u);
        kernel.set_spinner_phase(spinner, spinner_phase);
        if (!kernel.pressed(slider)) {
            kernel.set_value(slider, value);
        }

        gui.record_commands(cmd_buf);
        append_path_icon(cmd_buf, screen_width);
        append_compaction_probe(cmd_buf, screen_width, screen_height);
        append_display_overlay(cmd_buf, tile_backend);
        if (show_perf_overlay) {
            append_perf_overlay(cmd_buf,
                                screen_width,
                                last_tile_stats,
                                last_exec_stats,
                                last_stats_tiles,
                                last_stats_valid);
        }
        cmd_buf.compact(gui_compaction_workspace);
        const auto cmd_stats = cmd_buf.stats();

        ui::draw_cmd::DrawCmdTileStats tile_stats{};
        ui::draw_cmd::DrawCmdExecStats exec_stats{};
        std::uint32_t dirty_area = 0;
        std::uint8_t dirty_pct = 0;
        std::uint8_t tile_hit_pct = 0;
        if (use_tiles) {
            tile_stats = cmd_exec.execute_tiles(tile_backend, tile_view, cmd_buf, tile_config);
            if (tile_stats.tiles_total > 0) {
                tile_hit_pct = static_cast<std::uint8_t>(
                    (static_cast<std::uint32_t>(tile_stats.tiles_drawn) * 100u)
                    / static_cast<std::uint32_t>(tile_stats.tiles_total));
            }
            Rect dirty{};
            if (tile_backend.dirty_rect(dirty)) {
                const std::uint64_t area = static_cast<std::uint64_t>(dirty.w)
                    * static_cast<std::uint64_t>(dirty.h);
                dirty_area = static_cast<std::uint32_t>(area);
                const std::uint64_t screen_area = static_cast<std::uint64_t>(screen_width)
                    * static_cast<std::uint64_t>(screen_height);
                dirty_pct = (screen_area > 0)
                    ? static_cast<std::uint8_t>((area * 100u) / screen_area)
                    : 0;
                if (tile_backend.display.mode == ui::gfx::DisplayMode::Eink) {
                    tile_backend.decide_refresh(static_cast<int>(dirty_area),
                                                static_cast<int>(screen_area),
                                                SDL_GetTicks());
                }
                SDL_Rect rect{dirty.x, dirty.y, dirty.w, dirty.h};
                const std::size_t bpp = DefaultFrameBuffer::bytes_per_pixel;
                const std::size_t stride = DefaultFrameBuffer::stride_bytes;
                const std::byte* src = fb.data()
                    + static_cast<std::size_t>(dirty.y) * stride
                    + static_cast<std::size_t>(dirty.x) * bpp;
                SDL_UpdateTexture(texture, &rect, src, static_cast<int>(stride));
            }
        } else {
            fb.clear(kDemoBg);
            canvas.begin_frame();
            exec_stats = cmd_exec.execute(canvas, cmd_buf);
            canvas.end_frame();
            SDL_UpdateTexture(texture, nullptr, canvas.data(), static_cast<int>(DefaultFrameBuffer::stride_bytes));
        }
        if (use_tiles) {
            last_tile_stats = tile_stats;
            last_stats_tiles = true;
            last_stats_valid = true;
        } else {
            last_exec_stats = exec_stats;
            last_stats_tiles = false;
            last_stats_valid = true;
        }
        if (last_stats_valid) {
            PerfOverlay::OverlayStats overlay{};
            overlay.batch_shrink = static_cast<std::uint32_t>(cmd_stats.batch_shrink);
            overlay.batch_shrink_rect = static_cast<std::uint32_t>(cmd_stats.batch_shrink_rect);
            overlay.batch_shrink_round = static_cast<std::uint32_t>(cmd_stats.batch_shrink_round);
            if (last_stats_tiles) {
                overlay.dispatch_groups = static_cast<std::uint32_t>(last_tile_stats.dispatch_groups);
                overlay.batch_flushes = static_cast<std::uint32_t>(last_tile_stats.batch_flushes);
                overlay.failed_cmds = static_cast<std::uint32_t>(last_tile_stats.failed_cmds);
                overlay.group_rect = static_cast<std::uint32_t>(last_tile_stats.group_rect);
                overlay.group_text = static_cast<std::uint32_t>(last_tile_stats.group_text);
                overlay.group_image = static_cast<std::uint32_t>(last_tile_stats.group_image);
                overlay.group_line = static_cast<std::uint32_t>(last_tile_stats.group_line);
                overlay.group_path = static_cast<std::uint32_t>(last_tile_stats.group_path);
                overlay.group_other = static_cast<std::uint32_t>(last_tile_stats.group_other);
                overlay.cmd_rect = static_cast<std::uint32_t>(last_tile_stats.cmd_rect);
                overlay.cmd_text = static_cast<std::uint32_t>(last_tile_stats.cmd_text);
                overlay.cmd_image = static_cast<std::uint32_t>(last_tile_stats.cmd_image);
                overlay.cmd_line = static_cast<std::uint32_t>(last_tile_stats.cmd_line);
                overlay.cmd_path = static_cast<std::uint32_t>(last_tile_stats.cmd_path);
                overlay.cmd_other = static_cast<std::uint32_t>(last_tile_stats.cmd_other);
            } else {
                overlay.dispatch_groups = static_cast<std::uint32_t>(last_exec_stats.dispatch_groups);
                overlay.batch_flushes = static_cast<std::uint32_t>(last_exec_stats.batch_flushes);
                overlay.failed_cmds = static_cast<std::uint32_t>(last_exec_stats.failed_cmds);
                overlay.group_rect = static_cast<std::uint32_t>(last_exec_stats.group_rect);
                overlay.group_text = static_cast<std::uint32_t>(last_exec_stats.group_text);
                overlay.group_image = static_cast<std::uint32_t>(last_exec_stats.group_image);
                overlay.group_line = static_cast<std::uint32_t>(last_exec_stats.group_line);
                overlay.group_path = static_cast<std::uint32_t>(last_exec_stats.group_path);
                overlay.group_other = static_cast<std::uint32_t>(last_exec_stats.group_other);
                overlay.cmd_rect = static_cast<std::uint32_t>(last_exec_stats.cmd_rect);
                overlay.cmd_text = static_cast<std::uint32_t>(last_exec_stats.cmd_text);
                overlay.cmd_image = static_cast<std::uint32_t>(last_exec_stats.cmd_image);
                overlay.cmd_line = static_cast<std::uint32_t>(last_exec_stats.cmd_line);
                overlay.cmd_path = static_cast<std::uint32_t>(last_exec_stats.cmd_path);
                overlay.cmd_other = static_cast<std::uint32_t>(last_exec_stats.cmd_other);
            }
            set_perf_overlay_stats(overlay);
        } else {
            clear_perf_overlay_stats();
        }

        if (print_stats) {
            ++stat_frame;
            if (stat_frame >= stat_interval) {
                stat_frame = 0;
                    if (use_tiles) {
                        (void)out::println<"[soa] cmds={} bytes={} overflow={} text_overflow={} blob_overflow={} batch_shrink={}({}/{}/{}/{}/{}/{}) tiles={}/{} flushes={} dirty_area={} dirty_pct={} tile_hit_pct={}">(
                            g_console,
                            static_cast<std::uint32_t>(cmd_stats.cmd_count),
                            static_cast<std::uint32_t>(cmd_stats.cmd_bytes),
                            cmd_stats.cmd_overflowed ? 1 : 0,
                            cmd_stats.text_overflowed ? 1 : 0,
                            cmd_stats.blob_overflowed ? 1 : 0,
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_line),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_path),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_rect),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_round),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_image),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_focus),
                            tile_stats.tiles_drawn,
                            tile_stats.tiles_total,
                            tile_stats.tile_flush_count,
                            dirty_area,
                            static_cast<std::uint32_t>(dirty_pct),
                            static_cast<std::uint32_t>(tile_hit_pct));
                    if (tile_backend.display.mode == ui::gfx::DisplayMode::Eink) {
                        (void)out::println<"[soa] eink refresh={} partial_count={} dirty_pct={}">(
                            g_console,
                            tile_backend.last_refresh_name(),
                            tile_backend.partial_count,
                            tile_backend.last_dirty_pct);
                    }
                    } else {
                        (void)out::println<"[soa] cmds={} bytes={} overflow={} text_overflow={} blob_overflow={} batch_shrink={}({}/{}/{}/{}/{}/{})">(
                            g_console,
                            static_cast<std::uint32_t>(cmd_stats.cmd_count),
                            static_cast<std::uint32_t>(cmd_stats.cmd_bytes),
                            cmd_stats.cmd_overflowed ? 1 : 0,
                            cmd_stats.text_overflowed ? 1 : 0,
                            cmd_stats.blob_overflowed ? 1 : 0,
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_line),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_path),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_rect),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_round),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_image),
                            static_cast<std::uint32_t>(cmd_stats.batch_shrink_focus));
                    }
#if defined(VIVID_SOA_TRACE_INPUT)
                dump_payload_stats(kernel.payload_stats());
#endif
            }
        }
        SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
        SDL_RenderClear(renderer);
        SDL_FRect dst{
            static_cast<float>(vp.x),
            static_cast<float>(vp.y),
            static_cast<float>(vp.w),
            static_cast<float>(vp.h)
        };
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (use_eink && tile_backend.display.mode == ui::gfx::DisplayMode::Eink) {
        (void)out::println<"[soa] eink summary full={} partial={} last_refresh={} last_dirty_pct={}">(
            g_console,
            tile_backend.total_full_count,
            tile_backend.total_partial_count,
            tile_backend.last_refresh_name(),
            tile_backend.last_dirty_pct);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
#if defined(VIVID_SOA_TRACE_INPUT)
    close_regress_log();
#endif
    return 0;
}



