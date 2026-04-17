#pragma once

#include <cstdint>

#include "armv7a_runtime_trap_live_adapter_contract.hpp"

enum class Armv7aRuntimeTrapFrameAdapterPath : std::uint8_t {
    none = 0,
    live_frame_adapter,
};

constexpr const char* armv7a_runtime_trap_frame_adapter_path_name(
    Armv7aRuntimeTrapFrameAdapterPath path) noexcept
{
    switch (path) {
    case Armv7aRuntimeTrapFrameAdapterPath::live_frame_adapter:
        return "live-frame-adapter";
    case Armv7aRuntimeTrapFrameAdapterPath::none:
    default:
        return "none";
    }
}

enum class Armv7aRuntimeTrapIngressDisposition : std::uint8_t {
    handled = 0,
    rejected,
    unsupported,
};

enum class Armv7aRuntimeTrapIngressError : std::uint8_t {
    none = 0,
    decode_failed,
    writeback_failed,
    unsupported_service,
    unbound_adapter,
};

struct Armv7aRuntimeTrapIngressResult {
    Armv7aRuntimeTrapIngressDisposition disposition =
        Armv7aRuntimeTrapIngressDisposition::rejected;
    Armv7aRuntimeTrapIngressError error =
        Armv7aRuntimeTrapIngressError::unsupported_service;
    std::uint64_t value = 0u;

    [[nodiscard]] constexpr bool ok() const noexcept
    {
        return disposition == Armv7aRuntimeTrapIngressDisposition::handled &&
               error == Armv7aRuntimeTrapIngressError::none;
    }
};

struct Armv7aRuntimeTrapFrameAdapterContext {
    Armv7aRuntimeTrapMappingPolicy policy{};
    Armv7aRuntimeTrapIngressContext ingress{};
};

struct Armv7aRuntimeTrapFrameAdapter {
    void* ctx = nullptr;
    bool (*capture)(void* ctx,
                    const Armv7aRuntimeTrapLiveFrame& frame,
                    Armv7aRuntimeTrapSeamFrameView& out) noexcept = nullptr;
    bool (*apply_result)(void* ctx,
                         Armv7aRuntimeTrapLiveFrame& frame,
                         const Armv7aRuntimeTrapIngressResult& result) noexcept =
        nullptr;
};

constexpr bool armv7a_runtime_trap_frame_adapter_ready(
    const Armv7aRuntimeTrapFrameAdapter& adapter) noexcept
{
    return adapter.capture != nullptr && adapter.apply_result != nullptr;
}

inline bool armv7a_runtime_trap_frame_adapter_capture(
    void* ctx,
    const Armv7aRuntimeTrapLiveFrame& frame,
    Armv7aRuntimeTrapSeamFrameView& out) noexcept
{
    if (ctx == nullptr) {
        return false;
    }

    const auto& config =
        *static_cast<const Armv7aRuntimeTrapFrameAdapterContext*>(ctx);
    return armv7a_capture_runtime_trap_live_frame_view(
        frame, config.policy, config.ingress, out);
}

inline bool armv7a_runtime_trap_frame_adapter_apply_result(
    void*,
    Armv7aRuntimeTrapLiveFrame& frame,
    const Armv7aRuntimeTrapIngressResult& result) noexcept
{
    return armv7a_apply_runtime_trap_live_result(
        frame, armv7a_make_runtime_trap_seam_result(result.value));
}

inline Armv7aRuntimeTrapFrameAdapter armv7a_make_runtime_trap_frame_adapter(
    Armv7aRuntimeTrapFrameAdapterContext& context) noexcept
{
    return Armv7aRuntimeTrapFrameAdapter{
        .ctx = &context,
        .capture = armv7a_runtime_trap_frame_adapter_capture,
        .apply_result = armv7a_runtime_trap_frame_adapter_apply_result,
    };
}
