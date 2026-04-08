module;

#include <array>
#include <cstddef>
#include <span>

export module usb.plan;

import util.error;
import usb.class_cdc;
import usb.class_msc;
import usb.class_msc_block;
import usb.common;
import usb.dsl;
import usb.model;

export namespace usb::plan {
    using StringTableView = usb::model::StringTableView;

    struct FsTargetConstraints {
        usb::u8 max_interface_number{7};
        usb::u8 max_endpoint_number{7};
        usb::u16 max_bulk_mps{64};
        usb::u8 max_packet_size0{64};
    };

    inline constexpr FsTargetConstraints stm32_fs_constraints() noexcept {
        return FsTargetConstraints{};
    }

    struct DevicePlan {
        usb::dsl::DeviceInfo dev_info{};
        usb::dsl::ConfigInfo cfg_info{};
        StringTableView strings{};
    };

    struct InterfaceAllocation {
        usb::dsl::InterfaceInfo info{};
        usb::u8 num_endpoints{0};
    };

    struct EndpointAllocation {
        usb::dsl::EndpointInfo info{};
    };

    struct MscFunctionPlan {
        const char* cap_name{"usb.msc0"};
        const char* block_cap{"block.sd0"};
        usb::class_driver::MscConfig msc_cfg{};
        usb::class_driver::MscBlockConfig storage_cfg{};
        std::array<InterfaceAllocation, 1> interfaces{};
        std::array<EndpointAllocation, 2> endpoints{};

        [[nodiscard]] std::span<const InterfaceAllocation> interface_allocations() const noexcept {
            return std::span<const InterfaceAllocation>(interfaces.data(), interfaces.size());
        }

        [[nodiscard]] std::span<const EndpointAllocation> endpoint_allocations() const noexcept {
            return std::span<const EndpointAllocation>(endpoints.data(), endpoints.size());
        }
    };

    struct CdcFunctionPlan {
        const char* cap_name{"usb.cdc0"};
        usb::class_driver::CdcConfig cdc_cfg{};
        std::array<InterfaceAllocation, 2> interfaces{};
        std::array<EndpointAllocation, 3> endpoints{};

        [[nodiscard]] std::span<const InterfaceAllocation> interface_allocations() const noexcept {
            return std::span<const InterfaceAllocation>(interfaces.data(), interfaces.size());
        }

        [[nodiscard]] std::span<const EndpointAllocation> endpoint_allocations() const noexcept {
            return std::span<const EndpointAllocation>(endpoints.data(), endpoints.size());
        }
    };

    struct MscDevicePlan {
        DevicePlan device{};
        MscFunctionPlan msc{};
    };

    struct CdcDevicePlan {
        DevicePlan device{};
        CdcFunctionPlan cdc{};
    };

    struct MscCdcDevicePlan {
        DevicePlan device{};
        MscFunctionPlan msc{};
        CdcFunctionPlan cdc{};
    };

    struct AllocationCursor {
        usb::u8 next_interface{0};
        usb::u8 next_out_endpoint{1};
        usb::u8 next_in_endpoint{1};
    };

    inline void reserve_endpoint_number(AllocationCursor& cursor,
                                        bool in_direction,
                                        usb::u8 number) noexcept {
        if (number == 0) {
            return;
        }
        auto& next = in_direction ? cursor.next_in_endpoint : cursor.next_out_endpoint;
        if (next <= number) {
            next = static_cast<usb::u8>(number + 1);
        }
    }

