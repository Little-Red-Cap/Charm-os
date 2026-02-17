module;

// Optional/experimental module: compile-time dependency checks.
export module kernel.deps;

export namespace kernel {
    template <typename ServiceCfg, typename ComponentCfg>
    consteval void validate_deps() {
        if constexpr (ComponentCfg::use_demo_component) {
            static_assert(ServiceCfg::use_ring_queue, "demo component requires ring_queue service");
        }
        if constexpr (ComponentCfg::use_demo_component) {
            static_assert(ServiceCfg::use_static_pool, "demo component requires static_pool service");
        }
        if constexpr (ComponentCfg::use_pool_component) {
            static_assert(ServiceCfg::use_slot_pool, "pool component requires slot_pool service");
        }
        if constexpr (!ServiceCfg::use_ring_queue) {
            static_assert(!ComponentCfg::use_demo_component, "demo component disabled when ring_queue is off");
        }
    }
}
