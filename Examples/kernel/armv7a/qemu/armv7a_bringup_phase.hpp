#pragma once

enum class Armv7aBringupPhase : unsigned char {
    kReset = 0,
    kBootCpuState,
    kMemoryProbePrepare,
    kMemoryProbeDescribe,
    kMmuActivate,
    kSmallPageProbe,
    kAttributeProbe,
    kIcacheProbe,
    kAbortSmoke,
    kExceptionSmoke,
    kDcacheProbe,
    kPageTableProbe,
    kSectionSplitProbe,
    kSvcSmoke,
    kTimerIrqSmoke,
    kSgiIrqSmoke,
    kSgiFiqSmoke,
    kIdle,
};

const char* armv7a_bringup_phase_name(Armv7aBringupPhase phase);
Armv7aBringupPhase armv7a_current_bringup_phase();
void armv7a_enter_bringup_phase(Armv7aBringupPhase phase);
