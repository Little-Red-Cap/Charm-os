    struct ScrollContainerCiNode {
        WidgetHandle handle{};
        ObjectBase* object{nullptr};
    };

    struct ScrollContainerCiFixture {
        std::array<WidgetHandle, 2> child_storage{};
        ScrollContainer scroll{};
        std::array<ScrollContainerCiNode, 2> nodes{};
        std::size_t count{0};

        ScrollContainerCiFixture() noexcept {
            scroll.attach_child_storage(child_storage);
            scroll.set_rect({0, 0, 120, 80});
        }

        ObjectBase* resolve(WidgetHandle handle) noexcept {
            for (std::size_t i = 0; i < count; ++i) {
                if (nodes[i].handle == handle) {
                    return nodes[i].object;
                }
            }
            return nullptr;
        }

        template<typename Widget>
        WidgetHandle add_child(WidgetKind kind, std::uint16_t index, Widget& widget) noexcept {
            if (count >= nodes.size()) return {};
            const WidgetHandle handle{kind, index, 1};
            const bool linked = scroll.add_child(handle);
            if (!linked) return {};
            nodes[count++] = ScrollContainerCiNode{handle, &widget};
            return handle;
        }
    };

    bool run_scroll_container_ci() noexcept {
        ScrollContainerCiFixture fixture{};
        ScrollContainer child_a{};
        ScrollContainer child_b{};

        child_a.set_rect({4, 6, 100, 40});
        child_b.set_rect({4, 74, 100, 40});

        const auto handle_a = fixture.add_child(WidgetKind::ScrollContainer, 1, child_a);
        const auto handle_b = fixture.add_child(WidgetKind::ScrollContainer, 2, child_b);
        if (!handle_a || !handle_b) {
            return false;
        }

        fixture.scroll.sync_child_bases([&](WidgetHandle handle) noexcept {
            return fixture.resolve(handle);
        });
        const Rect before_sync_b = child_b.get_rect();

        const bool idle_mouse_up_consumed = fixture.scroll.on_event(Event::mouse(Event::Type::MouseUp, 8, 8));
        const bool idle_drag_end_consumed =
            fixture.scroll.on_event(Event::drag(Event::Type::DragEnd, 8, 8, 0, 0));
        if (idle_mouse_up_consumed || idle_drag_end_consumed) {
            return false;
        }

        const bool wheel_consumed = fixture.scroll.on_event(Event::wheel(8, 8, -1));
        if (!wheel_consumed || fixture.scroll.scroll_y() <= 0) {
            return false;
        }

        const int scroll_after_wheel = fixture.scroll.scroll_y();
        fixture.scroll.apply_scroll([&](WidgetHandle handle) noexcept {
            return fixture.resolve(handle);
        }, false);
        const Rect after_apply_b = child_b.get_rect();
        const bool child_followed_scroll = after_apply_b.y == before_sync_b.y - scroll_after_wheel;
        if (!child_followed_scroll) {
            return false;
        }

        fixture.scroll.sync_child_bases([&](WidgetHandle handle) noexcept {
            return fixture.resolve(handle);
        });
        const Rect after_sync_b = child_b.get_rect();
        const bool sync_kept_scroll = fixture.scroll.scroll_y() == scroll_after_wheel;
        const bool sync_kept_positions = after_sync_b.y == after_apply_b.y;
        if (!sync_kept_scroll || !sync_kept_positions) {
            return false;
        }

        const bool drag_start_consumed =
            fixture.scroll.on_event(Event::drag(Event::Type::DragStart, 8, 8, 0, 0));
        const bool drag_move_consumed =
            fixture.scroll.on_event(Event::drag(Event::Type::DragMove, 8, 28, 0, 20));
        const bool drag_end_consumed =
            fixture.scroll.on_event(Event::drag(Event::Type::DragEnd, 8, 28, 0, 0));
        return drag_start_consumed && drag_move_consumed && drag_end_consumed;
    }

    template<typename Widget>
    bool ui_ci_idle_release_rejected(Widget& widget) noexcept {
        const bool mouse_up_consumed = widget.on_event(Event::mouse(Event::Type::MouseUp, 8, 8));
        const bool drag_end_consumed = widget.on_event(Event::drag(Event::Type::DragEnd, 8, 8, 0, 0));
        return !mouse_up_consumed && !drag_end_consumed;
    }

    bool run_object_tree_release_guard_ci() noexcept {
        ListView list_view{};
        list_view.set_rect({0, 0, 160, 120});

        Slider slider{};
        slider.set_rect({0, 0, 160, 24});

        SpinZoomWidget spin_zoom{};
        spin_zoom.set_rect({0, 0, 120, 120});

        TextList text_list{};
        text_list.set_rect({0, 0, 180, 120});

        TextTrackingList text_tracking_list{};
        text_tracking_list.set_rect({0, 0, 180, 120});

        ScrollBar scrollbar{};
        scrollbar.set_rect({0, 0, 160, 12});

        const bool release_guards_ok =
            ui_ci_idle_release_rejected(list_view)
            && ui_ci_idle_release_rejected(slider)
            && ui_ci_idle_release_rejected(spin_zoom)
            && ui_ci_idle_release_rejected(text_list)
            && ui_ci_idle_release_rejected(text_tracking_list)
            && ui_ci_idle_release_rejected(scrollbar);

        if (!release_guards_ok) {
            return false;
        }

        const bool page_press_consumed = scrollbar.on_event(Event::mouse(Event::Type::MouseDown, 140, 6));
        const bool page_release_consumed = scrollbar.on_event(Event::mouse(Event::Type::MouseUp, 140, 6));
        return page_press_consumed && page_release_consumed;
    }

    bool run_popup_mouse_down_guard_ci() noexcept {
        SoaKernel kernel{};
        SoaFactory factory{kernel};
        const auto root = factory.create_container();
        const auto host = factory.create_button_static("Mode");
        factory.link(root, host);
        kernel.set_rect(root, {0, 0, 320, 240});
        kernel.set_rect(host, {8, 8, 100, 28});

        DropdownPopup popup{factory, host, root};
        const char* const options[] = {"Low", "Mid", "High"};
        popup.set_options(options, 3);
        popup.open_at(20, 40, 120);
        const bool popup_inside_down =
            popup.handle_event(Event::mouse(Event::Type::MouseDown, 28, 52, 1));
        if (!popup_inside_down || !popup.is_open()) {
            return false;
        }
        const bool popup_outside_down =
            popup.handle_event(Event::mouse(Event::Type::MouseDown, 4, 4, 1));
        if (!popup_outside_down || popup.is_open()) {
            return false;
        }

        struct MenuData {
            static std::uint16_t count(const void*, int menu_id) noexcept {
                return (menu_id == 0) ? 2 : 0;
            }
            static const char* label(const void*, int, std::uint16_t index) noexcept {
                static constexpr const char* kRoot[] = {"Play", "Queue"};
                return (index < 2) ? kRoot[index] : "";
            }
        };
        struct MenuSelection {
            int selected{0};
            static int get_selected(const void* ctx, int) noexcept {
                const auto* self = static_cast<const MenuSelection*>(ctx);
                return self ? self->selected : -1;
            }
            static void set_selected(const void* ctx, int, int index) noexcept {
                auto* self = static_cast<MenuSelection*>(const_cast<void*>(ctx));
                if (self) self->selected = index;
            }
        };

        MenuSelection selection{};
        MenuTree menu{};
        menu.init(factory, root);
        menu.set_root_menu(0);
        menu.set_item_height(24);
        menu.set_provider(MenuTree::MenuProvider{
            &selection,
            &MenuData::count,
            &MenuData::label,
            nullptr,
            nullptr,
            nullptr,
        });
        menu.set_selection_model(MenuTree::MenuSelectionModel{
            &selection,
            &MenuSelection::get_selected,
            &MenuSelection::set_selected,
            nullptr,
        });
        menu.open_at(20, 80, 120);
        const bool menu_inside_down =
            menu.handle_event(Event::mouse(Event::Type::MouseDown, 28, 92, 1));
        if (!menu_inside_down || !menu.is_open()) {
            return false;
        }
        const bool menu_outside_down =
            menu.handle_event(Event::mouse(Event::Type::MouseDown, 4, 4, 1));
        return menu_outside_down && !menu.is_open();
    }

    bool run_dropdown_popup_commit_ci() noexcept {
        SoaKernel kernel{};
        SoaFactory factory{kernel};
        const auto root = factory.create_container();
        const auto host = factory.create_button_static("Mode");
        factory.link(root, host);
        kernel.set_rect(root, {0, 0, 320, 240});
        kernel.set_rect(host, {8, 8, 100, 28});

        struct PopupProbe {
            int select_calls{0};
            int selected_calls{0};
            int last_confirmed{-1};
            int last_selected{-1};
            int last_selected_prev{-1};

            void on_select(const int& value) noexcept {
                select_calls += 1;
                last_confirmed = value;
            }

            void on_selected(const int& value, const int& previous) noexcept {
                selected_calls += 1;
                last_selected = value;
                last_selected_prev = previous;
            }
        };

        const char* const options[] = {"Low", "Mid", "High"};
        PopupProbe probe{};
        DropdownPopup popup{factory, host, root};
        popup.set_options(options, 3);
        popup.set_selection(1);
        popup.set_on_select(DropdownPopup::select_callback::bind<&PopupProbe::on_select>(probe));
        const auto selected_conn = popup.observe_selected(DropdownPopup::selected_slot_type::bind<&PopupProbe::on_selected>(probe));
        if (!selected_conn) {
            return false;
        }

        popup.open_at(20, 40, 120);
        if (!popup.is_open() || popup.selected() != 1) {
            return false;
        }

        const bool blank_click =
            popup.handle_event(Event::mouse(Event::Type::Click, 28, 120, 1));
        if (!blank_click || popup.is_open() || popup.selected() != 1
            || probe.select_calls != 0 || probe.last_confirmed != -1) {
            return false;
        }

        popup.open_at(20, 40, 120);
        const bool item_click =
            popup.handle_event(Event::mouse(Event::Type::Click, 28, 92, 1));
        return item_click
            && !popup.is_open()
            && popup.selected() == 2
            && probe.select_calls == 1
            && probe.last_confirmed == 2
            && probe.selected_calls >= 1
            && probe.last_selected == 2
            && probe.last_selected_prev == 1;
    }

    bool run_menu_tree_disabled_item_ci() noexcept {
        SoaKernel kernel{};
        SoaFactory factory{kernel};
        const auto root = factory.create_container();
        kernel.set_rect(root, {0, 0, 400, 240});

        struct MenuData {
            static std::uint16_t count(const void*, int menu_id) noexcept {
                switch (menu_id) {
                case 0: return 2;
                case 1: return 1;
                default: return 0;
                }
            }

            static const char* label(const void*, int menu_id, std::uint16_t index) noexcept {
                static constexpr const char* kRoot[] = {"Disabled Group", "Enabled Leaf"};
                static constexpr const char* kSub[] = {"Child"};
                if (menu_id == 0) return (index < 2) ? kRoot[index] : "";
                if (menu_id == 1) return (index < 1) ? kSub[index] : "";
                return "";
            }

            static bool enabled(const void*, int menu_id, std::uint16_t index) noexcept {
                if (menu_id == 0 && index == 0) return false;
                return true;
            }

            static bool has_children(const void*, int menu_id, std::uint16_t index) noexcept {
                return menu_id == 0 && index == 0;
            }

            static int child_id(const void*, int menu_id, std::uint16_t index) noexcept {
                return (menu_id == 0 && index == 0) ? 1 : -1;
            }
        };

        struct MenuSelection {
            int root_selected{-1};
            int submenu_selected{-1};

            static int get_selected(const void* ctx, int menu_id) noexcept {
                const auto* self = static_cast<const MenuSelection*>(ctx);
                if (!self) return -1;
                return (menu_id == 1) ? self->submenu_selected : self->root_selected;
            }

            static void set_selected(const void* ctx, int menu_id, int index) noexcept {
                auto* self = static_cast<MenuSelection*>(const_cast<void*>(ctx));
                if (!self) return;
                if (menu_id == 1) self->submenu_selected = index;
                else self->root_selected = index;
            }

            static void clear_selected(const void* ctx, int menu_id) noexcept {
                auto* self = static_cast<MenuSelection*>(const_cast<void*>(ctx));
                if (!self) return;
                if (menu_id == 1) self->submenu_selected = -1;
                else self->root_selected = -1;
            }
        };

        struct MenuSelectProbe {
            int select_calls{0};
            MenuTree::menu_item_ref last_ref{};

            void on_select(const MenuTree::menu_item_ref& ref) noexcept {
                select_calls += 1;
                last_ref = ref;
            }
        };

        MenuSelection selection{};
        MenuSelectProbe probe{};
        MenuTree menu{};
        menu.init(factory, root);
        menu.set_root_menu(0);
        menu.set_item_height(24);
        menu.set_provider(MenuTree::MenuProvider{
            &selection,
            &MenuData::count,
            &MenuData::label,
            &MenuData::enabled,
            &MenuData::has_children,
            &MenuData::child_id,
        });
        menu.set_selection_model(MenuTree::MenuSelectionModel{
            &selection,
            &MenuSelection::get_selected,
            &MenuSelection::set_selected,
            &MenuSelection::clear_selected,
        });
        menu.set_on_select(MenuTree::select_callback::bind<&MenuSelectProbe::on_select>(probe));

        menu.open_at(20, 80, 120);
        if (!menu.is_open() || selection.root_selected != 0) {
            return false;
        }

        const bool disabled_move =
            menu.handle_event(Event::mouse(Event::Type::MouseMove, 28, 92, 0));
        if (!disabled_move || selection.root_selected != 0 || probe.select_calls != 0) {
            return false;
        }

        const bool disabled_click =
            menu.handle_event(Event::mouse(Event::Type::Click, 28, 92, 1));
        if (!disabled_click || !menu.is_open() || selection.root_selected != 0
            || probe.select_calls != 0) {
            return false;
        }

        // If the disabled group had opened a submenu, this click would stay inside the menu system and keep it open.
        const bool submenu_area_click =
            menu.handle_event(Event::mouse(Event::Type::Click, 148, 92, 1));
        if (!submenu_area_click || menu.is_open() || probe.select_calls != 0) {
            return false;
        }

        menu.open_at(20, 80, 120);
        const bool enabled_click =
            menu.handle_event(Event::mouse(Event::Type::Click, 28, 116, 1));
        return enabled_click
            && !menu.is_open()
            && selection.root_selected == 1
            && probe.select_calls == 1
            && probe.last_ref.menu_id == 0
            && probe.last_ref.index == 1;
    }

    bool run_table_view_hit_ci() noexcept {
        struct TableSource {
            static int row_count(void*) noexcept { return 2; }
            static int col_count(void*) noexcept { return 2; }
        };
        struct SelectionProbe {
            int calls{0};
            int row{-1};
            int col{-1};

            static void on_select(void* ctx, int row, int col) noexcept {
                auto* self = static_cast<SelectionProbe*>(ctx);
                if (!self) return;
                self->calls += 1;
                self->row = row;
                self->col = col;
            }
        };

        std::array<int, 2> table_column_widths{};
        TableView table{};
        table.set_rect({0, 0, 160, 96});
        table.set_row_height(24);
        table.set_data_source(&TableSource::row_count, &TableSource::col_count, nullptr, nullptr);
        if (!table.attach_column_width_storage(table_column_widths)
            || !table.set_column_width(0, 40)
            || !table.set_column_width(1, 50)) {
            return false;
        }

        SelectionProbe probe{};
        table.set_on_select(&SelectionProbe::on_select, &probe);

        const bool valid_click = table.on_event(Event::mouse(Event::Type::Click, 44, 12, 1));
        if (!valid_click || probe.calls != 1 || probe.row != 0 || probe.col != 1) {
            return false;
        }

        const bool trailing_blank_click = table.on_event(Event::mouse(Event::Type::Click, 120, 12, 1));
        if (trailing_blank_click || probe.calls != 1 || probe.row != 0 || probe.col != 1) {
            return false;
        }

        const bool bottom_blank_click = table.on_event(Event::mouse(Event::Type::Click, 12, 80, 1));
        return !bottom_blank_click && probe.calls == 1 && probe.row == 0 && probe.col == 1;
    }

    bool run_list_view_disabled_row_ci() noexcept {
        struct ListProbe {
            int select_calls{0};
            int last_selected{-1};

            static int count(void*) noexcept { return 3; }

            static std::uint8_t row_flags(const void*, std::uint16_t index) noexcept {
                return (index == 1) ? kStructuredListRowFlagDisabled : 0;
            }

            static void on_select(void* ctx, int index) noexcept {
                auto* self = static_cast<ListProbe*>(ctx);
                if (!self) return;
                self->select_calls += 1;
                self->last_selected = index;
            }
        };

        ListProbe probe{};
        ListView list{};
        list.set_rect({0, 0, 180, 120});
        list.set_item_count(3);
        list.set_row_height(24);
        list.set_row_flags_fn(&ListProbe::row_flags, &probe);
        list.set_on_select(&ListProbe::on_select, &probe);

        list.set_selected(0);
        if (list.selected() != 0 || probe.select_calls != 1 || probe.last_selected != 0) {
            return false;
        }

        const bool disabled_click = list.on_event(Event::mouse(Event::Type::Click, 12, 36, 1));
        if (!disabled_click || list.selected() != 0 || probe.select_calls != 1 || probe.last_selected != 0) {
            return false;
        }

        const bool enabled_click = list.on_event(Event::mouse(Event::Type::Click, 12, 60, 1));
        return enabled_click && list.selected() == 2 && probe.select_calls == 2 && probe.last_selected == 2;
    }

    bool run_text_list_blank_click_ci() noexcept {
        static constexpr const char* kItemLiterals[] = {"Alpha", "Beta"};
        const char* items[] = {kItemLiterals[0], kItemLiterals[1]};

        TextList text_list{};
        text_list.set_rect({0, 0, 180, 120});
        text_list.set_row_height(24);
        text_list.set_items(items, 2);
        text_list.set_selected(1);

        const bool text_valid_click = text_list.on_event(Event::mouse(Event::Type::Click, 12, 12, 1));
        if (!text_valid_click || text_list.selected() != 0) {
            return false;
        }

        text_list.set_selected(1);
        const bool text_blank_click = text_list.on_event(Event::mouse(Event::Type::Click, 12, 84, 1));
        if (text_blank_click || text_list.selected() != 1) {
            return false;
        }

        TextTrackingList tracking_list{};
        tracking_list.set_rect({0, 0, 180, 120});
        tracking_list.set_row_height(24);
        tracking_list.set_items(items, 2);
        tracking_list.set_selected(1);

        const bool tracking_valid_click = tracking_list.on_event(Event::mouse(Event::Type::Click, 12, 12, 1));
        if (!tracking_valid_click || tracking_list.selected() != 0) {
            return false;
        }

        tracking_list.set_selected(1);
        const bool tracking_blank_click = tracking_list.on_event(Event::mouse(Event::Type::Click, 12, 84, 1));
        return !tracking_blank_click && tracking_list.selected() == 1;
    }

    bool run_icon_list_contract_ci() noexcept {
        static constexpr IconListItem kItems[]{
            IconListItem{nullptr, "One", rgba{255, 180, 90, 80}},
            IconListItem{nullptr, "Two", rgba{255, 80, 140, 220}},
            IconListItem{nullptr, "Three", rgba{255, 90, 180, 120}},
        };

        struct IconProbe {
            static std::uint8_t row_flags(const void*, std::uint16_t index) noexcept {
                return (index == 1) ? kStructuredListRowFlagDisabled : 0;
            }
        };

        IconList list{};
        list.set_rect({0, 0, 200, 120});
        list.set_row_height(24);
        list.set_items(kItems, 3);
        list.set_row_flags_fn(&IconProbe::row_flags, nullptr);
        list.set_selected(0);

        const bool disabled_click = list.on_event(Event::mouse(Event::Type::Click, 16, 36, 1));
        if (!disabled_click || list.selected() != 0) {
            return false;
        }

        const bool enabled_click = list.on_event(Event::mouse(Event::Type::Click, 16, 60, 1));
        if (!enabled_click || list.selected() != 2) {
            return false;
        }

        list.set_selected(2);
        const bool blank_click = list.on_event(Event::mouse(Event::Type::Click, 16, 96, 1));
        return !blank_click && list.selected() == 2;
    }

    bool run_modal_dialog_modal_guard_ci() noexcept {
        struct Probe {
            int ok_calls{0};
            int cancel_calls{0};

            void on_ok() noexcept {
                ok_calls += 1;
            }

            void on_cancel() noexcept {
                cancel_calls += 1;
            }
        };

        Probe probe{};
        ModalDialog dialog{};
        dialog.set_rect({20, 20, 160, 120});
        dialog.set_visible(true);
        dialog.set_show_cancel(true);
        dialog.set_dismiss_on_background(false);
        dialog.set_on_ok(Callback::bind<&Probe::on_ok>(probe));
        dialog.set_on_cancel(Callback::bind<&Probe::on_cancel>(probe));

        const bool outside_move = dialog.on_event(Event::mouse(Event::Type::MouseMove, 4, 4, 0));
        const bool outside_down = dialog.on_event(Event::mouse(Event::Type::MouseDown, 4, 4, 1));
        const bool outside_up = dialog.on_event(Event::mouse(Event::Type::MouseUp, 4, 4, 1));
        const bool outside_click = dialog.on_event(Event::mouse(Event::Type::Click, 4, 4, 1));
        const bool outside_wheel = dialog.on_event(Event::wheel(4, 4, -1));
        if (!outside_move || !outside_down || !outside_up || !outside_click || !outside_wheel
            || !dialog.is_visible() || probe.ok_calls != 0 || probe.cancel_calls != 0) {
            return false;
        }

        dialog.set_dismiss_on_background(true);
        const bool dismiss_down = dialog.on_event(Event::mouse(Event::Type::MouseDown, 4, 4, 1));
        if (!dismiss_down || dialog.is_visible() || probe.cancel_calls != 1 || probe.ok_calls != 0) {
            return false;
        }

        dialog.set_visible(true);
        const bool ok_down = dialog.on_event(Event::mouse(Event::Type::MouseDown, 72, 126, 1));
        const bool ok_up = dialog.on_event(Event::mouse(Event::Type::MouseUp, 72, 126, 1));
        return ok_down && ok_up && probe.ok_calls == 1 && probe.cancel_calls == 1;
    }

    bool run_tree_view_click_contract_ci() noexcept {
        struct TreeProbe {
            bool expanded{false};
            int toggle_calls{0};
            int select_calls{0};
            int last_toggled{-1};
            int last_selected{-1};

            static int count(void* ctx) noexcept {
                auto* self = static_cast<TreeProbe*>(ctx);
                return self ? 2 : 0;
            }

            static TreeView::NodeInfo node(void* ctx, int index) noexcept {
                auto* self = static_cast<TreeProbe*>(ctx);
                if (!self || index < 0 || index >= 2) return {};
                if (index == 0) {
                    return TreeView::NodeInfo{0, self->expanded, true, "Root"};
                }
                return TreeView::NodeInfo{1, false, false, "Leaf"};
            }

            static void on_toggle(void* ctx, int index) noexcept {
                auto* self = static_cast<TreeProbe*>(ctx);
                if (!self) return;
                self->toggle_calls += 1;
                self->last_toggled = index;
                if (index == 0) {
                    self->expanded = !self->expanded;
                }
            }

            static void on_select(void* ctx, int index) noexcept {
                auto* self = static_cast<TreeProbe*>(ctx);
                if (!self) return;
                self->select_calls += 1;
                self->last_selected = index;
            }
        };

        TreeProbe probe{};
        TreeView tree{};
        tree.set_rect({0, 0, 200, 80});
        tree.set_row_height(24);
        tree.set_data_source(&TreeProbe::count, &TreeProbe::node, nullptr, &probe);
        tree.set_on_toggle(&TreeProbe::on_toggle);
        tree.set_on_select(&TreeProbe::on_select, &probe);

        const bool toggle_click = tree.on_event(Event::mouse(Event::Type::Click, 8, 12, 1));
        if (!toggle_click
            || probe.toggle_calls != 1
            || probe.last_toggled != 0
            || !probe.expanded
            || probe.select_calls != 1
            || probe.last_selected != 0) {
            return false;
        }

        const bool label_click = tree.on_event(Event::mouse(Event::Type::Click, 24, 12, 1));
        if (!label_click
            || probe.toggle_calls != 1
            || probe.select_calls != 2
            || probe.last_selected != 0) {
            return false;
        }

        const bool second_row_click = tree.on_event(Event::mouse(Event::Type::Click, 40, 36, 1));
        if (!second_row_click
            || probe.toggle_calls != 1
            || probe.select_calls != 3
            || probe.last_selected != 1) {
            return false;
        }

        const bool blank_click = tree.on_event(Event::mouse(Event::Type::Click, 40, 72, 1));
        return !blank_click
            && probe.toggle_calls == 1
            && probe.select_calls == 3
            && probe.last_selected == 1;
    }

    void ui_ci_record_case(UiCiResult& res,
                           const char* name,
                           const char* reason,
                           bool ok) noexcept {
        ui_ci_emit(name, ok, ok ? nullptr : reason);
        if (!ok) {
            res.ok = false;
            res.failed++;
        }
    }

    void run_object_tree_input_regressions(UiCiResult& res) {
        ui_ci_record_case(res,
                          "scroll_container_object_tree_regression",
                          "scroll_container_object_tree",
                          run_scroll_container_ci());
        ui_ci_record_case(res,
                          "object_tree_release_guard_regression",
                          "object_tree_release_guard",
                          run_object_tree_release_guard_ci());
        ui_ci_record_case(res,
                          "popup_mouse_down_guard_regression",
                          "popup_mouse_down_guard",
                          run_popup_mouse_down_guard_ci());
        ui_ci_record_case(res,
                          "dropdown_popup_commit_regression",
                          "dropdown_popup_commit",
                          run_dropdown_popup_commit_ci());
        ui_ci_record_case(res,
                          "menu_tree_disabled_item_regression",
                          "menu_tree_disabled_item",
                          run_menu_tree_disabled_item_ci());
        ui_ci_record_case(res,
                          "table_view_hit_regression",
                          "table_view_hit",
                          run_table_view_hit_ci());
        ui_ci_record_case(res,
                          "list_view_disabled_row_regression",
                          "list_view_disabled_row",
                          run_list_view_disabled_row_ci());
        ui_ci_record_case(res,
                          "text_list_blank_click_regression",
                          "text_list_blank_click",
                          run_text_list_blank_click_ci());
        ui_ci_record_case(res,
                          "icon_list_contract_regression",
                          "icon_list_contract",
                          run_icon_list_contract_ci());
        ui_ci_record_case(res,
                          "modal_dialog_modal_guard_regression",
                          "modal_dialog_modal_guard",
                          run_modal_dialog_modal_guard_ci());
        ui_ci_record_case(res,
                          "tree_view_click_contract_regression",
                          "tree_view_click_contract",
                          run_tree_view_click_contract_ci());
    }
