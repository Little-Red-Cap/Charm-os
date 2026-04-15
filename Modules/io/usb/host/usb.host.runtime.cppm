module;

#include <array>

export module usb.host.runtime;

import device.bus;
import device.desc;
import device.registry;
import usb.host.core;
import util.core;
import util.error;

export namespace usb::host {
    inline constexpr bool same_desc(const device::DeviceDesc& lhs,
                                    const device::DeviceDesc& rhs) noexcept {
        return lhs.class_id == rhs.class_id &&
               lhs.vendor_id == rhs.vendor_id &&
               lhs.product_id == rhs.product_id &&
               lhs.type.compare(rhs.type) == 0;
    }

    struct RuntimeDeviceRecord {
        device::DeviceDesc desc{};
        void* ctx{nullptr};
        bool enumerated{false};
    };

    inline constexpr bool same_record(const RuntimeDeviceRecord& lhs,
                                      const RuntimeDeviceRecord& rhs) noexcept {
        return lhs.ctx == rhs.ctx && same_desc(lhs.desc, rhs.desc);
    }

    class SingleDeviceRuntimeBus {
    public:
        SingleDeviceRuntimeBus(const device::DeviceDesc& desc,
                               void* ctx = nullptr,
                               const char* name = "usb.host") noexcept
            : record_{desc, ctx},
              host_(this,
                    HostOps{
                        &SingleDeviceRuntimeBus::enumerate_cb,
                        nullptr,
                        nullptr,
                        &SingleDeviceRuntimeBus::try_enumerate_cb
                    },
                    name) {
        }

        SingleDeviceRuntimeBus(const SingleDeviceRuntimeBus&) = delete;
        SingleDeviceRuntimeBus& operator=(const SingleDeviceRuntimeBus&) = delete;
        SingleDeviceRuntimeBus(SingleDeviceRuntimeBus&&) = delete;
        SingleDeviceRuntimeBus& operator=(SingleDeviceRuntimeBus&&) = delete;

        device::Bus bus() const noexcept {
            return host_.bus();
        }

        [[nodiscard]] util::Result<void> try_enumerate(device::RegistryBase& registry) noexcept {
            if (record_.enumerated) {
                return {};
            }
            auto added = registry.try_add_device(record_.desc, record_.ctx);
            if (added) {
                record_.enumerated = true;
            }
            return added;
        }

        bool enumerate(device::RegistryBase& registry) noexcept {
            return static_cast<bool>(try_enumerate(registry));
        }

        void set_ctx(void* ctx) noexcept {
            record_.ctx = ctx;
        }

        void reset_enumeration() noexcept {
            record_.enumerated = false;
        }

        [[nodiscard]] bool enumerated() const noexcept {
            return record_.enumerated;
        }

        RuntimeDeviceRecord& record() noexcept { return record_; }
        const RuntimeDeviceRecord& record() const noexcept { return record_; }

    private:
        static util::Result<void> try_enumerate_cb(void* ctx,
                                                   device::RegistryBase& registry) noexcept {
            auto* self = static_cast<SingleDeviceRuntimeBus*>(ctx);
            if (!self) {
                return util::unexpected(util::Errc::bad_state);
            }
            if (self->record_.enumerated) {
                return {};
            }
            return self->try_enumerate(registry);
        }

        static bool enumerate_cb(void* ctx, device::RegistryBase& registry) noexcept {
            return static_cast<bool>(try_enumerate_cb(ctx, registry));
        }

        RuntimeDeviceRecord record_{};
        HostBus host_;
    };

    template <util::usize MaxDevices>
    class DeviceListRuntimeBus {
    public:
        explicit DeviceListRuntimeBus(const char* name = "usb.host") noexcept
            : host_(this,
                    HostOps{
                        &DeviceListRuntimeBus::enumerate_cb,
                        nullptr,
                        nullptr,
                        &DeviceListRuntimeBus::try_enumerate_cb
                    },
                    name) {
        }

        DeviceListRuntimeBus(const DeviceListRuntimeBus&) = delete;
        DeviceListRuntimeBus& operator=(const DeviceListRuntimeBus&) = delete;
        DeviceListRuntimeBus(DeviceListRuntimeBus&&) = delete;
        DeviceListRuntimeBus& operator=(DeviceListRuntimeBus&&) = delete;

        [[nodiscard]] util::Result<void> try_add_device(const device::DeviceDesc& desc,
                                                        void* ctx = nullptr) noexcept {
            return try_add_device(RuntimeDeviceRecord{desc, ctx, false});
        }

        bool add_device(const device::DeviceDesc& desc, void* ctx = nullptr) noexcept {
            return static_cast<bool>(try_add_device(desc, ctx));
        }

        [[nodiscard]] util::Result<void> try_add_device(const RuntimeDeviceRecord& record) noexcept {
            if (contains(record)) return {};
            if (count_ >= MaxDevices) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            records_[count_++] = record;
            return {};
        }

        bool add_device(const RuntimeDeviceRecord& record) noexcept {
            return static_cast<bool>(try_add_device(record));
        }

        device::Bus bus() const noexcept {
            return host_.bus();
        }

        [[nodiscard]] util::Result<void> try_enumerate(device::RegistryBase& registry) noexcept {
            util::Errc first_error = util::Errc::ok;
            for (util::usize i = 0; i < count_; ++i) {
                auto& record = records_[i];
                if (record.enumerated) {
                    continue;
                }
                auto added = registry.try_add_device(record.desc, record.ctx);
                if (added) {
                    record.enumerated = true;
                    continue;
                }
                if (first_error == util::Errc::ok) {
                    first_error = added.error();
                }
            }
            if (first_error != util::Errc::ok) {
                return util::unexpected(first_error);
            }
            return {};
        }

