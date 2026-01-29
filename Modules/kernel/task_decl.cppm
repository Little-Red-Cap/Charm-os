module;

#include <cstddef>

export module kernel.task_decl;

import kernel.eda;
import kernel.evt;
import kernel.thread;

export namespace kernel {
    template <typename Handler, Priority Prio>
    struct EdaTaskDecl {
        static constexpr Priority priority{Prio};
        Handler handler{};

        void on_start() {
            if constexpr (requires(Handler& h) { h.on_start(); }) {
                handler.on_start();
            }
        }

        void on_stop() {
            if constexpr (requires(Handler& h) { h.on_stop(); }) {
                handler.on_stop();
            }
        }

        void on_event(Event evt) {
            handler(evt);
        }
    };

    template <typename Handler, Priority Prio>
    struct TedaTaskDecl {
        static constexpr Priority priority{Prio};
        Handler handler{};

        void on_start() {
            if constexpr (requires(Handler& h) { h.on_start(); }) {
                handler.on_start();
            }
        }

        void on_stop() {
            if constexpr (requires(Handler& h) { h.on_stop(); }) {
                handler.on_stop();
            }
        }

        void on_event(Event evt) {
            handler(evt);
        }
    };

    template <typename Context, void (*Step)(Context&, ThreadControl&, Event), Priority Prio>
    using ThreadTaskDecl = ThreadTask<Context, Step, Prio>;
}
