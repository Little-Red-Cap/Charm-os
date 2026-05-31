#include "usb_dev_loader.h"

#include "usbd_cdc.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "stm32h7xx.h"

#include <string.h>

#define USB_DEV_LOADER_RX_PACKET_SIZE CDC_DATA_FS_MAX_PACKET_SIZE
#define USB_DEV_LOADER_RX_RING_SIZE 8192U

USBD_HandleTypeDef hUsbDeviceFS;

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

static uint8_t g_cdc_rx_packet[USB_DEV_LOADER_RX_PACKET_SIZE];
static uint8_t g_rx_ring[USB_DEV_LOADER_RX_RING_SIZE];
static volatile h747_usb_dev_loader_status_t g_status = {0};
static volatile uint32_t g_rx_head = 0U;
static volatile uint32_t g_rx_tail = 0U;

static int8_t cdc_init(void);
static int8_t cdc_deinit(void);
static int8_t cdc_control(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t cdc_receive(uint8_t* Buf, uint32_t* Len);
static int8_t cdc_transmit_complete(uint8_t* Buf, uint32_t* Len, uint8_t epnum);
static void usb_device_soft_disconnect(uint8_t disconnected);
static void copy_prefix(uint8_t* dst, uint8_t* dst_len, uint32_t dst_capacity, const uint8_t* src, uint16_t src_len);
extern uint8_t usb_last_setup_valid(void);
extern void usb_copy_last_setup(uint8_t out[8]);

static USBD_CDC_ItfTypeDef g_cdc_interface = {
    cdc_init,
    cdc_deinit,
    cdc_control,
    cdc_receive,
    cdc_transmit_complete,
};

static uint8_t ring_push(uint8_t byte) {
    const uint32_t next = (g_rx_head + 1U) % USB_DEV_LOADER_RX_RING_SIZE;
    if (next == g_rx_tail) {
        return 0U;
    }
    g_rx_ring[g_rx_head] = byte;
    g_rx_head = next;
    return 1U;
}

static uint8_t ring_pop(uint8_t* out) {
    if (out == NULL || g_rx_tail == g_rx_head) {
        return 0U;
    }
    *out = g_rx_ring[g_rx_tail];
    g_rx_tail = (g_rx_tail + 1U) % USB_DEV_LOADER_RX_RING_SIZE;
    return 1U;
}

static void reset_runtime_state(void) {
    g_rx_head = 0U;
    g_rx_tail = 0U;
    memset(g_rx_ring, 0, sizeof(g_rx_ring));
    memset(g_cdc_rx_packet, 0, sizeof(g_cdc_rx_packet));
}

void h747_usb_dev_loader_init(void) {
    reset_runtime_state();
    memset((void*)&g_status, 0, sizeof(g_status));
    g_status.init_called = 1U;

    g_status.usbd_init_status = (int32_t)USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    g_status.pcd_init_status = usb_otg_fs_pcd_init_status();
    if (g_status.usbd_init_status != (int32_t)USBD_OK) {
        return;
    }

    usb_device_soft_disconnect(1U);

    g_status.register_class_status = (int32_t)USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    if (g_status.register_class_status != (int32_t)USBD_OK) {
        return;
    }

    g_status.register_interface_status = (int32_t)USBD_CDC_RegisterInterface(&hUsbDeviceFS, &g_cdc_interface);
    if (g_status.register_interface_status != (int32_t)USBD_OK) {
        return;
    }

    g_status.usbd_start_status = (int32_t)USBD_Start(&hUsbDeviceFS);
    if (g_status.usbd_start_status != (int32_t)USBD_OK) {
        return;
    }

    HAL_PWREx_EnableUSBVoltageDetector();
    g_status.vbus_detector_enabled = 1U;
    HAL_Delay(20U);
    usb_device_soft_disconnect(0U);
    g_status.started = 1U;
}

void h747_usb_dev_loader_poll_irq(void) {
    if (usb_otg_fs_pcd_ready() == 0U || hpcd_USB_OTG_FS.Instance == NULL) {
        return;
    }
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

void h747_usb_dev_loader_stop(void) {
    const uint8_t pcd_accessible =
        (usb_otg_fs_pcd_ready() != 0U && hpcd_USB_OTG_FS.Instance != NULL) ? 1U : 0U;

    if (g_status.init_called == 0U) {
        reset_runtime_state();
        g_status.started = 0U;
        g_status.cdc_ready = 0U;
        return;
    }

    if (pcd_accessible != 0U) {
        usb_device_soft_disconnect(1U);
    }
    if (g_status.started != 0U && pcd_accessible != 0U) {
        (void)USBD_Stop(&hUsbDeviceFS);
    }
    if (pcd_accessible != 0U) {
        (void)USBD_DeInit(&hUsbDeviceFS);
    }
    usb_otg_fs_pcd_mark_stopped();
    reset_runtime_state();
    g_status.started = 0U;
    g_status.cdc_ready = 0U;
    g_status.init_called = 0U;
}

size_t h747_usb_dev_loader_read(uint8_t* output, size_t capacity) {
    size_t count = 0U;
    if (output == NULL || capacity == 0U) {
        return 0U;
    }
    while (count < capacity && ring_pop(&output[count]) != 0U) {
        ++count;
    }
    g_status.bytes_read += (uint32_t)count;
    return count;
}

h747_usb_dev_loader_status_t h747_usb_dev_loader_status(void) {
    h747_usb_dev_loader_status_t status = g_status;

    status.pcd_init_status = usb_otg_fs_pcd_init_status();
    status.pcd_ready = usb_otg_fs_pcd_ready();
    status.setup_count = usb_setup_count();
    status.reset_count = usb_reset_count();
    status.suspend_count = usb_suspend_count();
    status.resume_count = usb_resume_count();
    status.connect_count = usb_connect_count();
    status.disconnect_count = usb_disconnect_count();
    status.out_ep0_hits = usb_out_ep_hits(0U);
    status.in_ep0_hits = usb_in_ep_hits(0U);
    status.out_ep1_hits = usb_out_ep_hits(1U);
    status.in_ep1_hits = usb_in_ep_hits(1U);
    status.last_setup_valid = usb_last_setup_valid();
    usb_copy_last_setup(status.last_setup);

    if (status.pcd_ready != 0U && hpcd_USB_OTG_FS.Instance != NULL) {
        const USB_OTG_GlobalTypeDef* usb = USB_OTG_FS;
        const USB_OTG_DeviceTypeDef* usb_dev =
            (const USB_OTG_DeviceTypeDef*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
        const USB_OTG_INEndpointTypeDef* in0 =
            (const USB_OTG_INEndpointTypeDef*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE);
        const USB_OTG_OUTEndpointTypeDef* out0 =
            (const USB_OTG_OUTEndpointTypeDef*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE);
        status.gusbcfg = (uint32_t)usb->GUSBCFG;
        status.gahbcfg = (uint32_t)usb->GAHBCFG;
        status.gintsts = (uint32_t)usb->GINTSTS;
        status.gintmsk = (uint32_t)usb->GINTMSK;
        status.dctl = (uint32_t)usb_dev->DCTL;
        status.dsts = (uint32_t)usb_dev->DSTS;
        status.gotgctl = (uint32_t)usb->GOTGCTL;
        status.gccfg = (uint32_t)usb->GCCFG;
        status.diepctl0 = (uint32_t)in0->DIEPCTL;
        status.diepint0 = (uint32_t)in0->DIEPINT;
        status.doepctl0 = (uint32_t)out0->DOEPCTL;
        status.doepint0 = (uint32_t)out0->DOEPINT;
    }

    if ((hUsbDeviceFS.pDesc != NULL) && (hUsbDeviceFS.pDesc->GetDeviceDescriptor != NULL)) {
        uint16_t len = 0U;
        const uint8_t* desc = hUsbDeviceFS.pDesc->GetDeviceDescriptor(hUsbDeviceFS.dev_speed, &len);
        status.dev_desc_len = (uint8_t)((len > 255U) ? 255U : len);
        copy_prefix(status.dev_desc_prefix, &status.dev_desc_prefix_len, sizeof(status.dev_desc_prefix), desc, len);
    }

    if ((hUsbDeviceFS.pClass[0] != NULL) && (hUsbDeviceFS.pClass[0]->GetFSConfigDescriptor != NULL)) {
        uint16_t len = 0U;
        const uint8_t* desc = hUsbDeviceFS.pClass[0]->GetFSConfigDescriptor(&len);
        status.cfg_desc_len = (uint8_t)((len > 255U) ? 255U : len);
        copy_prefix(status.cfg_desc_prefix, &status.cfg_desc_prefix_len, sizeof(status.cfg_desc_prefix), desc, len);
    }
    return status;
}

static void copy_prefix(uint8_t* dst, uint8_t* dst_len, uint32_t dst_capacity, const uint8_t* src, uint16_t src_len) {
    if (dst == NULL || dst_len == NULL || src == NULL || src_len == 0U || dst_capacity == 0U) {
        if (dst_len != NULL) {
            *dst_len = 0U;
        }
        return;
    }
    const uint32_t copy_len = (src_len < dst_capacity) ? src_len : dst_capacity;
    memcpy(dst, src, copy_len);
    *dst_len = (uint8_t)copy_len;
}

static int8_t cdc_init(void) {
    reset_runtime_state();
    g_status.cdc_ready = 1U;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, g_cdc_rx_packet);
    (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (int8_t)USBD_OK;
}

static int8_t cdc_deinit(void) {
    g_status.cdc_ready = 0U;
    return (int8_t)USBD_OK;
}

static int8_t cdc_control(uint8_t cmd, uint8_t* pbuf, uint16_t length) {
    (void)pbuf;
    ++g_status.control_requests;
    g_status.last_control_cmd = cmd;
    g_status.last_control_length = length;
    return (int8_t)USBD_OK;
}

static int8_t cdc_receive(uint8_t* Buf, uint32_t* Len) {
    if (Buf == NULL || Len == NULL) {
        return (int8_t)USBD_FAIL;
    }

    ++g_status.rx_packets;
    g_status.rx_bytes += *Len;
    for (uint32_t i = 0U; i < *Len; ++i) {
        if (ring_push(Buf[i]) == 0U) {
            ++g_status.rx_dropped_bytes;
            g_status.rx_overflow_count = g_status.rx_overflow_count + 1U;
        }
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, g_cdc_rx_packet);
    (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (int8_t)USBD_OK;
}

static int8_t cdc_transmit_complete(uint8_t* Buf, uint32_t* Len, uint8_t epnum) {
    (void)Buf;
    (void)Len;
    (void)epnum;
    return (int8_t)USBD_OK;
}

static void usb_device_soft_disconnect(uint8_t disconnected) {
    USB_OTG_DeviceTypeDef* usb_dev =
        (USB_OTG_DeviceTypeDef*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
    if (disconnected != 0U) {
        usb_dev->DCTL |= USB_OTG_DCTL_SDIS;
    } else {
        usb_dev->DCTL &= ~USB_OTG_DCTL_SDIS;
    }
}