        bool enumerate(device::RegistryBase& registry) noexcept {
            return static_cast<bool>(try_enumerate(registry));
        }

        void reset_enumeration() noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                records_[i].enumerated = false;
            }
        }

        [[nodiscard]] util::Result<void> try_reset_device(const RuntimeDeviceRecord& record) noexcept {
            const auto index = find_record_index(record);
            if (index >= count_) {
                return util::unexpected(util::Errc::noent);
            }
            records_[index].enumerated = false;
            return {};
        }

        bool reset_device(const RuntimeDeviceRecord& record) noexcept {
            return static_cast<bool>(try_reset_device(record));
        }

        [[nodiscard]] util::Result<void> try_remove_device(const RuntimeDeviceRecord& record) noexcept {
            const auto index = find_record_index(record);
            if (index >= count_) {
                return util::unexpected(util::Errc::noent);
            }
            for (util::usize i = index + 1; i < count_; ++i) {
                records_[i - 1] = records_[i];
            }
            if (count_ > 0) {
                records_[count_ - 1] = {};
                --count_;
            }
            return {};
        }

        bool remove_device(const RuntimeDeviceRecord& record) noexcept {
            return static_cast<bool>(try_remove_device(record));
        }

        [[nodiscard]] bool contains(const RuntimeDeviceRecord& record) const noexcept {
            return find_record_index(record) < count_;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

        [[nodiscard]] util::usize capacity() const noexcept {
            return MaxDevices;
        }

        RuntimeDeviceRecord& record_at(util::usize index) noexcept {
            return records_[index];
        }

        const RuntimeDeviceRecord& record_at(util::usize index) const noexcept {
            return records_[index];
        }

    private:
        util::usize find_record_index(const RuntimeDeviceRecord& record) const noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (same_record(records_[i], record)) {
                    return i;
                }
            }
            return count_;
        }

        static util::Result<void> try_enumerate_cb(void* ctx,
                                                   device::RegistryBase& registry) noexcept {
            auto* self = static_cast<DeviceListRuntimeBus*>(ctx);
            if (!self) {
                return util::unexpected(util::Errc::bad_state);
            }
            return self->try_enumerate(registry);
        }

        static bool enumerate_cb(void* ctx, device::RegistryBase& registry) noexcept {
            return static_cast<bool>(try_enumerate_cb(ctx, registry));
        }

        std::array<RuntimeDeviceRecord, MaxDevices> records_{};
        util::usize count_{0};
        HostBus host_;
    };

#ifndef NDEBUG
    inline bool single_device_runtime_bus_self_check() noexcept {
        int marker = 42;
        SingleDeviceRuntimeBus bus{
            device::DeviceDesc{
                .class_id = 0x08,
                .vendor_id = 0x1234,
                .product_id = 0x5678,
                .type = "usb.host.msc"
            },
            &marker,
            "usb.host.test"
        };

        device::Registry<2, 1> registry{};
        auto exported_bus = bus.bus();
        if (exported_bus.ops.try_enumerate == nullptr) return false;
        if (!exported_bus.ops.try_enumerate(exported_bus.ctx, registry)) return false;
        if (!bus.enumerated()) return false;
        if (registry.device_count() != 1) return false;
        if (registry.device_at(0).ctx != &marker) return false;
        if (!same_desc(registry.device_at(0).desc, bus.record().desc)) return false;
        return exported_bus.ops.try_enumerate(exported_bus.ctx, registry) &&
               registry.device_count() == 1;
    }

    inline bool device_list_runtime_bus_self_check() noexcept {
        int marker_a = 1;
        int marker_b = 2;
        DeviceListRuntimeBus<2> bus{"usb.host.multi"};
        const auto msc_record = RuntimeDeviceRecord{
            device::DeviceDesc{
                .class_id = 0x08,
                .vendor_id = 0x1234,
                .product_id = 0x5678,
                .type = "usb.host.msc"
            },
            &marker_a,
            false
        };
        const auto cdc_record = RuntimeDeviceRecord{
            device::DeviceDesc{
                .class_id = 0x02,
                .vendor_id = 0x1234,
                .product_id = 0x5679,
                .type = "usb.host.cdc"
            },
            &marker_b,
            false
        };
        if (!bus.try_add_device(msc_record)) {
            return false;
        }
        if (!bus.try_add_device(cdc_record)) {
            return false;
        }

        device::Registry<4, 1> registry{};
        auto exported_bus = bus.bus();
        if (exported_bus.ops.try_enumerate == nullptr) return false;
        if (!exported_bus.ops.try_enumerate(exported_bus.ctx, registry)) return false;
        if (registry.device_count() != 2) return false;
        if (registry.device_at(0).ctx != &marker_a) return false;
        if (registry.device_at(1).ctx != &marker_b) return false;
        if (!bus.record_at(0).enumerated || !bus.record_at(1).enumerated) return false;
        if (!bus.try_remove_device(cdc_record)) return false;
        if (bus.size() != 1) return false;
        auto missing = bus.try_reset_device(cdc_record);
        if (missing || missing.error() != util::Errc::noent) return false;
        return exported_bus.ops.try_enumerate(exported_bus.ctx, registry) &&
               registry.device_count() == 2;
    }
#endif
}
