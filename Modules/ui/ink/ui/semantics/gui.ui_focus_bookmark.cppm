// gui.ui_focus_bookmark.cppm
// Focus bookmark helpers: save/restore UiSemantics focus without app-specific logic.

module;
#include <cstdint>

export module gui.ui_focus_bookmark;

import gui.ui_semantics;
import gui.ui_list;
import gui.ui_tree;

export namespace gui::ui
{
    struct FocusBookmark {
        NodeId      domain_id{kNullId};
        std::int16_t index{-1};
        std::int16_t count{0};
        NodeId      target_id{kNullId};
    };

    inline void save_focus(const UiSemantics& sem, FocusBookmark& out) noexcept
    {
        out.domain_id = sem.focus.domain_id;
        out.index     = sem.focus.index;
        out.count     = sem.focus.count;
        out.target_id = sem.focus.target_id;
    }

    inline void restore_focus(UiSemantics& sem,
                              const FocusBookmark& bk,
                              NodeId default_domain,
                              std::int16_t default_count,
                              std::int16_t fallback_index = 0) noexcept
    {
        const bool has_bookmark = (bk.domain_id != kNullId) && (bk.count > 0);
        sem.focus.domain_id = has_bookmark ? bk.domain_id : default_domain;
        sem.focus.count     = has_bookmark ? bk.count     : default_count;
        auto idx = has_bookmark ? bk.index : fallback_index;
        if (idx < 0 || idx >= sem.focus.count) idx = fallback_index;
        sem.focus.index = idx;
        sem.focus.target_id = (has_bookmark && bk.target_id != kNullId)
            ? bk.target_id
            : list_id(sem.focus.domain_id, (std::uint16_t)(idx + 1));
        // callers may still clear capture/nav as needed
    }
}
