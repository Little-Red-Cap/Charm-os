#pragma once

#include <cstdint>

namespace vivid::evidence {
    constexpr Rect kSemanticFocusSceneBounds{0, 0, 360, 208};
    constexpr Rect kSemanticFocusScopeBounds{12, 12, 210, 184};
    constexpr Rect kSemanticFocusPrimaryBounds{24, 24, 132, 30};
    constexpr Rect kSemanticFocusSecondaryBounds{24, 66, 132, 30};
    constexpr Rect kSemanticFocusInfoBounds{24, 108, 132, 30};
    constexpr Rect kSemanticFocusDisabledBounds{24, 150, 132, 30};
    constexpr Rect kSemanticFocusOutsideBounds{240, 24, 96, 30};
    constexpr Rect kSemanticFocusDuplicateBounds{240, 66, 96, 30};

    struct SemanticFocusFixtureHandles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle primary{};
        WidgetHandle secondary{};
        WidgetHandle info{};
        WidgetHandle disabled{};
        WidgetHandle outside{};
        WidgetHandle duplicate{};
    };

    inline void build_semantic_focus_fixture(::ui::scene::Scene& scene,
                                             SemanticFocusFixtureHandles& handles) {
        scene.build([&](::ui::scene::SceneBuilder& builder) {
            handles.root = builder.create_container();
            handles.scope = builder.create_container();
            handles.primary = builder.create_button_static("Primary");
            handles.secondary = builder.create_list_item("Secondary");
            handles.info = builder.create_container();
            handles.disabled = builder.create_button_static("Disabled");
            handles.outside = builder.create_button_static("Outside");
            handles.duplicate = builder.create_button_static("Duplicate");

            builder.link(handles.root, handles.scope);
            builder.link(handles.scope, handles.primary);
            builder.link(handles.scope, handles.secondary);
            builder.link(handles.scope, handles.info);
            builder.link(handles.scope, handles.disabled);
            builder.link(handles.root, handles.outside);
            builder.link(handles.root, handles.duplicate);

            builder.set_rect(handles.root, kSemanticFocusSceneBounds);
            builder.set_rect(handles.scope, kSemanticFocusScopeBounds);
            builder.set_rect(handles.primary, kSemanticFocusPrimaryBounds);
            builder.set_rect(handles.secondary, kSemanticFocusSecondaryBounds);
            builder.set_rect(handles.info, kSemanticFocusInfoBounds);
            builder.set_rect(handles.disabled, kSemanticFocusDisabledBounds);
            builder.set_rect(handles.outside, kSemanticFocusOutsideBounds);
            builder.set_rect(handles.duplicate, kSemanticFocusDuplicateBounds);
            builder.set_semantic_default(handles.primary, "action.primary");
            builder.set_semantic_default(handles.secondary, "row.secondary");
            builder.set_semantic(handles.info, SemanticRole::Container, "panel.info", "Info panel");
            builder.set_semantic_default(handles.disabled, "action.disabled");
            builder.set_semantic_default(handles.outside, "action.outside");
            builder.set_semantic_default(handles.duplicate, "action.primary", "Duplicate primary");
            builder.set_input_root(handles.root);
            builder.set_focus_scope(handles.scope, handles.primary, true);
            builder.set_root(handles.root);
        });
    }

    inline void configure_semantic_focus_fixture(::ui::scene::SceneAccess& access,
                                                 const SemanticFocusFixtureHandles& handles) noexcept {
        access.set_focusable(handles.primary, true);
        access.set_focusable(handles.secondary, true);
        access.set_focusable(handles.disabled, true);
        access.set_focusable(handles.outside, true);
        access.set_focusable(handles.duplicate, true);
        access.set_enabled(handles.disabled, false);
    }

    [[nodiscard]] inline bool focus_semantic_fixture_primary(::ui::scene::Scene& scene,
                                                             ::ui::scene::SceneAccess& access,
                                                             const SemanticFocusFixtureHandles& handles,
                                                             std::uint32_t ms = 10) {
        click_center(scene, kSemanticFocusPrimaryBounds, ms);
        return same_handle(access.input_focused(), handles.primary);
    }
}
