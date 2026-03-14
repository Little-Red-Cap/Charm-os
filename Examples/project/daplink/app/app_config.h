#ifndef DAPLINK_APP_CONFIG_H
#define DAPLINK_APP_CONFIG_H

// USB identifiers.
#ifndef DAPLINK_USB_VID
#define DAPLINK_USB_VID 0xCAFE
#endif
#ifndef DAPLINK_USB_PID
#define DAPLINK_USB_PID 0x4001
#endif

// USB strings.
#ifndef DAPLINK_USB_MANUFACTURER
#define DAPLINK_USB_MANUFACTURER "Charm"
#endif
#ifndef DAPLINK_USB_PRODUCT
#define DAPLINK_USB_PRODUCT "Charm CMSIS-DAP"
#endif
#ifndef DAPLINK_USB_SERIAL
#define DAPLINK_USB_SERIAL "0001"
#endif

// Firmware string for CMSIS-DAP info.
#ifndef DAPLINK_FW_VERSION
#define DAPLINK_FW_VERSION "0.1.0"
#endif

// Default SWD settings.
#ifndef DAPLINK_SWD_DEFAULT_HZ
#define DAPLINK_SWD_DEFAULT_HZ 5000000U
#endif
#ifndef DAPLINK_SWD_TURNAROUND
#define DAPLINK_SWD_TURNAROUND 1
#endif
#ifndef DAPLINK_SWD_IDLE_CYCLES
#define DAPLINK_SWD_IDLE_CYCLES 0
#endif
#ifndef DAPLINK_SWD_RETRY_COUNT
#define DAPLINK_SWD_RETRY_COUNT 100
#endif

// HID endpoints and packet size.
#ifndef DAPLINK_USB_HID_EP_OUT
#define DAPLINK_USB_HID_EP_OUT 0x01
#endif
#ifndef DAPLINK_USB_HID_EP_IN
#define DAPLINK_USB_HID_EP_IN 0x81
#endif
#ifndef DAPLINK_USB_HID_EP_MPS
#define DAPLINK_USB_HID_EP_MPS 64
#endif

// CDC endpoints and packet size.
#ifndef DAPLINK_USB_CDC_EP_CMD
#define DAPLINK_USB_CDC_EP_CMD 0x83
#endif
#ifndef DAPLINK_USB_CDC_EP_OUT
#define DAPLINK_USB_CDC_EP_OUT 0x04
#endif
#ifndef DAPLINK_USB_CDC_EP_IN
#define DAPLINK_USB_CDC_EP_IN 0x84
#endif
#ifndef DAPLINK_USB_CDC_EP_CMD_MPS
#define DAPLINK_USB_CDC_EP_CMD_MPS 8
#endif
#ifndef DAPLINK_USB_CDC_EP_MPS
#define DAPLINK_USB_CDC_EP_MPS 64
#endif

// USB endpoint zero max packet size.
#ifndef DAPLINK_USB_EP0_MPS
#define DAPLINK_USB_EP0_MPS 64
#endif

#endif
