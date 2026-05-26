module;

export module input.raw_sink;

import input.raw_event;

export namespace input {
    using RawSinkFn = bool (*)(void* ctx, const RawInputEvent& ev) noexcept;

    class RawSinkRef {
    public:
        constexpr RawSinkRef() noexcept = default;

        static constexpr RawSinkRef raw(RawSinkFn fn, void* ctx) noexcept {
            return RawSinkRef{fn, ctx};
        }

        template <typename Sink>
        static constexpr RawSinkRef bind(Sink& sink) noexcept {
            return RawSinkRef{&invoke<Sink>, &sink};
        }

        [[nodiscard]] constexpr RawSinkFn fn() const noexcept {
            return fn_;
        }

        [[nodiscard]] constexpr void* ctx() const noexcept {
            return ctx_;
        }

    private:
        constexpr RawSinkRef(RawSinkFn fn, void* ctx) noexcept
            : fn_(fn),
              ctx_(ctx) {}

        template <typename Sink>
        static bool invoke(void* ctx, const RawInputEvent& ev) noexcept {
            auto* sink = static_cast<Sink*>(ctx);
            if (!sink) {
                return true;
            }
            if constexpr (requires(Sink& value, const RawInputEvent& event) {
                              value.on_raw(event);
                          }) {
                return sink->on_raw(ev);
            } else {
                return (*sink)(ev);
            }
        }

        RawSinkFn fn_{nullptr};
        void* ctx_{nullptr};
    };
}
