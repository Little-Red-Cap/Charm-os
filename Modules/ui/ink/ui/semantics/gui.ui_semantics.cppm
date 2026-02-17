//
// UI semantics: focus, navigation, capture, activation (frame-local value type).
//

module;
#include <cstdint>
#include <optional>

export module gui.ui_semantics;

import gui.ui_tree;
import input.intent;

export namespace gui::ui
{
    enum class InteractionPhase : std::uint8_t {
        Idle = 0,
        Navigate = 1,
        Activate = 2,
    };

    enum class NavKind : std::uint8_t {
        List = 0,
        Grid = 1,
        Free = 2,
    };

    enum class NavWrap : std::uint8_t {
        Clamp = 0,
        Ring = 1,
    };

    struct NavMode {
        NavKind kind{NavKind::List};
        NavWrap wrap{NavWrap::Ring};
        std::int16_t cols{0};
        std::int16_t rows{0};
    };

    enum class CaptureKind : std::uint8_t {
        None = 0,
        Popup = 1,
        Modal = 2,
    };

    struct CaptureState {
        CaptureKind kind{CaptureKind::None};
        NodeId owner_id{kNullId};
    };

    enum class ActivationKind : std::uint8_t {
        None = 0,
        Activate = 1,
        Toggle = 2,
        Submit = 3,
        Back = 4,
    };

    struct Activation {
        ActivationKind kind{ActivationKind::None};
        NodeId target_id{kNullId};
    };

    struct FocusState {
        NodeId target_id{kNullId};
        NodeId domain_id{kNullId};
        std::int16_t index{-1};
        std::int16_t count{0};
        std::int16_t last_dir{0};
        bool last_jump{false};
    };

    struct UiSemantics {
        NodeId model_id{kNullId};
        FocusState focus{};
        NavMode nav{};
        CaptureState capture{};
        Activation activation{};
        InteractionPhase phase{InteractionPhase::Idle};
    };

    [[nodiscard]] inline std::int16_t sign_i16(std::int16_t v) noexcept
    {
        return (v > 0) ? 1 : (v < 0 ? -1 : 0);
    }

    [[nodiscard]] inline std::int16_t apply_wrap(std::int16_t index,
                                                 std::int16_t count,
                                                 NavWrap wrap) noexcept
    {
        if (count <= 0) return -1;
        if (wrap == NavWrap::Ring) {
            int i = index % count;
            if (i < 0) i += count;
            return (std::int16_t)i;
        }
        if (index < 0) return 0;
        if (index >= count) return (std::int16_t)(count - 1);
        return index;
    }

