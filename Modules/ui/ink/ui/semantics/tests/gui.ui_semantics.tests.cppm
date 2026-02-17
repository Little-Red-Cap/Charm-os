//
// Minimal table-driven tests for gui.ui_semantics (no framework).
//

module;
#include <cstdint>
#include <cstdlib>
#include <optional>

export module gui.ui_semantics.tests;

import gui.ui_semantics;
import gui.ui_tree;
import input.intent;

#if defined(UI_SEM_TEST) && UI_SEM_TEST

namespace {
    using gui::ui::UiSemantics;
    using gui::ui::ActivationKind;
    using gui::ui::CaptureKind;
    using gui::ui::InteractionPhase;
    using gui::ui::NavKind;
    using gui::ui::NavWrap;
    using gui::ui::NodeId;
    using gui::ui::kNullId;

    constexpr NodeId kDomain = gui::ui::fnv1a("test_domain");
    constexpr NodeId kOwner = gui::ui::fnv1a("popup_owner");
    constexpr NodeId kModel = gui::ui::fnv1a("test_model");

    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void assert_true(bool v) noexcept { if (!v) fail(); }
    template<class A, class B>
    inline void assert_eq(const A& a, const B& b) noexcept { if (!(a == b)) fail(); }
    template<class A, class B>
    inline void assert_ne(const A& a, const B& b) noexcept { if (!(a != b)) fail(); }

    inline input::Intent I_navprev() noexcept { return input::Intent{input::IntentType::NavPrev, 0, 0, 0}; }
    inline input::Intent I_navnext() noexcept { return input::Intent{input::IntentType::NavNext, 0, 0, 0}; }
    inline input::Intent I_activate() noexcept { return input::Intent{input::IntentType::Activate, 0, 0, 0}; }
    inline input::Intent I_back() noexcept { return input::Intent{input::IntentType::Back, 0, 0, 0}; }
    inline input::Intent I_adjust(std::int16_t a) noexcept { return input::Intent{input::IntentType::Adjust, a, 0, 0}; }

    inline UiSemantics make_base_list(std::int16_t count,
                                      std::int16_t index,
                                      NavWrap wrap = NavWrap::Ring) noexcept
    {
        UiSemantics s{};
        s.model_id = kModel;
        s.focus.domain_id = kDomain;
        s.focus.count = count;
        s.focus.index = index;
        s.focus.target_id = (index >= 0 && index < count)
                                ? gui::ui::list_id(kDomain, (std::uint16_t)(index + 1))
                                : kNullId;
        s.nav.kind = NavKind::List;
        s.nav.wrap = wrap;
        s.phase = InteractionPhase::Idle;
        return s;
    }

    inline UiSemantics make_base_grid(std::int16_t count,
                                      std::int16_t index,
                                      std::int16_t cols,
                                      NavWrap wrap) noexcept
    {
        UiSemantics s = make_base_list(count, index, wrap);
        s.nav.kind = NavKind::Grid;
        s.nav.cols = cols;
        return s;
    }

    inline UiSemantics make_base_free(NodeId target = kNullId) noexcept
    {
        UiSemantics s{};
        s.model_id = kModel;
        s.focus.domain_id = kDomain;
        s.focus.index = -1;
        s.focus.count = 0;
        s.focus.target_id = target;
        s.nav.kind = NavKind::Free;
        s.nav.wrap = NavWrap::Clamp;
        s.phase = InteractionPhase::Idle;
        return s;
    }

    struct TestCase {
        const char* name;
        UiSemantics prev;
        std::optional<input::Intent> it;
        void (*check)(const UiSemantics& next);
    };

    void check_A1(const UiSemantics& next) {
        assert_eq(next.activation.kind, ActivationKind::None);
        assert_eq(next.phase, InteractionPhase::Idle);
    }

    void check_A2(const UiSemantics& next) {
        assert_eq(next.activation.kind, ActivationKind::None);
        assert_eq(next.phase, InteractionPhase::Navigate);
        assert_eq(next.focus.index, (std::int16_t)2);
    }

    void check_B1(const UiSemantics& next) {
        assert_eq(next.phase, InteractionPhase::Activate);
        assert_eq(next.activation.kind, ActivationKind::Activate);
        assert_eq(next.activation.target_id, kOwner);
        assert_eq(next.capture.kind, CaptureKind::None);
    }

    void check_B2(const UiSemantics& next) {
        assert_eq(next.phase, InteractionPhase::Activate);
        assert_eq(next.activation.kind, ActivationKind::Back);
        assert_eq(next.activation.target_id, kOwner);
        assert_eq(next.capture.kind, CaptureKind::None);
    }

    void check_B3(const UiSemantics& next) {
        assert_eq(next.phase, InteractionPhase::Activate);
        assert_eq(next.activation.target_id, kOwner);
        assert_eq(next.capture.kind, CaptureKind::None);
    }

    void check_B4(const UiSemantics& next) {
        assert_eq(next.phase, InteractionPhase::Idle);
        assert_eq(next.activation.kind, ActivationKind::None);
        assert_eq(next.focus.index, (std::int16_t)1);
    }

    void check_C1(const UiSemantics& next) {
        assert_eq(next.phase, InteractionPhase::Navigate);
        assert_eq(next.activation.kind, ActivationKind::Submit);
        assert_eq(next.activation.target_id, kOwner);
        assert_eq(next.capture.kind, CaptureKind::Popup);
        assert_eq(next.focus.index, (std::int16_t)2);
        assert_eq(next.focus.last_dir, (std::int16_t)1);
        assert_true(next.focus.last_jump);
    }

