#ifndef CHARM_DAPLINK_H503_MATH_H
#define CHARM_DAPLINK_H503_MATH_H

#if defined(__GNUC__)
#pragma GCC system_header
#endif

#if defined(__cplusplus) && defined(CHARM_STM32H5_SUPPRESS_MATH_HEADER)
// Work around GCC modules conflicts caused by stm32h5xx.h including math.h
// inside module interface builds. Regular C/C++ code still falls through to
// the toolchain header via include_next.
#else
#include_next <math.h>
#endif

#endif
