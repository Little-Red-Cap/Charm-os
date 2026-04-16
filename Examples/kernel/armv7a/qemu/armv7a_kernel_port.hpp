#pragma once

#include "armv7a_kernel_port_contract.hpp"

Armv7aKernelPortContract armv7a_make_qemu_kernel_port_contract() noexcept;
void armv7a_print_kernel_port_status();
