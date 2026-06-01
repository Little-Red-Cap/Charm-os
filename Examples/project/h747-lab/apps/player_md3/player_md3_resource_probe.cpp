#include <cstdint>
#include <string_view>

#include "player_md3_resource_probe.hpp"
#include "player_md3_runtime.hpp"

import fs_core;
import fs_errno;
import fs_vfs;
import player.cover;
import player.cover_resource;
import player.fixed_string;
import player.fs_utils;
import player.storage;
import player.track_probe;
import player.ui;

namespace {

constexpr const char* kPrimaryFontPaths[] = {
    "/font/NotoSansSC-Regular.ttf",
    "/fonts/NotoSansSC-Regular.ttf",
};
constexpr const char* kFallbackFontPaths[] = {
    "/font/NotoSans-Regular.ttf",
    "/fonts/NotoSans-Regular.ttf",
};

std::int32_t err_code(fs::Errc err) noexcept {
    return static_cast<std::int32_t>(err);
}

bool open_probe(std::string_view path, std::int32_t& out_err) noexcept {
    fs::File f{};
    const auto st = fs::vfs_open(path, f);
    out_err = err_code(st.err);
    if (!st) {
        return false;
    }
    const auto close_st = fs::vfs_close(f);
    if (!close_st) {
        out_err = err_code(close_st.err);
        return false;
    }
    out_err = 0;
    return true;
}

template <std::size_t N>
bool open_first_available(const char* const (&paths)[N], std::int32_t& out_err) noexcept {
    std::int32_t last_err = err_code(fs::Errc::noent);
    for (const char* path : paths) {
        std::int32_t err = 0;
        if (open_probe(path, err)) {
            out_err = 0;
            return true;
        }
        last_err = err;
    }
    out_err = last_err;
    return false;
}

void probe_storage_view(h747::apps::player_md3::PlayerMd3State& st) noexcept {
    auto* shell = h747::apps::player_md3::shell_ref();
    auto* app = shell ? shell->app() : nullptr;
    if (!app) {
        st.fs_mount_ok = 0;
        st.fs_has_tracks = 0;
        st.fs_track_count = 0;
        st.fs_mount_err = err_code(fs::Errc::bad_state);
        return;
    }

    const auto& storage = app->storage_state();
    st.fs_mount_ok = storage.fs_ready ? 1U : 0U;
    st.fs_has_tracks = storage.has_tracks ? 1U : 0U;
    st.fs_track_count = static_cast<std::uint32_t>(storage.tracks.size());
    st.fs_mount_err = storage.fs_ready ? 0 : err_code(fs::Errc::noent);
}

const char* first_track_path() noexcept {
    auto* shell = h747::apps::player_md3::shell_ref();
    auto* app = shell ? shell->app() : nullptr;
    if (!app) {
        return nullptr;
    }
    const auto& storage = app->storage_state();
    return storage.tracks.size() == 0 ? nullptr : storage.tracks[0].c_str();
}

void probe_fonts(h747::apps::player_md3::PlayerMd3State& st) noexcept {
    std::int32_t primary_err = 0;
    std::int32_t fallback_err = 0;
    st.font_primary_open = open_first_available(kPrimaryFontPaths, primary_err) ? 1U : 0U;
    st.font_fallback_open = open_first_available(kFallbackFontPaths, fallback_err) ? 1U : 0U;
    st.font_cache_ready = ::player::ui::font_package_bound() ? 1U : 0U;
    if (!st.font_primary_open) {
        st.font_err = primary_err;
    } else if (!st.font_fallback_open) {
        st.font_err = fallback_err;
    } else if (!st.font_cache_ready) {
        st.font_err = err_code(fs::Errc::notsup);
    } else {
        st.font_err = 0;
    }
}

void probe_media(h747::apps::player_md3::PlayerMd3State& st) noexcept {
    const auto* first = first_track_path();
    if (!first) {
        st.media_first_open = 0;
        st.media_duration_ok = 0;
        st.media_track_ready = 0;
        st.media_err = st.fs_mount_ok ? err_code(fs::Errc::noent) : st.fs_mount_err;
        return;
    }

    std::int32_t open_err = 0;
    const std::string_view first_view{first};
    st.media_first_open = open_probe(first_view, open_err) ? 1U : 0U;
    st.media_err = open_err;

    ::player::FixedString<128> ready_status{};
    st.media_track_ready = ::player::check_track_ready(first_view, ready_status) ? 1U : 0U;
    if (!st.media_track_ready && st.media_err == 0) {
        st.media_err = err_code(fs::Errc::io);
    }

    int seconds = 0;
    st.media_duration_ok = ::player::probe_duration_seconds(first, seconds) ? 1U : 0U;
    if (!st.media_duration_ok && st.media_err == 0) {
        st.media_err = err_code(fs::Errc::decode_error);
    }
}

void probe_cover(h747::apps::player_md3::PlayerMd3State& st) noexcept {
    st.cover_folder_found = 0;
    st.cover_decode_ok = 0;
    st.cover_width = 0;
    st.cover_height = 0;

    const auto* first = first_track_path();
    if (!first) {
        st.cover_err = st.fs_mount_ok ? err_code(fs::Errc::noent) : st.fs_mount_err;
        return;
    }

    const std::string_view first_view{first};
    ::player::FixedString<260> cover_path{};
    const bool found = ::player::fs_utils::find_cover_for_track(first_view, cover_path);
    st.cover_folder_found = found ? 1U : 0U;
    const std::string_view decode_path = found ? cover_path.view() : first_view;

    ::player::CoverResourceRequest request{};
    request.path = decode_path;
    request.kind = found ? ::player::CoverResourceKind::FolderFile
                         : ::player::CoverResourceKind::EmbeddedTrack;
    request.fallback_variant = ::player::DefaultCoverVariant::HomeHeroPill;
    ::player::CoverResourceView cover{};
    const bool provider_available = ::player::cover_resource_provider_binding().valid();
    const bool resolved = ::player::resolve_cover_resource(request, cover);
    st.cover_decode_ok = resolved ? 1U : 0U;
    if (resolved) {
        st.cover_width = static_cast<std::uint32_t>(cover.width > 0 ? cover.width : 0);
        st.cover_height = static_cast<std::uint32_t>(cover.height > 0 ? cover.height : 0);
        st.cover_err = 0;
    } else if (found && !provider_available) {
        st.cover_err = err_code(fs::Errc::notsup);
    } else {
        st.cover_err = found ? err_code(fs::Errc::decode_error) : err_code(fs::Errc::noent);
    }
}

} // namespace

namespace h747::apps::player_md3 {

void run_resource_probe_now() noexcept {
    auto& st = state();
    refresh_resource_probe_state();
    probe_fonts(st);
    probe_media(st);
    probe_cover(st);
    st.resource_probe_done = 1U;
}

void run_resource_probe_once() noexcept {
    if (state().resource_probe_done != 0U) {
        return;
    }
    run_resource_probe_now();
}

void refresh_resource_probe_state() noexcept {
    auto& st = state();
    probe_storage_view(st);
    st.font_cache_ready = ::player::ui::font_package_bound() ? 1U : 0U;
}

} // namespace h747::apps::player_md3
