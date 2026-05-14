#pragma once

// Historical build entries may still define CHARM_DAP_* macros.
// Bridge them into the neutral DAPLINK_* names in one place so the
// rest of the codebase can converge on the public-facing naming.

#if defined(CHARM_DAP_USB_PROFILE) && !defined(DAPLINK_USB_PROFILE_VALUE)
#define DAPLINK_USB_PROFILE_VALUE CHARM_DAP_USB_PROFILE
#endif

#if defined(CHARM_DAP_CDC_UART) && !defined(DAPLINK_CDC_UART_INDEX)
#define DAPLINK_CDC_UART_INDEX CHARM_DAP_CDC_UART
#endif

#if defined(CHARM_DAP_BURST_LIMIT) && !defined(DAPLINK_DAP_BURST_LIMIT)
#define DAPLINK_DAP_BURST_LIMIT CHARM_DAP_BURST_LIMIT
#endif

#if defined(CHARM_DAP_PACKET_COUNT) && !defined(DAPLINK_DAP_PACKET_COUNT)
#define DAPLINK_DAP_PACKET_COUNT CHARM_DAP_PACKET_COUNT
#endif

#if defined(CHARM_DAP_CDC_IN_TIMEOUT_MS) && !defined(DAPLINK_CDC_IN_TIMEOUT_MS)
#define DAPLINK_CDC_IN_TIMEOUT_MS CHARM_DAP_CDC_IN_TIMEOUT_MS
#endif

#if defined(CHARM_DAP_CDC_POLICY) && !defined(DAPLINK_CDC_POLICY)
#define DAPLINK_CDC_POLICY CHARM_DAP_CDC_POLICY
#endif

#if defined(CHARM_DAP_ENABLE_SWO) && !defined(DAPLINK_ENABLE_SWO)
#define DAPLINK_ENABLE_SWO CHARM_DAP_ENABLE_SWO
#endif

#if defined(CHARM_DAP_ENABLE_SWO_STREAM) && !defined(DAPLINK_ENABLE_SWO_STREAM)
#define DAPLINK_ENABLE_SWO_STREAM CHARM_DAP_ENABLE_SWO_STREAM
#endif

#if defined(CHARM_DAP_ENABLE_DAP_UART) && !defined(DAPLINK_ENABLE_DAP_UART)
#define DAPLINK_ENABLE_DAP_UART CHARM_DAP_ENABLE_DAP_UART
#endif
