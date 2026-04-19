#include <cstdio>
#include <cstdint>

import charm.core.event;
import charm.core.soa_factory;
import charm.widgets.menu_tree;

namespace {
    constexpr int kPopupX = 20;
    constexpr int kPopupY = 40;
    constexpr int kPopupW = 120;
    constexpr int kItemH = 24;

    struct MenuData {
        static std::uint16_t count(const void*, int menu_id) noexcept {
            switch (menu_id) {
            case 0: return 2;
            case 1: return 2;
            default: return 0;
            }
        }

        static const char* label(const void*, int menu_id, std::uint16_t index) noexcept {
            static constexpr const char* kRoot[] = {"File", "Help"};
            static constexpr const char* kFile[] = {"Open", "Save"};
            if (menu_id == 0 && index < 2) return kRoot[index];
            if (menu_id == 1 && index < 2) return kFile[index];
            return "";
        }

        static bool has_children(const void*, int menu_id, std::uint16_t index) noexcept {
            return menu_id == 0 && index == 0;
        }

        static int child_menu(const void*, int menu_id, std::uint16_t index) noexcept {
            if (menu_id == 0 && index == 0) return 1;
            return -1;
        }
    };

    struct MenuSelection {
        int root_sel{-1};
        int file_sel{-1};

        static int get_selected(const void* ctx, int menu_id) noexcept {
            const auto* self = static_cast<const MenuSelection*>(ctx);
            if (!self) return -1;
            return (menu_id == 0) ? self->root_sel : self->file_sel;
        }

        static void set_selected(const void* ctx, int menu_id, int index) noexcept {
            auto* self = static_cast<MenuSelection*>(const_cast<void*>(ctx));
            if (!self) return;
            if (menu_id == 0) {
                self->root_sel = index;
            } else if (menu_id == 1) {
                self->file_sel = index;
            }
        }

        static void clear(const void* ctx, int menu_id) noexcept {
            auto* self = static_cast<MenuSelection*>(const_cast<void*>(ctx));
            if (!self) return;
            if (menu_id == 0) {
                self->root_sel = -1;
            } else if (menu_id == 1) {
                self->file_sel = -1;
            }
        }
    };

    struct Probe {
        int select_edges{0};
        int last_menu_id{-1};
        int last_index{-1};

        void on_select(const MenuTree::menu_item_ref& item) noexcept {
            ++select_edges;
            last_menu_id = item.menu_id;
            last_index = item.index;
        }
    };

    int g_legacy_selects = 0;

    void on_legacy_select() noexcept {
        ++g_legacy_selects;
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] int row_center_y(int panel_y, int index) noexcept {
        return panel_y + index * kItemH + kItemH / 2;
    }
}

