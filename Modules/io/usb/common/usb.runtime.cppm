module;

#include <span>

export module usb.runtime;

import init.node;
import usb.class_cdc;
import usb.class_cdc_acm.node;
import usb.class_msc;
import usb.class_msc_cdc.node;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import usb.dsl;
import usb.driver;
import usb.plan;
import util.core;

export namespace usb::runtime {
    struct MscReadyHook {
        void (*fn)(void* ctx,
                   usb::class_driver::MscBot* bot,
                   const usb::class_driver::MscConfig* cfg) noexcept { nullptr };
        void* ctx{nullptr};
    };

    struct CdcReadyHook {
        void (*fn)(void* ctx,
                   usb::class_driver::CdcAcm* cdc,
                   const usb::class_driver::CdcConfig* cfg) noexcept { nullptr };
        void* ctx{nullptr};
    };

    struct CdcRuntimeConfig {
        void* ctx{nullptr};
        usb::class_driver::CdcOps ops{};
        CdcReadyHook ready{};
    };

    struct Stm32FsRuntime {
        usb::driver::DcdOps dcd{};
        void* dcd_ctx{nullptr};
        usb::driver::DcdDeviceAdapter* adapter{nullptr};
        MscReadyHook ready{};
        CdcRuntimeConfig cdc{};
        init::Phase phase{init::Phase::app};
        util::u32 runlevel_mask{static_cast<util::u32>(init::Runlevel::all)};
    };

    inline constexpr Stm32FsRuntime stm32_fs(const usb::driver::DcdOps& dcd,
                                             void* dcd_ctx,
                                             usb::driver::DcdDeviceAdapter* adapter,
                                             MscReadyHook ready = {},
                                             CdcRuntimeConfig cdc = {},
                                             init::Phase phase = init::Phase::app,
                                             util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept {
        return Stm32FsRuntime{dcd, dcd_ctx, adapter, ready, cdc, phase, runlevel_mask};
    }

    inline constexpr Stm32FsRuntime host_mock(const usb::driver::DcdOps& dcd,
                                              void* dcd_ctx,
                                              usb::driver::DcdDeviceAdapter* adapter,
                                              MscReadyHook ready = {},
                                              CdcRuntimeConfig cdc = {},
                                              init::Phase phase = init::Phase::app,
                                              util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept {
        return stm32_fs(dcd, dcd_ctx, adapter, ready, cdc, phase, runlevel_mask);
    }

    inline usb::device::CdcAcmDesc make_desc(const usb::plan::CdcDevicePlan& plan,
                                             const Stm32FsRuntime& runtime) noexcept {
        usb::device::CdcAcmDesc desc{};
        desc.cap_name = plan.cdc.cap_name;
        desc.dcd = runtime.dcd;
        desc.dcd_ctx = runtime.dcd_ctx;
        desc.adapter = runtime.adapter;
        desc.dev_info = plan.device.dev_info;
        desc.cfg_info = plan.device.cfg_info;
        desc.cdc_cfg = plan.cdc.cdc_cfg;
        desc.strings = plan.device.strings;
        desc.cdc_ctx = runtime.cdc.ctx;
        desc.cdc_ops = runtime.cdc.ops;
        desc.on_ready = runtime.cdc.ready.fn;
        desc.on_ready_ctx = runtime.cdc.ready.ctx;
        return desc;
    }

    inline usb::device::MscBlockDesc make_desc(const usb::plan::MscDevicePlan& plan,
                                               const Stm32FsRuntime& runtime) noexcept {
        usb::device::MscBlockDesc desc{};
        desc.cap_name = plan.msc.cap_name;
        desc.block_cap = plan.msc.block_cap;
        desc.dcd = runtime.dcd;
        desc.dcd_ctx = runtime.dcd_ctx;
        desc.adapter = runtime.adapter;
        desc.dev_info = plan.device.dev_info;
        desc.cfg_info = plan.device.cfg_info;
        desc.msc_cfg = plan.msc.msc_cfg;
        desc.strings = plan.device.strings;
        desc.storage_cfg = plan.msc.storage_cfg;
        desc.on_ready = runtime.ready.fn;
        desc.on_ready_ctx = runtime.ready.ctx;
        return desc;
    }

    inline usb::device::MscCdcCompositeDesc make_desc(const usb::plan::MscCdcDevicePlan& plan,
                                                      const Stm32FsRuntime& runtime) noexcept {
        usb::device::MscCdcCompositeDesc desc{};
        desc.msc_cap_name = plan.msc.cap_name;
        desc.block_cap = plan.msc.block_cap;
        desc.cdc_cap_name = plan.cdc.cap_name;
        desc.dcd = runtime.dcd;
        desc.dcd_ctx = runtime.dcd_ctx;
        desc.adapter = runtime.adapter;
        desc.dev_info = plan.device.dev_info;
        desc.cfg_info = plan.device.cfg_info;
        desc.strings = plan.device.strings;
        desc.msc_cfg = plan.msc.msc_cfg;
        desc.cdc_cfg = plan.cdc.cdc_cfg;
        desc.storage_cfg = plan.msc.storage_cfg;
        desc.on_ready = runtime.ready.fn;
        desc.on_ready_ctx = runtime.ready.ctx;
        return desc;
    }

    template <typename RegistryT,
              util::usize IoBufSize = 4096,
              util::usize DevDescSize = 64,
              util::usize CfgDescSize = 256>
    inline auto make(const usb::plan::MscDevicePlan& plan,
                     RegistryT& registry,
                     const Stm32FsRuntime& runtime) noexcept {
        return usb::device::MscBlockBinding<RegistryT, IoBufSize, DevDescSize, CfgDescSize>{
            registry,
            make_desc(plan, runtime),
            runtime.phase,
            runtime.runlevel_mask
        };
    }

    template <util::usize DevDescSize = 64,
              util::usize CfgDescSize = 256>
    inline auto make(const usb::plan::CdcDevicePlan& plan,
                     const Stm32FsRuntime& runtime) noexcept {
        return usb::device::CdcAcmBinding<DevDescSize, CfgDescSize>{
            make_desc(plan, runtime),
            runtime.phase,
            runtime.runlevel_mask
        };
    }

    template <typename RegistryT,
              util::usize IoBufSize = 4096,
              util::usize DevDescSize = 64,
              util::usize CfgDescSize = 256,
              util::usize CdcBufSize = 512>
    inline auto make(const usb::plan::MscCdcDevicePlan& plan,
                     RegistryT& registry,
                     const Stm32FsRuntime& runtime) noexcept {
        return usb::device::MscCdcBinding<RegistryT, IoBufSize, DevDescSize, CfgDescSize, CdcBufSize>{
            registry,
            make_desc(plan, runtime),
            runtime.phase,
            runtime.runlevel_mask
        };
    }
}
