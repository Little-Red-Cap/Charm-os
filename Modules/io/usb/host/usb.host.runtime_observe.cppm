module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

export module usb.host.runtime_observe;

import block.device.slot_export;
import block.registry;
import io.channel.slot_export;
import io.registry;
import service_json;
import usb.host.runtime_manager;
import util.core;

namespace usb::host::detail {
    template <util::usize MaxValues>
    bool push_unique(std::array<std::string_view, MaxValues>& values,
                     util::usize& count,
                     std::string_view value) noexcept {
        if (value.empty()) {
            return true;
        }
        for (util::usize i = 0; i < count; ++i) {
            if (values[i] == value) {
                return true;
            }
        }
        if (count >= values.size()) {
            return false;
        }
        values[count++] = value;
        return true;
    }

    inline constexpr bool publish_state_is_published(io::PublishState state) noexcept {
        return state == io::PublishState::published;
    }

    inline constexpr bool publish_state_is_published(block::PublishState state) noexcept {
        return state == block::PublishState::published;
    }

    inline constexpr std::string_view export_state_text(io::ExportState state) noexcept {
        switch (state) {
            case io::ExportState::attached:
                return "attached";
            case io::ExportState::detached:
                return "detached";
            case io::ExportState::missing:
            default:
                return "missing";
        }
    }

    inline constexpr std::string_view export_state_text(block::ExportState state) noexcept {
        switch (state) {
            case block::ExportState::attached:
                return "attached";
            case block::ExportState::detached:
                return "detached";
            case block::ExportState::missing:
            default:
                return "missing";
        }
    }

    inline constexpr bool export_state_is_attached(io::ExportState state) noexcept {
        return state == io::ExportState::attached;
    }

    inline constexpr bool export_state_is_attached(block::ExportState state) noexcept {
        return state == block::ExportState::attached;
    }

    inline constexpr bool export_state_is_detached(io::ExportState state) noexcept {
        return state == io::ExportState::detached;
    }

    inline constexpr bool export_state_is_detached(block::ExportState state) noexcept {
        return state == block::ExportState::detached;
    }

    inline constexpr std::string_view export_action_text(io::ExportAction action) noexcept {
        switch (action) {
            case io::ExportAction::attach:
                return "attach";
            case io::ExportAction::detach:
                return "detach";
            case io::ExportAction::unexport:
                return "unexport";
            case io::ExportAction::ensure_exported:
            default:
                return "ensure_exported";
        }
    }

    inline constexpr std::string_view export_action_text(block::ExportAction action) noexcept {
        switch (action) {
            case block::ExportAction::attach:
                return "attach";
            case block::ExportAction::detach:
                return "detach";
            case block::ExportAction::unexport:
                return "unexport";
            case block::ExportAction::ensure_exported:
            default:
                return "ensure_exported";
        }
    }

    inline bool write_json_escaped(service::JsonWriter& writer, std::string_view value) noexcept {
        for (char ch : value) {
            switch (ch) {
                case '"':
                case '\\':
                    if (!writer.push('\\') || !writer.push(ch)) {
                        return false;
                    }
                    break;
                case '\b':
                    if (!writer.write_str("\\b")) return false;
                    break;
                case '\f':
                    if (!writer.write_str("\\f")) return false;
                    break;
                case '\n':
                    if (!writer.write_str("\\n")) return false;
                    break;
                case '\r':
                    if (!writer.write_str("\\r")) return false;
                    break;
                case '\t':
                    if (!writer.write_str("\\t")) return false;
                    break;
                default:
                    if (!writer.push(ch)) {
                        return false;
                    }
                    break;
            }
        }
        return true;
    }

    inline bool write_json_string(service::JsonWriter& writer, std::string_view value) noexcept {
        return writer.push('"') && write_json_escaped(writer, value) && writer.push('"');
    }

    inline bool write_key(service::JsonWriter& writer,
                          std::string_view key,
                          bool& first) noexcept {
        if (!first && !writer.push(',')) {
            return false;
        }
        first = false;
        return write_json_string(writer, key) && writer.push(':');
    }

    inline bool write_u64_value(service::JsonWriter& writer, util::u64 value) noexcept {
        return writer.write_u64(value);
    }

