#pragma once

#include "armv7a_scheduler_tick_contract.hpp"

Armv7aSchedulerTickIngressObservation armv7a_capture_scheduler_tick_ingress() noexcept;
void armv7a_print_scheduler_tick_ingress();