    void check_C2(const UiSemantics& next) {
        assert_eq(next.phase, InteractionPhase::Idle);
        assert_eq(next.activation.kind, ActivationKind::None);
        assert_eq(next.capture.kind, CaptureKind::Popup);
        assert_eq(next.focus.index, (std::int16_t)2);
    }

    void check_D1(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)4);
        assert_eq(next.focus.last_dir, (std::int16_t)-1);
        assert_eq(next.phase, InteractionPhase::Navigate);
    }

    void check_D2(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)0);
    }

    void check_D3(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)3);
        assert_true(next.focus.last_jump);
    }

    void check_E1(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)2);
        assert_eq(next.phase, InteractionPhase::Navigate);
    }

    void check_E2(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)7);
    }

    void check_E3(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)7);
    }

    void check_E4(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)7);
    }

    void check_E5(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)7);
    }

    void check_F1(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)-1);
        assert_eq(next.focus.count, (std::int16_t)0);
        assert_eq(next.focus.target_id, (NodeId)123);
    }

    void check_F2(const UiSemantics& next) {
        assert_eq(next.activation.kind, ActivationKind::Activate);
        assert_eq(next.activation.target_id, (NodeId)123);
    }

    void check_F3(const UiSemantics& next) {
        assert_eq(next.phase, InteractionPhase::Idle);
        assert_eq(next.activation.kind, ActivationKind::None);
        assert_eq(next.focus.index, (std::int16_t)-1);
        assert_eq(next.focus.count, (std::int16_t)0);
        assert_eq(next.focus.target_id, (NodeId)123);
    }

    void check_E6(const UiSemantics& next) {
        assert_eq(next.focus.index, (std::int16_t)4);
        assert_eq(next.phase, InteractionPhase::Navigate);
    }

    const TestCase kCases[] = {
        { "A1.clear_activation_no_intent",
          [](){ auto s = make_base_list(5, 1); s.activation.kind = ActivationKind::Activate; s.activation.target_id = 123; return s; }(),
          std::nullopt, check_A1 },
        { "A2.clear_activation_with_nav",
          [](){ auto s = make_base_list(5, 1); s.activation.kind = ActivationKind::Activate; s.activation.target_id = 123; return s; }(),
          I_navnext(), check_A2 },
        { "B1.popup_activate_releases",
          [](){ auto s = make_base_list(5, 1); s.capture.kind = CaptureKind::Popup; s.capture.owner_id = kOwner; return s; }(),
          I_activate(), check_B1 },
        { "B2.popup_back_releases",
          [](){ auto s = make_base_list(5, 1); s.capture.kind = CaptureKind::Popup; s.capture.owner_id = kOwner; return s; }(),
          I_back(), check_B2 },
        { "B3.modal_activate_releases",
          [](){ auto s = make_base_list(5, 1); s.capture.kind = CaptureKind::Modal; s.capture.owner_id = kOwner; return s; }(),
          I_activate(), check_B3 },
        { "B4.modal_nav_ignored",
          [](){ auto s = make_base_list(5, 1); s.capture.kind = CaptureKind::Modal; s.capture.owner_id = kOwner; return s; }(),
          I_navnext(), check_B4 },
        { "C1.popup_adjust_submit",
          [](){ auto s = make_base_list(5, 2); s.capture.kind = CaptureKind::Popup; s.capture.owner_id = kOwner; return s; }(),
          I_adjust(3), check_C1 },
        { "C2.popup_adjust_zero_no_submit",
          [](){ auto s = make_base_list(5, 2); s.capture.kind = CaptureKind::Popup; s.capture.owner_id = kOwner; return s; }(),
          I_adjust(0), check_C2 },
        { "D1.list_ring_wrap_prev",
          make_base_list(5, 0, NavWrap::Ring),
          I_navprev(), check_D1 },
        { "D2.list_clamp_prev",
          make_base_list(5, 0, NavWrap::Clamp),
          I_navprev(), check_D2 },
        { "D3.list_adjust_jump",
          make_base_list(5, 4, NavWrap::Ring),
          I_adjust(-2), check_D3 },
        { "E1.grid_ring_down_wrap",
          make_base_grid(8, 7, 3, NavWrap::Ring),
          I_adjust(1), check_E1 },
        { "E2.grid_ring_left_wrap",
          make_base_grid(8, 0, 3, NavWrap::Ring),
          I_navprev(), check_E2 },
        { "E3.grid_clamp_down_full",
          make_base_grid(8, 4, 3, NavWrap::Clamp),
          I_adjust(1), check_E3 },
        { "E4.grid_clamp_down_s1",
          make_base_grid(8, 5, 3, NavWrap::Clamp),
          I_adjust(1), check_E4 },
        { "E5.grid_clamp_right",
          make_base_grid(8, 6, 3, NavWrap::Clamp),
          I_navnext(), check_E5 },
        { "E6.grid_clamp_index_oob",
          [](){ auto s = make_base_grid(5, 99, 3, NavWrap::Clamp); return s; }(),
          I_adjust(1), check_E6 },
        { "F1.free_nav_ignored",
          make_base_free((NodeId)123),
          I_navnext(), check_F1 },
        { "F2.free_activate",
          make_base_free((NodeId)123),
          I_activate(), check_F2 },
        { "F3.free_adjust_zero_idle",
          make_base_free((NodeId)123),
          I_adjust(0), check_F3 },
    };

    void run_all_cases() {
        for (const auto& tc : kCases) {
            UiSemantics next = gui::ui::reduce_semantics(tc.prev, tc.it);
            if (!tc.check) fail();
            tc.check(next);
        }
    }
} // namespace

export void run_ui_semantics_tests() { run_all_cases(); }

#endif
