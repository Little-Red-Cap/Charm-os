module;

#include <cstdint>

export module charm.ui.scene.page_layers;

export import charm.core.handle;
export import charm.core.geometry;

import charm.ui.scene.builder_support;

export namespace ui::scene {
    enum class PageLayerRole : std::uint8_t {
        Backdrop,
        Content,
        Chrome,
        Popup,
    };

    struct PageLayers {
        WidgetHandle backdrop{};
        WidgetHandle content{};
        WidgetHandle chrome{};
        WidgetHandle popup{};

        [[nodiscard]] WidgetHandle get(PageLayerRole role) const noexcept {
            switch (role) {
            case PageLayerRole::Backdrop: return backdrop;
            case PageLayerRole::Content: return content;
            case PageLayerRole::Chrome: return chrome;
            case PageLayerRole::Popup: return popup;
            default: return {};
            }
        }
    };

    inline PageLayers create_page_layers(SceneBuilder& builder,
                                         WidgetHandle page_root,
                                         const Rect& page_rect) noexcept {
        PageLayers out{};
        out.backdrop = builder.create_container();
        builder.set_rect(out.backdrop, page_rect);
        builder.set_hit_testable(out.backdrop, false);

        out.content = builder.create_container();
        builder.set_rect(out.content, page_rect);
        builder.set_hit_testable(out.content, false);

        out.chrome = builder.create_container();
        builder.set_rect(out.chrome, page_rect);
        builder.set_hit_testable(out.chrome, false);

        out.popup = builder.create_container();
        builder.set_rect(out.popup, page_rect);
        builder.set_hit_testable(out.popup, false);

        builder.link(page_root, out.backdrop);
        builder.link(page_root, out.content);
        builder.link(page_root, out.chrome);
        builder.link(page_root, out.popup);
        return out;
    }
}