    [[nodiscard]] inline UiSemantics reduce_semantics(const UiSemantics& prev,
                                                      const std::optional<input::Intent>& it) noexcept
    {
        UiSemantics next = prev;
        next.activation = Activation{};

        if (!it) {
            if (prev.phase != InteractionPhase::Idle) {
                next.phase = InteractionPhase::Idle;
            }
            return next;
        }

        using input::IntentType;
        const auto& intent = *it;

        if (intent.type == IntentType::Activate || intent.type == IntentType::Back) {
            next.phase = InteractionPhase::Activate;
            if (intent.type == IntentType::Back) {
                next.activation.kind = ActivationKind::Back;
                next.activation.target_id = (next.capture.kind != CaptureKind::None)
                                                ? next.capture.owner_id
                                                : next.model_id;
                if (next.capture.kind != CaptureKind::None) {
                    next.capture = CaptureState{};
                }
            } else {
                next.activation.kind = ActivationKind::Activate;
                if (next.capture.kind != CaptureKind::None) {
                    next.activation.target_id = next.capture.owner_id;
                    next.capture = CaptureState{};
                } else {
                    next.activation.target_id = next.focus.target_id;
                }
            }
            return next;
        }

        if (next.capture.kind == CaptureKind::Popup) {
            if (intent.type == IntentType::Adjust) {
                const std::int16_t dir = sign_i16(intent.a);
                if (dir == 0) {
                    next.phase = InteractionPhase::Idle;
                    return next;
                }
                next.focus.last_dir = dir;
                next.focus.last_jump = (intent.a > 1 || intent.a < -1);
                next.phase = InteractionPhase::Navigate;
                next.activation.kind = ActivationKind::Submit;
                next.activation.target_id = next.capture.owner_id;
                return next;
            }
            next.phase = InteractionPhase::Idle;
            return next;
        }
        if (next.capture.kind != CaptureKind::None) {
            next.phase = InteractionPhase::Idle;
            return next;
        }

        if (intent.type == IntentType::NavPrev || intent.type == IntentType::NavNext || intent.type == IntentType::Adjust) {
            next.phase = InteractionPhase::Navigate;

            if (next.nav.kind == NavKind::Free) {
                next.focus.index = -1;
                next.focus.count = 0;
                const std::int16_t delta = (intent.type == IntentType::Adjust) ? sign_i16(intent.a)
                                                                               : (intent.type == IntentType::NavPrev ? -1 : 1);
                if (delta == 0) {
                    next.phase = InteractionPhase::Idle;
                    return next;
                }
                next.focus.last_dir = (delta > 0) ? 1 : -1;
                next.focus.last_jump = (intent.type == IntentType::Adjust && (intent.a > 1 || intent.a < -1));
                return next;
            }

            if (next.focus.count <= 0) return next;

            if (next.nav.kind == NavKind::List) {
                const std::int16_t delta = (intent.type == IntentType::Adjust) ? sign_i16(intent.a)
                                                                               : (intent.type == IntentType::NavPrev ? -1 : 1);
                if (delta == 0) {
                    next.phase = InteractionPhase::Idle;
                    return next;
                }
                next.focus.last_dir = (delta > 0) ? 1 : -1;
                next.focus.last_jump = (intent.type == IntentType::Adjust && (intent.a > 1 || intent.a < -1));
                const std::int16_t next_index = (std::int16_t)(next.focus.index + delta);
                next.focus.index = apply_wrap(next_index, next.focus.count, next.nav.wrap);
                if (next.focus.index >= 0) {
                    next.focus.target_id = list_id(next.focus.domain_id, (std::uint16_t)(next.focus.index + 1));
                }
                return next;
            }

            if (next.nav.kind == NavKind::Grid) {
                const std::int16_t cols = next.nav.cols;
                if (cols <= 0) return next;

                std::int16_t delta = 0;
                if (intent.type == IntentType::NavPrev) delta = -1;
                else if (intent.type == IntentType::NavNext) delta = 1;
                else if (intent.type == IntentType::Adjust) delta = sign_i16(intent.a);

                if (delta == 0) {
                    next.phase = InteractionPhase::Idle;
                    return next;
                }

                next.focus.last_dir = (delta > 0) ? 1 : -1;
                next.focus.last_jump = (intent.type == IntentType::Adjust && (intent.a > 1 || intent.a < -1));

                std::int16_t next_index = next.focus.index;
                if (next.nav.wrap == NavWrap::Ring) {
                    const std::int16_t delta_linear =
                        (intent.type == IntentType::Adjust) ? (std::int16_t)(delta * cols) : delta;
                    next_index = apply_wrap((std::int16_t)(next.focus.index + delta_linear), next.focus.count, NavWrap::Ring);
                } else {
                    std::int16_t base_index = next.focus.index;
                    if (base_index < 0) base_index = 0;
                    if (base_index >= next.focus.count) base_index = (std::int16_t)(next.focus.count - 1);
                    if (intent.type == IntentType::Adjust) {
                        const std::int16_t row = (std::int16_t)(base_index / cols);
                        const std::int16_t col = (std::int16_t)(base_index % cols);
                        const std::int16_t max_row = (std::int16_t)((next.focus.count - 1) / cols);
                        std::int16_t new_row = (std::int16_t)(row + delta);
                        if (new_row < 0) new_row = 0;
                        if (new_row > max_row) new_row = max_row;
                        next_index = (std::int16_t)(new_row * cols + col);
                        if (next_index >= next.focus.count) {
                            const std::int16_t row_last = (std::int16_t)(new_row * cols + (cols - 1));
                            next_index = (row_last < next.focus.count) ? row_last
                                                                       : (std::int16_t)(next.focus.count - 1);
                        }
                    } else {
                        next_index = (std::int16_t)(base_index + delta);
                        if (next_index < 0) next_index = 0;
                        if (next_index >= next.focus.count) next_index = (std::int16_t)(next.focus.count - 1);
                    }
                }

                next.focus.index = next_index;
                if (next.focus.index >= 0) {
                    next.focus.target_id = list_id(next.focus.domain_id, (std::uint16_t)(next.focus.index + 1));
                }
                return next;
            }
        }

        return next;
    }
} // namespace gui::ui
