#pragma once

#include "targets/armv7a/common/armv7a_runtime_leaf_ports_contract.hpp"

Armv7aRuntimeLeafPortsContract armv7a_prepare_runtime_leaf_ports() noexcept;
Armv7aRuntimeLeafPortsContract armv7a_last_runtime_leaf_ports() noexcept;
void armv7a_bind_runtime_leaf_ports(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept;
void armv7a_unbind_runtime_leaf_ports() noexcept;
void armv7a_print_runtime_leaf_ports_observation();
