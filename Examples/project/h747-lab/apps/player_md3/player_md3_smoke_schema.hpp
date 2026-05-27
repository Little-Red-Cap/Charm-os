#pragma once

#include <cstdint>

namespace h747::apps::player_md3 {

inline constexpr std::uint32_t kPlayerMd3SmokeSchemaVersion = 1U;

// Serial smoke fields are an append-only board evidence schema. Existing token
// names and meanings must stay stable unless the smoke evidence document is
// updated in the same change.
enum class PlayerMd3SmokeField : std::uint8_t {
    RealMd3,
    Mock,
    Smoke,
    Delta,
    Display,
    Sdram1,
    RuntimeStorage,
    Boot,
    Render,
    Frames,
    Present,
    Layer,
    Framebuffer,
    Bytes,
    RenderBuffer,
    Platform,
    Runtime,
    PoolBytes,
    RenderSamples,
    CommandBuffer,
    CommandOverflow,
    TextBuffer,
    TextOverflow,
    ExecFailed,
    ExecCounts,
    FailTextImage,
    Input,
    Touch,
    Events,
    InputRoute,
    InputSmoke,
    Storage,
    StorageDetail,
    StorageBus,
    Audio,
    Fs,
    Font,
    Cover,
    Media,
    Playback,
    PlaybackSmoke,
    Content,
    PresentSource,
    PresentDestination,
    FrontBuffer,
    BackBuffer,
    LtdcFramebuffer,
    LtdcControl,
    LtdcPixelFormat,
};

[[nodiscard]] constexpr const char* smoke_field_token(
    const PlayerMd3SmokeField field) noexcept {
    switch (field) {
    case PlayerMd3SmokeField::RealMd3:
        return "real_md3";
    case PlayerMd3SmokeField::Mock:
        return "mock";
    case PlayerMd3SmokeField::Smoke:
        return "smoke";
    case PlayerMd3SmokeField::Delta:
        return "delta";
    case PlayerMd3SmokeField::Display:
        return "display";
    case PlayerMd3SmokeField::Sdram1:
        return "sdram1";
    case PlayerMd3SmokeField::RuntimeStorage:
        return "rt_store";
    case PlayerMd3SmokeField::Boot:
        return "boot";
    case PlayerMd3SmokeField::Render:
        return "render";
    case PlayerMd3SmokeField::Frames:
        return "frames";
    case PlayerMd3SmokeField::Present:
        return "present";
    case PlayerMd3SmokeField::Layer:
        return "layer";
    case PlayerMd3SmokeField::Framebuffer:
        return "fb";
    case PlayerMd3SmokeField::Bytes:
        return "bytes";
    case PlayerMd3SmokeField::RenderBuffer:
        return "render_buf";
    case PlayerMd3SmokeField::Platform:
        return "platform";
    case PlayerMd3SmokeField::Runtime:
        return "runtime";
    case PlayerMd3SmokeField::PoolBytes:
        return "pool_bytes";
    case PlayerMd3SmokeField::RenderSamples:
        return "r_s";
    case PlayerMd3SmokeField::CommandBuffer:
        return "cmd";
    case PlayerMd3SmokeField::CommandOverflow:
        return "co";
    case PlayerMd3SmokeField::TextBuffer:
        return "text";
    case PlayerMd3SmokeField::TextOverflow:
        return "to";
    case PlayerMd3SmokeField::ExecFailed:
        return "exec_fail";
    case PlayerMd3SmokeField::ExecCounts:
        return "exec";
    case PlayerMd3SmokeField::FailTextImage:
        return "fail_ti";
    case PlayerMd3SmokeField::Input:
        return "input";
    case PlayerMd3SmokeField::Touch:
        return "t";
    case PlayerMd3SmokeField::Events:
        return "e";
    case PlayerMd3SmokeField::InputRoute:
        return "input_route";
    case PlayerMd3SmokeField::InputSmoke:
        return "input_smoke";
    case PlayerMd3SmokeField::Storage:
        return "storage";
    case PlayerMd3SmokeField::StorageDetail:
        return "storage_detail";
    case PlayerMd3SmokeField::StorageBus:
        return "storage_bus";
    case PlayerMd3SmokeField::Audio:
        return "audio";
    case PlayerMd3SmokeField::Fs:
        return "fs";
    case PlayerMd3SmokeField::Font:
        return "font";
    case PlayerMd3SmokeField::Cover:
        return "cover";
    case PlayerMd3SmokeField::Media:
        return "media";
    case PlayerMd3SmokeField::Playback:
        return "playback";
    case PlayerMd3SmokeField::PlaybackSmoke:
        return "playback_smoke";
    case PlayerMd3SmokeField::Content:
        return "content";
    case PlayerMd3SmokeField::PresentSource:
        return "p_src";
    case PlayerMd3SmokeField::PresentDestination:
        return "p_dst";
    case PlayerMd3SmokeField::FrontBuffer:
        return "front";
    case PlayerMd3SmokeField::BackBuffer:
        return "back";
    case PlayerMd3SmokeField::LtdcFramebuffer:
        return "lfb";
    case PlayerMd3SmokeField::LtdcControl:
        return "lcr";
    case PlayerMd3SmokeField::LtdcPixelFormat:
        return "lpf";
    }
    return "unknown";
}

} // namespace h747::apps::player_md3
