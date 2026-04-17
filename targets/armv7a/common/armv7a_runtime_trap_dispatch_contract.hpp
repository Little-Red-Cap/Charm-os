#pragma once

#include "armv7a_runtime_trap_frame_adapter_contract.hpp"

struct Armv7aRuntimeTrapDispatchPort {
    void* ctx = nullptr;
    Armv7aRuntimeTrapIngressResult (*dispatch_frame)(
        void* ctx,
        const Armv7aRuntimeTrapSeamFrameView& frame) noexcept = nullptr;
};

constexpr bool armv7a_runtime_trap_dispatch_port_ready(
    const Armv7aRuntimeTrapDispatchPort& port) noexcept
{
    return port.dispatch_frame != nullptr;
}

inline Armv7aRuntimeTrapIngressResult armv7a_runtime_trap_dispatch_port_dispatch(
    const Armv7aRuntimeTrapDispatchPort& port,
    const Armv7aRuntimeTrapSeamFrameView& frame) noexcept
{
    if (!armv7a_runtime_trap_dispatch_port_ready(port)) {
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
            .error = Armv7aRuntimeTrapIngressError::unbound_adapter,
            .value = 0u,
        };
    }

    return port.dispatch_frame(port.ctx, frame);
}

inline Armv7aRuntimeTrapIngressResult armv7a_runtime_trap_dispatch_live_frame(
    Armv7aRuntimeTrapLiveFrame& live,
    const Armv7aRuntimeTrapFrameAdapter& adapter,
    const Armv7aRuntimeTrapDispatchPort& dispatch,
    Armv7aRuntimeTrapSeamFrameView* frame_view = nullptr) noexcept
{
    if (!armv7a_runtime_trap_frame_adapter_ready(adapter)) {
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
            .error = Armv7aRuntimeTrapIngressError::unbound_adapter,
            .value = 0u,
        };
    }

    Armv7aRuntimeTrapSeamFrameView captured{};
    if (!adapter.capture(adapter.ctx, live, captured)) {
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
            .error = Armv7aRuntimeTrapIngressError::decode_failed,
            .value = 0u,
        };
    }

    if (frame_view != nullptr) {
        *frame_view = captured;
    }

    const auto result =
        armv7a_runtime_trap_dispatch_port_dispatch(dispatch, captured);
    if (!adapter.apply_result(adapter.ctx, live, result)) {
        return Armv7aRuntimeTrapIngressResult{
            .disposition = Armv7aRuntimeTrapIngressDisposition::rejected,
            .error = Armv7aRuntimeTrapIngressError::writeback_failed,
            .value = result.value,
        };
    }

    return result;
}
