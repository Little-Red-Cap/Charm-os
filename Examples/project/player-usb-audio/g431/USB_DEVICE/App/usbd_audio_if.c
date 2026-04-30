#include "usbd_audio_if.h"

#include <string.h>

#include "stm32g4xx.h"
#include "usb_device.h"

static volatile uint32_t g_audio_rx_bytes = 0;
static volatile uint32_t g_audio_rx_pkts = 0;
static volatile uint32_t g_audio_rx_last_size = 0;
static volatile uint32_t g_audio_rx_overflows = 0;
static volatile uint32_t g_audio_freq = 0;
static volatile uint32_t g_audio_cmd = 0;
static volatile uint32_t g_audio_init_calls = 0;
static volatile uint32_t g_audio_cmd_calls = 0;

enum {
    USB_AUDIO_RING_SIZE = 8192,
};

static volatile uint32_t g_audio_ring_overflows = 0;
static volatile uint32_t g_audio_ring_high_watermark = 0;
static volatile uint32_t g_audio_ring_dropped_bytes = 0;
static uint8_t g_audio_ring[USB_AUDIO_RING_SIZE];
static volatile uint32_t g_audio_ring_wr = 0;
static volatile uint32_t g_audio_ring_rd = 0;
static volatile uint32_t g_audio_ring_used = 0;

static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options);
static int8_t AUDIO_DeInit_FS(uint32_t options);
static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_VolumeCtl_FS(uint8_t vol);
static int8_t AUDIO_MuteCtl_FS(uint8_t cmd);
static int8_t AUDIO_PeriodicTC_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_GetState_FS(void);

USBD_AUDIO_ItfTypeDef USBD_AUDIO_fops_FS = {
    AUDIO_Init_FS,
    AUDIO_DeInit_FS,
    AUDIO_AudioCmd_FS,
    AUDIO_VolumeCtl_FS,
    AUDIO_MuteCtl_FS,
    AUDIO_PeriodicTC_FS,
    AUDIO_GetState_FS,
};

static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options) {
    UNUSED(Volume);
    UNUSED(options);
    g_audio_freq = AudioFreq;
    g_audio_init_calls++;
    return USBD_OK;
}

static int8_t AUDIO_DeInit_FS(uint32_t options) {
    UNUSED(options);
    return USBD_OK;
}

static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd) {
    UNUSED(pbuf);
    UNUSED(size);
    g_audio_cmd = cmd;
    g_audio_cmd_calls++;
    return USBD_OK;
}

static int8_t AUDIO_VolumeCtl_FS(uint8_t vol) {
    UNUSED(vol);
    return USBD_OK;
}

static int8_t AUDIO_MuteCtl_FS(uint8_t cmd) {
    UNUSED(cmd);
    return USBD_OK;
}

static int8_t AUDIO_PeriodicTC_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd) {
    UNUSED(cmd);

    if (size > 0) {
        const uint32_t next = g_audio_rx_bytes + size;
        if (next < g_audio_rx_bytes) {
            g_audio_rx_overflows++;
        }
        g_audio_rx_bytes = next;
        g_audio_rx_pkts++;
        g_audio_rx_last_size = size;
    }

    if (size > 0) {
        __disable_irq();
        if ((g_audio_ring_used + size) > sizeof(g_audio_ring)) {
            g_audio_ring_overflows++;
            g_audio_ring_dropped_bytes += size;
            __enable_irq();
            return USBD_OK;
        }

        uint32_t wr = g_audio_ring_wr;
        const uint32_t remaining = sizeof(g_audio_ring) - wr;
        if (size <= remaining) {
            memcpy(&g_audio_ring[wr], pbuf, size);
            wr += size;
            if (wr >= sizeof(g_audio_ring)) {
                wr = 0;
            }
        } else {
            memcpy(&g_audio_ring[wr], pbuf, remaining);
            memcpy(&g_audio_ring[0], pbuf + remaining, size - remaining);
            wr = size - remaining;
        }

        g_audio_ring_wr = wr;
        g_audio_ring_used += size;
        if (g_audio_ring_used > g_audio_ring_high_watermark) {
            g_audio_ring_high_watermark = g_audio_ring_used;
        }
        __enable_irq();
    }

    return USBD_OK;
}

static int8_t AUDIO_GetState_FS(void) {
    return USBD_OK;
}

void TransferComplete_CallBack_FS(void) {
    USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_FULL);
}

void HalfTransfer_CallBack_FS(void) {
    USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_HALF);
}

uint32_t usb_audio_rx_bytes(void) { return g_audio_rx_bytes; }
uint32_t usb_audio_rx_pkts(void) { return g_audio_rx_pkts; }
uint32_t usb_audio_rx_last_size(void) { return g_audio_rx_last_size; }
uint32_t usb_audio_rx_overflows(void) { return g_audio_rx_overflows; }
uint32_t usb_audio_rx_freq(void) { return g_audio_freq; }
uint32_t usb_audio_rx_cmd(void) { return g_audio_cmd; }
uint32_t usb_audio_rx_init_calls(void) { return g_audio_init_calls; }
uint32_t usb_audio_rx_cmd_calls(void) { return g_audio_cmd_calls; }

void usb_audio_rx_reset(void) {
    __disable_irq();
    g_audio_rx_bytes = 0;
    g_audio_rx_pkts = 0;
    g_audio_rx_last_size = 0;
    g_audio_rx_overflows = 0;
    g_audio_init_calls = 0;
    g_audio_cmd_calls = 0;
    g_audio_ring_overflows = 0;
    g_audio_ring_high_watermark = 0;
    g_audio_ring_dropped_bytes = 0;
    g_audio_ring_wr = 0;
    g_audio_ring_rd = 0;
    g_audio_ring_used = 0;
    __enable_irq();
}

uint32_t usb_audio_ring_available(void) {
    return g_audio_ring_used;
}

uint32_t usb_audio_ring_overflows(void) {
    return g_audio_ring_overflows;
}

uint32_t usb_audio_ring_high_watermark(void) {
    return g_audio_ring_high_watermark;
}

uint32_t usb_audio_ring_dropped_bytes(void) {
    return g_audio_ring_dropped_bytes;
}

uint32_t usb_audio_ring_read(uint8_t* dst, uint32_t size) {
    if (!dst || size == 0) {
        return 0;
    }

    __disable_irq();
    uint32_t available = g_audio_ring_used;
    if (size > available) {
        size = available;
    }
    if (size == 0) {
        __enable_irq();
        return 0;
    }

    uint32_t rd = g_audio_ring_rd;
    const uint32_t remaining = sizeof(g_audio_ring) - rd;
    if (size <= remaining) {
        memcpy(dst, &g_audio_ring[rd], size);
        rd += size;
        if (rd >= sizeof(g_audio_ring)) {
            rd = 0;
        }
    } else {
        memcpy(dst, &g_audio_ring[rd], remaining);
        memcpy(dst + remaining, &g_audio_ring[0], size - remaining);
        rd = size - remaining;
    }

    g_audio_ring_rd = rd;
    g_audio_ring_used -= size;
    __enable_irq();
    return size;
}
