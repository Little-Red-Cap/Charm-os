module;

#include <cstdint>

export module charm.ui.scene.title_block;

import charm.core.geometry;
import charm.core.handle;
import charm.core.style;
import charm.ui.scene.builder_support;

export namespace ui::scene {
    struct TitleBlockSpec {
        Rect rect{};
        const char* title_text{""};
        const char* subtitle_text{""};
        StylePatch title_style{};
        StylePatch subtitle_style{};
        int title_height{0};
        int subtitle_height{0};
        int gap{0};
        int padding{0};
        TextAlignH title_align_h{TextAlignH::Center};
        TextAlignV title_align_v{TextAlignV::Center};
        TextAlignH subtitle_align_h{TextAlignH::Center};
        TextAlignV subtitle_align_v{TextAlignV::Center};
        LayoutAlign cross_align{LayoutAlign::Start};
        bool title_hit_testable{false};
        bool subtitle_hit_testable{false};
        bool clip{false};
    };

    struct TitleBlockHandles {
        WidgetHandle root{};
        WidgetHandle title{};
        WidgetHandle subtitle{};
    };

    inline TitleBlockHandles build_title_block(SceneBuilder& builder,
                                               WidgetHandle parent,
                                               const TitleBlockSpec& spec) {
        TitleBlockHandles out{};
        ColumnBuilder column{builder,
                             spec.rect,
                             spec.gap,
                             spec.padding,
                             spec.cross_align,
                             kStyleClassInvalid,
                             spec.clip};
        out.root = column.root();

        out.title = builder.create_label_static(spec.title_text ? spec.title_text : "");
        builder.set_style_override(out.title, spec.title_style);
        builder.set_label_align(out.title, spec.title_align_h, spec.title_align_v);
        builder.set_hit_testable(out.title, spec.title_hit_testable);

        out.subtitle = builder.create_label_static(spec.subtitle_text ? spec.subtitle_text : "");
        builder.set_style_override(out.subtitle, spec.subtitle_style);
        builder.set_label_align(out.subtitle, spec.subtitle_align_h, spec.subtitle_align_v);
        builder.set_hit_testable(out.subtitle, spec.subtitle_hit_testable);

        column.add(out.title, spec.rect.w, spec.title_height);
        column.add(out.subtitle, spec.rect.w, spec.subtitle_height);
        if (parent) {
            builder.link(parent, out.root);
        }
        return out;
    }
}
