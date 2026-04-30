#include "usbd_core.h"
#include "usbd_desc.h"

#define USBD_VID                 0xCAFEU
#define USBD_PID_FS_AUDIO        0x4311U
#define USBD_LANGID_STRING       1033U
#define USBD_MANUFACTURER_STRING "Charm"
#define USBD_PRODUCT_STRING      "Player USB Audio"
#define USBD_CONFIGURATION_STRING "Audio Config"
#define USBD_INTERFACE_STRING    "Audio Interface"

static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];

static void Get_SerialNum(void);
static void IntToUnicode(uint32_t value, uint8_t* pbuf, uint8_t len);

static uint8_t* USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);
static uint8_t* USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length);

USBD_DescriptorsTypeDef FS_Desc = {
    USBD_FS_DeviceDescriptor,
    USBD_FS_LangIDStrDescriptor,
    USBD_FS_ManufacturerStrDescriptor,
    USBD_FS_ProductStrDescriptor,
    USBD_FS_SerialStrDescriptor,
    USBD_FS_ConfigStrDescriptor,
    USBD_FS_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,
    USB_DESC_TYPE_DEVICE,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    USB_MAX_EP0_SIZE,
    LOBYTE(USBD_VID),
    HIBYTE(USBD_VID),
    LOBYTE(USBD_PID_FS_AUDIO),
    HIBYTE(USBD_PID_FS_AUDIO),
    0x00,
    0x01,
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION,
};

__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

static uint8_t* USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t* length) {
    (void)speed;
    *length = sizeof(USBD_FS_DeviceDesc);
    return USBD_FS_DeviceDesc;
}

static uint8_t* USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length) {
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t* USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length) {
    (void)speed;
    USBD_GetString((uint8_t*)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t* USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length) {
    (void)speed;
    USBD_GetString((uint8_t*)USBD_PRODUCT_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t* USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length) {
    (void)speed;
    *length = USB_SIZ_STRING_SERIAL;
    Get_SerialNum();
    return USBD_StrDesc;
}

static uint8_t* USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length) {
    (void)speed;
    USBD_GetString((uint8_t*)USBD_CONFIGURATION_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t* USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length) {
    (void)speed;
    USBD_GetString((uint8_t*)USBD_INTERFACE_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static void Get_SerialNum(void) {
    uint32_t deviceserial0 = *(uint32_t*)DEVICE_ID1;
    uint32_t deviceserial1 = *(uint32_t*)DEVICE_ID2;
    uint32_t deviceserial2 = *(uint32_t*)DEVICE_ID3;

    deviceserial0 += deviceserial2;

    if (deviceserial0 != 0U) {
        IntToUnicode(deviceserial0, &USBD_StrDesc[2], 8);
        IntToUnicode(deviceserial1, &USBD_StrDesc[18], 4);
    }
}

static void IntToUnicode(uint32_t value, uint8_t* pbuf, uint8_t len) {
    for (uint8_t idx = 0; idx < len; idx++) {
        if (((value >> 28U)) < 0xAU) {
            pbuf[2U * idx] = (value >> 28U) + '0';
        } else {
            pbuf[2U * idx] = (value >> 28U) + 'A' - 10U;
        }
        value <<= 4U;
        pbuf[2U * idx + 1U] = 0U;
    }

    USBD_StrDesc[0] = USB_SIZ_STRING_SERIAL;
    USBD_StrDesc[1] = USB_DESC_TYPE_STRING;
}
