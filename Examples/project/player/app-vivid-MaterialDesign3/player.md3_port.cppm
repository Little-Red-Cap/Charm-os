module;

#include <cstddef>

export module player.md3_port;

export import player.md3_runtime_config;
export import player.md3_types;

import player.port;

export namespace player {
    class PlayerMd3PortApplication {
    public:
        static constexpr std::size_t storage_capacity_bytes = 11u * 512u * 1024u;

        explicit PlayerMd3PortApplication(PlayerMd3RuntimeConfig<PlayerPage> config);
        ~PlayerMd3PortApplication() noexcept;

        PlayerMd3PortApplication(const PlayerMd3PortApplication&) = delete;
        PlayerMd3PortApplication& operator=(const PlayerMd3PortApplication&) = delete;
        PlayerMd3PortApplication(PlayerMd3PortApplication&&) = delete;
        PlayerMd3PortApplication& operator=(PlayerMd3PortApplication&&) = delete;

        [[nodiscard]] PlayerRuntimeEndpoint endpoint() noexcept;
        [[nodiscard]] bool ready() const noexcept;
        [[nodiscard]] bool has_track() const noexcept;
        [[nodiscard]] bool root_bound() const noexcept;
        [[nodiscard]] bool set_page(PlayerPage page) noexcept;

    private:
        struct State;

        [[nodiscard]] State& state() noexcept;
        [[nodiscard]] const State& state() const noexcept;

        alignas(std::max_align_t) std::byte storage_[storage_capacity_bytes]{};
    };
}
