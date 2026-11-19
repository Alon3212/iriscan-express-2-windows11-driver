/*
 * twain_ds.c - minimal TWAIN Data Source for the Plustek OpticSlim M12
 * (IRIScan Express 2), GT-6816 chipset, VID 07B3 PID 0412.
 *
 * Implements:
 *   - TWAIN identity and open/close.
 *   - Basic capabilities (resolution, pixel type, bit depth, feeder).
 *   - Memory image transfer (DAT_IMAGEMEMXFER).
 *   - Synthetic test image when COLORUSBSCANNER_TEST_PATTERN=1.
 *
 * Real scanner reading is in gt68xx_proto.c: firmware, motor and bulk read
 * ported from the SANE gt68xx backend.
 */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "twain.h"
#include "gt68xx_usb.h"
#include "gt68xx_proto.h"

#ifndef TWRC_NOTDSEVENT
#define TWRC_NOTDSEVENT 5
#endif

#define DS_VERSION_STRING "1.0.0"
#define DS_TEST_ENV       "COLORUSBSCANNER_TEST_PATTERN"

/* TWAIN 2.x: DS entry point in the DSM. The 1.9 header does not define it. */
#define GT68XX_DF_DS2          0x40000000UL
#define GT68XX_DAT_ENTRYPOINT  0x0403

typedef struct gt68xx_entrypoint {
    TW_UINT32 Size;
    DSMENTRYPROC DSM_Entry;
    TW_HANDLE (FAR PASCAL *DSM_MemAllocate)(TW_UINT32 size);
    void (FAR PASCAL *DSM_MemFree)(TW_HANDLE handle);
    TW_MEMREF (FAR PASCAL *DSM_MemLock)(TW_HANDLE handle);
    void (FAR PASCAL *DSM_MemUnlock)(TW_HANDLE handle);
} GT68XX_ENTRYPOINT;

static TW_IDENTITY  g_identity;
static TW_IDENTITY  g_app_identity;
static BOOL         g_have_app = FALSE;
static DSMENTRYPROC g_dsm_entry = NULL;
static HANDLE       g_xfer_thread = NULL;
static GT68XX_ENTRYPOINT g_entrypoint;

static TW_UINT16    g_condition = TWCC_SUCCESS;
static TW_UINT16    g_state = 3;       /* 3=DSM open, 4=DS open, 5=enabled */
static BOOL         g_open = FALSE;
static BOOL         g_enabled = FALSE;
static BOOL         g_test_mode = FALSE;
static BOOL         g_page_pending = FALSE;

static gt68xx_device *g_dev = NULL;
static gt68xx_image   g_image;
static BOOL           g_image_valid = FALSE;
static size_t         g_image_offset = 0;
static HGLOBAL        g_ds_buffer = NULL;

static int g_dpi = 300;
static int g_pixel_type = TWPT_RGB;
static int g_bit_depth = 8;
static int g_xfer_count = 1;

/* ------------------------------------------------------------------------- */
/* Utilities                                                                 */
/* ------------------------------------------------------------------------- */

static void set_condition(TW_UINT16 cc)
{
    g_condition = cc;
}

static int debug_enabled(void)
{
    static int enabled = -1;
    char value[16];
    if (enabled < 0) {
        DWORD n = GetEnvironmentVariableA("COLORUSBSCANNER_DEBUG", value, (DWORD)sizeof(value));
        enabled = (n > 0 && n < sizeof(value) && atoi(value) != 0) ? 1 : 0;
    }
    return enabled;
}

