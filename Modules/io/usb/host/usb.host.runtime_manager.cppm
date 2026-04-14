export module usb.host.runtime_manager;

import device.registry;
import usb.host.runtime;
import util.core;
import util.error;

export namespace usb::host {
    template <util::usize MaxDevices, util::usize MaxDrivers>
    class RuntimeManager {
    public:
        explicit RuntimeManager(const char* bus_name = "usb.host") noexcept
            : bus_(bus_name) {
        }

        template <typename BindingT>
        util::Result<void> try_add(BindingT& binding) noexcept {
            const auto record = binding.device_record();
            if (bus_.contains(record)) {
                return {};
            }
            auto added_bus = bus_.try_add_device(record);
            if (!added_bus) {
                return added_bus;
            }
            auto added_driver = registry_.try_add_driver(binding.driver());
            if (!added_driver) {
                (void)bus_.remove_device(record);
                return added_driver;
            }
            return {};
        }

        template <typename BindingT>
        bool add(BindingT& binding) noexcept {
            return static_cast<bool>(try_add(binding));
        }

        template <typename BindingT>
        util::Result<void> add_exported(BindingT& binding) noexcept {
            auto exported = binding.ensure_exported();
            if (!exported) {
                return exported;
            }
            return try_add(binding);
        }

        bool enumerate() noexcept {
            return bus_.enumerate(registry_);
        }

        bool scan() noexcept {
            const bool enumerated = enumerate();
            registry_.match_detected();
            return enumerated;
        }

        template <typename BindingT>
        [[nodiscard]] bool contains(const BindingT& binding) const noexcept {
            return find_record_index(binding.device_record()) < bus_.size();
        }

        template <typename BindingT>
        [[nodiscard]] bool enumerated(const BindingT& binding) const noexcept {
            const auto index = find_record_index(binding.device_record());
            return index < bus_.size() && bus_.record_at(index).enumerated;
        }

        template <typename BindingT>
        bool remove(BindingT& binding) noexcept {
            return binding.remove(registry_);
        }

        template <typename BindingT>
        bool rediscover(BindingT& binding) noexcept {
            return bus_.reset_device(binding.device_record()) && scan();
        }

        void reset_all() noexcept {
            bus_.reset_enumeration();
        }

        device::Registry<MaxDevices, MaxDrivers>& registry() noexcept {
            return registry_;
        }

        const device::Registry<MaxDevices, MaxDrivers>& registry() const noexcept {
            return registry_;
        }

        DeviceListRuntimeBus<MaxDevices>& bus() noexcept {
            return bus_;
        }

        const DeviceListRuntimeBus<MaxDevices>& bus() const noexcept {
            return bus_;
        }

    private:
        [[nodiscard]] util::usize find_record_index(const RuntimeDeviceRecord& record) const noexcept {
            for (util::usize i = 0; i < bus_.size(); ++i) {
                if (same_record(bus_.record_at(i), record)) {
                    return i;
                }
            }
            return bus_.size();
        }

        device::Registry<MaxDevices, MaxDrivers> registry_{};
        DeviceListRuntimeBus<MaxDevices> bus_;
    };
}
