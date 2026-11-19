/*
 * libusb_dyn.c - dynamic loading of libusb-1.0.dll
 */
#include "libusb_dyn.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

libusb_api g_libusb;

static char g_last_error[512];

static void set_last_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error) - 1, fmt, ap);
    g_last_error[sizeof(g_last_error) - 1] = 0;
    va_end(ap);
}

const char *gt68xx_libusb_last_error(void)
{
    return g_last_error;
}

const char *gt68xx_libusb_error_string(int code)
{
    if (g_libusb.error_name) {
        return g_libusb.error_name(code);
    }
    static char buf[64];
    snprintf(buf, sizeof(buf) - 1, "libusb error %d", code);
    buf[sizeof(buf) - 1] = 0;
    return buf;
}

/* Returns the .ds directory (without trailing separator). */
static int get_module_dir(char *dir, size_t dir_size)
{
    HMODULE mod = NULL;
    DWORD len;

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)(const void *)&gt68xx_libusb_load,
                            &mod)) {
        return -1;
    }

    len = GetModuleFileNameA(mod, dir, (DWORD)dir_size);
    if (len == 0 || len >= dir_size) {
        return -1;
    }

    while (len > 0 && dir[len - 1] != '\\' && dir[len - 1] != '/') {
        len--;
    }
    if (len > 0) {
        dir[len - 1] = 0;
    }
    return 0;
}

#define LOAD_REQUIRED(field, name)                                              \
    do {                                                                        \
        *(FARPROC *)&g_libusb.field = GetProcAddress(mod, name);                 \
        if (!g_libusb.field) {                                                   \
            set_last_error("libusb-1.0.dll: missing symbol %s", name);           \
            goto fail;                                                           \
        }                                                                        \
    } while (0)

#define LOAD_OPTIONAL(field, name)                                              \
    do {                                                                        \
        *(FARPROC *)&g_libusb.field = GetProcAddress(mod, name);                 \
    } while (0)

int gt68xx_libusb_load(void)
{
    char dir[MAX_PATH];
    char path[MAX_PATH];
    HMODULE mod;

    if (g_libusb.module) {
        return 0;
    }

    memset(&g_libusb, 0, sizeof(g_libusb));

    if (get_module_dir(dir, sizeof(dir)) == 0) {
        snprintf(path, sizeof(path) - 1, "%s\\libusb-1.0.dll", dir);
        path[sizeof(path) - 1] = 0;
        mod = LoadLibraryA(path);
    } else {
        mod = NULL;
    }

    if (!mod) {
        mod = LoadLibraryA("libusb-1.0.dll");
    }
    if (!mod) {
        set_last_error("Could not load libusb-1.0.dll (error %lu). "
                       "Copy the 32-bit DLL next to ColorUSBScanner.ds.",
                       GetLastError());
        return -1;
    }

    g_libusb.module = mod;

    LOAD_REQUIRED(init, "libusb_init");
    LOAD_REQUIRED(exit, "libusb_exit");
    LOAD_REQUIRED(get_device_list, "libusb_get_device_list");
    LOAD_REQUIRED(free_device_list, "libusb_free_device_list");
    LOAD_REQUIRED(get_device_descriptor, "libusb_get_device_descriptor");
    LOAD_REQUIRED(open, "libusb_open");
    LOAD_REQUIRED(close, "libusb_close");
    LOAD_REQUIRED(claim_interface, "libusb_claim_interface");
    LOAD_REQUIRED(release_interface, "libusb_release_interface");
    LOAD_REQUIRED(set_configuration, "libusb_set_configuration");
    LOAD_REQUIRED(reset_device, "libusb_reset_device");
    LOAD_REQUIRED(control_transfer, "libusb_control_transfer");
    LOAD_REQUIRED(bulk_transfer, "libusb_bulk_transfer");
    LOAD_REQUIRED(get_active_config_descriptor, "libusb_get_active_config_descriptor");
    LOAD_REQUIRED(free_config_descriptor, "libusb_free_config_descriptor");

    LOAD_OPTIONAL(error_name, "libusb_error_name");
    LOAD_OPTIONAL(set_auto_detach_kernel_driver, "libusb_set_auto_detach_kernel_driver");

    return 0;

fail:
    FreeLibrary(mod);
    memset(&g_libusb, 0, sizeof(g_libusb));
    return -1;
}

void gt68xx_libusb_unload(void)
{
    if (g_libusb.module) {
        FreeLibrary(g_libusb.module);
    }
    memset(&g_libusb, 0, sizeof(g_libusb));
}
