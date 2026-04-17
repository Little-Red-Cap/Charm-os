module;

#include <cstddef>

#include "targets/armv7a/common/armv7a_runtime_trap_frame_adapter_contract.hpp"

export module target.armv7a.kernel_runtime_trap_frame_adapter;

export import kernel.task_syscall_frame;

export struct Armv7aKernelRuntimeTrapFrameAdapterContext {
    Armv7aRuntimeTrapFrameAdapter lower{};
};

export [[nodiscard]] constexpr bool
armv7a_runtime_trap_origin_to_kernel_origin(Armv7aRuntimeTrapOrigin origin,
                                            kernel::TrapOrigin& out) noexcept
{
    switch (origin) {
    case Armv7aRuntimeTrapOrigin::kernel_thread:
        out = kernel::TrapOrigin::kernel_thread;
        return true;
    case Armv7aRuntimeTrapOrigin::user_task:
        out = kernel::TrapOrigin::user_task;
        return true;
    case Armv7aRuntimeTrapOrigin::supervisor:
        out = kernel::TrapOrigin::supervisor;
        return true;
    case Armv7aRuntimeTrapOrigin::isr:
        out = kernel::TrapOrigin::isr;
        return true;
    case Armv7aRuntimeTrapOrigin::unknown:
    default:
        return false;
    }
}

export [[nodiscard]] constexpr Armv7aRuntimeTrapIngressDisposition
armv7a_runtime_trap_ingress_disposition_from_kernel(
    kernel::TrapDisposition disposition) noexcept
{
    switch (disposition) {
    case kernel::TrapDisposition::handled:
        return Armv7aRuntimeTrapIngressDisposition::handled;
    case kernel::TrapDisposition::unsupported:
        return Armv7aRuntimeTrapIngressDisposition::unsupported;
    case kernel::TrapDisposition::rejected:
    default:
        return Armv7aRuntimeTrapIngressDisposition::rejected;
    }
}

export [[nodiscard]] constexpr Armv7aRuntimeTrapIngressError
armv7a_runtime_trap_ingress_error_from_kernel(
    kernel::TrapError error) noexcept
{
    switch (error) {
    case kernel::TrapError::none:
        return Armv7aRuntimeTrapIngressError::none;
    case kernel::TrapError::decode_failed:
        return Armv7aRuntimeTrapIngressError::decode_failed;
    case kernel::TrapError::writeback_failed:
        return Armv7aRuntimeTrapIngressError::writeback_failed;
    case kernel::TrapError::unsupported_service:
        return Armv7aRuntimeTrapIngressError::unsupported_service;
    case kernel::TrapError::unbound_adapter:
    case kernel::TrapError::unbound_bridge:
        return Armv7aRuntimeTrapIngressError::unbound_adapter;
    case kernel::TrapError::no_current_task:
    case kernel::TrapError::invalid_origin:
    case kernel::TrapError::invalid_argument:
    default:
        return Armv7aRuntimeTrapIngressError::unsupported_service;
    }
}

export [[nodiscard]] constexpr bool
armv7a_kernel_runtime_trap_frame_adapter_ready(
    const Armv7aKernelRuntimeTrapFrameAdapterContext& context) noexcept
{
    return armv7a_runtime_trap_frame_adapter_ready(context.lower);
}

export [[nodiscard]] bool armv7a_capture_kernel_runtime_trap_frame(
    void* ctx,
    const Armv7aRuntimeTrapLiveFrame& frame,
    kernel::TrapFrameView& out) noexcept
{
    auto* context = static_cast<Armv7aKernelRuntimeTrapFrameAdapterContext*>(ctx);
    if (context == nullptr || !armv7a_kernel_runtime_trap_frame_adapter_ready(
                                  *context)) {
        return false;
    }

    Armv7aRuntimeTrapSeamFrameView seam{};
    if (!context->lower.capture(context->lower.ctx, frame, seam)) {
        return false;
    }

    kernel::TrapOrigin origin{};
    if (!armv7a_runtime_trap_origin_to_kernel_origin(seam.origin, origin)) {
        return false;
    }

    out = kernel::TrapFrameView{
        .service_id = seam.service_id,
        .arg0 = seam.arg0,
        .arg1 = seam.arg1,
        .arg2 = seam.arg2,
        .arg3 = seam.arg3,
        .return_pc = seam.return_pc,
        .stack_pointer = seam.stack_pointer,
        .status = seam.status,
        .origin = origin,
        .task_valid = seam.task_valid,
    };
    out.task.value = static_cast<std::size_t>(seam.task);
    return true;
}