    inline bool write_string_value(service::JsonWriter& writer, std::string_view value) noexcept {
        return write_json_string(writer, value);
    }

    template <util::usize MaxValues>
    bool write_string_array(service::JsonWriter& writer,
                            const std::array<std::string_view, MaxValues>& values,
                            util::usize count) noexcept {
        if (!writer.push('[')) {
            return false;
        }
        for (util::usize i = 0; i < count; ++i) {
            if (i > 0 && !writer.push(',')) {
                return false;
            }
            if (!write_json_string(writer, values[i])) {
                return false;
            }
        }
        return writer.push(']');
    }

    inline std::size_t finish_json(service::JsonWriter& writer,
                                   char* out,
                                   std::size_t max) noexcept {
        if (!out || max == 0) {
            return 0;
        }
        const auto term = writer.pos < (max - 1u) ? static_cast<std::size_t>(writer.pos) : (max - 1u);
        out[term] = '\0';
        return term;
    }
}

export namespace usb::host {
    struct RuntimeObserveTransition {
        std::string_view capability{};
        std::string_view action{};
        std::string_view before{};
        std::string_view after{};
    };

    template <util::usize MaxCapabilities, util::usize MaxTransitions>
    struct RuntimeObserveSnapshot {
        std::array<std::string_view, MaxCapabilities> published_capabilities{};
        util::usize published_capability_count{0};
        std::array<std::string_view, MaxCapabilities> observed_capabilities{};
        util::usize observed_capability_count{0};
        util::usize publish_missing{0};
        util::usize publish_published{0};
        util::usize export_missing{0};
        util::usize export_detached{0};
        util::usize export_attached{0};
        std::array<RuntimeObserveTransition, MaxTransitions> recent_transitions{};
        util::usize recent_transition_count{0};
    };

    template <util::usize MaxCapabilities, util::usize MaxTransitions>
    class RuntimeObserveCollector {
    public:
        using Snapshot = RuntimeObserveSnapshot<MaxCapabilities, MaxTransitions>;

        void reset() noexcept {
            snapshot_ = {};
        }

        template <typename PublishStateT, typename ExportStateT>
        void add_binding_state(std::string_view capability,
                               const RuntimeBindingState<PublishStateT, ExportStateT>& state) noexcept {
            (void)detail::push_unique(snapshot_.observed_capabilities,
                                      snapshot_.observed_capability_count,
                                      capability);

            if (detail::publish_state_is_published(state.publish_state)) {
                ++snapshot_.publish_published;
                (void)detail::push_unique(snapshot_.published_capabilities,
                                          snapshot_.published_capability_count,
                                          capability);
            } else {
                ++snapshot_.publish_missing;
            }

            if (detail::export_state_is_attached(state.export_state)) {
                ++snapshot_.export_attached;
            } else if (detail::export_state_is_detached(state.export_state)) {
                ++snapshot_.export_detached;
            } else {
                ++snapshot_.export_missing;
            }
        }

        void add_transition(const RuntimeObserveTransition& transition) noexcept {
            if constexpr (MaxTransitions == 0) {
                return;
            }

            if (snapshot_.recent_transition_count < snapshot_.recent_transitions.size()) {
                snapshot_.recent_transitions[snapshot_.recent_transition_count++] = transition;
                return;
            }

            for (util::usize i = 1; i < snapshot_.recent_transitions.size(); ++i) {
                snapshot_.recent_transitions[i - 1] = snapshot_.recent_transitions[i];
            }
            snapshot_.recent_transitions[snapshot_.recent_transitions.size() - 1] = transition;
        }

        void add_transition(std::string_view capability, const io::ExportTransition& transition) noexcept {
            add_transition(RuntimeObserveTransition{
                capability,
                detail::export_action_text(transition.action),
                detail::export_state_text(transition.before),
                detail::export_state_text(transition.after)
            });
        }

        void add_transition(std::string_view capability, const block::ExportTransition& transition) noexcept {
            add_transition(RuntimeObserveTransition{
                capability,
                detail::export_action_text(transition.action),
                detail::export_state_text(transition.before),
                detail::export_state_text(transition.after)
            });
        }

