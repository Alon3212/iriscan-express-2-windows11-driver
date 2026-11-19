/*
 * libusb_dyn.h - dynamic loading of libusb-1.0.dll (32-bit)
 *
 * The Data Source does not link against libusb-1.0.lib: it loads the DLL at
 * runtime from the .ds directory. This lets the installer place
 * libusb-1.0.dll next to ColorUSBScanner.ds without touching SysWOW64.
 */
#ifndef GT68XX_LIBUSB_DYN_H
#define GT68XX_LIBUSB_DYN_H

#include <windows.h>
#include "libusb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libusb_api {
    HMODULE module;

    int  (LIBUSB_CALL *init)(libusb_context **ctx);
    void (LIBUSB_CALL *exit)(libusb_context *ctx);
    ssize_t (LIBUSB_CALL *get_device_list)(libusb_context *ctx, libusb_device ***list);
    void (LIBUSB_CALL *free_device_list)(libusb_device **list, int unref_devices);
    int  (LIBUSB_CALL *get_device_descriptor)(libusb_device *dev,
                                              struct libusb_device_descriptor *desc);
    int  (LIBUSB_CALL *open)(libusb_device *dev, libusb_device_handle **handle);
    void (LIBUSB_CALL *close)(libusb_device_handle *handle);
    int  (LIBUSB_CALL *claim_interface)(libusb_device_handle *handle, int interface_number);
    int  (LIBUSB_CALL *release_interface)(libusb_device_handle *handle, int interface_number);
    int  (LIBUSB_CALL *set_configuration)(libusb_device_handle *handle, int configuration);
    int  (LIBUSB_CALL *reset_device)(libusb_device_handle *handle);
    int  (LIBUSB_CALL *control_transfer)(libusb_device_handle *handle,
                                         uint8_t request_type, uint8_t request,
                                         uint16_t value, uint16_t index,
                                         unsigned char *data, uint16_t length,
                                         unsigned int timeout);
    int  (LIBUSB_CALL *bulk_transfer)(libusb_device_handle *handle,
                                      unsigned char endpoint,
                                      unsigned char *data, int length,
                                      int *actual_length, unsigned int timeout);
    int  (LIBUSB_CALL *get_active_config_descriptor)(libusb_device *dev,
                                                     struct libusb_config_descriptor **config);
    void (LIBUSB_CALL *free_config_descriptor)(struct libusb_config_descriptor *config);

    /* Optional depending on the libusb version. */
    const char *(LIBUSB_CALL *error_name)(int errcode);
    int  (LIBUSB_CALL *set_auto_detach_kernel_driver)(libusb_device_handle *handle, int enable);
} libusb_api;

extern libusb_api g_libusb;

/* 0 = OK, -1 = error (see gt68xx_libusb_last_error). */
int  gt68xx_libusb_load(void);
void gt68xx_libusb_unload(void);
const char *gt68xx_libusb_last_error(void);
const char *gt68xx_libusb_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif /* GT68XX_LIBUSB_DYN_H */
