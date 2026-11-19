/*
 * gt68xx_usb.c - device open/claim and USB transport.
 *
 * Windows requirement: the scanner must be bound to WinUSB/libusbK
 * (for example with Zadig), not to usbscan.sys. If it is bound to the
 * Plustek driver, libusb_open will fail with ACCESS/NOT_SUPPORTED.
 */
#include "gt68xx_usb.h"
#include "libusb_dyn.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_last_error[512];

static void set_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error) - 1, fmt, ap);
    g_last_error[sizeof(g_last_error) - 1] = 0;
    va_end(ap);
}

const char *gt68xx_usb_last_error(void)
{
    return g_last_error;
}

static int find_endpoints(gt68xx_device *dev, libusb_device *device)
{
    struct libusb_config_descriptor *config = NULL;
    int i, a, e;

    if (g_libusb.get_active_config_descriptor(device, &config) != 0 || !config) {
        set_error("Could not read the active USB configuration");
        return -1;
    }

    for (i = 0; i < config->bNumInterfaces; i++) {
        const struct libusb_interface *iface = &config->interface[i];
        for (a = 0; a < iface->num_altsetting; a++) {
            const struct libusb_interface_descriptor *alt = &iface->altsetting[a];
            if (alt->bInterfaceNumber != 0) {
                continue;
            }
            for (e = 0; e < alt->bNumEndpoints; e++) {
                const struct libusb_endpoint_descriptor *ep = &alt->endpoint[e];
                uint8_t type = (uint8_t)(ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK);

                if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
                    if (type == LIBUSB_TRANSFER_TYPE_BULK && dev->ep_in == 0) {
                        dev->ep_in = ep->bEndpointAddress;
                    } else if (type == LIBUSB_TRANSFER_TYPE_INTERRUPT && dev->ep_int == 0) {
                        dev->ep_int = ep->bEndpointAddress;
                    }
                } else if (type == LIBUSB_TRANSFER_TYPE_BULK && dev->ep_out == 0) {
                    dev->ep_out = ep->bEndpointAddress;
                }
            }
        }
    }

    g_libusb.free_config_descriptor(config);

    if (dev->ep_in == 0 || dev->ep_out == 0) {
        set_error("The device does not expose bulk IN/OUT endpoints on interface 0 "
                  "(ep_in=0x%02X ep_out=0x%02X)",
                  dev->ep_in, dev->ep_out);
        return -1;
    }
    return 0;
}

