module;

#include <array>
#include <cstddef>
#include <optional>

export module kernel.dynamic_registry;

import kernel.eda;
import kernel.evt;
import util.core;

export namespace kernel {
    template <std::size_t Capacity>
    class DynamicTaskRegistry {
    public:
        static constexpr std::size_t count = Capacity;

        [[nodiscard]] std::size_t active_count() const noexcept {
            std::size_t total = 0;
            for (std::size_t i = 0; i < Capacity; ++i) {
                if (slots_[i].active) {
                    ++total;
                }
            }
            return total;
        }

        [[nodiscard]] std::optional<TaskId> register_task(void* instance, Priority prio,
            void (*on_event)(void*, Event),
            void (*on_start)(void*),
            void (*on_stop)(void*),
            bool (*should_accept)(void*, Event) = nullptr,
            EventMask mask = EventMask{0xFFFF'FFFFu}) noexcept {
            if (free_count_ == 0 || instance == nullptr || on_event == nullptr) {
                return std::nullopt;
            }
            const auto index = free_[--free_count_];
            auto& slot = slots_[index];
            slot.instance = instance;
            slot.on_event = on_event;
            slot.on_start = on_start;
            slot.on_stop = on_stop;
            slot.should_accept = should_accept;
            slot.prio = prio;
            slot.mask = mask;
            slot.active = true;
            return TaskId{index};
        }

        template <typename T>
        [[nodiscard]] std::optional<TaskId> register_task(T& task, Priority prio) noexcept {
            return register_task(
                static_cast<void*>(&task),
                prio,
                [](void* ptr, Event evt) { static_cast<T*>(ptr)->on_event(evt); },
                [](void* ptr) {
                    if constexpr (requires(T& t) { t.on_start(); }) {
                        static_cast<T*>(ptr)->on_start();
                    }
                },
                [](void* ptr) {
                    if constexpr (requires(T& t) { t.on_stop(); }) {
                        static_cast<T*>(ptr)->on_stop();
                    }
                },
                [] (void* ptr, Event evt) {
                    if constexpr (requires(T& t, Event e) { t.should_accept(e); }) {
                        return static_cast<T*>(ptr)->should_accept(evt);
                    } else {
                        return true;
                    }
                },
                []() {
                    if constexpr (requires { T::mask; }) {
                        return T::mask;
                    } else {
                        return EventMask{0xFFFF'FFFFu};
                    }
                }());
        }

        void unregister(TaskId id) noexcept {
            if (id.value >= Capacity) {
                return;
            }
            auto& slot = slots_[id.value];
            if (!slot.active) {
                return;
            }
            slot = Slot{};
            free_[free_count_++] = id.value;
        }

        template <typename T>
        [[nodiscard]] bool replace(TaskId id, T& task, Priority prio) noexcept {
            if (id.value >= Capacity) {
                return false;
            }
            auto& slot = slots_[id.value];
            if (!slot.active) {
                return false;
            }
            slot.instance = static_cast<void*>(&task);
            slot.on_event = [](void* ptr, Event evt) { static_cast<T*>(ptr)->on_event(evt); };
            slot.on_start = [](void* ptr) {
                if constexpr (requires(T& t) { t.on_start(); }) {
                    static_cast<T*>(ptr)->on_start();
                }
            };
            slot.on_stop = [](void* ptr) {
                if constexpr (requires(T& t) { t.on_stop(); }) {
                    static_cast<T*>(ptr)->on_stop();
                }
            };
            slot.should_accept = [](void* ptr, Event evt) {
                if constexpr (requires(T& t, Event e) { t.should_accept(e); }) {
                    return static_cast<T*>(ptr)->should_accept(evt);
                } else {
                    return true;
                }
            };
            slot.prio = prio;
            slot.mask = []() {
                if constexpr (requires { T::mask; }) {
                    return T::mask;
                } else {
                    return EventMask{0xFFFF'FFFFu};
                }
            }();
            return true;
        }

        void init_all() {
            for (std::size_t i = 0; i < Capacity; ++i) {
                if (slots_[i].active && slots_[i].on_start != nullptr) {
                    slots_[i].on_start(slots_[i].instance);
                }
            }
        }

        void start(TaskId id) {
            if (id.value >= Capacity) {
                util::halt();
            }
            auto& slot = slots_[id.value];
            if (slot.active && slot.on_start != nullptr) {
                slot.on_start(slot.instance);
            }
        }

        void stop(TaskId id) {
            if (id.value >= Capacity) {
                util::halt();
            }
            auto& slot = slots_[id.value];
            if (slot.active && slot.on_stop != nullptr) {
                slot.on_stop(slot.instance);
            }
        }

        void dispatch(TaskId id, Event evt) {
            if (id.value >= Capacity) {
                util::halt();
            }
            auto& slot = slots_[id.value];
            if (!slot.active || slot.on_event == nullptr) {
                util::halt();
            }
            if ((slot.mask & event_mask(evt.id)) == 0 && evt.id != EventId::terminate) {
                return;
            }
            if (slot.should_accept != nullptr && evt.id != EventId::terminate) {
                if (!slot.should_accept(slot.instance, evt)) {
                    return;
                }
            }
            slot.on_event(slot.instance, evt);
        }

        template <typename Config>
        static consteval void priority_table() = delete;

        template <typename Config>
        void fill_priorities(std::array<std::size_t, Capacity>& out) const noexcept {
            (void)static_cast<const Config*>(nullptr);
            for (std::size_t i = 0; i < Capacity; ++i) {
                out[i] = slots_[i].active ? slots_[i].prio.value : 0;
            }
        }

        template <typename Config>
        [[nodiscard]] bool is_active(TaskId id) const noexcept {
            (void)static_cast<const Config*>(nullptr);
            return id.value < Capacity && slots_[id.value].active;
        }

    private:
        struct Slot {
            void* instance{nullptr};
            void (*on_event)(void*, Event){nullptr};
            void (*on_start)(void*){nullptr};
            void (*on_stop)(void*){nullptr};
            bool (*should_accept)(void*, Event){nullptr};
            Priority prio{};
            EventMask mask{0xFFFF'FFFFu};
            bool active{false};
        };

        std::array<Slot, Capacity> slots_{};
        std::array<std::size_t, Capacity> free_{};
        std::size_t free_count_{Capacity};

    public:
        DynamicTaskRegistry() noexcept {
            for (std::size_t i = 0; i < Capacity; ++i) {
                free_[i] = Capacity - 1 - i;
            }
        }
    };
}
