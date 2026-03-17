#include <stdint.h>

__attribute__((weak)) void app_usb_setup_sniff(const uint8_t setup[8]) {
    (void)setup;
}

__attribute__((weak)) void charm_audio_dma_irq_notify(void) {
}

__attribute__((weak)) void usbd_msc_debug_cbw(uint32_t sig, uint32_t data_len,
                                              uint8_t flags, uint8_t cb_len,
                                              uint8_t opcode) {
    (void)sig;
    (void)data_len;
    (void)flags;
    (void)cb_len;
    (void)opcode;
}

__attribute__((weak)) void usbd_msc_debug_cdb(const uint8_t *cb, uint8_t cb_len) {
    (void)cb;
    (void)cb_len;
}

__attribute__((weak)) void usbd_msc_debug_send(uint32_t kind, uint32_t len) {
    (void)kind;
    (void)len;
}

__attribute__((weak)) void usbd_msc_debug_bot_state(uint32_t kind, uint32_t state, uint8_t epnum) {
    (void)kind;
    (void)state;
    (void)epnum;
}