int gt68xx_usb_open(gt68xx_device **out)
{
    libusb_context *ctx = NULL;
    libusb_device **list = NULL;
    ssize_t count;
    ssize_t i;
    int result = -1;
    int last_libusb_error = 0;

    if (!out) {
        return -1;
    }
    *out = NULL;

    if (gt68xx_libusb_load() != 0) {
        set_error("%s", gt68xx_libusb_last_error());
        return -1;
    }

    if (g_libusb.init(&ctx) != 0) {
        set_error("libusb_init failed");
        return -1;
    }

    count = g_libusb.get_device_list(ctx, &list);
    if (count < 0) {
        set_error("libusb_get_device_list failed: %s",
                  gt68xx_libusb_error_string((int)count));
        g_libusb.exit(ctx);
        return -1;
    }

    for (i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;
        libusb_device_handle *handle = NULL;
        gt68xx_device *dev = NULL;

        if (g_libusb.get_device_descriptor(list[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != GT68XX_VID || desc.idProduct != GT68XX_PID) {
            continue;
        }

        if (g_libusb.open(list[i], &handle) != 0) {
            last_libusb_error = LIBUSB_ERROR_ACCESS;
            continue;
        }

        if (g_libusb.set_auto_detach_kernel_driver) {
            g_libusb.set_auto_detach_kernel_driver(handle, 1);
        }

        /* Reset may return NOT_FOUND if the device re-enumerates. */
        (void)g_libusb.reset_device(handle);

        /* Not fatal if the device is already configured. */
        (void)g_libusb.set_configuration(handle, 1);

        if (g_libusb.claim_interface(handle, 0) != 0) {
            last_libusb_error = LIBUSB_ERROR_BUSY;
            g_libusb.close(handle);
            continue;
        }

        dev = (gt68xx_device *)calloc(1, sizeof(*dev));
        if (!dev) {
            g_libusb.release_interface(handle, 0);
            g_libusb.close(handle);
            set_error("Out of memory for gt68xx_device");
            break;
        }

        dev->ctx = ctx;
        dev->handle = handle;
        dev->interface_number = 0;
        dev->configuration = 1;
        dev->opened = 1;

        if (find_endpoints(dev, list[i]) != 0) {
            g_libusb.release_interface(handle, 0);
            g_libusb.close(handle);
            free(dev);
            continue;
        }

        *out = dev;
        result = 0;
        break;
    }

    g_libusb.free_device_list(list, 1);

    if (result != 0) {
        if (last_libusb_error == LIBUSB_ERROR_ACCESS) {
            set_error("Could not open 07B3:0412. Check that the scanner is "
                      "bound to WinUSB/libusbK with Zadig, not to usbscan.sys.");
        } else if (last_libusb_error == LIBUSB_ERROR_BUSY) {
            set_error("Device 07B3:0412 is busy or interface 0 could not be "
                      "claimed.");
        } else {
            set_error("USB scanner 07B3:0412 was not found.");
        }
        g_libusb.exit(ctx);
    }

    return result;
}

void gt68xx_usb_close(gt68xx_device *dev)
{
    if (!dev) {
        return;
    }
    if (dev->opened && dev->handle) {
        g_libusb.release_interface(dev->handle, dev->interface_number);
        g_libusb.close(dev->handle);
    }
    if (dev->ctx) {
        g_libusb.exit(dev->ctx);
    }
    free(dev);
}

int gt68xx_usb_reset(gt68xx_device *dev)
{
    int r;
    if (!dev || !dev->handle) {
        return -1;
    }
    r = g_libusb.reset_device(dev->handle);
    if (r != 0) {
        set_error("libusb_reset_device failed: %s", gt68xx_libusb_error_string(r));
        return r;
    }
    return 0;
}

int gt68xx_usb_control(gt68xx_device *dev,
                       uint8_t request_type, uint8_t request,
                       uint16_t value, uint16_t index,
                       uint8_t *data, uint16_t length,
                       unsigned int timeout_ms)
{
    int r;
    if (!dev || !dev->handle) {
        return -1;
    }
    r = g_libusb.control_transfer(dev->handle, request_type, request,
                                  value, index, data, length, timeout_ms);
    if (r < 0) {
        set_error("control_transfer failed: %s", gt68xx_libusb_error_string(r));
    }
    return r;
}

int gt68xx_usb_bulk_write(gt68xx_device *dev, const uint8_t *data, int length,
                          unsigned int timeout_ms)
{
    int transferred = 0;
    int r;

    if (!dev || !dev->handle || dev->ep_out == 0) {
        return -1;
    }
    r = g_libusb.bulk_transfer(dev->handle, dev->ep_out,
                               (unsigned char *)data, length,
                               &transferred, timeout_ms);
    if (r < 0) {
        set_error("bulk_write failed: %s", gt68xx_libusb_error_string(r));
        return r;
    }
    return transferred;
}

int gt68xx_usb_bulk_read(gt68xx_device *dev, uint8_t *data, int length,
                         unsigned int timeout_ms)
{
    int transferred = 0;
    int r;

    if (!dev || !dev->handle || dev->ep_in == 0) {
        return -1;
    }
    r = g_libusb.bulk_transfer(dev->handle, dev->ep_in,
                               data, length, &transferred, timeout_ms);
    if (r < 0) {
        set_error("bulk_read failed: %s", gt68xx_libusb_error_string(r));
        return r;
    }
    return transferred;
}
