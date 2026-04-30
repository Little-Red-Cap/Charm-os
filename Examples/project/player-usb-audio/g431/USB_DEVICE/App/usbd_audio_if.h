#ifndef __USBD_AUDIO_IF_H__
#define __USBD_AUDIO_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_audio.h"

extern USBD_AUDIO_ItfTypeDef USBD_AUDIO_fops_FS;

void TransferComplete_CallBack_FS(void);
void HalfTransfer_CallBack_FS(void);

uint32_t usb_audio_rx_bytes(void);
uint32_t usb_audio_rx_pkts(void);
uint32_t usb_audio_rx_last_size(void);
uint32_t usb_audio_rx_overflows(void);
uint32_t usb_audio_rx_freq(void);
uint32_t usb_audio_rx_cmd(void);
uint32_t usb_audio_rx_init_calls(void);
uint32_t usb_audio_rx_cmd_calls(void);
void usb_audio_rx_reset(void);
uint32_t usb_audio_ring_available(void);
uint32_t usb_audio_ring_overflows(void);
uint32_t usb_audio_ring_high_watermark(void);
uint32_t usb_audio_ring_dropped_bytes(void);
uint32_t usb_audio_ring_read(uint8_t* dst, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