static void ds_log(const char *fmt, ...)
{
    char path[MAX_PATH];
    DWORD n;
    FILE *f;
    va_list ap;

    if (!debug_enabled()) {
        return;
    }
    n = GetTempPathA((DWORD)sizeof(path), path);
    if (n == 0 || n >= sizeof(path)) {
        return;
    }
    if (strlen(path) + strlen("ColorUSBScanner.log") + 1 >= sizeof(path)) {
        return;
    }
    strcat(path, "ColorUSBScanner.log");

    f = fopen(path, "a");
    if (!f) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void copy_str32(char *dst, const char *src)
{
    size_t n = sizeof(TW_STR32) - 1;
    if (!dst || !src) {
        return;
    }
    strncpy(dst, src, n);
    dst[n] = 0;
}

static void init_identity(void)
{
    memset(&g_identity, 0, sizeof(g_identity));
    g_identity.Id = 0;
    g_identity.Version.MajorNum = 1;
    g_identity.Version.MinorNum = 0;
    g_identity.Version.Language = 0x0009; /* English */
    g_identity.Version.Country = 0x0001;  /* USA */
    copy_str32(g_identity.Version.Info, DS_VERSION_STRING);
    g_identity.ProtocolMajor = 2;
    g_identity.ProtocolMinor = 0;
    g_identity.SupportedGroups = DG_IMAGE | DG_CONTROL | GT68XX_DF_DS2;
    copy_str32(g_identity.Manufacturer, "OpenSource GT68xx");
    copy_str32(g_identity.ProductFamily, "Plustek GT-6816");
    copy_str32(g_identity.ProductName, "Color USB Scanner");
}

static int env_flag(const char *name)
{
    char value[32];
    DWORD n = GetEnvironmentVariableA(name, value, (DWORD)sizeof(value));
    if (n == 0 || n >= sizeof(value)) {
        return 0;
    }
    return atoi(value) != 0;
}

static TW_FIX32 fix32_from_double(double v)
{
    TW_FIX32 f;
    f.Whole = (TW_INT16)v;
    f.Frac = (TW_UINT16)((v - (double)f.Whole) * 65536.0 + 0.5);
    return f;
}

static TW_UINT32 fix32_to_bits(TW_FIX32 f)
{
    TW_UINT32 u = 0;
    memcpy(&u, &f, sizeof(u));
    return u;
}

static TW_FIX32 bits_to_fix32(TW_UINT32 u)
{
    TW_FIX32 f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

static double fix32_to_double(TW_UINT32 bits)
{
    TW_FIX32 f = bits_to_fix32(bits);
    return (double)f.Whole + (double)f.Frac / 65536.0;
}

static void image_dimensions(int *w, int *h)
{
    *w = (int)((210.0 / 25.4) * (double)g_dpi + 0.5);
    *h = (int)((297.0 / 25.4) * (double)g_dpi + 0.5);
    if (*w < 1) {
        *w = 1;
    }
    if (*h < 1) {
        *h = 1;
    }
}

static void cleanup_transfer(void)
{
    if (g_image_valid) {
        gt68xx_image_free(&g_image);
        g_image_valid = FALSE;
    }
    g_image_offset = 0;
    if (g_ds_buffer) {
        GlobalFree(g_ds_buffer);
        g_ds_buffer = NULL;
    }
}

static void reset_caps(void)
{
    g_dpi = 300;
    g_pixel_type = TWPT_RGB;
    g_bit_depth = 8;
    g_xfer_count = 1;
    cleanup_transfer();
}

/* ------------------------------------------------------------------------- */
/* MSG_XFERREADY notification to the DSM                                     */
/* ------------------------------------------------------------------------- */

static void load_dsm_entry(void)
{
    static const char *modules[] = {
        "twaindsm.dll", "TWAINDSM.DLL",
        "twain_32.dll", "TWAIN_32.DLL",
        NULL
    };
    int i;

    if (g_dsm_entry) {
        return;
    }
    if (g_entrypoint.DSM_Entry) {
        g_dsm_entry = g_entrypoint.DSM_Entry;
        ds_log("DSM_Entry from DAT_ENTRYPOINT (%p)", (void *)g_dsm_entry);
        return;
    }
    for (i = 0; modules[i]; i++) {
        HMODULE h = GetModuleHandleA(modules[i]);
        if (!h) {
            continue;
        }
        *(FARPROC *)&g_dsm_entry = GetProcAddress(h, "DSM_Entry");
        if (!g_dsm_entry) {
            *(FARPROC *)&g_dsm_entry = GetProcAddress(h, "_DSM_Entry@20");
        }
        if (g_dsm_entry) {
            ds_log("DSM_Entry found in %s", modules[i]);
            return;
        }
    }
    ds_log("DSM_Entry NOT found");
}

static DWORD WINAPI xfer_ready_thread(LPVOID param)
{
    TW_UINT16 rc;
    (void)param;

    Sleep(100);
    if (!g_enabled || !g_have_app || !g_dsm_entry) {
        return 0;
    }

    ds_log("MSG_XFERREADY -> app.Id=0x%08lX app.Product=%s",
           (unsigned long)g_app_identity.Id, g_app_identity.ProductName);
    rc = g_dsm_entry(&g_identity, &g_app_identity, DG_CONTROL, DAT_NULL,
                     MSG_XFERREADY, NULL);
    ds_log("MSG_XFERREADY rc=0x%04X", rc);
    return 0;
}

static void notify_xfer_ready(void)
{
    if (!g_dsm_entry) {
        load_dsm_entry();
    }
    if (!g_dsm_entry || !g_have_app) {
        ds_log("cannot notify XFERREADY (dsm=%p app=%d)",
               (void *)g_dsm_entry, (int)g_have_app);
        return;
    }
    if (g_xfer_thread) {
        CloseHandle(g_xfer_thread);
        g_xfer_thread = NULL;
    }
    g_xfer_thread = CreateThread(NULL, 0, xfer_ready_thread, NULL, 0, NULL);
    if (!g_xfer_thread) {
        ds_log("CreateThread(XFERREADY) failed: %lu", GetLastError());
    }
}

/* ------------------------------------------------------------------------- */
/* Capabilities                                                              */
/* ------------------------------------------------------------------------- */

static const TW_UINT16 g_supported_caps[] = {
    CAP_XFERCOUNT,
    CAP_SUPPORTEDCAPS,
    CAP_DEVICEONLINE,
    CAP_FEEDERENABLED,
    CAP_FEEDERLOADED,
    CAP_DUPLEX,
    ICAP_XRESOLUTION,
    ICAP_YRESOLUTION,
    ICAP_PIXELTYPE,
    ICAP_BITDEPTH,
    ICAP_UNITS,
    ICAP_XFERMECH,
    ICAP_PHYSICALWIDTH,
    ICAP_PHYSICALHEIGHT
};

static int cap_is_supported(TW_UINT16 cap)
{
    size_t i;
    for (i = 0; i < sizeof(g_supported_caps) / sizeof(g_supported_caps[0]); i++) {
        if (g_supported_caps[i] == cap) {
            return 1;
        }
    }
    return 0;
}

static TW_UINT16 make_onevalue(pTW_CAPABILITY cap, TW_UINT16 type, TW_UINT32 value)
{
    HGLOBAL h;
    TW_ONEVALUE *v;

    h = GlobalAlloc(GHND | GMEM_ZEROINIT, sizeof(TW_ONEVALUE));
    if (!h) {
        return TWCC_LOWMEMORY;
    }
    v = (TW_ONEVALUE *)GlobalLock(h);
    if (!v) {
        GlobalFree(h);
        return TWCC_LOWMEMORY;
    }
    v->ItemType = type;
    v->Item = value;
    GlobalUnlock(h);
    cap->ConType = TWON_ONEVALUE;
    cap->hContainer = (TW_HANDLE)h;
    return TWCC_SUCCESS;
}

static TW_UINT16 make_range(pTW_CAPABILITY cap, TW_UINT16 type,
                            TW_UINT32 minv, TW_UINT32 maxv, TW_UINT32 step,
                            TW_UINT32 defv, TW_UINT32 current)
{
    HGLOBAL h;
    TW_RANGE *r;

    h = GlobalAlloc(GHND | GMEM_ZEROINIT, sizeof(TW_RANGE));
    if (!h) {
        return TWCC_LOWMEMORY;
    }
    r = (TW_RANGE *)GlobalLock(h);
    if (!r) {
        GlobalFree(h);
        return TWCC_LOWMEMORY;
    }
    r->ItemType = type;
    r->MinValue = minv;
    r->MaxValue = maxv;
    r->StepSize = step;
    r->DefaultValue = defv;
    r->CurrentValue = current;
    GlobalUnlock(h);
    cap->ConType = TWON_RANGE;
    cap->hContainer = (TW_HANDLE)h;
    return TWCC_SUCCESS;
}

static TW_UINT16 make_array(pTW_CAPABILITY cap, TW_UINT16 type,
                            const TW_UINT16 *items, TW_UINT32 count)
{
    HGLOBAL h;
    TW_ARRAY *a;
    size_t bytes;

    bytes = sizeof(TW_ARRAY);
    if (count > 1) {
        bytes += (size_t)(count - 1) * sizeof(TW_UINT16);
    }
    h = GlobalAlloc(GHND | GMEM_ZEROINIT, bytes);
    if (!h) {
        return TWCC_LOWMEMORY;
    }
    a = (TW_ARRAY *)GlobalLock(h);
    if (!a) {
        GlobalFree(h);
        return TWCC_LOWMEMORY;
    }
    a->ItemType = type;
    a->NumItems = count;
    if (count > 0) {
        memcpy(a->ItemList, items, (size_t)count * sizeof(TW_UINT16));
    }
    GlobalUnlock(h);
    cap->ConType = TWON_ARRAY;
    cap->hContainer = (TW_HANDLE)h;
    return TWCC_SUCCESS;
}

static TW_UINT16 cap_get(pTW_CAPABILITY cap, TW_UINT16 msg)
{
    int use_default = (msg == MSG_GETDEFAULT);
    int dpi = use_default ? 300 : g_dpi;
    int pixel_type = use_default ? TWPT_GRAY : g_pixel_type;

    if (!cap) {
        return TWCC_BADVALUE;
    }

    switch (cap->Cap) {
    case CAP_XFERCOUNT:
        return make_onevalue(cap, TWTY_INT16, (TW_UINT32)(TW_INT16)g_xfer_count);

    case CAP_SUPPORTEDCAPS:
        return make_array(cap, TWTY_UINT16, g_supported_caps,
                          (TW_UINT32)(sizeof(g_supported_caps) / sizeof(g_supported_caps[0])));

    case CAP_DEVICEONLINE:
        return make_onevalue(cap, TWTY_BOOL, 1);

    case CAP_FEEDERENABLED:
    case CAP_FEEDERLOADED:
        return make_onevalue(cap, TWTY_BOOL, 1);

    case CAP_DUPLEX:
        return make_onevalue(cap, TWTY_BOOL, 0);

    case ICAP_XRESOLUTION:
    case ICAP_YRESOLUTION:
        return make_range(cap, TWTY_FIX32,
                          fix32_to_bits(fix32_from_double(100.0)),
                          fix32_to_bits(fix32_from_double(600.0)),
                          fix32_to_bits(fix32_from_double(1.0)),
                          fix32_to_bits(fix32_from_double(300.0)),
                          fix32_to_bits(fix32_from_double((double)dpi)));

    case ICAP_PIXELTYPE:
        return make_onevalue(cap, TWTY_UINT16, (TW_UINT32)pixel_type);

    case ICAP_BITDEPTH:
        return make_onevalue(cap, TWTY_UINT16, (TW_UINT32)g_bit_depth);

    case ICAP_UNITS:
        return make_onevalue(cap, TWTY_UINT16, TWUN_INCHES);

    case ICAP_XFERMECH:
        return make_onevalue(cap, TWTY_UINT16, TWSX_MEMORY);

    case ICAP_PHYSICALWIDTH:
        return make_onevalue(cap, TWTY_FIX32, fix32_to_bits(fix32_from_double(210.0 / 25.4)));

    case ICAP_PHYSICALHEIGHT:
        return make_onevalue(cap, TWTY_FIX32, fix32_to_bits(fix32_from_double(297.0 / 25.4)));

    default:
        return TWCC_CAPUNSUPPORTED;
    }
}

static TW_UINT16 cap_set(pTW_CAPABILITY cap)
{
    HGLOBAL h;
    void *ptr;
    TW_UINT16 cc = TWCC_SUCCESS;

    if (!cap || !cap->hContainer) {
        return TWCC_BADVALUE;
    }
    h = (HGLOBAL)cap->hContainer;
    ptr = GlobalLock(h);
    if (!ptr) {
        return TWCC_LOWMEMORY;
    }

    if (cap->ConType == TWON_ONEVALUE) {
        TW_ONEVALUE *v = (TW_ONEVALUE *)ptr;
        switch (cap->Cap) {
        case CAP_XFERCOUNT:
            if (v->Item != 1) {
                cc = TWCC_BADVALUE;
            } else {
                g_xfer_count = 1;
            }
            break;

        case ICAP_XRESOLUTION:
        case ICAP_YRESOLUTION: {
            double d = fix32_to_double(v->Item);
            if (d < 100.0 || d > 600.0) {
                cc = TWCC_BADVALUE;
            } else {
                g_dpi = (int)(d + 0.5);
                cleanup_transfer();
            }
            break;
        }

        case ICAP_PIXELTYPE:
            if (v->Item != TWPT_GRAY && v->Item != TWPT_RGB) {
                cc = TWCC_BADVALUE;
            } else {
                g_pixel_type = (int)v->Item;
                cleanup_transfer();
            }
            break;

        case ICAP_BITDEPTH:
            if (v->Item != 8) {
                cc = TWCC_BADVALUE;
            } else {
                g_bit_depth = 8;
                cleanup_transfer();
            }
            break;

        case ICAP_UNITS:
            if (v->Item != TWUN_INCHES) {
                cc = TWCC_BADVALUE;
            }
            break;

        case ICAP_XFERMECH:
            if (v->Item != TWSX_MEMORY) {
                cc = TWCC_BADVALUE;
            }
            break;

        case CAP_FEEDERENABLED:
        case CAP_DUPLEX:
            /* Accepted; the M12 is sheet-fed and has no duplex. */
            break;

        default:
            cc = TWCC_CAPUNSUPPORTED;
            break;
        }
    } else if (cap->ConType == TWON_RANGE) {
        TW_RANGE *r = (TW_RANGE *)ptr;
        switch (cap->Cap) {
        case ICAP_XRESOLUTION:
        case ICAP_YRESOLUTION: {
            double d = fix32_to_double(r->CurrentValue);
            if (d < 100.0 || d > 600.0) {
                cc = TWCC_BADVALUE;
            } else {
                g_dpi = (int)(d + 0.5);
                cleanup_transfer();
            }
            break;
        }
        default:
            cc = TWCC_CAPUNSUPPORTED;
            break;
        }
    } else if (cap->ConType == TWON_ENUMERATION) {
        TW_ENUMERATION *e = (TW_ENUMERATION *)ptr;
        if (e->CurrentIndex >= e->NumItems) {
            cc = TWCC_BADVALUE;
        } else if (e->ItemType == TWTY_UINT16) {
            TW_UINT16 *items = (TW_UINT16 *)e->ItemList;
            TW_UINT16 value = items[e->CurrentIndex];
            switch (cap->Cap) {
            case ICAP_PIXELTYPE:
                if (value != TWPT_GRAY && value != TWPT_RGB) {
                    cc = TWCC_BADVALUE;
                } else {
                    g_pixel_type = value;
                    cleanup_transfer();
                }
                break;
            case ICAP_BITDEPTH:
                if (value != 8) {
                    cc = TWCC_BADVALUE;
                }
                break;
            default:
                cc = TWCC_CAPUNSUPPORTED;
                break;
            }
        } else {
            cc = TWCC_BADVALUE;
        }
    } else {
        cc = TWCC_CAPBADOPERATION;
    }

    GlobalUnlock(h);
    return cc;
}

static TW_UINT16 cap_query(pTW_CAPABILITY cap)
{
    TW_UINT16 *support;

    if (!cap || !cap->hContainer || !cap_is_supported(cap->Cap)) {
        return TWCC_CAPUNSUPPORTED;
    }

    /*
     * In MSG_QUERYSUPPORT, hContainer points to a TW_UINT16 that the DS must
     * fill with the TWQC_* mask. It is not a global memory handle.
     */
    support = (TW_UINT16 *)cap->hContainer;
    *support = (TW_UINT16)(TWQC_GET | TWQC_SET | TWQC_GETDEFAULT |
                           TWQC_GETCURRENT | TWQC_RESET);
    return TWCC_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Image                                                                     */
/* ------------------------------------------------------------------------- */

static int ensure_image(void)
{
    int rc;
    int pixel_type;

    if (g_image_valid) {
        return 1;
    }

    pixel_type = (g_pixel_type == TWPT_RGB) ? GT68XX_PIXEL_RGB : GT68XX_PIXEL_GRAY;

    if (g_test_mode) {
        rc = gt68xx_test_pattern(g_dpi, pixel_type, &g_image);
    } else if (g_dev) {
        rc = gt68xx_proto_scan(g_dev, g_dpi, pixel_type, &g_image);
    } else {
        set_condition(TWCC_OPERATIONERROR);
        return 0;
    }

    if (rc != 0) {
        ds_log("scan failed: %s", gt68xx_proto_last_error());
        set_condition(TWCC_OPERATIONERROR);
        return 0;
    }

    g_image_valid = TRUE;
    g_image_offset = 0;
    return 1;
}

static TW_UINT16 do_image_info(pTW_IMAGEINFO info)
{
    int w, h;
    int samples;

    if (!info) {
        return TWRC_FAILURE;
    }
    image_dimensions(&w, &h);
    samples = (g_pixel_type == TWPT_RGB) ? 3 : 1;

    memset(info, 0, sizeof(*info));
    info->XResolution = fix32_from_double((double)g_dpi);
    info->YResolution = fix32_from_double((double)g_dpi);
    info->ImageWidth = (TW_INT32)w;
    info->ImageLength = (TW_INT32)h;
    info->SamplesPerPixel = (TW_INT16)samples;
    info->BitsPerSample[0] = 8;
    if (samples == 3) {
        info->BitsPerSample[1] = 8;
        info->BitsPerSample[2] = 8;
    }
    info->BitsPerPixel = (TW_INT16)(8 * samples);
    info->Planar = FALSE;
    info->PixelType = (TW_INT16)g_pixel_type;
    info->Compression = TWCP_NONE;
    return TWRC_SUCCESS;
}

static TW_UINT16 do_image_layout(pTW_IMAGELAYOUT layout, TW_UINT16 msg)
{
    if (!layout) {
        return TWRC_FAILURE;
    }
    if (msg == MSG_GET) {
        layout->Frame.Left = fix32_from_double(0.0);
        layout->Frame.Top = fix32_from_double(0.0);
        layout->Frame.Right = fix32_from_double(210.0 / 25.4);
        layout->Frame.Bottom = fix32_from_double(297.0 / 25.4);
        layout->DocumentNumber = 1;
        layout->PageNumber = 1;
        layout->FrameNumber = 1;
    }
    /* MSG_SET: the area is accepted, but the scaffold uses the full area. */
    return TWRC_SUCCESS;
}

static TW_UINT16 do_mem_xfer(pTW_IMAGEMEMXFER xfer)
{
    size_t row;
    size_t remaining;
    size_t n;
    size_t rows;

    if (!xfer) {
        return TWRC_FAILURE;
    }
    if (!ensure_image()) {
        return TWRC_FAILURE;
    }
    if (g_image_offset >= g_image.size) {
        ds_log("mem_xfer: XFERDONE (size=%lu offset=%lu)",
               (unsigned long)g_image.size, (unsigned long)g_image_offset);
        memset(xfer, 0, sizeof(*xfer));
        return TWRC_XFERDONE;
    }

    ds_log("mem_xfer: flags=0x%08lX len=%lu theMem=%p image=%dx%d bpp=%d offset=%lu",
           (unsigned long)xfer->Memory.Flags,
           (unsigned long)xfer->Memory.Length,
           xfer->Memory.TheMem,
           g_image.width, g_image.height, g_image.bytes_per_pixel,
           (unsigned long)g_image_offset);

    row = (size_t)g_image.width * (size_t)g_image.bytes_per_pixel;
    remaining = g_image.size - g_image_offset;
    if (row == 0) {
        set_condition(TWCC_OPERATIONERROR);
        return TWRC_FAILURE;
    }

    if (xfer->Memory.TheMem && xfer->Memory.Length > 0) {
        n = xfer->Memory.Length;
        if (n > remaining) {
            n = remaining;
        }
        if (n < row) {
            set_condition(TWCC_LOWMEMORY);
            return TWRC_FAILURE;
        }
        n -= n % row;
        memcpy(xfer->Memory.TheMem, g_image.data + g_image_offset, n);
        xfer->Memory.Flags = TWMF_APPOWNS | TWMF_POINTER;
        xfer->Memory.Length = (TW_UINT32)n;
    } else {
        n = remaining;
        if (!g_ds_buffer) {
            g_ds_buffer = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, n);
        }
        if (!g_ds_buffer) {
            set_condition(TWCC_LOWMEMORY);
            return TWRC_FAILURE;
        }
        memcpy((void *)g_ds_buffer, g_image.data + g_image_offset, n);
        xfer->Memory.Flags = TWMF_DSOWNS | TWMF_POINTER;
        xfer->Memory.Length = (TW_UINT32)n;
        xfer->Memory.TheMem = (TW_MEMREF)g_ds_buffer;
    }

    rows = n / row;
    xfer->Compression = TWCP_NONE;
    xfer->BytesPerRow = (TW_UINT32)row;
    xfer->Columns = (TW_UINT32)g_image.width;
    xfer->Rows = (TW_UINT32)rows;
    xfer->XOffset = 0;
    xfer->YOffset = (TW_UINT32)(g_image_offset / row);
    xfer->BytesWritten = (TW_UINT32)n;

    g_image_offset += n;
    ds_log("mem_xfer: wrote=%lu rows=%lu newOffset=%lu",
           (unsigned long)n, (unsigned long)rows,
           (unsigned long)g_image_offset);
    return TWRC_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* DS_Entry                                                                  */
/* ------------------------------------------------------------------------- */

TW_UINT16 FAR PASCAL DS_Entry(pTW_IDENTITY pOrigin,
                              TW_UINT32 DG,
                              TW_UINT16 DAT,
                              TW_UINT16 MSG,
                              TW_MEMREF pData)
{
    (void)pOrigin;
    g_condition = TWCC_SUCCESS;
    ds_log("DS_Entry DG=0x%04lX DAT=0x%04X MSG=0x%04X pData=%p state=%u",
           (unsigned long)DG, DAT, MSG, pData, (unsigned)g_state);

    if (DG == DG_CONTROL) {
        switch (DAT) {
        case DAT_IDENTITY:
            if (MSG == MSG_GET) {
                if (!pData) {
                    set_condition(TWCC_BADVALUE);
                    return TWRC_FAILURE;
                }
                *(pTW_IDENTITY)pData = g_identity;
                return TWRC_SUCCESS;
            }
            if (MSG == MSG_OPENDS) {
                if (g_open) {
                    return TWRC_SUCCESS;
                }
                if (pOrigin) {
                    g_app_identity = *pOrigin;
                    g_have_app = TRUE;
                }
                if (pData) {
                    /* The DSM assigns the Source Id in its TW_IDENTITY. */
                    g_identity.Id = ((pTW_IDENTITY)pData)->Id;
                }
                ds_log("OPENDS src.Id=0x%08lX app.Id=0x%08lX app.Product=%s pData=%p",
                       (unsigned long)g_identity.Id,
                       (unsigned long)g_app_identity.Id,
                       g_app_identity.ProductName, pData);
                g_test_mode = env_flag(DS_TEST_ENV) ? TRUE : FALSE;
                ds_log("OPENDS test_mode=%d", (int)g_test_mode);
                if (!g_test_mode) {
                    if (gt68xx_usb_open(&g_dev) != 0) {
                        ds_log("OPENDS usb_open failed: %s",
                               gt68xx_usb_last_error());
                        set_condition(TWCC_OPERATIONERROR);
                        return TWRC_FAILURE;
                    }
                    if (gt68xx_proto_init(g_dev) != 0) {
                        ds_log("OPENDS proto_init failed: %s",
                               gt68xx_proto_last_error());
                        gt68xx_usb_close(g_dev);
                        g_dev = NULL;
                        set_condition(TWCC_OPERATIONERROR);
                        return TWRC_FAILURE;
                    }
                }
                g_open = TRUE;
                g_enabled = FALSE;
                g_state = 4;
                return TWRC_SUCCESS;
            }
            if (MSG == MSG_CLOSEDS) {
                cleanup_transfer();
                if (g_dev) {
                    gt68xx_usb_close(g_dev);
                    g_dev = NULL;
                }
                g_open = FALSE;
                g_enabled = FALSE;
                g_test_mode = FALSE;
                g_page_pending = FALSE;
                g_state = 3;
                return TWRC_SUCCESS;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case GT68XX_DAT_ENTRYPOINT:
            if (MSG == MSG_SET && pData) {
                TW_UINT32 size = *(TW_UINT32 *)pData;
                if (size > sizeof(g_entrypoint)) {
                    size = sizeof(g_entrypoint);
                }
                memset(&g_entrypoint, 0, sizeof(g_entrypoint));
                memcpy(&g_entrypoint, pData, size);
                g_dsm_entry = g_entrypoint.DSM_Entry;
                ds_log("DAT_ENTRYPOINT size=%lu DSM_Entry=%p",
                       (unsigned long)size, (void *)g_dsm_entry);
                return TWRC_SUCCESS;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_STATUS:
            if (MSG == MSG_GET && pData) {
                pTW_STATUS status = (pTW_STATUS)pData;
                status->ConditionCode = g_condition;
                status->Reserved = 0;
                return TWRC_SUCCESS;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_CAPABILITY:
            if (MSG == MSG_GET || MSG == MSG_GETCURRENT || MSG == MSG_GETDEFAULT) {
                TW_UINT16 cc = cap_get((pTW_CAPABILITY)pData, MSG);
                if (cc == TWCC_SUCCESS) {
                    return TWRC_SUCCESS;
                }
                set_condition(cc);
                return TWRC_FAILURE;
            }
            if (MSG == MSG_SET) {
                TW_UINT16 cc = cap_set((pTW_CAPABILITY)pData);
                if (cc == TWCC_SUCCESS) {
                    return TWRC_SUCCESS;
                }
                set_condition(cc);
                return TWRC_FAILURE;
            }
            if (MSG == MSG_RESET) {
                reset_caps();
                return TWRC_SUCCESS;
            }
            if (MSG == MSG_QUERYSUPPORT) {
                TW_UINT16 cc = cap_query((pTW_CAPABILITY)pData);
                if (cc == TWCC_SUCCESS) {
                    return TWRC_SUCCESS;
                }
                set_condition(cc);
                return TWRC_FAILURE;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_USERINTERFACE:
            if (MSG == MSG_ENABLEDS && pData) {
                pTW_USERINTERFACE ui = (pTW_USERINTERFACE)pData;
                ui->ShowUI = FALSE;
                ui->ModalUI = FALSE;
                g_enabled = TRUE;
                g_page_pending = TRUE;
                g_state = 5;
                notify_xfer_ready();
                return TWRC_SUCCESS;
            }
            if (MSG == MSG_DISABLEDS) {
                g_enabled = FALSE;
                g_page_pending = FALSE;
                cleanup_transfer();
                g_state = 4;
                if (g_xfer_thread) {
                    CloseHandle(g_xfer_thread);
                    g_xfer_thread = NULL;
                }
                return TWRC_SUCCESS;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_PENDINGXFERS:
            if (MSG == MSG_ENDXFER && pData) {
                pTW_PENDINGXFERS pending = (pTW_PENDINGXFERS)pData;
                cleanup_transfer();
                g_page_pending = FALSE;
                pending->Count = 0;
                pending->EOJ = 0;
                return TWRC_SUCCESS;
            }
            if (MSG == MSG_GET && pData) {
                pTW_PENDINGXFERS pending = (pTW_PENDINGXFERS)pData;
                pending->Count = g_page_pending ? 1 : 0;
                pending->EOJ = 0;
                return TWRC_SUCCESS;
            }
            if (MSG == MSG_RESET && pData) {
                pTW_PENDINGXFERS pending = (pTW_PENDINGXFERS)pData;
                cleanup_transfer();
                g_page_pending = FALSE;
                pending->Count = 0;
                pending->EOJ = 0;
                return TWRC_SUCCESS;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_SETUPMEMXFER:
            if (MSG == MSG_GET && pData) {
                pTW_SETUPMEMXFER setup = (pTW_SETUPMEMXFER)pData;
                setup->MinBufSize = 64 * 1024;
                setup->MaxBufSize = 16 * 1024 * 1024;
                setup->Preferred = 1024 * 1024;
                return TWRC_SUCCESS;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

#ifdef DAT_ENTRYPOINT
        case DAT_ENTRYPOINT:
            /* TWAIN 2.x: accepted, but this DS uses 1.x global memory. */
            return TWRC_SUCCESS;
#endif

        case DAT_XFERGROUP:
            if (MSG == MSG_GET && pData) {
                *(TW_UINT32 *)pData = 0;
                return TWRC_SUCCESS;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_EVENT:
            if (MSG == MSG_PROCESSEVENT) {
                return TWRC_NOTDSEVENT;
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        default:
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;
        }
    }

    if (DG == DG_IMAGE) {
        switch (DAT) {
        case DAT_IMAGEINFO:
            if (MSG == MSG_GET) {
                return do_image_info((pTW_IMAGEINFO)pData);
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_IMAGELAYOUT:
            if (MSG == MSG_GET || MSG == MSG_SET) {
                return do_image_layout((pTW_IMAGELAYOUT)pData, MSG);
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_IMAGEMEMXFER:
            if (MSG == MSG_GET) {
                return do_mem_xfer((pTW_IMAGEMEMXFER)pData);
            }
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;

        case DAT_IMAGENATIVEXFER:
            set_condition(TWCC_CAPUNSUPPORTED);
            return TWRC_FAILURE;

        default:
            set_condition(TWCC_BADPROTOCOL);
            return TWRC_FAILURE;
        }
    }

    set_condition(TWCC_BADPROTOCOL);
    return TWRC_FAILURE;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)hinst;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        init_identity();
        memset(&g_image, 0, sizeof(g_image));
    } else if (reason == DLL_PROCESS_DETACH) {
        cleanup_transfer();
        if (g_dev) {
            gt68xx_usb_close(g_dev);
            g_dev = NULL;
        }
    }
    return TRUE;
}
