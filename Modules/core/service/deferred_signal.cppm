module;

export module service.deferred_signal;

export namespace service {
    template <class Event, class Poster>
    class deferred_signal {
    public:
        // deferred_signal is a post-only semantic wrapper.
        // It does not perform direct callback dispatch.
        explicit constexpr deferred_signal(Poster& poster) noexcept
            : poster_(&poster) {}

        [[nodiscard]] constexpr auto post(const Event& event) noexcept(noexcept(poster_->post(event))) {
            return poster_->post(event);
        }

        [[nodiscard]] constexpr Poster& poster() noexcept {
            return *poster_;
        }

        [[nodiscard]] constexpr const Poster& poster() const noexcept {
            return *poster_;
        }

    private:
        Poster* poster_{nullptr};
    };
}
