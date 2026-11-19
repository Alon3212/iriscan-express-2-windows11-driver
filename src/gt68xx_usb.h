/*
 * gt68xx_usb.h - capa USB mínima para el GT-6816 (Plustek OpticSlim M12)
 */
#ifndef GT68XX_USB_H
#define GT68XX_USB_H

#include <stdint.h>
#include "libusb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GT68XX_VID 0x07B3
#define GT68XX_PID 0x0412

typedef struct gt68xx_device {
    libusb_context       *ctx;
    libusb_device_handle *handle;
    uint8_t               ep_in;       /* bulk IN  */
    uint8_t               ep_out;      /* bulk OUT */
    uint8_t               ep_int;      /* interrupt IN, 0 si no hay */
    int                   interface_number;
    int                   configuration;
    int                   opened;
} gt68xx_device;

/* 0 = OK, <0 = error. */
int  gt68xx_usb_open(gt68xx_device **out);
void gt68xx_usb_close(gt68xx_device *dev);
int  gt68xx_usb_reset(gt68xx_device *dev);

int  gt68xx_usb_control(gt68xx_device *dev,
                        uint8_t request_type, uint8_t request,
                        uint16_t value, uint16_t index,
                        uint8_t *data, uint16_t length,
                        unsigned int timeout_ms);

int  gt68xx_usb_bulk_write(gt68xx_device *dev, const uint8_t *data, int length,
                           unsigned int timeout_ms);
int  gt68xx_usb_bulk_read(gt68xx_device *dev, uint8_t *data, int length,
                          unsigned int timeout_ms);

const char *gt68xx_usb_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* GT68XX_USB_H */