    inline util::Result<void> validate_device_plan(const DevicePlan& device,
                                                   const FsTargetConstraints& constraints) noexcept {
        if (device.dev_info.max_packet_size0 == 0 ||
            device.dev_info.max_packet_size0 > constraints.max_packet_size0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        return {};
    }

    inline util::Result<void> allocate_interfaces(std::span<InterfaceAllocation> interfaces,
                                                  std::span<const usb::model::InterfaceIntent> intents,
                                                  AllocationCursor& cursor,
                                                  const FsTargetConstraints& constraints) noexcept {
        if (interfaces.size() < intents.size()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }

        for (std::size_t i = 0; i < intents.size(); ++i) {
            interfaces[i].info = intents[i].info;
            interfaces[i].num_endpoints = intents[i].num_endpoints;
            if (intents[i].fixed_number) {
                if (interfaces[i].info.interface_number > constraints.max_interface_number) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                continue;
            }
            if (cursor.next_interface > constraints.max_interface_number) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            interfaces[i].info.interface_number = cursor.next_interface++;
        }

        return {};
    }

    inline util::Result<void> validate_interface_allocations(
        std::span<const InterfaceAllocation> interfaces) noexcept {
        if (interfaces.empty()) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        for (std::size_t i = 0; i < interfaces.size(); ++i) {
            if (interfaces[i].num_endpoints == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (std::size_t j = i + 1; j < interfaces.size(); ++j) {
                if (interfaces[i].info.interface_number == interfaces[j].info.interface_number &&
                    interfaces[i].info.alternate_setting == interfaces[j].info.alternate_setting) {
                    return util::unexpected(util::Errc::exist);
                }
            }
        }
        return {};
    }

    inline util::Result<void> validate_interface_allocations(
        std::span<const InterfaceAllocation> lhs,
        std::span<const InterfaceAllocation> rhs) noexcept {
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            for (std::size_t j = 0; j < rhs.size(); ++j) {
                if (lhs[i].info.interface_number == rhs[j].info.interface_number &&
                    lhs[i].info.alternate_setting == rhs[j].info.alternate_setting) {
                    return util::unexpected(util::Errc::exist);
                }
            }
        }
        return {};
    }

    inline util::Result<void> validate_endpoint_allocations(
        std::span<const EndpointAllocation> endpoints,
        const FsTargetConstraints& constraints) noexcept {
        if (endpoints.size() < 2) {
            return util::unexpected(util::Errc::invalid_arg);
        }

        bool has_in = false;
        bool has_out = false;
        for (std::size_t i = 0; i < endpoints.size(); ++i) {
            const auto& info = endpoints[i].info;
            if ((info.address & 0x0Fu) == 0u) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if ((info.address & 0x0Fu) > constraints.max_endpoint_number) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (info.max_packet_size == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (info.type != usb::EndpointType::bulk &&
                info.type != usb::EndpointType::interrupt) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (info.max_packet_size > constraints.max_bulk_mps) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if ((info.address & 0x80u) != 0u) {
                has_in = true;
            } else {
                has_out = true;
            }
            for (std::size_t j = i + 1; j < endpoints.size(); ++j) {
                if (info.address == endpoints[j].info.address) {
                    return util::unexpected(util::Errc::exist);
                }
            }
        }

        if (!has_in || !has_out) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        return {};
    }

    inline util::Result<void> validate_endpoint_allocations(
        std::span<const EndpointAllocation> lhs,
        std::span<const EndpointAllocation> rhs,
        const FsTargetConstraints& constraints) noexcept {
        auto left_ok = validate_endpoint_allocations(lhs, constraints);
        if (!left_ok) {
            return util::unexpected(left_ok.error());
        }
        auto right_ok = validate_endpoint_allocations(rhs, constraints);
        if (!right_ok) {
            return util::unexpected(right_ok.error());
        }
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            for (std::size_t j = 0; j < rhs.size(); ++j) {
                if (lhs[i].info.address == rhs[j].info.address) {
                    return util::unexpected(util::Errc::exist);
                }
            }
        }
        return {};
    }

    inline util::Result<void> allocate_endpoints(std::span<EndpointAllocation> endpoints,
                                                 std::span<const usb::model::EndpointIntent> intents,
                                                 AllocationCursor& cursor,
                                                 const FsTargetConstraints& constraints) noexcept {
        if (endpoints.size() < intents.size()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }

        for (std::size_t i = 0; i < intents.size(); ++i) {
            endpoints[i].info = intents[i].info;

            const auto desired = intents[i].info.address;
            const auto in_direction = (desired & 0x80u) != 0u;
            const auto number = static_cast<usb::u8>(desired & 0x0Fu);

            if (intents[i].fixed_address) {
                reserve_endpoint_number(cursor, in_direction, number);
                continue;
            }

            auto& next = in_direction ? cursor.next_in_endpoint : cursor.next_out_endpoint;
            if (next == 0 || next > constraints.max_endpoint_number) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            endpoints[i].info.address = static_cast<usb::u8>((in_direction ? 0x80u : 0x00u) | next);
            ++next;
        }

        return {};
    }

    inline usb::class_driver::MscConfig materialize_msc_config(
        const usb::class_driver::MscConfig& base,
        std::span<const InterfaceAllocation> interfaces,
        std::span<const EndpointAllocation> endpoints) noexcept {
        auto cfg = base;

        if (!interfaces.empty()) {
            cfg.interface_number = interfaces[0].info.interface_number;
        }

        for (std::size_t index = 0; index < endpoints.size(); ++index) {
            const auto& info = endpoints[index].info;
            if (info.type != usb::EndpointType::bulk) {
                continue;
            }
            cfg.ep_mps = info.max_packet_size;
            if ((info.address & 0x80u) != 0u) {
                cfg.ep_in = info.address;
            } else {
                cfg.ep_out = info.address;
            }
        }

        return cfg;
    }

    inline usb::class_driver::CdcConfig materialize_cdc_config(
        const usb::class_driver::CdcConfig& base,
        std::span<const InterfaceAllocation> interfaces,
        std::span<const EndpointAllocation> endpoints) noexcept {
        auto cfg = base;

        if (!interfaces.empty()) {
            cfg.ctrl_ifc = interfaces[0].info.interface_number;
        }
        if (interfaces.size() > 1) {
            cfg.data_ifc = interfaces[1].info.interface_number;
        }

        for (std::size_t index = 0; index < endpoints.size(); ++index) {
            const auto& info = endpoints[index].info;
            cfg.ep_mps = info.max_packet_size;
            if (info.type == usb::EndpointType::interrupt) {
                cfg.ep_notify = info.address;
            } else if ((info.address & 0x80u) != 0u) {
                cfg.ep_in = info.address;
            } else {
                cfg.ep_out = info.address;
            }
        }

        return cfg;
    }

    inline util::Result<MscDevicePlan> build(const usb::model::MscDeviceModel& model,
                                             const FsTargetConstraints& constraints = stm32_fs_constraints()) noexcept {
        MscDevicePlan plan{};
        plan.device.dev_info = model.device.dev_info;
        plan.device.cfg_info = model.device.cfg_info;
        plan.device.strings = model.device.strings;

        auto dev_ok = validate_device_plan(plan.device, constraints);
        if (!dev_ok) {
            return util::unexpected(dev_ok.error());
        }

        plan.msc.cap_name = model.msc.cap_name;
        plan.msc.block_cap = model.msc.block_cap;
        plan.msc.storage_cfg = model.msc.storage_cfg;
        plan.msc.msc_cfg = model.msc.msc_cfg;

        const auto interfaces = model.msc.interface_intents();
        AllocationCursor cursor{};
        auto alloc_ok = allocate_interfaces(
            std::span<InterfaceAllocation>(plan.msc.interfaces.data(), plan.msc.interfaces.size()),
            interfaces,
            cursor,
            constraints);
        if (!alloc_ok) {
            return util::unexpected(alloc_ok.error());
        }

        const auto endpoints = model.msc.endpoint_intents();
        auto ep_alloc_ok = allocate_endpoints(
            std::span<EndpointAllocation>(plan.msc.endpoints.data(), plan.msc.endpoints.size()),
            endpoints,
            cursor,
            constraints);
        if (!ep_alloc_ok) {
            return util::unexpected(ep_alloc_ok.error());
        }

        plan.msc.msc_cfg = materialize_msc_config(
            model.msc.msc_cfg,
            plan.msc.interface_allocations(),
            plan.msc.endpoint_allocations());

        auto iface_ok = validate_interface_allocations(plan.msc.interface_allocations());
        if (!iface_ok) {
            return util::unexpected(iface_ok.error());
        }

        auto ep_ok = validate_endpoint_allocations(plan.msc.endpoint_allocations(), constraints);
        if (!ep_ok) {
            return util::unexpected(ep_ok.error());
        }

        return plan;
    }

    inline util::Result<CdcDevicePlan> build(const usb::model::CdcDeviceModel& model,
                                             const FsTargetConstraints& constraints = stm32_fs_constraints()) noexcept {
        CdcDevicePlan plan{};
        plan.device.dev_info = model.device.dev_info;
        plan.device.cfg_info = model.device.cfg_info;
        plan.device.strings = model.device.strings;

        auto dev_ok = validate_device_plan(plan.device, constraints);
        if (!dev_ok) {
            return util::unexpected(dev_ok.error());
        }

        plan.cdc.cap_name = model.cdc.cap_name;
        plan.cdc.cdc_cfg = model.cdc.cdc_cfg;

        AllocationCursor cursor{};
        auto iface_ok = allocate_interfaces(
            std::span<InterfaceAllocation>(plan.cdc.interfaces.data(), plan.cdc.interfaces.size()),
            model.cdc.interface_intents(),
            cursor,
            constraints);
        if (!iface_ok) {
            return util::unexpected(iface_ok.error());
        }

        auto ep_ok = allocate_endpoints(
            std::span<EndpointAllocation>(plan.cdc.endpoints.data(), plan.cdc.endpoints.size()),
            model.cdc.endpoint_intents(),
            cursor,
            constraints);
        if (!ep_ok) {
            return util::unexpected(ep_ok.error());
        }

        plan.cdc.cdc_cfg = materialize_cdc_config(
            model.cdc.cdc_cfg,
            plan.cdc.interface_allocations(),
            plan.cdc.endpoint_allocations());

        auto iface_valid = validate_interface_allocations(plan.cdc.interface_allocations());
        if (!iface_valid) {
            return util::unexpected(iface_valid.error());
        }

        auto ep_valid = validate_endpoint_allocations(plan.cdc.endpoint_allocations(), constraints);
        if (!ep_valid) {
            return util::unexpected(ep_valid.error());
        }

        return plan;
    }

    inline util::Result<MscCdcDevicePlan> build(const usb::model::MscCdcDeviceModel& model,
                                                const FsTargetConstraints& constraints = stm32_fs_constraints()) noexcept {
        MscCdcDevicePlan plan{};
        plan.device.dev_info = model.device.dev_info;
        plan.device.cfg_info = model.device.cfg_info;
        plan.device.strings = model.device.strings;

        auto dev_ok = validate_device_plan(plan.device, constraints);
        if (!dev_ok) {
            return util::unexpected(dev_ok.error());
        }

        plan.msc.cap_name = model.msc.cap_name;
        plan.msc.block_cap = model.msc.block_cap;
        plan.msc.storage_cfg = model.msc.storage_cfg;
        plan.msc.msc_cfg = model.msc.msc_cfg;

        plan.cdc.cap_name = model.cdc.cap_name;
        plan.cdc.cdc_cfg = model.cdc.cdc_cfg;

        AllocationCursor cursor{};
        auto msc_iface_ok = allocate_interfaces(
            std::span<InterfaceAllocation>(plan.msc.interfaces.data(), plan.msc.interfaces.size()),
            model.msc.interface_intents(),
            cursor,
            constraints);
        if (!msc_iface_ok) {
            return util::unexpected(msc_iface_ok.error());
        }

        auto cdc_iface_ok = allocate_interfaces(
            std::span<InterfaceAllocation>(plan.cdc.interfaces.data(), plan.cdc.interfaces.size()),
            model.cdc.interface_intents(),
            cursor,
            constraints);
        if (!cdc_iface_ok) {
            return util::unexpected(cdc_iface_ok.error());
        }

        auto msc_ep_ok = allocate_endpoints(
            std::span<EndpointAllocation>(plan.msc.endpoints.data(), plan.msc.endpoints.size()),
            model.msc.endpoint_intents(),
            cursor,
            constraints);
        if (!msc_ep_ok) {
            return util::unexpected(msc_ep_ok.error());
        }

        auto cdc_ep_ok = allocate_endpoints(
            std::span<EndpointAllocation>(plan.cdc.endpoints.data(), plan.cdc.endpoints.size()),
            model.cdc.endpoint_intents(),
            cursor,
            constraints);
        if (!cdc_ep_ok) {
            return util::unexpected(cdc_ep_ok.error());
        }

        plan.msc.msc_cfg = materialize_msc_config(
            model.msc.msc_cfg,
            plan.msc.interface_allocations(),
            plan.msc.endpoint_allocations());
        plan.cdc.cdc_cfg = materialize_cdc_config(
            model.cdc.cdc_cfg,
            plan.cdc.interface_allocations(),
            plan.cdc.endpoint_allocations());

        auto msc_iface_valid = validate_interface_allocations(plan.msc.interface_allocations());
        if (!msc_iface_valid) {
            return util::unexpected(msc_iface_valid.error());
        }
        auto cdc_iface_valid = validate_interface_allocations(plan.cdc.interface_allocations());
        if (!cdc_iface_valid) {
            return util::unexpected(cdc_iface_valid.error());
        }
        auto cross_iface_valid = validate_interface_allocations(
            plan.msc.interface_allocations(),
            plan.cdc.interface_allocations());
        if (!cross_iface_valid) {
            return util::unexpected(cross_iface_valid.error());
        }

        auto cross_ep_valid = validate_endpoint_allocations(
            plan.msc.endpoint_allocations(),
            plan.cdc.endpoint_allocations(),
            constraints);
        if (!cross_ep_valid) {
            return util::unexpected(cross_ep_valid.error());
        }

        return plan;
    }
}
