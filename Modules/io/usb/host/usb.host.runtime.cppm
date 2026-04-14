module;

#include <array>

export module usb.host.runtime;

import device.bus;
import device.desc;
import device.registry;
import usb.host.core;
import util.core;

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
                        nullptr
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

        bool enumerate(device::RegistryBase& registry) noexcept {
            auto bus_value = bus();
            if (!bus_value.ops.enumerate) {
                return false;
            }
            return bus_value.ops.enumerate(bus_value.ctx, registry);
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
        static bool enumerate_cb(void* ctx, device::RegistryBase& registry) noexcept {
            auto* self = static_cast<SingleDeviceRuntimeBus*>(ctx);
            if (!self) {
                return false;
            }
            if (self->record_.enumerated) {
                return true;
            }
            const bool added = registry.add_device(self->record_.desc, self->record_.ctx);
            if (added) {
                self->record_.enumerated = true;
            }
            return added;
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
                        nullptr
                    },
                    name) {
        }

        DeviceListRuntimeBus(const DeviceListRuntimeBus&) = delete;
        DeviceListRuntimeBus& operator=(const DeviceListRuntimeBus&) = delete;
        DeviceListRuntimeBus(DeviceListRuntimeBus&&) = delete;
        DeviceListRuntimeBus& operator=(DeviceListRuntimeBus&&) = delete;

        bool add_device(const device::DeviceDesc& desc, void* ctx = nullptr) noexcept {
            return add_device(RuntimeDeviceRecord{desc, ctx, false});
        }

        bool add_device(const RuntimeDeviceRecord& record) noexcept {
            if (contains(record)) return true;
            if (count_ >= MaxDevices) {
                return false;
            }
            records_[count_++] = record;
            return true;
        }

        device::Bus bus() const noexcept {
            return host_.bus();
        }

        bool enumerate(device::RegistryBase& registry) noexcept {
            auto bus_value = bus();
            if (!bus_value.ops.enumerate) {
                return false;
            }
            return bus_value.ops.enumerate(bus_value.ctx, registry);
        }

        void reset_enumeration() noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                records_[i].enumerated = false;
            }
        }

        bool reset_device(const RuntimeDeviceRecord& record) noexcept {
            const auto index = find_record_index(record);
            if (index >= count_) return false;
            records_[index].enumerated = false;
            return true;
        }

        bool remove_device(const RuntimeDeviceRecord& record) noexcept {
            const auto index = find_record_index(record);
            if (index >= count_) return false;
            for (util::usize i = index + 1; i < count_; ++i) {
                records_[i - 1] = records_[i];
            }
            if (count_ > 0) {
                records_[count_ - 1] = {};
                --count_;
            }
            return true;
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

        static bool enumerate_cb(void* ctx, device::RegistryBase& registry) noexcept {
            auto* self = static_cast<DeviceListRuntimeBus*>(ctx);
            if (!self) {
                return false;
            }

            bool ok = true;
            for (util::usize i = 0; i < self->count_; ++i) {
                auto& record = self->records_[i];
                if (record.enumerated) {
                    continue;
                }
                const bool added = registry.add_device(record.desc, record.ctx);
                if (added) {
                    record.enumerated = true;
                } else {
                    ok = false;
                }
            }
            return ok;
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
        if (!bus.enumerate(registry)) return false;
        if (!bus.enumerated()) return false;
        if (registry.device_count() != 1) return false;
        if (registry.device_at(0).ctx != &marker) return false;
        if (!same_desc(registry.device_at(0).desc, bus.record().desc)) return false;
        return bus.enumerate(registry) && registry.device_count() == 1;
    }

    inline bool device_list_runtime_bus_self_check() noexcept {
        int marker_a = 1;
        int marker_b = 2;
        DeviceListRuntimeBus<2> bus{"usb.host.multi"};
        if (!bus.add_device(device::DeviceDesc{
                .class_id = 0x08,
                .vendor_id = 0x1234,
                .product_id = 0x5678,
                .type = "usb.host.msc"
            }, &marker_a)) {
            return false;
        }
        if (!bus.add_device(device::DeviceDesc{
                .class_id = 0x02,
                .vendor_id = 0x1234,
                .product_id = 0x5679,
                .type = "usb.host.cdc"
            }, &marker_b)) {
            return false;
        }

        device::Registry<4, 1> registry{};
        if (!bus.enumerate(registry)) return false;
        if (registry.device_count() != 2) return false;
        if (registry.device_at(0).ctx != &marker_a) return false;
        if (registry.device_at(1).ctx != &marker_b) return false;
        if (!bus.record_at(0).enumerated || !bus.record_at(1).enumerated) return false;
        if (!bus.remove_device(bus.record_at(1))) return false;
        if (bus.size() != 1) return false;
        return bus.enumerate(registry) && registry.device_count() == 2;
    }
#endif
}