export [[nodiscard]] bool armv7a_apply_kernel_runtime_trap_result(
    void* ctx,
    Armv7aRuntimeTrapLiveFrame& frame,
    const kernel::TrapResult& result) noexcept
{
    auto* context = static_cast<Armv7aKernelRuntimeTrapFrameAdapterContext*>(ctx);
    if (context == nullptr || !armv7a_kernel_runtime_trap_frame_adapter_ready(
                                  *context)) {
        return false;
    }

    return context->lower.apply_result(
        context->lower.ctx,
        frame,
        Armv7aRuntimeTrapIngressResult{
            .disposition = armv7a_runtime_trap_ingress_disposition_from_kernel(
                result.disposition),
            .error = armv7a_runtime_trap_ingress_error_from_kernel(result.error),
            .value = result.value,
        });
}

export [[nodiscard]] auto armv7a_make_kernel_runtime_trap_frame_adapter(
    Armv7aKernelRuntimeTrapFrameAdapterContext& context) noexcept
    -> kernel::RuntimeTrapFrameAdapter<Armv7aRuntimeTrapLiveFrame>
{
    return kernel::RuntimeTrapFrameAdapter<Armv7aRuntimeTrapLiveFrame>{
        .ctx = &context,
        .capture = &armv7a_capture_kernel_runtime_trap_frame,
        .apply_result = &armv7a_apply_kernel_runtime_trap_result,
    };
}

export template <
    typename Table,
    typename TraceBuffer = kernel::TaskSyscallFrameTraceBuffer<1>>
class Armv7aTaskSyscallFrameBridge {
public:
    using table_type = Table;
    using frame_type = Armv7aRuntimeTrapLiveFrame;
    using trace_type = TraceBuffer;
    using runtime_adapter_type = kernel::RuntimeTrapFrameAdapter<frame_type>;
    using bridge_type = kernel::TaskSyscallFrameBridge<Table, frame_type, TraceBuffer>;
    using port_type = kernel::TaskSyscallFramePort<frame_type>;

    Armv7aTaskSyscallFrameBridge(Table& table,
                                 Armv7aKernelRuntimeTrapFrameAdapterContext& context,
                                 TraceBuffer* trace = nullptr) noexcept
        : runtime_adapter_(armv7a_make_kernel_runtime_trap_frame_adapter(context)),
          bridge_(table,
                  kernel::make_task_syscall_frame_adapter(runtime_adapter_),
                  trace)
    {
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return bridge_.valid();
    }

    [[nodiscard]] runtime_adapter_type& runtime_adapter() noexcept
    {
        return runtime_adapter_;
    }

    [[nodiscard]] const runtime_adapter_type& runtime_adapter() const noexcept
    {
        return runtime_adapter_;
    }

    [[nodiscard]] bridge_type& bridge() noexcept
    {
        return bridge_;
    }

    [[nodiscard]] const bridge_type& bridge() const noexcept
    {
        return bridge_;
    }

    [[nodiscard]] port_type port() noexcept
    {
        return kernel::make_task_syscall_frame_port(bridge_);
    }

    [[nodiscard]] kernel::TrapResult dispatch(frame_type& frame) noexcept
    {
        return bridge_.dispatch(frame);
    }

private:
    runtime_adapter_type runtime_adapter_{};
    bridge_type bridge_{};
};

export template <typename Table>
[[nodiscard]] auto armv7a_make_task_syscall_frame_bridge(
    Table& table,
    Armv7aKernelRuntimeTrapFrameAdapterContext& context) noexcept
    -> Armv7aTaskSyscallFrameBridge<Table>
{
    return Armv7aTaskSyscallFrameBridge<Table>{table, context};
}

export template <typename Table, typename TraceBuffer>
[[nodiscard]] auto armv7a_make_task_syscall_frame_bridge(
    Table& table,
    Armv7aKernelRuntimeTrapFrameAdapterContext& context,
    TraceBuffer* trace) noexcept
    -> Armv7aTaskSyscallFrameBridge<Table, TraceBuffer>
{
    return Armv7aTaskSyscallFrameBridge<Table, TraceBuffer>{
        table, context, trace};
}
