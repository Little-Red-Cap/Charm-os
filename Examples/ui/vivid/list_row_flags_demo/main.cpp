#include <cstdio>
#include <cstdint>

import charm.core.event;
import charm.gfx.canvas;
import charm.ui.scene;

namespace {
    constexpr int kRowHeight = 24;

    struct Handles {
        WidgetHandle root{};
        WidgetHandle list{};
    };

    struct RowData {
        static constexpr const char* kItems[3] = {
            "Group",
            "Disabled",
            "Enabled",
        };

        static const char* text(const void*, std::uint16_t index) noexcept {
            return index < 3 ? kItems[index] : "";
        }

        static std::uint8_t row_flags(const void*, std::uint16_t index) noexcept {
            switch (index) {
            case 0:
                return ::ui::scene::kListViewRowFlagGroup;
            case 1:
                return ::ui::scene::kListViewRowFlagDisabled;
            default:
                return 0;
            }
        }
    };

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    void dispatch_row_click(::ui::scene::Scene& scene,
                            ::ui::scene::SceneAccess& access,
                            WidgetHandle list,
                            int row) noexcept {
        const Rect rect = access.world_rect(list);
        const int x = rect.x + 8;
        const int y = rect.y + row * kRowHeight + kRowHeight / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseMove, x, y, 0));
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1));
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1));
    }
}

int main() {
    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    Handles handles{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        handles.root = builder.create_container();
        handles.list = builder.create_list_view();

        builder.link(handles.root, handles.list);
        builder.set_rect(handles.root, {0, 0, 180, 120});
        builder.set_rect(handles.list, {12, 12, 156, 84});
        builder.set_input_root(handles.root);
        builder.set_root(handles.root);
    });

    auto access = scene.access();
    access.set_list_view_source(handles.list, 3, nullptr, &RowData::text);
    access.set_list_view_row_flags_source(handles.list, nullptr, &RowData::row_flags);
    access.set_list_row_height(handles.list, kRowHeight);
    access.set_scroll_step(handles.list, kRowHeight);

    if (!expect(access.list_view_selected(handles.list) == -1, "selection starts empty")) return 1;
    if (!expect((access.list_view_item_row_flags(handles.list, 0) & ::ui::scene::kListViewRowFlagGroup) != 0,
                "group row flag is visible through SceneAccess")) {
        return 1;
    }
    if (!expect((access.list_view_item_row_flags(handles.list, 1) & ::ui::scene::kListViewRowFlagDisabled) != 0,
                "disabled row flag is visible through SceneAccess")) {
        return 1;
    }
    if (!expect(access.list_view_item_row_flags(handles.list, 2) == 0,
                "plain row exposes no extra capability")) {
        return 1;
    }

    dispatch_row_click(scene, access, handles.list, 1);
    if (!expect(access.list_view_selected(handles.list) == -1,
                "disabled row click does not change selection truth")) {
        return 1;
    }

    dispatch_row_click(scene, access, handles.list, 2);
    if (!expect(access.list_view_selected(handles.list) == 2,
                "enabled row click advances selection truth")) {
        return 1;
    }

    access.set_list_view_selected(handles.list, -1);
    if (!expect(access.list_view_selected(handles.list) == -1,
                "programmatic selection can clear back to empty truth")) {
        return 1;
    }

    access.set_list_view_selected(handles.list, 1);
    if (!expect(access.list_view_selected(handles.list) == 1,
                "programmatic selection can still target disabled row")) {
        return 1;
    }

    std::printf("[list_row_flags] selected=%d flags0=%u flags1=%u flags2=%u\n",
                access.list_view_selected(handles.list),
                static_cast<unsigned>(access.list_view_item_row_flags(handles.list, 0)),
                static_cast<unsigned>(access.list_view_item_row_flags(handles.list, 1)),
                static_cast<unsigned>(access.list_view_item_row_flags(handles.list, 2)));
    std::puts("[list_row_flags_demo] ok");
    return 0;
}