        template <typename TransitionT>
        void add_transition_log(std::string_view capability,
                                std::span<const TransitionT> transitions) noexcept {
            for (const auto& transition : transitions) {
                add_transition(capability, transition);
            }
        }

        [[nodiscard]] const Snapshot& snapshot() const noexcept {
            return snapshot_;
        }

        [[nodiscard]] std::size_t format_json(std::string_view generated_at_utc,
                                              std::string_view generator,
                                              char* out,
                                              std::size_t max) const noexcept {
            if (!out || max == 0) {
                return 0;
            }

            service::JsonWriter writer{std::span<char>(out, max > 0 ? max - 1u : 0u)};
            bool first = true;

            if (!writer.push('{')) {
                return detail::finish_json(writer, out, max);
            }

            if (!detail::write_key(writer, "schema", first) ||
                !detail::write_string_value(writer, "system_compiler.runtime_observe_snapshot/v0") ||
                !detail::write_key(writer, "generated_at_utc", first) ||
                !detail::write_string_value(writer, generated_at_utc) ||
                !detail::write_key(writer, "generator", first) ||
                !detail::write_string_value(writer, generator) ||
                !detail::write_key(writer, "published_capabilities", first) ||
                !detail::write_string_array(writer,
                                            snapshot_.published_capabilities,
                                            snapshot_.published_capability_count) ||
                !detail::write_key(writer, "observed_capabilities", first) ||
                !detail::write_string_array(writer,
                                            snapshot_.observed_capabilities,
                                            snapshot_.observed_capability_count)) {
                return detail::finish_json(writer, out, max);
            }

            if (!detail::write_key(writer, "publish_state_summary", first) || !writer.push('{')) {
                return detail::finish_json(writer, out, max);
            }
            bool publish_first = true;
            if (!detail::write_key(writer, "missing", publish_first) ||
                !detail::write_u64_value(writer, snapshot_.publish_missing) ||
                !detail::write_key(writer, "published", publish_first) ||
                !detail::write_u64_value(writer, snapshot_.publish_published) ||
                !writer.push('}')) {
                return detail::finish_json(writer, out, max);
            }

            if (!detail::write_key(writer, "export_state_summary", first) || !writer.push('{')) {
                return detail::finish_json(writer, out, max);
            }
            bool export_first = true;
            if (!detail::write_key(writer, "missing", export_first) ||
                !detail::write_u64_value(writer, snapshot_.export_missing) ||
                !detail::write_key(writer, "detached", export_first) ||
                !detail::write_u64_value(writer, snapshot_.export_detached) ||
                !detail::write_key(writer, "attached", export_first) ||
                !detail::write_u64_value(writer, snapshot_.export_attached) ||
                !writer.push('}')) {
                return detail::finish_json(writer, out, max);
            }

            if (!detail::write_key(writer, "recent_transitions", first) || !writer.push('[')) {
                return detail::finish_json(writer, out, max);
            }
            for (util::usize i = 0; i < snapshot_.recent_transition_count; ++i) {
                if (i > 0 && !writer.push(',')) {
                    return detail::finish_json(writer, out, max);
                }
                if (!writer.push('{')) {
                    return detail::finish_json(writer, out, max);
                }
                bool transition_first = true;
                const auto& transition = snapshot_.recent_transitions[i];
                if (!detail::write_key(writer, "capability", transition_first) ||
                    !detail::write_string_value(writer, transition.capability) ||
                    !detail::write_key(writer, "action", transition_first) ||
                    !detail::write_string_value(writer, transition.action) ||
                    !detail::write_key(writer, "before", transition_first) ||
                    !detail::write_string_value(writer, transition.before) ||
                    !detail::write_key(writer, "after", transition_first) ||
                    !detail::write_string_value(writer, transition.after) ||
                    !writer.push('}')) {
                    return detail::finish_json(writer, out, max);
                }
            }
            if (!writer.push(']') || !writer.push('}')) {
                return detail::finish_json(writer, out, max);
            }

            return detail::finish_json(writer, out, max);
        }

    private:
        Snapshot snapshot_{};
    };
}
