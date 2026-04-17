module;

#include "targets/armv7a/common/armv7a_runtime_trap_live_adapter_contract.hpp"

export module target.armv7a.kernel_task_syscall_call_frame_adapter;

export import kernel.task_syscall_frame;

export template <typename Tick>
[[nodiscard]] bool armv7a_make_task_syscall_call_frame(
    void* self,
    kernel::TaskSyscallRequest request,
    Armv7aRuntimeTrapLiveFrame& out) noexcept
{
    auto* adapter =
        static_cast<kernel::RuntimeTrapCallFrameAdapter<
            Armv7aRuntimeTrapLiveFrame,
            Tick>*>(self);
    if (adapter == nullptr ||
        !kernel::runtime_trap_call_frame_adapter_ready(*adapter)) {
        return false;
    }

    switch (request.syscall) {
    case kernel::TaskSyscallId::yield:
        return adapter->make_yield_frame(
            adapter->ctx, kernel::TrapYieldCurrentView{}, out);
    case kernel::TaskSyscallId::sleep_until:
        return adapter->make_sleep_frame(
            adapter->ctx,
            kernel::TrapSleepUntilView<Tick>{
                .due = static_cast<Tick>(request.arg0),
            },
            out);
    case kernel::TaskSyscallId::debug_write:
        return adapter->make_debug_write_frame != nullptr &&
               adapter->make_debug_write_frame(
                   adapter->ctx,
                   kernel::TrapDebugWriteView{
                       .value = request.arg0,
                   },
                   out);
    case kernel::TaskSyscallId::capability_call:
        return adapter->make_capability_call_frame != nullptr &&
               adapter->make_capability_call_frame(
                   adapter->ctx,
                   kernel::TrapCapabilityCallView{
                       .capability_id = request.arg0,
                       .operation = request.arg1,
                       .payload = request.arg2,
                   },
                   out);
    case kernel::TaskSyscallId::invalid:
    default:
        return false;
    }
}

export template <typename Tick>
[[nodiscard]] bool armv7a_task_syscall_call_frame_result_ready(
    void* self,
    const Armv7aRuntimeTrapLiveFrame& frame,
    const kernel::TrapResult& result) noexcept
{
    auto* adapter =
        static_cast<kernel::RuntimeTrapCallFrameAdapter<
            Armv7aRuntimeTrapLiveFrame,
            Tick>*>(self);
    if (adapter == nullptr || adapter->result_ready == nullptr) {
        return false;
    }

    return adapter->result_ready(adapter->ctx, frame, result);
}

export template <typename Tick>
[[nodiscard]] auto armv7a_make_task_syscall_call_frame_adapter(
    kernel::RuntimeTrapCallFrameAdapter<Armv7aRuntimeTrapLiveFrame, Tick>&
        adapter) noexcept
    -> kernel::TaskSyscallCallFrameAdapter<Armv7aRuntimeTrapLiveFrame>
{
    return kernel::TaskSyscallCallFrameAdapter<Armv7aRuntimeTrapLiveFrame>{
        .ctx = &adapter,
        .make_frame = &armv7a_make_task_syscall_call_frame<Tick>,
        .result_ready = adapter.result_ready == nullptr
            ? nullptr
            : &armv7a_task_syscall_call_frame_result_ready<Tick>,
    };
}
