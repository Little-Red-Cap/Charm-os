#pragma once

#include <cstdint>

enum class Armv7aGicInterruptGroup : std::uint8_t {
    kGroup0 = 0,
    kGroup1 = 1,
};

struct Armv7aGicLineState {
    std::uint32_t igroupr = 0;
    std::uint32_t isenabler = 0;
    std::uint32_t ispendr = 0;
    std::uint32_t isactiver = 0;
};

struct Armv7aGicDistributorState {
    std::uint32_t ctlr = 0;
};

struct Armv7aGicCpuState {
    std::uint32_t ctlr = 0;
    std::uint32_t pmr = 0;
    std::uint32_t bpr = 0;
    std::uint32_t hppir = 0;
};

inline constexpr unsigned int kArmv7aGicSecureTimerIntId = 29u;
inline constexpr unsigned int kArmv7aGicNonSecureTimerIntId = 30u;
inline constexpr unsigned int kArmv7aGicSelfSgiIntId = 1u;
inline constexpr unsigned int kArmv7aGicSpecialIntIdMin = 1020u;
inline constexpr unsigned int kArmv7aGicSpuriousIntId = 1023u;

void armv7a_gic_init_timer_irq();
void armv7a_gic_init_sgi_line(unsigned int intid, Armv7aGicInterruptGroup group);
void armv7a_gic_init_sgi_irq(Armv7aGicInterruptGroup group);
void armv7a_gic_enable_interfaces(bool fiq_enabled);
void armv7a_gic_disable_interfaces();

void armv7a_gic_disable_line(unsigned int intid);
void armv7a_gic_clear_pending(unsigned int intid);
void armv7a_gic_clear_sgi_pending(unsigned int intid);
void armv7a_gic_send_self_sgi(unsigned int intid);

std::uint32_t armv7a_gic_acknowledge_irq();
void armv7a_gic_end_irq(std::uint32_t iar);

Armv7aGicLineState armv7a_gic_read_line_state(unsigned int intid);
Armv7aGicDistributorState armv7a_gic_read_distributor_state();
Armv7aGicCpuState armv7a_gic_read_cpu_state();

bool armv7a_gic_is_timer_intid(unsigned int intid);
bool armv7a_gic_is_sgi_intid(unsigned int intid);
