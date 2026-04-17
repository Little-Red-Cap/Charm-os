module;

#include <array>
#include <span>
#include <string_view>

export module init.connection;

import init.binding;
import init.meta;
import init.node;
import util.core;

export namespace init {
    enum class connection_mode : util::u8 {
        direct,
        deferred,
    };

    [[nodiscard]] constexpr std::string_view to_text(connection_mode mode) noexcept {
        switch (mode) {
            case connection_mode::direct: return "direct";
            case connection_mode::deferred: return "deferred";
        }
        return "unknown";
    }

    struct connection_binding {
        std::array<CapId, 2> requires_caps{};
        std::array<std::string_view, 2> require_names{};
        Node node{};
        std::string_view source_name{};
        std::string_view sink_name{};
        init::connection_mode mode_value{init::connection_mode::direct};

        constexpr connection_binding() noexcept = default;

        constexpr connection_binding(std::string_view name,
                                     std::string_view source_cap_name,
                                     std::string_view sink_cap_name,
                                     init::connection_mode mode,
                                     Phase phase = Phase::service,
                                     util::u32 runlevel_mask = static_cast<util::u32>(Runlevel::all),
                                     InitFn init = nullptr,
                                     DeinitFn deinit = nullptr,
                                     void* ctx = nullptr) noexcept
            : requires_caps{
                cap_id_or_zero(source_cap_name),
                cap_id_or_zero(sink_cap_name),
            },
              require_names{
                capability_name_view(source_cap_name),
                capability_name_view(sink_cap_name),
            },
              node{
                name,
                phase,
                runlevel_mask,
                {},
                {},
                init,
                deinit,
                ctx,
            },
              source_name{capability_name_view(source_cap_name)},
              sink_name{capability_name_view(sink_cap_name)},
              mode_value{mode} {
            rebind_node();
        }

        constexpr connection_binding(const connection_binding& other) noexcept
            : requires_caps{other.requires_caps},
              require_names{other.require_names},
              node{other.node},
              source_name{other.source_name},
              sink_name{other.sink_name},
              mode_value{other.mode_value} {
            rebind_node();
        }

        constexpr connection_binding(connection_binding&& other) noexcept
            : requires_caps{other.requires_caps},
              require_names{other.require_names},
              node{other.node},
              source_name{other.source_name},
              sink_name{other.sink_name},
              mode_value{other.mode_value} {
            rebind_node();
        }

        constexpr connection_binding& operator=(const connection_binding& other) noexcept {
            if (this == &other) {
                return *this;
            }
            requires_caps = other.requires_caps;
            require_names = other.require_names;
            node = other.node;
            source_name = other.source_name;
            sink_name = other.sink_name;
            mode_value = other.mode_value;
            rebind_node();
            return *this;
        }

        constexpr connection_binding& operator=(connection_binding&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            requires_caps = other.requires_caps;
            require_names = other.require_names;
            node = other.node;
            source_name = other.source_name;
            sink_name = other.sink_name;
            mode_value = other.mode_value;
            rebind_node();
            return *this;
        }

        [[nodiscard]] constexpr std::string_view source_capability() const noexcept {
            return source_name;
        }

        [[nodiscard]] constexpr std::string_view sink_capability() const noexcept {
            return sink_name;
        }

        [[nodiscard]] constexpr std::string_view connection_mode() const noexcept {
            return to_text(mode_value);
        }

        [[nodiscard]] constexpr init::connection_mode mode() const noexcept {
            return mode_value;
        }

        [[nodiscard]] constexpr std::string_view capability_name(CapId id) const noexcept {
            if (id == requires_caps[0]) {
                return require_names[0];
            }
            if (id == requires_caps[1]) {
                return require_names[1];
            }
            return {};
        }

    private:
        constexpr void rebind_node() noexcept {
            node.provides = {};
            node.requires_caps = std::span<const CapId>{requires_caps.data(), requires_caps.size()};
        }
    };

    [[nodiscard]] constexpr connection_binding direct_connection(
        std::string_view name,
        std::string_view source_cap_name,
        std::string_view sink_cap_name,
        Phase phase = Phase::service,
        util::u32 runlevel_mask = static_cast<util::u32>(Runlevel::all),
        InitFn init = nullptr,
        DeinitFn deinit = nullptr,
        void* ctx = nullptr) noexcept {
        return connection_binding{
            name,
            source_cap_name,
            sink_cap_name,
            init::connection_mode::direct,
            phase,
            runlevel_mask,
            init,
            deinit,
            ctx,
        };
    }

    [[nodiscard]] constexpr connection_binding deferred_connection(
        std::string_view name,
        std::string_view source_cap_name,
        std::string_view sink_cap_name,
        Phase phase = Phase::service,
        util::u32 runlevel_mask = static_cast<util::u32>(Runlevel::all),
        InitFn init = nullptr,
        DeinitFn deinit = nullptr,
        void* ctx = nullptr) noexcept {
        return connection_binding{
            name,
            source_cap_name,
            sink_cap_name,
            init::connection_mode::deferred,
            phase,
            runlevel_mask,
            init,
            deinit,
            ctx,
        };
    }

    template <fixed_string Name,
              typename SourceCap,
              typename SinkCap,
              Phase PhaseV = Phase::service,
              util::u32 RunlevelMask = static_cast<util::u32>(Runlevel::all)>
    [[nodiscard]] constexpr connection_binding direct_connection(InitFn init = nullptr,
                                                                 DeinitFn deinit = nullptr,
                                                                 void* ctx = nullptr) noexcept {
        return direct_connection(Name.sv(),
                                 SourceCap::view(),
                                 SinkCap::view(),
                                 PhaseV,
                                 RunlevelMask,
                                 init,
                                 deinit,
                                 ctx);
    }

    template <fixed_string Name,
              typename SourceCap,
              typename SinkCap,
              Phase PhaseV = Phase::service,
              util::u32 RunlevelMask = static_cast<util::u32>(Runlevel::all)>
    [[nodiscard]] constexpr connection_binding deferred_connection(InitFn init = nullptr,
                                                                   DeinitFn deinit = nullptr,
                                                                   void* ctx = nullptr) noexcept {
        return deferred_connection(Name.sv(),
                                   SourceCap::view(),
                                   SinkCap::view(),
                                   PhaseV,
                                   RunlevelMask,
                                   init,
                                   deinit,
                                   ctx);
    }
}
