/**
 * @file usb_descriptors.c
 * @brief Dual CDC programmer and UART bridge support.
 * @copyright Copyright © 2026 MTA, Inc.
 */
#include "tusb.h"
#include "pico/unique_id.h"
#include <string.h>

// Development identity, distinct from the former single-CDC Pico SDK identity.
static const tusb_desc_device_t device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = 64, .idVendor = 0x1209, .idProduct = 0x7636,
    .bcdDevice = 0x0100, .iManufacturer = 1, .iProduct = 2,
    .iSerialNumber = 3, .bNumConfigurations = 1,
};

static const uint8_t configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 4, 0, TUD_CONFIG_DESC_LEN + 2 * TUD_CDC_DESC_LEN, 0, 100),
    TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64),
    TUD_CDC_DESCRIPTOR(2, 5, 0x83, 8, 0x04, 0x84, 64),
};

/**
 * @brief Return the immutable dual-CDC device descriptor.
 */
const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&device;
}

/**
 * @brief Return the single supported configuration, or reject an invalid index.
 */
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    return index == 0 ? configuration : NULL;
}

/**
 * @brief Build a bounded UTF-16 descriptor using the physical Pico unique serial.
 */
const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t language) {
    (void)language;
    static uint16_t result[64];
    static char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    const char *strings[] = {"", "T76", "T76 Pico Programmer + UART", serial,
                             "Ardulink Programmer", "CH32 UART Bridge"};
    if (index == 0) {
        result[0] = (TUSB_DESC_STRING << 8) | 4;
        result[1] = 0x0409;
        return result;
    }
    if (index >= sizeof(strings) / sizeof(strings[0])) return NULL;
    if (index == 3) pico_get_unique_board_id_string(serial, sizeof(serial));
    size_t size = strlen(strings[index]);
    if (size > 63) size = 63;
    for (size_t i = 0; i < size; ++i) result[i + 1] = (uint8_t)strings[index][i];
    result[0] = (TUSB_DESC_STRING << 8) | (2 * size + 2);
    return result;
}