int main() {
    SoaKernel kernel{};
    SoaFactory factory{kernel};
    const auto root = factory.create_container();
    kernel.set_rect(root, {0, 0, 320, 200});

    MenuSelection selection{};
    MenuTree menu{};
    menu.init(factory, root);
    menu.set_root_menu(0);
    menu.set_item_height(kItemH);
    menu.set_provider(MenuTree::MenuProvider{
        &selection,
        &MenuData::count,
        &MenuData::label,
        nullptr,
        &MenuData::has_children,
        &MenuData::child_menu,
    });
    menu.set_selection_model(MenuTree::MenuSelectionModel{
        &selection,
        &MenuSelection::get_selected,
        &MenuSelection::set_selected,
        &MenuSelection::clear,
    });

    Probe probe{};
    menu.set_on_select(Callback::bind<&on_legacy_select>());
    const auto select_conn =
        menu.observe_select(util::delegate<const MenuTree::menu_item_ref&>::bind<&Probe::on_select>(probe));
    if (!expect(static_cast<bool>(select_conn), "connect menu tree confirm edge observer")) return 1;

    menu.open_at(kPopupX, kPopupY, kPopupW);
    if (!expect(menu.is_open(), "menu opens")) return 1;
    if (!expect(selection.root_sel == 0, "opening menu materializes root highlight truth")) return 1;
    if (!expect(selection.file_sel == -1, "submenu truth stays untouched before submenu opens")) return 1;
    if (!expect(probe.select_edges == 0 && g_legacy_selects == 0, "opening menu does not emit confirm edge")) return 1;

    if (!expect(menu.handle_event(Event::mouse(Event::Type::MouseMove, kPopupX + 8, row_center_y(kPopupY, 1), 0)),
                "hovering root item is accepted")) {
        return 1;
    }
    if (!expect(selection.root_sel == 1, "hover updates external root highlight truth")) return 1;
    if (!expect(probe.select_edges == 0 && g_legacy_selects == 0, "hover stays silent for confirm edge")) return 1;

    if (!expect(menu.handle_event(Event::mouse(Event::Type::Click, kPopupX + 8, row_center_y(kPopupY, 1), 1)),
                "clicking root leaf is accepted")) {
        return 1;
    }
    if (!expect(!menu.is_open(), "root leaf confirm closes menu")) return 1;
    if (!expect(probe.select_edges == 1 && probe.last_menu_id == 0 && probe.last_index == 1,
                "root leaf click emits confirm edge")) {
        return 1;
    }
    if (!expect(g_legacy_selects == 1, "root leaf click still triggers legacy callback")) return 1;

    menu.open_at(kPopupX, kPopupY, kPopupW);
    if (!expect(menu.handle_event(Event::mouse(Event::Type::MouseMove, kPopupX + 8, row_center_y(kPopupY, 0), 0)),
                "hovering submenu root item is accepted")) {
        return 1;
    }
    if (!expect(selection.root_sel == 0, "hover returns root highlight truth to submenu owner")) return 1;
    if (!expect(selection.file_sel == 0, "opening submenu materializes submenu highlight truth")) return 1;
    if (!expect(probe.select_edges == 1 && g_legacy_selects == 1, "opening submenu does not emit confirm edge")) {
        return 1;
    }

    if (!expect(menu.handle_event(Event::mouse(Event::Type::MouseMove, kPopupX + kPopupW + 8, row_center_y(kPopupY, 1), 0)),
                "hovering submenu leaf is accepted")) {
        return 1;
    }
    if (!expect(selection.file_sel == 1, "hover updates external submenu highlight truth")) return 1;
    if (!expect(probe.select_edges == 1 && g_legacy_selects == 1, "submenu hover stays silent")) return 1;

    if (!expect(menu.handle_event(Event::key(Event::Type::KeyDown, Event::Key::Enter)),
                "enter confirms highlighted submenu leaf")) {
        return 1;
    }
    if (!expect(!menu.is_open(), "submenu leaf confirm closes menu")) return 1;
    if (!expect(probe.select_edges == 2 && probe.last_menu_id == 1 && probe.last_index == 1,
                "submenu enter emits confirm edge")) {
        return 1;
    }
    if (!expect(g_legacy_selects == 2, "submenu enter still triggers legacy callback")) return 1;

    menu.open_at(kPopupX, kPopupY, kPopupW);
    if (!expect(menu.handle_event(Event::mouse(Event::Type::Click, 4, 4, 1)),
                "outside click closes menu")) {
        return 1;
    }
    if (!expect(!menu.is_open(), "outside click closes menu without confirm")) return 1;
    if (!expect(probe.select_edges == 2 && g_legacy_selects == 2, "outside click does not emit hidden edge")) {
        return 1;
    }

    if (!expect(menu.unobserve_select(select_conn.value()), "disconnect menu tree edge observer")) return 1;
    if (!expect(!menu.unobserve_select(select_conn.value()), "stale menu tree edge token rejected")) return 1;

    menu.open_at(kPopupX, kPopupY, kPopupW);
    if (!expect(menu.handle_event(Event::mouse(Event::Type::Click, kPopupX + 8, row_center_y(kPopupY, 1), 1)),
                "root leaf still confirms after observer disconnect")) {
        return 1;
    }
    if (!expect(probe.select_edges == 2, "disconnected menu tree edge observer stays silent")) return 1;
    if (!expect(g_legacy_selects == 3, "legacy callback still works after observer disconnect")) return 1;

    std::printf("[menu_tree] root_sel=%d file_sel=%d edges=%d legacy=%d\n",
                selection.root_sel,
                selection.file_sel,
                probe.select_edges,
                g_legacy_selects);
    std::puts("[menu_tree_demo] ok");
    return 0;
}
