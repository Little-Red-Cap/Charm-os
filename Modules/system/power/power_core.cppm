export module power.core;

import util.core;
import power.types;
import power.policy;
import power.trace;

export namespace power {
    class Manager {
    public:
        void set_policy(Policy* policy) noexcept { policy_ = policy; }

        void request(State state) noexcept {
            requested_ = state;
            trace::record(trace::EventId::request_state, static_cast<util::u32>(state));
        }

        void add_wake_source(const WakeRequest& req) noexcept {
            if (req.source <= WakeSource::other) {
                wake_sources_mask_ |= (1u << static_cast<util::u32>(req.source));
                trace::record(trace::EventId::wake_source_add, static_cast<util::u32>(req.source));
            }
        }

        void remove_wake_source(const WakeRequest& req) noexcept {
            if (req.source <= WakeSource::other) {
                wake_sources_mask_ &= ~(1u << static_cast<util::u32>(req.source));
                trace::record(trace::EventId::wake_source_remove, static_cast<util::u32>(req.source));
            }
        }

        void add_clock_domain(const ClockRequest& req) noexcept {
            if (req.domain <= ClockDomain::peripheral) {
                clock_domains_mask_ |= (1u << static_cast<util::u32>(req.domain));
                trace::record(trace::EventId::clock_domain_add, static_cast<util::u32>(req.domain));
            }
        }

        void remove_clock_domain(const ClockRequest& req) noexcept {
            if (req.domain <= ClockDomain::peripheral) {
                clock_domains_mask_ &= ~(1u << static_cast<util::u32>(req.domain));
                trace::record(trace::EventId::clock_domain_remove, static_cast<util::u32>(req.domain));
            }
        }

        State decide_target() const noexcept {
            if (!policy_) return requested_;
            PolicySnapshot snapshot{
                .target = requested_,
                .active_wake_sources = wake_sources_mask_,
                .active_clock_domains = clock_domains_mask_
            };
            const auto constraints = policy_->constraints();
            auto chosen = policy_->choose_target(snapshot);
            if (chosen < constraints.min_state) {
                chosen = constraints.min_state;
            }
            if (!constraints.allow_deep && (chosen == State::deep_sleep || chosen == State::stop || chosen == State::standby)) {
                chosen = State::sleep;
            }
            return chosen;
        }

        void enter_state(State state) noexcept {
            current_ = state;
            trace::record(trace::EventId::enter_state, static_cast<util::u32>(state));
        }

        void exit_state(State state) noexcept {
            trace::record(trace::EventId::exit_state, static_cast<util::u32>(state));
            current_ = State::active;
        }

        [[nodiscard]] State current() const noexcept { return current_; }

    private:
        Policy* policy_{nullptr};
        State requested_{State::active};
        State current_{State::active};
        util::u32 wake_sources_mask_{0};
        util::u32 clock_domains_mask_{0};
    };
}
