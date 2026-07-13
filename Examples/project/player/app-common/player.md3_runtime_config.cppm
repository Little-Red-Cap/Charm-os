export module player.md3_runtime_config;

import audio.player;
import charm.gfx.color;
import player.app_config;
import player.cover_resource;
import player.storage;

export namespace player {
    struct PlayerMd3AppInit {
        AppConfig app_config{};
        audio::PlayerBindings audio_bindings{};
        StorageBinding storage_binding{};
    };

    struct PlayerMd3RuntimeHooks {
        using TickVisualFn = void (*)(void* ctx, float t_sec, bool active) noexcept;

        void* tick_visual_ctx{nullptr};
        TickVisualFn tick_visual{nullptr};
    };

    template <typename Page>
    struct PlayerMd3RuntimeConfig {
        AppConfig app_config{};
        audio::PlayerBindings audio_bindings{};
        audio::AudioSourceBinding probe_source_binding{};
        StorageBinding storage_binding{};
        // Compatibility bridge for legacy board/runtime callers.
        StorageConfig storage_config{};
        PlayerCoverResourceProviderBinding cover_resource_provider{};
        PlayerCoverResourceRecordTableView cover_resource_records{};
        Page start_page{};
        int initial_track_index{0};
        bool auto_start{false};
        rgba clear_color{0, 0, 0, 255};

        [[nodiscard]] StorageBinding resolved_storage_binding() noexcept {
            auto binding = storage_binding;
            if (!binding.valid() && storage_config.mount) {
                binding = make_storage_binding(storage_config);
            }
            return binding.valid() ? binding : unavailable_storage_binding();
        }

        [[nodiscard]] PlayerCoverResourceProviderBinding
        resolved_cover_resource_binding() const noexcept {
            if (cover_resource_provider.valid()) {
                return cover_resource_provider;
            }
            return cover_resource_records.valid()
                ? make_cover_resource_record_table_binding(cover_resource_records)
                : PlayerCoverResourceProviderBinding{};
        }

        [[nodiscard]] PlayerMd3AppInit resolved_app_init() noexcept {
            return PlayerMd3AppInit{
                .app_config = app_config,
                .audio_bindings = audio_bindings,
                .storage_binding = resolved_storage_binding(),
            };
        }
    };
}
